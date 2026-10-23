#pragma once

#include "VoicePresets.h"
#include "VoiceSynth.h"

// Voices of the characters for each emotion (facial expression of StackChan).
// This header does not depend on Arduino / M5Unified, nor on stackchan-display.
namespace stackchan::voice
{
    /**
     * @brief Emotions that have a voice
     *
     * The names and the order are the same as the emotional expressions of stackchan-display
     * (`stackchan::display::Expression::kNeutral` .. `kRelax`), so that the value can be converted with
     * `static_cast<stackchan::voice::Emotion>(expression)` for these 10 expressions.
     */
    enum class Emotion : uint8_t
    {
        kNeutral = 0,
        kAngry,
        kSad,
        kDoubt,
        kSleepy,
        kSmile,
        kGrin,
        kSurprised,
        kUpset,
        kRelax,
        kCount,
    };

    /// The characters whose voice is made
    enum class Character : uint8_t
    {
        kRobot = 0,     ///< electronic chirps
        kCat,           ///< meows
        kDog,           ///< barks, growls and whines
        kBird,          ///< a small bird: high, pure, quick chirps and trills
        kOwl,           ///< hoots: low, soft and round
        kFriendlyRobot, ///< a warm robot that almost talks: a wobbling voice-like sound, not chirps
        kCount,
    };

    /// the most notes of a voice made by emotionVoice()
    constexpr size_t kMaxEmotionNotes = 8;

    /// @return the name of the emotion, such as "smile"
    inline const char *emotionName(Emotion emotion)
    {
        static const char *names[] = {"neutral", "angry", "sad", "doubt", "sleepy",
                                      "smile", "grin", "surprised", "upset", "relax"};
        size_t i = static_cast<size_t>(emotion);
        return i < static_cast<size_t>(Emotion::kCount) ? names[i] : "?";
    }

    /// @return the name of the character, such as "cat"
    inline const char *characterName(Character character)
    {
        static const char *names[] = {"robot", "cat", "dog", "bird", "owl", "friendly robot"};
        size_t i = static_cast<size_t>(character);
        return i < static_cast<size_t>(Character::kCount) ? names[i] : "?";
    }

    namespace emotion_detail
    {
        // small setters, so that each note below reads like a description of the sound
        inline void pitch(VoiceNote &n, float start, float peak, float end, float peak_position = 0.3f)
        {
            n.f_start = start;
            n.f_peak = peak;
            n.f_end = end;
            n.peak_position = peak_position;
        }

        inline void glide(VoiceNote &n, float start, float end)
        {
            n.f_start = start;
            n.f_peak = 0.0f;
            n.f_end = end;
        }

        inline void timbre(VoiceNote &n, uint8_t harmonics, float rolloff_start, float rolloff_end, float stretch = 0.0f)
        {
            n.harmonics = harmonics;
            n.rolloff_start = rolloff_start;
            n.rolloff_end = rolloff_end;
            n.stretch = stretch;
        }

        inline void vibrato(VoiceNote &n, float hz, float semitones)
        {
            n.vibrato_hz = hz;
            n.vibrato_semitones = semitones;
        }

        inline void envelope(VoiceNote &n, uint32_t attack_ms, uint32_t release_ms)
        {
            n.attack_ms = attack_ms;
            n.release_ms = release_ms;
        }

        inline void length(VoiceNote &n, uint32_t duration_ms, uint32_t gap_ms = 0)
        {
            n.duration_ms = duration_ms;
            n.gap_ms = gap_ms;
        }

