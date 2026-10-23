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

    bool VoicePlayer::playSine(float frequency, uint32_t duration_ms, float amplitude)
    {
        if (!M5.Speaker.isEnabled() || frequency <= 0.0f || duration_ms == 0 || duration_ms > kMaxDurationMs)
        {
            return false;
        }
        // the wave must be below the Nyquist frequency
        if (frequency >= sample_rate_ / 2.0f)
        {
            return false;
        }

        auto &buffer = buffers_[next_buffer_];
        buffer.resize(sampleCount(duration_ms, sample_rate_));
        generateSine(buffer.data(), buffer.size(), frequency, sample_rate_, amplitude);

        // channel 0, stop the current sound
        if (!M5.Speaker.playRaw(buffer.data(), buffer.size(), sample_rate_, false, 1, 0, true))
        {
            return false;
        }
        next_buffer_ = (next_buffer_ + 1) % 2;
        return true;
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
