// aurora-ncs: the Aurora NCS Visualizer as a standalone Wayland layer-shell
// renderer, driven by the Noctalia plugin "tausif/ncs-visualizer".
//
//   aurora-ncs --config <file.json>
//
// The plugin writes the config (where each orb is, colours, motion settings)
// and keeps a heartbeat file fresh; this process follows the config live and
// exits when the config disappears, the heartbeat stops, or no orbs are left.

#include "driver.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QLockFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QSurfaceFormat>

#include <csignal>

int main(int argc, char *argv[])
{
    // The renderer is a QQuickFramebufferObject: it needs Qt Quick on OpenGL.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    // Layer-shell only makes sense on Wayland.
    qputenv("QT_QPA_PLATFORM", "wayland");

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("aurora-ncs"));
    QGuiApplication::setApplicationVersion(QStringLiteral(AURORA_NCS_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("aurora-ncs"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Aurora NCS Visualizer renderer for Noctalia"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOpt({QStringLiteral("c"), QStringLiteral("config")},
                                 QStringLiteral("JSON config written by the Noctalia plugin."),
                                 QStringLiteral("file"));
    parser.addOption(configOpt);
    parser.process(app);

    const QString config = parser.value(configOpt);
    if (config.isEmpty()) {
        parser.showHelp(2);
    }

    // One renderer per config file, so repeated launches from the plugin are harmless.
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString lockName = QStringLiteral("aurora-ncs-%1.lock")
                                 .arg(QString::number(qHash(QDir(config).absolutePath()), 16));
    QLockFile lock(QDir(runtime.isEmpty() ? QDir::tempPath() : runtime).filePath(lockName));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        qInfo("aurora-ncs: already running for %s", qPrintable(config));
        return 0;
    }

    // Leave cleanly (stopping CAVA) when the plugin pkills us.
    std::signal(SIGTERM, [](int) { QGuiApplication::quit(); });
    std::signal(SIGINT, [](int) { QGuiApplication::quit(); });

    QQmlEngine engine;
    Driver driver(&engine, config);
    engine.rootContext()->setContextProperty(QStringLiteral("driver"), &driver);

    if (!driver.start()) {
        return 1;
    }
    QGuiApplication::setQuitOnLastWindowClosed(false);
    return app.exec();
}