        // ---------------------------------------------------------------------------------------------
        // Robot: electronic chirps. The emotion is in the pitch, the speed and the direction of the chirps.
        // The seed adds a little randomness to the pitch, so the voice is not exactly the same every time.
        // ---------------------------------------------------------------------------------------------
        inline size_t robotVoice(Emotion emotion, VoiceNote *out, size_t capacity, uint32_t seed)
        {
            presets::detail::Random random(seed);
            size_t count = 0;

            // timbre of the chirps: harmonics, rolloff and stretch (metallic), and the envelope
            struct Style
            {
                uint8_t harmonics;
                float rolloff;
                float stretch;
                uint32_t attack_ms;
                uint32_t release_ms;
            };
            const Style bright = {4, 1.3f, 0.04f, 3, 15};  // the normal electronic sound
            const Style harsh = {7, 0.7f, 0.08f, 2, 20};   // rough and aggressive
            const Style soft = {3, 2.0f, 0.02f, 20, 80};   // gentle and dull
            const Style sleepy = {2, 2.5f, 0.0f, 60, 200}; // almost a sine wave, slow fade

            auto add = [&](const Style &style, float f_start, float f_end, uint32_t duration_ms, uint32_t gap_ms,
                           float vibrato_hz = 0.0f, float vibrato_semitones = 0.0f)
            {
                if (count >= capacity)
                {
                    return;
                }
                // a different pitch in each note, up to 0.08 octave
                const float j = exp2f(random.range(-0.08f, 0.08f));
                VoiceNote note;
                glide(note, f_start * j, f_end * j);
                length(note, duration_ms, gap_ms);
                timbre(note, style.harmonics, style.rolloff, style.rolloff, style.stretch);
                envelope(note, style.attack_ms, style.release_ms);
                vibrato(note, vibrato_hz, vibrato_semitones);
                out[count++] = note;
            };

            switch (emotion)
            {
            case Emotion::kNeutral: // "pi-po": two short blips
                add(bright, 900, 900, 70, 30);
                add(bright, 1200, 1200, 70, 0);
                break;
            case Emotion::kAngry: // low, rough chirps falling one after another
                add(harsh, 700, 400, 140, 20);
                add(harsh, 650, 350, 140, 20);
                add(harsh, 600, 300, 200, 0);
                break;
            case Emotion::kSad: // slow falling chirps, soft
                add(soft, 900, 600, 260, 60);
                add(soft, 700, 450, 300, 60);
                add(soft, 600, 350, 420, 0);
                break;
            case Emotion::kDoubt: // two flat blips and a rising chirp, like a question
                add(bright, 900, 900, 90, 40);
                add(bright, 950, 950, 90, 60);
                add(bright, 800, 1800, 260, 0);
                break;
            case Emotion::kSleepy: // slow, low, falling like a yawn
                add(sleepy, 700, 350, 700, 100);
                add(sleepy, 500, 250, 800, 0);
                break;
            case Emotion::kSmile: // three happy rising chirps
                add(bright, 900, 1500, 100, 25);
                add(bright, 1100, 1900, 110, 25);
                add(bright, 1500, 2300, 140, 0);
                break;
            case Emotion::kGrin: // fast rising chirps and trills
                add(bright, 1000, 1700, 80, 20);
                add(bright, 1200, 2000, 80, 20);
                add(bright, 1800, 1800, 150, 20, 28.0f, 4.0f);
                add(bright, 1400, 2400, 90, 20);
                add(bright, 1700, 2600, 100, 20);
                add(bright, 2200, 2200, 160, 0, 25.0f, 4.0f);
                break;
            case Emotion::kSurprised: // a sudden high chirp and a blip
                add(bright, 1500, 3000, 100, 25);
                add(bright, 3000, 3000, 40, 0);
                break;
            case Emotion::kUpset: // fast, high, shaky: trills and blips
                add(bright, 1800, 1800, 90, 15, 30.0f, 6.0f);
                add(bright, 1300, 2200, 60, 15);
                add(bright, 2200, 2200, 50, 15);
                add(bright, 1600, 1600, 100, 15, 28.0f, 5.0f);
                add(bright, 2400, 1500, 70, 15);
                add(bright, 1900, 1900, 90, 15, 32.0f, 6.0f);
                add(bright, 1400, 2300, 70, 0);
                break;
            case Emotion::kRelax: // two gentle slow chirps
                add(soft, 700, 900, 220, 80, 6.0f, 0.3f);
                add(soft, 900, 700, 300, 0, 6.0f, 0.3f);
                break;
            default:
                break;
            }
            return count;
        }

