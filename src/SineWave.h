#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Wave form generation. This header does not depend on Arduino / M5Unified,
// so it can be built and tested on a PC.
namespace stackchan::voice
{
    /**
     * @brief Number of samples of a sound
     *
     * @param duration_ms duration [msec]
     * @param sample_rate sampling rate [Hz]
     */
    inline size_t sampleCount(uint32_t duration_ms, uint32_t sample_rate)
    {
        return static_cast<size_t>(static_cast<uint64_t>(duration_ms) * sample_rate / 1000);
    }

    /**
     * @brief Generate a mono sine wave of signed 16 bit samples
     *
     * The wave fades in and out linearly to avoid clicks at the start and the end of the sound.
     *
     * @param out destination of `length` samples
     * @param length number of samples
     * @param frequency frequency of the wave [Hz]
     * @param sample_rate sampling rate [Hz]
     * @param amplitude 0.0 (silent) to 1.0 (full scale)
     * @param fade_ms length of the fade in and the fade out [msec]
     */
    inline void generateSine(int16_t *out, size_t length, float frequency, uint32_t sample_rate,
                             float amplitude = 0.5f, uint32_t fade_ms = 5)
    {
        if (out == nullptr || length == 0 || sample_rate == 0)
        {
            return;
        }
        if (amplitude < 0.0f)
        {
            amplitude = 0.0f;
        }
        if (amplitude > 1.0f)
        {
            amplitude = 1.0f;
        }

        size_t fade = sampleCount(fade_ms, sample_rate);
        if (fade * 2 > length)
        {
            fade = length / 2; // the fades must not overlap
        }

        const double w = 2.0 * M_PI * static_cast<double>(frequency) / static_cast<double>(sample_rate);
        for (size_t i = 0; i < length; i++)
        {
            double envelope = 1.0;
            if (i < fade)
            {
                envelope = static_cast<double>(i) / static_cast<double>(fade);
            }
            else if (i >= length - fade)
            {
                envelope = static_cast<double>(length - 1 - i) / static_cast<double>(fade);
            }
            out[i] = static_cast<int16_t>(32767.0 * amplitude * envelope * sin(w * static_cast<double>(i)));
        }
    }
} // namespace stackchan::voice
