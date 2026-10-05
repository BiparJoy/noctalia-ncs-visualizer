#include "driver.h"

#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>

#include <LayerShellQt/window.h>

#include <algorithm>

Driver::Driver(QQmlEngine *engine, const QString &configPath, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_configPath(configPath)
{
    connect(&m_cava, &Cava::frame, this, &Driver::onCavaFrame);

    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &Driver::tick);

    // The config is tiny; polling its mtime is simpler and more robust than a
    // file watcher across editors and atomic replaces.
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        pollConfig();
        checkHeartbeat();
    });

    m_idleTimer.setSingleShot(true);
    connect(&m_idleTimer, &QTimer::timeout, this, [this] { setIdle(true); });
}

Driver::~Driver()
{
    m_cava.stop();
    for (const auto &w : std::as_const(m_windows)) {
        delete w.data();
    }
}

bool Driver::start()
{
    if (!loadConfig()) {
        qWarning("aurora-ncs: cannot read config %s", qPrintable(m_configPath));
        return false;
    }
    m_poll.start();
    m_cava.start();
    return true;
}

void Driver::pollConfig()
{
    const QFileInfo fi(m_configPath);
    if (!fi.exists()) {
        // The plugin removes the config when the last orb is taken off the desktop.
        qInfo("aurora-ncs: config removed, exiting");
        QGuiApplication::quit();
        return;
    }
    if (fi.lastModified() != m_configMtime || fi.size() != m_configSize) {
        loadConfig();
    }
}

void Driver::checkHeartbeat()
{
    if (m_heartbeatPath.isEmpty()) {
        return;
    }
    const QFileInfo fi(m_heartbeatPath);
    if (!fi.exists() || fi.lastModified().secsTo(QDateTime::currentDateTime()) > m_heartbeatTimeout) {
        // Noctalia is gone (or the plugin was disabled): don't linger.
        qInfo("aurora-ncs: no heartbeat from the Noctalia plugin, exiting");
        QGuiApplication::quit();
    }
}

