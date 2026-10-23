#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "SineWave.h"

// Voice synthesis: a fundamental frequency and its harmonics, with a pitch curve, vibrato and an envelope.
// This header does not depend on Arduino / M5Unified, so it can be built and tested on a PC.
namespace stackchan::voice
{
    /**
     * @brief One sound of a character voice (a chirp, a meow, ...)
     *
     * It is harmonicSeries() whose parameters change in time:
     *
     * - pitch: the fundamental frequency glides from `f_start` to `f_end`, optionally through `f_peak`.
     *   The glides are straight lines in musical pitch (the same number of semitones per second).
     * - timbre: `harmonics` sine waves at multiples of the fundamental frequency, the k-th with the amplitude
     *   1 / k^rolloff. The rolloff glides from `rolloff_start` to `rolloff_end`: a small rolloff is a bright sound,
     *   a large one is a soft sound.
     * - `stretch` moves the harmonics away from the exact multiples (k^(1 + stretch)), which makes the sound
     *   metallic like a robot or a bell.
     * - vibrato: the pitch swings around the curve.
     * - envelope: linear fade in (attack) and fade out (release).
     */
    struct VoiceNote
    {
        // pitch curve [Hz]
        float f_start = 440.0f;
        float f_end = 440.0f;
        float f_peak = 0.0f;        ///< pitch of the peak in the middle. 0 or less: no peak, a straight glide from f_start to f_end.
        float peak_position = 0.5f; ///< where the peak is, 0.0 (start) to 1.0 (end)

        uint32_t duration_ms = 300; ///< length of the sound
        uint32_t gap_ms = 0;        ///< silence after the sound, used when notes are played one after another

        // timbre
        uint8_t harmonics = 6;       ///< number of sine waves including the fundamental, 1 to kMaxPartials
        float rolloff_start = 1.0f;  ///< amplitude of the k-th harmonic is 1 / k^rolloff
        float rolloff_end = 1.0f;
        float stretch = 0.0f;        ///< 0: exact harmonics. A little more (e.g. 0.04): metallic. -0.5 to 1.

        // vibrato
        float vibrato_hz = 0.0f;
        float vibrato_semitones = 0.0f; ///< how far the pitch swings up and down

        // envelope
        uint32_t attack_ms = 10;
        uint32_t release_ms = 30;

        float amplitude = 1.0f; ///< level of this note relative to the whole phrase, 0.0 to 1.0
    };

    namespace detail
    {
        inline float clampf(float x, float lo, float hi)
        {
            return x < lo ? lo : (x > hi ? hi : x);
        }
    } // namespace detail

