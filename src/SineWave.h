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
     * @brief One sine wave component of a sound
     *
     * A real voice is a sum of waves of many frequencies. A sound is described as a list of partials,
     * and the waves are added by generateSineMix().
     */
    struct Partial
    {
        float frequency = 0.0f; ///< frequency [Hz]
        float amplitude = 1.0f; ///< relative amplitude. Only the ratio between the partials matters.
    };

    /// the most partials that are added. The partials after this are ignored.
    constexpr size_t kMaxPartials = 16;

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
     * @brief Check if a partial can be played: its frequency is positive and below the Nyquist frequency,
     * and its amplitude is positive.
     */
    inline bool isValidPartial(const Partial &partial, uint32_t sample_rate)
    {
        return partial.frequency > 0.0f && partial.frequency < sample_rate / 2.0f && partial.amplitude > 0.0f;
    }

    /**
     * @brief Generate a mono wave of signed 16 bit samples by adding sine waves of several frequencies
     *
     * - The sum is normalized by the total amplitude of the partials, so it never exceeds `amplitude`
     *   and never clips, however many partials are added.
     * - Invalid partials (see isValidPartial()) are skipped. If no partial is valid, the output is silent.
     * - The wave fades in and out linearly to avoid clicks at the start and the end of the sound.
     *
     * @param out destination of `length` samples
     * @param length number of samples
     * @param partials the sine waves to add
     * @param count number of the partials. At most `kMaxPartials` are used.
     * @param sample_rate sampling rate [Hz]
     * @param amplitude 0.0 (silent) to 1.0 (full scale)
     * @param fade_ms length of the fade in and the fade out [msec]
     */
    inline void generateSineMix(int16_t *out, size_t length, const Partial *partials, size_t count,
                                uint32_t sample_rate, float amplitude = 0.5f, uint32_t fade_ms = 5)
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
        if (count > kMaxPartials)
        {
            count = kMaxPartials;
        }

        // collect the valid partials
        float phase[kMaxPartials];
        float step[kMaxPartials];   // phase advance per sample [rad]
        float weight[kMaxPartials]; // amplitude / total amplitude
        size_t n = 0;
        float total = 0.0f;
        for (size_t k = 0; partials != nullptr && k < count; k++)
        {
            if (isValidPartial(partials[k], sample_rate))
            {
                phase[n] = 0.0f;
                step[n] = 2.0f * static_cast<float>(M_PI) * partials[k].frequency / static_cast<float>(sample_rate);
                weight[n] = partials[k].amplitude;
                total += partials[k].amplitude;
                n++;
            }
        }
        if (n == 0)
        {
            for (size_t i = 0; i < length; i++)
            {
                out[i] = 0;
            }
            return;
        }
        for (size_t k = 0; k < n; k++)
        {
            weight[k] /= total;
        }

        size_t fade = sampleCount(fade_ms, sample_rate);
        if (fade * 2 > length)
        {
            fade = length / 2; // the fades must not overlap
        }

        const float two_pi = 2.0f * static_cast<float>(M_PI);
        for (size_t i = 0; i < length; i++)
        {
            float envelope = 1.0f;
            if (i < fade)
            {
                envelope = static_cast<float>(i) / static_cast<float>(fade);
            }
            else if (i >= length - fade)
            {
                envelope = static_cast<float>(length - 1 - i) / static_cast<float>(fade);
            }

            // sum of the waves, in -1.0 .. 1.0
            float sum = 0.0f;
            for (size_t k = 0; k < n; k++)
            {
                sum += weight[k] * sinf(phase[k]);
                phase[k] += step[k];
                if (phase[k] >= two_pi)
                {
                    phase[k] -= two_pi; // step is below pi, so one subtraction is enough
                }
            }
            out[i] = static_cast<int16_t>(32767.0f * amplitude * envelope * sum);
        }
    }

    /**
     * @brief Generate a mono sine wave of signed 16 bit samples
     *
     * It is generateSineMix() with one partial.
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
        Partial partial;
        partial.frequency = frequency;
        partial.amplitude = 1.0f;
        generateSineMix(out, length, &partial, 1, sample_rate, amplitude, fade_ms);
    }

    /**
     * @brief Make the partials of a harmonic series: a fundamental frequency and its integer multiples
     *
     * A voiced sound, like a voice or an instrument, is mostly a fundamental frequency f0 and the harmonics
     * at 2 * f0, 3 * f0, ... The k-th harmonic (k = 1, 2, ...) gets the amplitude 1 / k^rolloff,
     * so a larger `rolloff` gives a softer sound, and 0 gives all the harmonics the same amplitude.
     *
     * @param f0 fundamental frequency [Hz]
     * @param count number of the partials, including the fundamental
     * @param rolloff how fast the amplitude falls with the harmonic number
     * @param out destination of the partials
     * @param capacity size of `out`
     * @return number of the partials written to `out`
     */
    inline size_t harmonicSeries(float f0, size_t count, float rolloff, Partial *out, size_t capacity)
    {
        if (out == nullptr)
        {
            return 0;
        }
        if (count > capacity)
        {
            count = capacity;
        }
        for (size_t k = 0; k < count; k++)
        {
            float n = static_cast<float>(k + 1);
            out[k].frequency = f0 * n;
            out[k].amplitude = powf(n, -rolloff);
        }
        return count;
    }
} // namespace stackchan::voice
