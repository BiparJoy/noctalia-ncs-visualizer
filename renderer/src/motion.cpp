#include "motion.h"

#include <algorithm>
#include <cmath>

namespace
{
double clamp01(double v)
{
    return std::min(1.0, std::max(0.0, v));
}
}

double Motion::movingAverage(double dt, double level)
{
    m_hist.emplace_back(dt, level);
    m_histSum += dt * level;
    m_histTime += dt;
    while (m_histTime > averageWindow && m_hist.size() > 1) {
        const auto [d, v] = m_hist.front();
        m_hist.pop_front();
        m_histSum -= d * v;
        m_histTime -= d;
    }
    return m_histTime > 0 ? m_histSum / m_histTime : level;
}

void Motion::setAudio(double level, double bass)
{
    m_level = clamp01(level);
    m_bass = clamp01(bass);
}

void Motion::update(double dt)
{
    dt = std::min(dt, 1.0 / 30.0);

    if (model == ModelOriginal) {
        amplitude = movingAverage(dt, m_level);
        // upstream: d/dt of (0.5 * t + integral(amplitude)) * 75 * 0.01
        noiseOffset += dt * 0.75 * (0.5 + amplitude) * flowSpeed;
        // keep the other model's state coherent in case it is switched on
        m_amp = amplitude;
        m_smoothTarget = amplitude;
        m_beat = 0;
        return;
    }

    m_beat *= std::exp(-dt * beatDecay);
    m_bassAvg += (m_bass - m_bassAvg) * std::min(1.0, dt * 3);
    m_cooldown -= dt;
    if (m_bass > m_bassAvg * onset + 0.015 && m_bass > 0.08 && m_cooldown <= 0) {
        m_beat = 1;
        m_cooldown = 0.11;
    }

    m_smoothTarget += (m_level - m_smoothTarget) * std::min(1.0, dt * levelSmooth);
    const double k = bodyStiffness;
    const double c = 2 * std::sqrt(k) * bodyDamping;
    const double a = k * (m_smoothTarget - m_amp) - c * m_vel;
    m_vel += a * dt;
    m_amp = clamp01(m_amp + m_vel * dt);

    amplitude = std::min(1.0, m_amp + punch * m_beat);
    noiseOffset += dt * (0.05 + 2.6 * m_amp + 4.5 * m_beat) * flowSpeed;
}

AudioReading analyseBars(const std::vector<int> &values, int maxRange, double gain, bool stereo)
{
    const int n = static_cast<int>(values.size());
    if (n == 0) {
        return {};
    }
    const double inv = 1.0 / std::max(1, maxRange);

    double sum = 0;
    for (int v : values) {
        const double x = v * inv;
        sum += x * x;
    }
    const double level = std::min(1.0, std::sqrt(sum / n) * 1.9 * gain);

    // lowest ~18% of the spectrum
    const int span = std::max(1, static_cast<int>(std::lround(n * 0.18)));
    int lo, hi;
    if (stereo) {
        // CAVA lays stereo out as [left reversed | right forward]: the bass of
        // both channels meets either side of the centre.
        const int mid = n >> 1;
        const int half = std::max(1, static_cast<int>(std::lround(span / 2.0)));
        lo = std::max(0, mid - half);
        hi = std::min(n, mid + half);
    } else {
        lo = 0;
        hi = span;
    }

    double bs = 0;
    for (int i = lo; i < hi; ++i) {
        bs += values[i] * inv;
    }
    const double bass = std::min(1.0, (bs / (hi - lo)) * 1.5 * gain);
    return {level, bass};
}
