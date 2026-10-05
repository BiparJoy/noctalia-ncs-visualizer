#pragma once

#include <deque>
#include <utility>
#include <vector>

// What drives the visualizer. A line-for-line port of the Plasma widget's
// package/contents/ui/code/ncs.js; see that file for the reasoning.
//
// Upstream (spicetify-visualizer) derives its uniforms from Spotify's loudness
// curve as
//     uAmplitude   = sampleAmplitudeMovingAverage(curve, t, 0.15)
//     uNoiseOffset = (0.5 * t + integral(curve, t)) * 75 * 0.01
// ModelOriginal reproduces that from live loudness with a trailing box filter.
// ModelAurora is the spring + beat-punch model from the Aurora player.
class Motion
{
public:
    enum Model { ModelOriginal = 0, ModelAurora = 1 };

    int model = ModelOriginal;

    // ModelOriginal
    double averageWindow = 0.15;
    double flowSpeed = 1.0;

    // ModelAurora
    double bodyStiffness = 20.0;
    double bodyDamping = 0.85;
    double levelSmooth = 6.5;
    double punch = 0.5;
    double beatDecay = 7.0;
    double onset = 1.18;

    double amplitude = 0.15; // -> uAmplitude
    double noiseOffset = 0.0; // -> uNoiseOffset

    void setAudio(double level, double bass);
    void update(double dt);

private:
    double movingAverage(double dt, double level);

    std::deque<std::pair<double, double>> m_hist;
    double m_histSum = 0.0;
    double m_histTime = 0.0;

    double m_amp = 0.15;
    double m_vel = 0.0;
    double m_smoothTarget = 0.15;
    double m_beat = 0.0;
    double m_bassAvg = 0.0;
    double m_cooldown = 0.0;
    double m_level = 0.15;
    double m_bass = 0.0;
};

struct AudioReading {
    double level = 0.0;
    double bass = 0.0;
};

// CAVA hands us a bar spectrum; reduce it to one loudness plus a bass reading.
// Port of ncs.js analyseBars().
AudioReading analyseBars(const std::vector<int> &values, int maxRange, double gain, bool stereo);
