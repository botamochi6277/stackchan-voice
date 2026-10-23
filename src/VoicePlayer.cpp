#include "VoicePlayer.h"

namespace stackchan::voice
{
    bool VoicePlayer::begin()
    {
        if (!M5.Speaker.isEnabled())
        {
            return false;
        }
        if (!M5.Speaker.isRunning())
        {
            return M5.Speaker.begin();
        }
        return true;
    }

    void VoicePlayer::setVolume(uint8_t volume)
    {
        M5.Speaker.setVolume(volume);
    }

    template <typename Generator>
    bool VoicePlayer::playGenerated(uint32_t duration_ms, Generator generate)
    {
        if (!M5.Speaker.isEnabled() || duration_ms == 0 || duration_ms > kMaxDurationMs)
        {
            return false;
        }

        auto &buffer = buffers_[next_buffer_];
        buffer.resize(sampleCount(duration_ms, sample_rate_));
        generate(buffer.data(), buffer.size());

        // channel 0, stop the current sound
        if (!M5.Speaker.playRaw(buffer.data(), buffer.size(), sample_rate_, false, 1, 0, true))
        {
            return false;
        }
        next_buffer_ = (next_buffer_ + 1) % 2;
        return true;
    }

    bool VoicePlayer::playSine(float frequency, uint32_t duration_ms, float amplitude)
    {
        Partial partial;
        partial.frequency = frequency;
        return playSineMix(&partial, 1, duration_ms, amplitude);
    }

    bool VoicePlayer::playSineMix(const Partial *partials, size_t count, uint32_t duration_ms, float amplitude)
    {
        if (partials == nullptr || count == 0)
        {
            return false;
        }
        if (count > kMaxPartials)
        {
            count = kMaxPartials;
        }

        // the waves must be below the Nyquist frequency. Refuse a sound that would be silent.
        bool playable = false;
        for (size_t k = 0; k < count; k++)
        {
            playable = playable || isValidPartial(partials[k], sample_rate_);
        }
        if (!playable)
        {
            return false;
        }

        return playGenerated(duration_ms, [&](int16_t *out, size_t length)
                             { generateSineMix(out, length, partials, count, sample_rate_, amplitude); });
    }

    bool VoicePlayer::playPhrase(const VoiceNote *notes, size_t count, float amplitude)
    {
        if (notes == nullptr || count == 0)
        {
            return false;
        }
        return playGenerated(phraseDurationMs(notes, count), [&](int16_t *out, size_t length)
                             { renderPhrase(out, length, notes, count, sample_rate_, amplitude); });
    }

    bool VoicePlayer::isPlaying() const
    {
        return M5.Speaker.isPlaying();
    }

    void VoicePlayer::stop()
    {
        M5.Speaker.stop();
    }

    void VoicePlayer::waitUntilFinished() const
    {
        while (M5.Speaker.isPlaying())
        {
            M5.delay(1);
        }
    }
} // namespace stackchan::voice