        // ---------------------------------------------------------------------------------------------
        // Cat: meows. The pitch goes up to a peak and comes down. Bright to dark = the mouth closing.
        // ---------------------------------------------------------------------------------------------
        inline size_t catVoice(Emotion emotion, VoiceNote *out, size_t capacity)
        {
            size_t count = 0;
            auto next = [&]() -> VoiceNote *
            {
                if (count >= capacity)
                {
                    return nullptr;
                }
                out[count] = VoiceNote();
                return &out[count++];
            };
            VoiceNote *n = nullptr;

            switch (emotion)
            {
            case Emotion::kNeutral: // "mya": a short meow
                if ((n = next()))
                {
                    length(*n, 350);
                    pitch(*n, 450, 600, 500);
                    timbre(*n, 10, 1.0f, 1.5f);
                    envelope(*n, 30, 80);
                }
                break;
            case Emotion::kAngry: // a long low rough meow, like a growl
                if ((n = next()))
                {
                    length(*n, 700);
                    pitch(*n, 300, 400, 250);
                    timbre(*n, 12, 0.5f, 0.8f, 0.03f);
                    vibrato(*n, 12.0f, 0.6f); // roughness
                    envelope(*n, 15, 100);
                }
                break;
            case Emotion::kSad: // a long, falling, dark meow
                if ((n = next()))
                {
                    length(*n, 1000);
                    pitch(*n, 500, 650, 350, 0.2f);
                    timbre(*n, 8, 1.5f, 2.2f);
                    vibrato(*n, 5.0f, 0.5f);
                    envelope(*n, 80, 250);
                }
                break;
            case Emotion::kDoubt: // "mya?": the pitch goes up at the end
                if ((n = next()))
                {
                    length(*n, 450);
                    glide(*n, 400, 800);
                    timbre(*n, 8, 1.0f, 1.4f);
                    envelope(*n, 30, 70);
                }
                break;
            case Emotion::kSleepy: // a low, soft, long "myuu..."
                if ((n = next()))
                {
                    length(*n, 1100);
                    pitch(*n, 350, 380, 300, 0.2f);
                    timbre(*n, 6, 1.8f, 2.4f);
                    vibrato(*n, 4.0f, 0.3f);
                    envelope(*n, 150, 400);
                }
                break;
            case Emotion::kSmile: // a higher, cheerful meow
                if ((n = next()))
                {
                    length(*n, 450);
                    pitch(*n, 600, 950, 800, 0.4f);
                    timbre(*n, 10, 0.9f, 1.2f);
                    vibrato(*n, 7.0f, 0.2f);
                    envelope(*n, 25, 80);
                }
                break;
            case Emotion::kGrin: // two quick rising meows
                for (int i = 0; i < 2; i++)
                {
                    if ((n = next()))
                    {
                        length(*n, 250, i == 0 ? 60 : 0);
                        pitch(*n, 650, 1000, 850, 0.5f);
                        timbre(*n, 10, 0.8f, 1.1f);
                        envelope(*n, 15, 60);
                    }
                }
                break;
            case Emotion::kSurprised: // a sharp high "nya!"
                if ((n = next()))
                {
                    length(*n, 180);
                    glide(*n, 800, 1300);
                    timbre(*n, 12, 0.7f, 0.9f);
                    envelope(*n, 5, 50);
                }
                break;
            case Emotion::kUpset: // two shaky, high meows
                for (int i = 0; i < 2; i++)
                {
                    if ((n = next()))
                    {
                        length(*n, 400, i == 0 ? 100 : 0);
                        pitch(*n, 700, 1100, 700, 0.4f);
                        timbre(*n, 10, 0.9f, 1.3f);
                        vibrato(*n, 9.0f, 1.0f); // trembling
                        envelope(*n, 20, 90);
                    }
                }
                break;
            case Emotion::kRelax: // a gentle, slow meow
                if ((n = next()))
                {
                    length(*n, 600);
                    pitch(*n, 400, 520, 420, 0.4f);
                    timbre(*n, 8, 1.6f, 2.0f);
                    vibrato(*n, 4.0f, 0.3f);
                    envelope(*n, 80, 200);
                }
                break;
            default:
                break;
            }
            return count;
        }

