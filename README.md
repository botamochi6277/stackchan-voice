# StackChan voice

*stackchan-voice* is a Arduino library to enable StackChan to tweet.
This library is for [StackChan Core](https://docs.m5stack.com/ja/core/StackChan_Core)

target voice: R2D2 voice, LOVOT voice, etc.

> [!WARNING]
> This library is WIP (*work in progress*). Currently it only plays sine waves.

## Installment

you can install with pioarduino.

<!-- TODO: write instruction -->

## Usage

```cpp
#include <M5Unified.h>
#include <StackChanVoice.h>

stackchan::voice::VoicePlayer voice;

void setup()
{
  auto cfg = M5.config();
  M5.begin(cfg);
  voice.begin();          // start the speaker
  voice.setVolume(128);   // 0 - 255
}

void loop()
{
  voice.playSine(440.0f, 500);  // A4 for 500 msec, it does not block
  M5.delay(1000);
}
```

- `VoicePlayer::playSine(frequency, duration_ms, amplitude)` generates a sine wave, and plays it with `M5.Speaker`. It does not wait for the end of the sound, and it replaces the sound being played. `duration_ms` is at most 2000.
- The wave fades in and out for 5 msec, so there are no clicks at the start and the end.
- `generateSine()` in [src/SineWave.h](src/SineWave.h) generates the samples only. It does not depend on Arduino / M5Unified.

## Demo

[examples/SineWave](examples/SineWave/SineWave.ino) plays a C major scale up and down, with the frequency on the display.
Tap the screen (or press the button A) to change the volume.

## Development

`platformio.ini` builds the demo with the library in this directory:

```bash
pio run -e m5stack-cores3 -t upload   # CoreS3 (default)
pio run -e m5stack-core2 -t upload    # Core2
pio device monitor                    # 115200 baud
```

All environments use the [pioarduino](https://github.com/pioarduino/platform-espressif32) platform (Arduino-ESP32 3.x / ESP-IDF 5.x), not the stock `espressif32` platform. It is set in `platformio.ini`.

The wave generator is tested on a PC, without hardware:

```bash
g++ -std=c++17 -Wall -Isrc test/test_sine.cpp -o test_sine
./test_sine
```

## Notes

- The speaker and the microphone share the I2S port of the CoreS3. Starting the speaker stops the microphone (and the other way around), so the loud sound detection of [stackchan-agent](../stackchan-agent) cannot run while sounds are played. This has to be solved when the voice is used in the agent.