    /**
     * @brief Generate the samples of one note
     *
     * Harmonics that would be above the Nyquist frequency (half of the sampling rate) are left out.
     * If the pitch itself is invalid, the output is silent.
     *
     * @param out destination of `length` samples. Use sampleCount(note.duration_ms, sample_rate) for `length`.
     * @param length number of samples
     * @param note the sound
     * @param sample_rate sampling rate [Hz]
     * @param amplitude 0.0 (silent) to 1.0 (full scale) of the whole sound
     */
    inline void renderVoiceNote(int16_t *out, size_t length, const VoiceNote &note, uint32_t sample_rate,
                                float amplitude = 0.5f)
    {
        if (out == nullptr || length == 0 || sample_rate == 0)
        {
            return;
        }
        for (size_t i = 0; i < length; i++)
        {
            out[i] = 0;
        }

        const float f_start = note.f_start;
        const float f_end = note.f_end;
        const float f_peak = note.f_peak;
        const bool has_peak = f_peak > 0.0f;
        if (f_start <= 0.0f || f_end <= 0.0f)
        {
            return;
        }

        const float sr = static_cast<float>(sample_rate);
        const float two_pi = 2.0f * static_cast<float>(M_PI);
        amplitude = detail::clampf(amplitude, 0.0f, 1.0f) * detail::clampf(note.amplitude, 0.0f, 1.0f);
        const float peak_position = detail::clampf(note.peak_position, 0.01f, 0.99f);
        const float stretch = detail::clampf(note.stretch, -0.5f, 1.0f);

        // the highest frequency the wave reaches
        const bool vibrato = note.vibrato_hz > 0.0f && note.vibrato_semitones > 0.0f;
        const float vibrato_depth = vibrato ? note.vibrato_semitones / 12.0f : 0.0f; // in octaves
        float f_max = f_start > f_end ? f_start : f_end;
        if (has_peak && f_peak > f_max)
        {
            f_max = f_peak;
        }
        f_max *= exp2f(vibrato_depth);

        // the partials: frequency ratio to the fundamental, and the amplitude at the start and at the end
        size_t n = note.harmonics;
        if (n < 1)
        {
            n = 1;
        }
        if (n > kMaxPartials)
        {
            n = kMaxPartials;
        }
        float ratio[kMaxPartials];
        float weight_start[kMaxPartials];
        float weight_end[kMaxPartials];
        float total_start = 0.0f;
        float total_end = 0.0f;
        size_t used = 0;
        for (size_t k = 0; k < n; k++)
        {
            float r = powf(static_cast<float>(k + 1), 1.0f + stretch);
            if (r * f_max >= sr / 2.0f)
            {
                break; // this and the higher partials would alias
            }
            ratio[used] = r;
            weight_start[used] = powf(static_cast<float>(k + 1), -note.rolloff_start);
            weight_end[used] = powf(static_cast<float>(k + 1), -note.rolloff_end);
            total_start += weight_start[used];
            total_end += weight_end[used];
            used++;
        }
        if (used == 0)
        {
            return;
        }

        // pitch curve: straight lines in log frequency
        const float ln_up = has_peak ? logf(f_peak / f_start) : 0.0f;
        const float ln_down = has_peak ? logf(f_end / f_peak) : 0.0f;
        const float ln_glide = logf(f_end / f_start);

        // envelope
        float attack = static_cast<float>(sampleCount(note.attack_ms, sample_rate));
        float release = static_cast<float>(sampleCount(note.release_ms, sample_rate));
        if (attack + release > static_cast<float>(length))
        {
            float scale = static_cast<float>(length) / (attack + release);
            attack *= scale;
            release *= scale;
        }

        float phase[kMaxPartials];
        for (size_t k = 0; k < used; k++)
        {
            phase[k] = 0.0f;
        }
        float vibrato_phase = 0.0f;

        for (size_t i = 0; i < length; i++)
        {
            const float u = length > 1 ? static_cast<float>(i) / static_cast<float>(length - 1) : 0.0f;

            float f;
            if (!has_peak)
            {
                f = f_start * expf(ln_glide * u);
            }
            else if (u < peak_position)
            {
                f = f_start * expf(ln_up * (u / peak_position));
            }
            else
            {
                f = f_peak * expf(ln_down * ((u - peak_position) / (1.0f - peak_position)));
            }
            if (vibrato)
            {
                f *= exp2f(vibrato_depth * sinf(vibrato_phase));
                vibrato_phase += two_pi * note.vibrato_hz / sr;
                if (vibrato_phase >= two_pi)
                {
                    vibrato_phase -= two_pi;
                }
            }

            // the timbre changes from the start to the end
            const float inv_total = 1.0f / ((1.0f - u) * total_start + u * total_end);
            float sum = 0.0f;
            for (size_t k = 0; k < used; k++)
            {
                const float w = (1.0f - u) * weight_start[k] + u * weight_end[k];
                sum += w * sinf(phase[k]);
                phase[k] += two_pi * f * ratio[k] / sr;
                if (phase[k] >= two_pi)
                {
                    phase[k] -= two_pi; // below the Nyquist frequency, so one subtraction is enough
                }
            }
            sum *= inv_total;

            float envelope = 1.0f;
            if (attack > 0.0f && static_cast<float>(i) < attack)
            {
                envelope = static_cast<float>(i) / attack;
            }
            if (release > 0.0f && static_cast<float>(i) >= static_cast<float>(length) - release)
            {
                float fall = static_cast<float>(length - 1 - i) / release;
                envelope = fall < envelope ? fall : envelope;
            }

            out[i] = static_cast<int16_t>(32767.0f * amplitude * envelope * sum);
        }
    }

    /// @return length of a phrase [msec], including the gap after each note
    inline uint32_t phraseDurationMs(const VoiceNote *notes, size_t count)
    {
        uint32_t total = 0;
        for (size_t i = 0; notes != nullptr && i < count; i++)
        {
            total += notes[i].duration_ms + notes[i].gap_ms;
        }
        return total;
    }

    /**
     * @brief Generate the samples of notes one after another. Each note is followed by its `gap_ms` of silence.
     *
     * Only the notes that fit in `capacity` samples are written.
     *
     * @param out destination of the samples
     * @param capacity size of `out` in samples
     * @param notes the notes
     * @param count number of the notes
     * @param sample_rate sampling rate [Hz]
     * @param amplitude 0.0 (silent) to 1.0 (full scale) of the whole phrase
     * @return number of samples written
     */
    inline size_t renderPhrase(int16_t *out, size_t capacity, const VoiceNote *notes, size_t count,
                               uint32_t sample_rate, float amplitude = 0.5f)
    {
        if (out == nullptr || notes == nullptr)
        {
            return 0;
        }
        size_t pos = 0;
        for (size_t i = 0; i < count; i++)
        {
            const size_t length = sampleCount(notes[i].duration_ms, sample_rate);
            const size_t gap = sampleCount(notes[i].gap_ms, sample_rate);
            if (pos + length > capacity)
            {
                break;
            }
            renderVoiceNote(out + pos, length, notes[i], sample_rate, amplitude);
            pos += length;

            if (pos + gap > capacity)
            {
                break; // the note is written, but its gap does not fit
            }
            for (size_t j = 0; j < gap; j++)
            {
                out[pos + j] = 0;
            }
            pos += gap;
        }
        return pos;
    }
} // namespace stackchan::voice