bool Driver::loadConfig()
{
    QFile f(m_configPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QFileInfo fi(m_configPath);
    m_configMtime = fi.lastModified();
    m_configSize = fi.size();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        // Probably caught mid-write; the next poll will retry.
        m_configMtime = {};
        return true;
    }
    const QJsonObject o = doc.object();

    auto num = [&o](const char *key, double def) { return o.value(QLatin1String(key)).toDouble(def); };

    m_heartbeatPath = o.value(QLatin1String("heartbeatFile")).toString();
    m_heartbeatTimeout = o.value(QLatin1String("heartbeatTimeout")).toInt(20);

    // style
    const QColor color(o.value(QLatin1String("color")).toString());
    const QColor glow(o.value(QLatin1String("glowColor")).toString());
    m_color = color.isValid() ? color : QColor(QStringLiteral("#ff2e88"));
    m_glowColor = glow.isValid() ? glow : m_color;
    m_dotCount = std::clamp(o.value(QLatin1String("dotCount")).toInt(322), 64, 512);
    m_dotScale = std::clamp(num("dotScale", 1.0), 0.2, 4.0);
    m_glowScale = std::clamp(num("glowScale", 1.0), 0.0, 4.0);
    m_orbScale = std::clamp(num("orbScale", 1.0), 0.2, 1.0);
    m_opacity = std::clamp(num("opacity", 1.0), 0.05, 1.0);
    Q_EMIT styleChanged();

    // motion
    m_motion.model = o.value(QLatin1String("motionModel")).toString() == QLatin1String("aurora") ? Motion::ModelAurora
                                                                                                 : Motion::ModelOriginal;
    m_motion.averageWindow = std::clamp(num("averageWindow", 0.15), 0.02, 2.0);
    m_motion.flowSpeed = std::clamp(num("flowSpeed", 1.0), 0.0, 5.0);
    m_motion.bodyStiffness = std::clamp(num("bodyStiffness", 20.0), 1.0, 200.0);
    m_motion.bodyDamping = std::clamp(num("bodyDamping", 0.85), 0.1, 2.0);
    m_motion.levelSmooth = std::clamp(num("levelSmooth", 6.5), 0.5, 30.0);
    m_motion.punch = std::clamp(num("punch", 0.5), 0.0, 2.0);
    m_motion.beatDecay = std::clamp(num("beatDecay", 7.0), 0.5, 30.0);
    m_motion.onset = std::clamp(num("onset", 1.18), 1.0, 3.0);
    m_gain = std::clamp(num("gain", 1.0), 0.1, 5.0);

    // timing
    m_fps = std::clamp(o.value(QLatin1String("fps")).toInt(60), 10, 240);
    m_tick.setInterval(1000 / m_fps);
    m_idleSeconds = std::clamp(o.value(QLatin1String("idleSeconds")).toInt(5), 1, 600);
    const bool hideWas = hidden();
    m_hideWhenIdle = o.value(QLatin1String("hideWhenIdle")).toBool(false);
    if (hideWas != hidden()) {
        Q_EMIT hiddenChanged();
    }

    // layer: "background" | "bottom" (behind windows, default) | "top" (above windows)
    const QString layer = o.value(QLatin1String("layer")).toString();
    const int newLayer = layer == QLatin1String("top") ? LayerShellQt::Window::LayerTop
        : layer == QLatin1String("background")         ? LayerShellQt::Window::LayerBackground
                                                       : LayerShellQt::Window::LayerBottom;
    if (newLayer != m_layer) {
        m_layer = newLayer;
        for (const auto &w : std::as_const(m_windows)) {
            if (w) {
                LayerShellQt::Window::get(w)->setLayer(static_cast<LayerShellQt::Window::Layer>(m_layer));
            }
        }
    }

    // audio
    const QJsonObject c = o.value(QLatin1String("cava")).toObject();
    CavaSettings cs;
    cs.framerate = std::max(m_fps, 30);
    cs.bars = std::clamp(c.value(QLatin1String("bars")).toInt(32), 8, 256);
    cs.noiseReduction = std::clamp(c.value(QLatin1String("noiseReduction")).toInt(55), 0, 100);
    cs.lowerCutoff = std::clamp(c.value(QLatin1String("lowerCutoff")).toInt(50), 10, 1000);
    cs.higherCutoff = std::clamp(c.value(QLatin1String("higherCutoff")).toInt(10000), 1000, 22000);
    cs.method = c.value(QLatin1String("method")).toString();
    cs.source = c.value(QLatin1String("source")).toString();
    m_cava.setSettings(cs);

    // placements
    QList<Placement> placements;
    for (const QJsonValue &v : o.value(QLatin1String("instances")).toArray()) {
        const QJsonObject i = v.toObject();
        Placement p;
        p.id = i.value(QLatin1String("id")).toString();
        p.output = i.value(QLatin1String("output")).toString();
        p.rect = QRect(qRound(i.value(QLatin1String("x")).toDouble()), qRound(i.value(QLatin1String("y")).toDouble()),
                       std::max(16, qRound(i.value(QLatin1String("width")).toDouble())),
                       std::max(16, qRound(i.value(QLatin1String("height")).toDouble())));
        if (!p.id.isEmpty()) {
            placements.append(p);
        }
    }
    // Noctalia re-creates a widget's surface on layout changes, and the new
    // surface stacks above ours; the plugin bumps "remap" so we map again on top.
    const qint64 remap = static_cast<qint64>(o.value(QLatin1String("remap")).toDouble(0));
    const bool doRemap = m_remap >= 0 && remap != m_remap;
    m_remap = remap;
    applyPlacements(placements, doRemap);

    if (!m_idle && !m_tick.isActive()) {
        m_tick.start();
    }
    return true;
}

