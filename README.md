# StackChan voice

*stackchan-voice* is a Arduino library to enable StackChan to tweet.
This library is for [StackChan Core](https://docs.m5stack.com/ja/core/StackChan_Core)

target voice: R2D2 voice, LOVOT voice, etc.

> [!WARNING]
> This library is WIP (*work in progress*). Currently it plays sine waves, sums of sine waves, and character voices (R2D2-like, cat-like, puppy-like).

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

### Sum of sine waves

A real voice is a sum of waves of many frequencies. `VoicePlayer::playSineMix()` adds the sine waves of a list of `Partial` (frequency and relative amplitude):

```cpp
using stackchan::voice::Partial;

// a chord: C4 + E4 + G4
Partial chord[] = {{261.63f, 1.0f}, {329.63f, 1.0f}, {392.00f, 1.0f}};
voice.playSineMix(chord, 3, 500);

// a fundamental frequency and its 5 harmonics, the higher the softer
Partial voiced[6];
size_t n = stackchan::voice::harmonicSeries(220.0f, 6, 1.0f, voiced, 6);
voice.playSineMix(voiced, n, 500);
```

- Only the *ratio* between the amplitudes matters. The sum is normalized by the total amplitude, so it never clips however many partials are added. The `amplitude` argument is the level of the whole sound.
- At most 16 partials (`kMaxPartials`) are added. A partial that is silent, or at or above the Nyquist frequency (half of the sampling rate, 8000 Hz at the default 16 kHz), is skipped. `playSineMix()` returns false if no partial can be played.
- `harmonicSeries(f0, count, rolloff, out, capacity)` makes a fundamental frequency and its integer multiples. The k-th harmonic gets the amplitude `1 / k^rolloff`, so a larger `rolloff` is softer, and 0 gives all the harmonics the same amplitude.
- `generateSineMix()` generates the samples only, like `generateSine()`, and can be used on a PC.

### Character voices

`VoiceNote` is `harmonicSeries()` whose parameters change in time. It makes one sound of a character: a chirp, a meow, a whine. `VoicePlayer::playPhrase()` plays notes one after another.

```cpp
using namespace stackchan::voice;

VoiceNote notes[presets::kMaxR2D2Notes];
size_t n = presets::r2d2(notes, presets::kMaxR2D2Notes, /* seed */ 1);  // R2D2-like burble
voice.playPhrase(notes, n);

n = presets::catMeow(notes, presets::kMaxR2D2Notes);      // cat-like meow
n = presets::puppyWhine(notes, presets::kMaxR2D2Notes);   // puppy-like whine
```

The presets only fill `VoiceNote`s. Change their fields before playing to tune the voice, or make your own notes:

| field | what it does | try |
|---|---|---|
| `f_start`, `f_end` | pitch glides from `f_start` to `f_end` [Hz] (straight in musical pitch) | up = a question or a happy sound, down = a sad or tired sound |
| `f_peak`, `peak_position` | pitch goes through a peak at `peak_position` (0 to 1) | a meow rises fast and falls slowly (peak at 0.2 to 0.3) |
| `duration_ms`, `gap_ms` | length of the sound, and the silence after it | short (40 to 150 ms) is a chirp, long (500 ms or more) is a cry |
| `harmonics` | number of sine waves, the fundamental and its harmonics (1 to 16) | 1 is a pure beep, 4 is electronic, 8 to 12 is a voice |
| `rolloff_start`, `rolloff_end` | the k-th harmonic has the amplitude `1 / k^rolloff`. Small is bright, large is soft. It glides from start to end | bright to dark makes a "mee" to "ow" change |
| `stretch` | moves the harmonics off the exact multiples (`k^(1+stretch)`) | 0 is a voice, 0.04 is metallic, a robot or a bell |
| `vibrato_hz`, `vibrato_semitones` | the pitch swings around the curve | 5 to 8 Hz, under 1 semitone: a living voice. 20 to 30 Hz, 3 to 7 semitones: the R2D2 trill |
| `attack_ms`, `release_ms` | fade in and fade out | 3 ms is a hard electronic start, 30 to 150 ms is soft |
| `amplitude` | level of the note in the phrase | accents |

- The presets are made of these fields. `r2d2()` chooses 6 to 8 sounds (chirp up, chirp down, trill, blip) at random pitches between 600 and 2600 Hz with 4 slightly stretched harmonics. The same seed always gives the same phrase, on every board.
- Harmonics that would be above the Nyquist frequency are left out, so a high pitch with many harmonics never aliases.
- A phrase can be at most 2 seconds (`VoicePlayer::kMaxDurationMs`). `phraseDurationMs()` tells the length.
- `renderVoiceNote()` and `renderPhrase()` in [src/VoiceSynth.h](src/VoiceSynth.h) only generate the samples, like `generateSine()`, so they can be used on a PC.

## Demo

[examples/SineWave](examples/SineWave/SineWave.ino) has three modes:

| mode | sound |
|---|---|
| single sine | a C major scale up and down, one sine wave at a time |
| chord: 3 sines | the chords C - F - G - C, each made of 3 added sine waves |
| harmonics: 6 sines | a C major scale, each note is a fundamental sine wave plus 5 harmonics |
| R2D2-like | a different R2D2-like burble every time |
| cat-like | a meow |
| puppy-like | a whine |

CoreS3: tap the upper half of the screen to change the volume, and the lower half to change the mode.
Core2: the button A changes the volume, and the button B changes the mode.

## Development

`platformio.ini` builds the demo with the library in this directory:

```bash
pio run -e m5stack-cores3 -t upload   # CoreS3 (default)
pio run -e m5stack-core2 -t upload    # Core2
pio device monitor                    # 115200 baud
```

All environments use the [pioarduino](https://github.com/pioarduino/platform-espressif32) platform (Arduino-ESP32 3.x / ESP-IDF 5.x), not the stock `espressif32` platform. It is set in `platformio.ini`.

The wave generators are tested on a PC, without hardware:

```bash
g++ -std=c++17 -Wall -Isrc test/test_sine.cpp -o test_sine && ./test_sine
g++ -std=c++17 -Wall -Isrc test/test_voice.cpp -o test_voice && ./test_voice
```

## Notes

- The speaker and the microphone share the I2S port of the CoreS3. Starting the speaker stops the microphone (and the other way around), so the loud sound detection of [stackchan-agent](../stackchan-agent) cannot run while sounds are played. This has to be solved when the voice is used in the agent.
