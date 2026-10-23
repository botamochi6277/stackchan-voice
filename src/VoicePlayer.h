#pragma once

#include <M5Unified.h>

#include <vector>

#include "SineWave.h"

namespace stackchan::voice
{
    /**
     * @brief Plays generated sounds with the speaker of M5Stack
     *
     * It generates the wave form, keeps it alive while the speaker reads it and plays it with `M5.Speaker`.
     */
    class VoicePlayer
    {
    public:
        /// longest sound that can be played at once [msec]
        static constexpr uint32_t kMaxDurationMs = 2000;

        /**
         * @param sample_rate sampling rate of the generated sounds [Hz]
         */
        explicit VoicePlayer(uint32_t sample_rate = 16000) : sample_rate_(sample_rate) {}

        /**
         * @brief Start the speaker. Call it after `M5.begin()`.
         *
         * @return false if the board has no speaker
         */
        bool begin();

        /**
         * @param volume 0 (mute) to 255
         */
        void setVolume(uint8_t volume);

        /**
         * @brief Play a sine wave. It does not wait for the end of the sound, and it replaces the sound being played.
         *
         * @param frequency frequency [Hz]
         * @param duration_ms duration [msec], at most `kMaxDurationMs`
         * @param amplitude 0.0 to 1.0
         * @return false if the speaker is not available or the arguments are invalid
         */
        bool playSine(float frequency, uint32_t duration_ms, float amplitude = 0.5f);

        /// @return true while a sound is being played
        bool isPlaying() const;

        /// @brief Stop the sound being played
        void stop();

        /// @brief Wait until the sound being played ends
        void waitUntilFinished() const;

        uint32_t getSampleRate() const
        {
            return sample_rate_;
        }

    private:
        uint32_t sample_rate_;
        // The speaker reads the samples while playing, so they must stay alive.
        // Two buffers are used alternately so that a new sound does not overwrite the sound being stopped.
        std::vector<int16_t> buffers_[2];
        uint8_t next_buffer_ = 0;
    };
} // namespace stackchan::voice