        // ---------------------------------------------------------------------------------------------
        // Dog: barks, growls and whines. A bark is a short sound with a fast attack and a falling pitch.
        // ---------------------------------------------------------------------------------------------
        inline size_t dogVoice(Emotion emotion, VoiceNote *out, size_t capacity)
        {
            size_t count = 0;
            auto next = [&]() -> VoiceNote *
            {
                if (count >= capacity)
                {
                    return nullptr;
                }
                out[count] = VoiceNote();
                return &out[count++];
            };
            VoiceNote *n = nullptr;

            switch (emotion)
            {
            case Emotion::kNeutral: // "wan": one bark
                if ((n = next()))
                {
                    length(*n, 200);
                    pitch(*n, 350, 420, 250, 0.15f);
                    timbre(*n, 12, 0.8f, 1.2f);
                    envelope(*n, 5, 60);
                }
                break;
            case Emotion::kAngry: // "grrr": a low growl, rough
                if ((n = next()))
                {
                    length(*n, 800);
                    pitch(*n, 120, 140, 110, 0.3f);
                    timbre(*n, 14, 0.6f, 0.9f, 0.01f);
                    vibrato(*n, 28.0f, 0.8f); // roughness
                    envelope(*n, 40, 150);
                }
                break;
            case Emotion::kSad: // "kuuun": a slow whine, rising and falling
                if ((n = next()))
                {
                    length(*n, 1000);
                    pitch(*n, 400, 650, 300, 0.25f);
                    timbre(*n, 8, 1.3f, 2.0f);
                    vibrato(*n, 6.0f, 0.7f);
                    envelope(*n, 60, 250);
                }
                break;
            case Emotion::kDoubt: // "un?": the pitch goes up at the end
                if ((n = next()))
                {
                    length(*n, 350);
                    glide(*n, 300, 500);
                    timbre(*n, 8, 1.0f, 1.4f);
                    vibrato(*n, 6.0f, 0.3f);
                    envelope(*n, 20, 80);
                }
                break;
            case Emotion::kSleepy: // a low, soft, long sigh
                if ((n = next()))
                {
                    length(*n, 1100);
                    pitch(*n, 250, 270, 200, 0.2f);
                    timbre(*n, 5, 2.0f, 2.6f);
                    envelope(*n, 150, 450);
                }
                break;
            case Emotion::kSmile: // "wan wan": two happy, higher barks
                for (int i = 0; i < 2; i++)
                {
                    if ((n = next()))
                    {
                        length(*n, 150, i == 0 ? 80 : 0);
                        pitch(*n, 450, 560, 330, 0.15f);
                        timbre(*n, 12, 0.8f, 1.1f);
                        envelope(*n, 5, 50);
                    }
                }
                break;
            case Emotion::kGrin: // "wan wan wan": three quick, high yips
                for (int i = 0; i < 3; i++)
                {
                    if ((n = next()))
                    {
                        length(*n, 120, i < 2 ? 60 : 0);
                        pitch(*n, 550, 700, 420, 0.15f);
                        timbre(*n, 10, 0.8f, 1.0f);
                        envelope(*n, 4, 40);
                    }
                }
                break;
            case Emotion::kSurprised: // "kyan!": a short, high yelp
                if ((n = next()))
                {
                    length(*n, 180);
                    pitch(*n, 700, 1100, 600, 0.3f);
                    timbre(*n, 10, 0.7f, 1.0f);
                    envelope(*n, 4, 50);
                }
                break;
            case Emotion::kUpset: // "kuun kuun": two shaky short whines
                for (int i = 0; i < 2; i++)
                {
                    if ((n = next()))
                    {
                        length(*n, 350, i == 0 ? 100 : 0);
                        pitch(*n, 500, 800, 500, 0.4f);
                        timbre(*n, 8, 1.1f, 1.5f);
                        vibrato(*n, 8.0f, 1.0f);
                        envelope(*n, 20, 80);
                    }
                }
                break;
            case Emotion::kRelax: // "fuun": a soft, low, calm sound
                if ((n = next()))
                {
                    length(*n, 700);
                    pitch(*n, 280, 330, 260, 0.4f);
                    timbre(*n, 6, 1.6f, 2.2f);
                    vibrato(*n, 4.0f, 0.3f);
                    envelope(*n, 80, 250);
                }
                break;
            default:
                break;
            }
            return count;
        }

