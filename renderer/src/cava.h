#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <vector>

// Runs CAVA in raw ASCII mode and emits each frame of bar values.
// Settings mirror the Plasma widget's defaults.
struct CavaSettings {
    int framerate = 60;
    int bars = 32;
    int noiseReduction = 55;
    int lowerCutoff = 50;
    int higherCutoff = 10000;
    QString method;  // empty = CAVA's autodetect (PipeWire / Pulse)
    QString source;  // empty = default output monitor

    bool operator==(const CavaSettings &o) const
    {
        return framerate == o.framerate && bars == o.bars && noiseReduction == o.noiseReduction
            && lowerCutoff == o.lowerCutoff && higherCutoff == o.higherCutoff && method == o.method
            && source == o.source;
    }
    bool operator!=(const CavaSettings &o) const { return !(*this == o); }
};

class Cava : public QObject
{
    Q_OBJECT
public:
    static constexpr int MaxRange = 1000;

    explicit Cava(QObject *parent = nullptr);
    ~Cava() override;

    void setSettings(const CavaSettings &settings);
    void start();
    void stop();

Q_SIGNALS:
    void frame(const std::vector<int> &values);

private:
    void writeConfig();
    void onReadyRead();

    CavaSettings m_settings;
    QProcess m_process;
    QTimer m_restart;
    QByteArray m_buffer;
    QString m_configPath;
    bool m_wanted = false;
};