static QScreen *screenNamed(const QString &name)
{
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        if (s->name() == name) {
            return s;
        }
    }
    return QGuiApplication::primaryScreen();
}

void Driver::placeWindow(QQuickView *view, const Placement &p)
{
    auto *lw = LayerShellQt::Window::get(view);
    lw->setMargins(QMargins(p.rect.x(), p.rect.y(), 0, 0));
    view->resize(p.rect.size());
}

QQuickView *Driver::createWindow(const Placement &p)
{
    auto *view = new QQuickView(m_engine, nullptr);
    view->setColor(Qt::transparent);
    // Click-through: the desktop and Noctalia's widget editor stay usable.
    view->setFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setScreen(screenNamed(p.output));

    auto *lw = LayerShellQt::Window::get(view);
    lw->setScope(QStringLiteral("aurora-ncs"));
    lw->setLayer(static_cast<LayerShellQt::Window::Layer>(m_layer));
    lw->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorLeft));
    lw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    // -1: ignore the bar's exclusive zone, so margins are relative to the
    // output edge exactly like Noctalia's desktop-widget coordinates.
    lw->setExclusiveZone(-1);
    lw->setWantsToBeOnActiveScreen(false);
    lw->setScreen(screenNamed(p.output));
    placeWindow(view, p);

    view->loadFromModule(QStringLiteral("AuroraNcs"), QStringLiteral("Orb"));
    if (view->status() == QQuickView::Error) {
        for (const auto &e : view->errors()) {
            qWarning("aurora-ncs: %s", qPrintable(e.toString()));
        }
    }
    view->show();
    return view;
}

void Driver::applyPlacements(const QList<Placement> &placements, bool remap)
{
    QSet<QString> seen;
    for (const Placement &p : placements) {
        seen.insert(p.id);
        QPointer<QQuickView> view = m_windows.value(p.id);
        const auto old = m_placements.find(p.id);
        if (!remap && view && old != m_placements.end() && *old == p) {
            continue;
        }
        if (!remap && view && old != m_placements.end() && old->output == p.output) {
            placeWindow(view, p);
        } else {
            // Map the replacement before dropping the old window: no blank frame.
            QQuickView *fresh = createWindow(p);
            delete view.data();
            view = fresh;
        }
        m_windows.insert(p.id, view);
        m_placements.insert(p.id, p);
    }
    for (auto it = m_windows.begin(); it != m_windows.end();) {
        if (!seen.contains(it.key())) {
            delete it.value().data();
            m_placements.remove(it.key());
            it = m_windows.erase(it);
        } else {
            ++it;
        }
    }
    if (m_windows.isEmpty()) {
        qInfo("aurora-ncs: no orbs placed, exiting");
        QGuiApplication::quit();
    }
}

void Driver::onCavaFrame(const std::vector<int> &values)
{
    const AudioReading a = analyseBars(values, Cava::MaxRange, m_gain, false);
    m_level = a.level;
    m_bass = a.bass;
    const bool sound = std::any_of(values.begin(), values.end(), [](int v) { return v > 0; });
    if (sound) {
        setIdle(false);
        m_idleTimer.start(m_idleSeconds * 1000);
    }
}

void Driver::setIdle(bool idle)
{
    if (m_idle == idle) {
        return;
    }
    const bool hideWas = hidden();
    m_idle = idle;
    if (idle) {
        // Hold the last frame; nothing to animate without sound.
        m_tick.stop();
    } else {
        m_clock.restart();
        m_tick.start();
    }
    if (hideWas != hidden()) {
        Q_EMIT hiddenChanged();
    }
}

void Driver::tick()
{
    const double dt = m_clock.isValid() ? std::min(m_clock.restart() / 1000.0, 0.25) : 1.0 / m_fps;
    if (!m_clock.isValid()) {
        m_clock.start();
    }
    m_motion.setAudio(m_level, m_bass);
    m_motion.update(dt);
    Q_EMIT frameAdvanced();
}