        // ---------------------------------------------------------------------------------------------
        // Bird: a small bird. High, almost pure sine waves (1 or 2 harmonics), quick chirps and trills.
        // ---------------------------------------------------------------------------------------------
        inline size_t birdVoice(Emotion emotion, VoiceNote *out, size_t capacity)
        {
            size_t count = 0;
            // add a chirp. A bird sings in sine waves: the harmonics are few and fade fast.
            auto chirp = [&](float f_start, float f_end, uint32_t duration_ms, uint32_t gap_ms,
                             float vibrato_hz = 0.0f, float vibrato_semitones = 0.0f,
                             uint8_t harmonics = 2, float rolloff = 2.0f,
                             uint32_t attack_ms = 4, uint32_t release_ms = 25)
            {
                if (count >= capacity)
                {
                    return;
                }
                VoiceNote note;
                glide(note, f_start, f_end);
                length(note, duration_ms, gap_ms);
                timbre(note, harmonics, rolloff, rolloff);
                vibrato(note, vibrato_hz, vibrato_semitones);
                envelope(note, attack_ms, release_ms);
                out[count++] = note;
            };

            switch (emotion)
            {
            case Emotion::kNeutral: // "pi": one short chirp
                chirp(2600, 3000, 90, 0);
                break;
            case Emotion::kAngry: // a scolding chatter: low, harsh, short and repeated
                for (int i = 0; i < 4; i++)
                {
                    chirp(1900, 1600, 70, i < 3 ? 40 : 0, 0.0f, 0.0f, 4, 0.8f, 2, 20);
                }
                break;
            case Emotion::kSad: // slow falling, drooping chirps with a weak wobble
                chirp(2400, 1500, 500, 80, 6.0f, 0.8f, 2, 2.5f, 40, 150);
                chirp(2200, 1300, 600, 0, 6.0f, 0.8f, 2, 2.5f, 40, 150);
                break;
            case Emotion::kDoubt: // a blip, and a chirp that goes up at the end, like a question
                chirp(2400, 2400, 80, 80);
                chirp(2200, 3200, 220, 0);
                break;
            case Emotion::kSleepy: // slow, low and soft, fading away
                chirp(1500, 1200, 700, 100, 0.0f, 0.0f, 1, 3.0f, 120, 300);
                chirp(1300, 1000, 800, 0, 0.0f, 0.0f, 1, 3.0f, 120, 300);
                break;
            case Emotion::kSmile: // a cheerful song: four rising chirps
                chirp(3000, 3500, 70, 30);
                chirp(3100, 3600, 70, 30);
                chirp(3000, 3500, 70, 30);
                chirp(3200, 3600, 90, 0);
                break;
            case Emotion::kGrin: // a lively song with a trill
                chirp(3000, 3600, 60, 25);
                chirp(3100, 3700, 60, 25);
                chirp(3000, 3600, 60, 25);
                chirp(3300, 3300, 180, 25, 30.0f, 5.0f);
                chirp(3200, 3700, 70, 0);
                break;
            case Emotion::kSurprised: // a sudden high chirp
                chirp(2800, 3700, 70, 0, 0.0f, 0.0f, 2, 2.0f, 3, 20);
                break;
            case Emotion::kUpset: // a frantic chatter of trills and chirps
                chirp(3300, 3300, 80, 15, 35.0f, 4.0f);
                chirp(2600, 3400, 60, 15);
                chirp(3500, 2800, 60, 15);
                chirp(3000, 3000, 90, 15, 35.0f, 4.0f);
                chirp(2700, 3500, 60, 15);
                chirp(3400, 3400, 80, 0, 35.0f, 4.0f);
                break;
            case Emotion::kRelax: // a gentle two-note song
                chirp(2200, 2500, 250, 100, 5.0f, 0.4f, 2, 2.5f, 30, 120);
                chirp(2500, 2100, 350, 0, 5.0f, 0.4f, 2, 2.5f, 30, 120);
                break;
            default:
                break;
            }
            return count;
        }

