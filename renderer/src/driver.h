#pragma once

#include "cava.h"
#include "motion.h"

#include <QColor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QTimer>

class QQmlEngine;
class QQuickView;

// One orb on the desktop, as placed in Noctalia's desktop-widget editor.
struct Placement {
    QString id;
    QString output;
    QRect rect; // logical px, relative to the output's top-left

    bool operator==(const Placement &o) const { return id == o.id && output == o.output && rect == o.rect; }
};

// Owns everything that is shared between orbs: the config file written by the
// Noctalia plugin, CAVA, the motion model and the frame clock. QML binds to
// its properties; one layer-shell window is kept per placement.
class Driver : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal amplitude READ amplitude NOTIFY frameAdvanced)
    Q_PROPERTY(qreal noiseOffset READ noiseOffset NOTIFY frameAdvanced)
    Q_PROPERTY(QColor color READ color NOTIFY styleChanged)
    Q_PROPERTY(QColor glowColor READ glowColor NOTIFY styleChanged)
    Q_PROPERTY(int dotCount READ dotCount NOTIFY styleChanged)
    Q_PROPERTY(qreal dotScale READ dotScale NOTIFY styleChanged)
    Q_PROPERTY(qreal glowScale READ glowScale NOTIFY styleChanged)
    Q_PROPERTY(qreal orbScale READ orbScale NOTIFY styleChanged)
    Q_PROPERTY(qreal opacity READ opacity NOTIFY styleChanged)
    Q_PROPERTY(bool hidden READ hidden NOTIFY hiddenChanged)

public:
    Driver(QQmlEngine *engine, const QString &configPath, QObject *parent = nullptr);
    ~Driver() override;

    bool start();

    qreal amplitude() const { return m_motion.amplitude; }
    qreal noiseOffset() const { return m_motion.noiseOffset; }
    QColor color() const { return m_color; }
    QColor glowColor() const { return m_glowColor; }
    int dotCount() const { return m_dotCount; }
    qreal dotScale() const { return m_dotScale; }
    qreal glowScale() const { return m_glowScale; }
    qreal orbScale() const { return m_orbScale; }
    qreal opacity() const { return m_opacity; }
    bool hidden() const { return m_hideWhenIdle && m_idle; }

Q_SIGNALS:
    void frameAdvanced();
    void styleChanged();
    void hiddenChanged();

private:
    void pollConfig();
    bool loadConfig();
    void applyPlacements(const QList<Placement> &placements, bool remap);
    QQuickView *createWindow(const Placement &p);
    void placeWindow(QQuickView *view, const Placement &p);
    void onCavaFrame(const std::vector<int> &values);
    void tick();
    void setIdle(bool idle);
    void checkHeartbeat();

    QQmlEngine *m_engine;
    QString m_configPath;
    QDateTime m_configMtime;
    qint64 m_configSize = -1;

    QString m_heartbeatPath;
    int m_heartbeatTimeout = 20;

    Motion m_motion;
    Cava m_cava;
    QTimer m_tick;
    QTimer m_poll;
    QTimer m_idleTimer;
    QElapsedTimer m_clock;

    double m_gain = 1.0;
    double m_level = 0.0;
    double m_bass = 0.0;
    int m_fps = 60;
    int m_idleSeconds = 5;
    bool m_hideWhenIdle = false;
    bool m_idle = true;
    int m_layer = 1; // LayerShellQt::Window::LayerBottom
    qint64 m_remap = -1;

    QColor m_color = QColor(QStringLiteral("#ff2e88"));
    QColor m_glowColor = QColor(QStringLiteral("#ff2e88"));
    int m_dotCount = 322;
    qreal m_dotScale = 1.0;
    qreal m_glowScale = 1.0;
    qreal m_orbScale = 1.0;
    qreal m_opacity = 1.0;

    QHash<QString, QPointer<QQuickView>> m_windows;
    QHash<QString, Placement> m_placements;
};
