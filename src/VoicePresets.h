#pragma once

#include "VoiceSynth.h"

// Presets of character voices. Each preset fills an array of VoiceNote, which can be played with
// VoicePlayer::playPhrase(), and the notes can be modified before playing to tune the voice.
// This header does not depend on Arduino / M5Unified.
namespace stackchan::voice::presets
{
    namespace detail
    {
        // a small random number generator, so that a seed always gives the same phrase on every board
        struct Random
        {
            uint32_t state;

            explicit Random(uint32_t seed)
            {
                state = seed * 2654435761u + 12345u;
                if (state == 0)
                {
                    state = 1;
                }
            }

            /// @return a random number in 0.0 .. 1.0 (not including 1.0)
            float next()
            {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                return static_cast<float>(state >> 8) / 16777216.0f;
            }

            float range(float lo, float hi)
            {
                return lo + (hi - lo) * next();
            }
        };
    } // namespace detail

    /// longest phrase made by r2d2() [notes]
    constexpr size_t kMaxR2D2Notes = 8;

    /**
     * @brief R2D2-like voice: a burble of short electronic chirps
     *
     * The phrase is 6 to 8 sounds of 4 kinds: a chirp going up, a chirp going down, a trill (very fast vibrato)
     * and a short blip. Their pitch is random between 600 and 2600 Hz. They have only a few harmonics, a bit
     * off the exact multiples (`stretch`), which gives the metallic electronic sound.
     * The same seed gives the same phrase. It is shorter than VoicePlayer::kMaxDurationMs.
     *
     * @param out destination of the notes
     * @param capacity size of `out`, `kMaxR2D2Notes` is enough
     * @param seed a different seed gives a different phrase
     * @return number of the notes
     */
    inline size_t r2d2(VoiceNote *out, size_t capacity, uint32_t seed = 1)
    {
        if (out == nullptr)
        {
            return 0;
        }
        detail::Random random(seed);

        size_t count = 6 + static_cast<size_t>(random.next() * 3.0f); // 6 to 8
        if (count > capacity)
        {
            count = capacity;
        }

        for (size_t i = 0; i < count; i++)
        {
            VoiceNote note;
            note.harmonics = 4;
            note.rolloff_start = 1.3f;
            note.rolloff_end = 1.3f;
            note.stretch = 0.04f;
            note.attack_ms = 3;
            note.release_ms = 15;
            note.gap_ms = static_cast<uint32_t>(random.range(15.0f, 35.0f));

            // pitch in 600 .. 2600 Hz, evenly in musical pitch
            const float f = 600.0f * powf(2600.0f / 600.0f, random.next());
            const int kind = static_cast<int>(random.next() * 4.0f);
            const float octaves = random.range(0.7f, 1.7f);
            const float top = 3200.0f; // keeps the harmonics below the Nyquist frequency

            if (kind == 0) // chirp up
            {
                note.duration_ms = static_cast<uint32_t>(random.range(90.0f, 150.0f));
                note.f_start = f;
                note.f_end = fminf(f * exp2f(octaves), top);
            }
            else if (kind == 1) // chirp down
            {
                note.duration_ms = static_cast<uint32_t>(random.range(90.0f, 150.0f));
                note.f_start = fminf(f * exp2f(octaves), top);
                note.f_end = f;
            }
            else if (kind == 2) // trill
            {
                note.duration_ms = static_cast<uint32_t>(random.range(110.0f, 180.0f));
                note.f_start = f;
                note.f_end = f;
                note.vibrato_hz = random.range(18.0f, 32.0f);
                note.vibrato_semitones = random.range(3.0f, 7.0f);
            }
            else // blip
            {
                note.duration_ms = static_cast<uint32_t>(random.range(35.0f, 60.0f));
                note.f_start = f;
                note.f_end = f;
            }
            out[i] = note;
        }
        return count;
    }

    /**
     * @brief Cat-like voice: a meow
     *
     * The pitch rises quickly to a peak and falls slowly. The sound is bright at the start and gets darker
     * as it goes (the many harmonics fade, like the mouth closing from "mee" to "ow"), with a slight vibrato.
     *
     * @param out destination of the notes
     * @param capacity size of `out`
     * @return number of the notes (1)
     */
    inline size_t catMeow(VoiceNote *out, size_t capacity)
    {
        if (out == nullptr || capacity < 1)
        {
            return 0;
        }
        VoiceNote note;
        note.duration_ms = 650;
        note.f_start = 420.0f;
        note.f_peak = 820.0f;
        note.peak_position = 0.3f;
        note.f_end = 480.0f;
        note.harmonics = 12;
        note.rolloff_start = 0.9f; // bright
        note.rolloff_end = 1.7f;   // dark
        note.vibrato_hz = 6.0f;
        note.vibrato_semitones = 0.3f;
        note.attack_ms = 40;
        note.release_ms = 120;
        out[0] = note;
        return 1;
    }

    /**
     * @brief Puppy-like voice: a whine, "kyuun"
     *
     * The pitch jumps up to a high peak, and falls slowly with a wobbling vibrato. The tone is soft.
     *
     * @param out destination of the notes
     * @param capacity size of `out`
     * @return number of the notes (1)
     */
    inline size_t puppyWhine(VoiceNote *out, size_t capacity)
    {
        if (out == nullptr || capacity < 1)
        {
            return 0;
        }
        VoiceNote note;
        note.duration_ms = 900;
        note.f_start = 500.0f;
        note.f_peak = 1000.0f;
        note.peak_position = 0.2f;
        note.f_end = 650.0f;
        note.harmonics = 8;
        note.rolloff_start = 1.2f;
        note.rolloff_end = 1.8f;
        note.vibrato_hz = 7.0f;
        note.vibrato_semitones = 0.8f;
        note.attack_ms = 30;
        note.release_ms = 150;
        out[0] = note;
        return 1;
    }
} // namespace stackchan::voice::presets