        // ---------------------------------------------------------------------------------------------
        // Owl: hoots. Low, round and soft: few harmonics, a slow attack, and the pitch settles down.
        // ---------------------------------------------------------------------------------------------
        inline size_t owlVoice(Emotion emotion, VoiceNote *out, size_t capacity)
        {
            size_t count = 0;
            // add a hoot
            auto hoot = [&](float f_start, float f_peak, float f_end, uint32_t duration_ms, uint32_t gap_ms,
                            uint8_t harmonics = 3, float rolloff = 2.2f,
                            uint32_t attack_ms = 60, uint32_t release_ms = 150,
                            float vibrato_hz = 0.0f, float vibrato_semitones = 0.0f)
            {
                if (count >= capacity)
                {
                    return;
                }
                VoiceNote note;
                pitch(note, f_start, f_peak, f_end, 0.2f);
                length(note, duration_ms, gap_ms);
                timbre(note, harmonics, rolloff, rolloff);
                vibrato(note, vibrato_hz, vibrato_semitones);
                envelope(note, attack_ms, release_ms);
                out[count++] = note;
            };

            switch (emotion)
            {
            case Emotion::kNeutral: // "hoo": one hoot
                hoot(420, 440, 380, 450, 0);
                break;
            case Emotion::kAngry: // short, low, rough "hoh! hoh!"
                for (int i = 0; i < 3; i++)
                {
                    hoot(300, 310, 260, 150, i < 2 ? 60 : 0, 6, 1.0f, 10, 60, 14.0f, 0.5f);
                }
                break;
            case Emotion::kSad: // long, sinking hoots
                hoot(380, 390, 300, 900, 100, 3, 3.0f, 120, 300, 4.0f, 0.4f);
                hoot(330, 340, 240, 900, 0, 3, 3.0f, 120, 300, 4.0f, 0.4f);
                break;
            case Emotion::kDoubt: // "hoo? hoo?": the second hoot goes up
                hoot(400, 400, 400, 250, 100);
                hoot(330, 330, 520, 350, 0);
                break;
            case Emotion::kSleepy: // one very low, slow hoot
                hoot(300, 300, 230, 1000, 0, 2, 3.0f, 200, 400);
                break;
            case Emotion::kSmile: // "ho-hoo": two higher hoots
                hoot(480, 520, 450, 200, 80);
                hoot(480, 520, 450, 350, 0);
                break;
            case Emotion::kGrin: // "ho-ho-hoo": three hoots
                hoot(500, 560, 470, 150, 60, 3, 2.0f, 30, 80);
                hoot(500, 560, 470, 150, 60, 3, 2.0f, 30, 80);
                hoot(500, 560, 470, 350, 0, 3, 2.0f, 30, 120);
                break;
            case Emotion::kSurprised: // a sharp "hoh!"
                hoot(450, 650, 700, 120, 0, 3, 1.8f, 8, 40);
                break;
            case Emotion::kUpset: // restless, trembling hoots
                for (int i = 0; i < 3; i++)
                {
                    hoot(450, 520, 430, 200, i < 2 ? 60 : 0, 3, 1.8f, 20, 80, 8.0f, 1.0f);
                }
                break;
            case Emotion::kRelax: // one slow, gentle hoot
                hoot(380, 400, 340, 700, 0, 3, 2.4f, 150, 250, 3.0f, 0.2f);
                break;
            default:
                break;
            }
            return count;
        }

