#include "cava.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

Cava::Cava(QObject *parent)
    : QObject(parent)
{
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    m_configPath = QDir(runtime.isEmpty() ? QDir::tempPath() : runtime)
                       .filePath(QStringLiteral("aurora-ncs-%1.cava").arg(QCoreApplication::applicationPid()));

    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &Cava::onReadyRead);
    connect(&m_process, &QProcess::finished, this, [this] {
        // CAVA exits when PipeWire restarts or the source disappears.
        if (m_wanted) {
            m_restart.start();
        }
    });

    m_restart.setSingleShot(true);
    m_restart.setInterval(2000);
    connect(&m_restart, &QTimer::timeout, this, &Cava::start);
}

Cava::~Cava()
{
    stop();
    QFile::remove(m_configPath);
}

void Cava::setSettings(const CavaSettings &settings)
{
    if (settings == m_settings && m_process.state() != QProcess::NotRunning) {
        return;
    }
    m_settings = settings;
    if (m_wanted) {
        stop();
        start();
    }
}

void Cava::writeConfig()
{
    QString c;
    c += QStringLiteral("[general]\nframerate=%1\nbars=%2\nautosens=1\nlower_cutoff_freq=%3\nhigher_cutoff_freq=%4\nsleep_timer=5\n")
             .arg(m_settings.framerate)
             .arg(m_settings.bars)
             .arg(m_settings.lowerCutoff)
             .arg(m_settings.higherCutoff);
    c += QStringLiteral("[input]\n");
    if (!m_settings.method.isEmpty()) {
        c += QStringLiteral("method=%1\n").arg(m_settings.method);
    }
    if (!m_settings.source.isEmpty()) {
        c += QStringLiteral("source=%1\n").arg(m_settings.source);
    }
    c += QStringLiteral("sample_rate=44100\nsample_bits=16\nchannels=2\nautoconnect=2\nactive=0\nremix=1\nvirtual=1\n");
    c += QStringLiteral("[output]\nchannels=mono\nmono_option=average\nreverse=0\nmethod=raw\nraw_target=/dev/stdout\n"
                        "data_format=ascii\nascii_max_range=%1\nwaveform=0\n")
             .arg(MaxRange);
    c += QStringLiteral("[smoothing]\nnoise_reduction=%1\nmonstercat=0\nwaves=0\n").arg(m_settings.noiseReduction);

    QFile f(m_configPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(c.toUtf8());
    }
}

void Cava::start()
{
    m_wanted = true;
    if (m_process.state() != QProcess::NotRunning) {
        return;
    }
    writeConfig();
    m_buffer.clear();
    m_process.start(QStringLiteral("cava"), {QStringLiteral("-p"), m_configPath});
}

void Cava::stop()
{
    m_wanted = false;
    m_restart.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(1000)) {
            m_process.kill();
            m_process.waitForFinished(500);
        }
    }
}

void Cava::onReadyRead()
{
    m_buffer += m_process.readAllStandardOutput();
    const int end = m_buffer.lastIndexOf('\n');
    if (end < 0) {
        return;
    }
    // Only the newest complete frame matters.
    const int start = m_buffer.lastIndexOf('\n', end - 1) + 1;
    QByteArray line = m_buffer.mid(start, end - start).trimmed();
    m_buffer.remove(0, end + 1);
    if (line.endsWith(';')) {
        line.chop(1);
    }
    if (line.isEmpty()) {
        return;
    }
    std::vector<int> values;
    const auto parts = line.split(';');
    values.reserve(parts.size());
    for (const QByteArray &p : parts) {
        values.push_back(p.toInt());
    }
    Q_EMIT frame(values);
}