        // ---------------------------------------------------------------------------------------------
        // Friendly robot: a warm robot that almost talks. A voice-like sound in the middle of the pitch
        // range, with many harmonics, a slight metallic tone and a wobble, in slow syllables.
        // ---------------------------------------------------------------------------------------------
        inline size_t friendlyRobotVoice(Emotion emotion, VoiceNote *out, size_t capacity)
        {
            size_t count = 0;
            // add a syllable
            auto syllable = [&](float f_start, float f_peak, float f_end, uint32_t duration_ms, uint32_t gap_ms,
                                float rolloff_start = 1.0f, float rolloff_end = 1.4f,
                                float vibrato_hz = 9.0f, float vibrato_semitones = 0.4f,
                                uint32_t attack_ms = 30, uint32_t release_ms = 90, float stretch = 0.02f,
                                uint8_t harmonics = 9)
            {
                if (count >= capacity)
                {
                    return;
                }
                VoiceNote note;
                pitch(note, f_start, f_peak, f_end, 0.3f);
                length(note, duration_ms, gap_ms);
                timbre(note, harmonics, rolloff_start, rolloff_end, stretch);
                vibrato(note, vibrato_hz, vibrato_semitones);
                envelope(note, attack_ms, release_ms);
                out[count++] = note;
            };

            switch (emotion)
            {
            case Emotion::kNeutral: // "wo": one syllable
                syllable(330, 380, 300, 400, 0);
                break;
            case Emotion::kAngry: // gruff, low and rough "gah! gah!"
                syllable(200, 210, 160, 250, 60, 0.5f, 0.7f, 20.0f, 0.8f, 5, 50, 0.05f, 12);
                syllable(190, 200, 150, 250, 0, 0.5f, 0.7f, 20.0f, 0.8f, 5, 50, 0.05f, 12);
                break;
            case Emotion::kSad: // low, drooping syllables
                syllable(340, 350, 240, 600, 100, 1.6f, 2.4f, 5.0f, 0.6f, 80, 200);
                syllable(280, 290, 170, 800, 0, 1.6f, 2.4f, 5.0f, 0.6f, 80, 250);
                break;
            case Emotion::kDoubt: // "uh-huh?": the second syllable goes up
                syllable(300, 300, 300, 250, 80);
                syllable(280, 280, 460, 350, 0);
                break;
            case Emotion::kSleepy: // powering down: low, slow, falling
                syllable(260, 260, 130, 1100, 0, 2.2f, 2.8f, 4.0f, 0.4f, 150, 400);
                break;
            case Emotion::kSmile: // "wa-ow": two rising syllables
                syllable(380, 450, 500, 220, 70);
                syllable(420, 520, 560, 300, 0);
                break;
            case Emotion::kGrin: // a giggle of three syllables
                syllable(450, 500, 520, 120, 50, 0.9f, 1.1f, 9.0f, 0.4f, 15, 40);
                syllable(450, 500, 520, 120, 50, 0.9f, 1.1f, 9.0f, 0.4f, 15, 40);
                syllable(480, 560, 600, 250, 0, 0.9f, 1.1f, 9.0f, 0.4f, 15, 80);
                break;
            case Emotion::kSurprised: // a quick "wow!" going up
                syllable(300, 450, 650, 150, 0, 0.8f, 1.0f, 9.0f, 0.4f, 8, 40);
                break;
            case Emotion::kUpset: // a worried stutter
                for (int i = 0; i < 4; i++)
                {
                    syllable(420, 430, 380, 100, i < 3 ? 40 : 0, 1.0f, 1.3f, 14.0f, 1.0f, 10, 40);
                }
                break;
            case Emotion::kRelax: // one slow, soft syllable
                syllable(300, 330, 280, 700, 0, 1.4f, 1.8f, 4.0f, 0.3f, 120, 250);
                break;
            default:
                break;
            }
            return count;
        }
    } // namespace emotion_detail

    /**
     * @brief Make the voice of a character for an emotion
     *
     * The voice is an array of notes. Play it with VoicePlayer::playPhrase(), or change the notes before
     * playing to tune the voice. It is shorter than VoicePlayer::kMaxDurationMs.
     *
     * @param character whose voice
     * @param emotion the emotion
     * @param out destination of the notes
     * @param capacity size of `out`, `kMaxEmotionNotes` is enough
     * @param seed The robot voice changes the pitch a little with the seed. The same seed gives the same voice.
     * @return number of the notes
     */
    inline size_t emotionVoice(Character character, Emotion emotion, VoiceNote *out, size_t capacity,
                               uint32_t seed = 1)
    {
        if (out == nullptr || capacity == 0)
        {
            return 0;
        }
        switch (character)
        {
        case Character::kRobot:
            return emotion_detail::robotVoice(emotion, out, capacity, seed);
        case Character::kCat:
            return emotion_detail::catVoice(emotion, out, capacity);
        case Character::kDog:
            return emotion_detail::dogVoice(emotion, out, capacity);
        case Character::kBird:
            return emotion_detail::birdVoice(emotion, out, capacity);
        case Character::kOwl:
            return emotion_detail::owlVoice(emotion, out, capacity);
        case Character::kFriendlyRobot:
            return emotion_detail::friendlyRobotVoice(emotion, out, capacity);
        default:
            return 0;
        }
    }
} // namespace stackchan::voice
