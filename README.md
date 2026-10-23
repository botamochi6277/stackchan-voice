# StackChan voice

*stackchan-voice* is a Arduino library to enable StackChan to tweet.
This library is for [StackChan Core](https://docs.m5stack.com/ja/core/StackChan_Core)

target voice: R2D2 voice, LOVOT voice, etc.

> [!WARNING]
> This library is WIP (*work in progress*). Currently it plays sine waves, sums of sine waves, and character voices for each emotion (robot, cat, dog, bird, owl, friendly robot).
> The voices are made from the characteristics of each sound and have not been tuned with a real device yet. Breaking changes are expected.

## Installment

you can install with pioarduino.

Add this repository and the [pioarduino](https://github.com/pioarduino/platform-espressif32) platform (Arduino-ESP32 3.x / ESP-IDF 5.x) to the `platformio.ini` of your project:

```ini
[env:m5stack-cores3]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = m5stack-cores3
framework = arduino
lib_deps =
  m5stack/M5Unified
  https://github.com/botamochi6277/stackchan-voice.git
```

- The only dependency is [M5Unified](https://github.com/m5stack/M5Unified). The speaker is played with `M5.Speaker`.
- The demo is built for `m5stack-cores3` and `m5stack-core2`. The stock `espressif32` platform is not tested.
- Include `<StackChanVoice.h>` to use everything.

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

VoiceNote notes[presets::kMaxRobotNotes];
size_t n = presets::robot(notes, presets::kMaxRobotNotes, /* seed */ 1);  // robot-like burble
voice.playPhrase(notes, n);

n = presets::catMeow(notes, presets::kMaxRobotNotes);      // cat-like meow
n = presets::puppyWhine(notes, presets::kMaxRobotNotes);   // puppy-like whine
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
| `vibrato_hz`, `vibrato_semitones` | the pitch swings around the curve | 5 to 8 Hz, under 1 semitone: a living voice. 20 to 30 Hz, 3 to 7 semitones: the robot trill |
| `attack_ms`, `release_ms` | fade in and fade out | 3 ms is a hard electronic start, 30 to 150 ms is soft |
| `amplitude` | level of the note in the phrase | accents |

- The presets are made of these fields. `robot()` chooses 6 to 8 sounds (chirp up, chirp down, trill, blip) at random pitches between 600 and 2600 Hz with 4 slightly stretched harmonics. The same seed always gives the same phrase, on every board.
- Harmonics that would be above the Nyquist frequency are left out, so a high pitch with many harmonics never aliases.
- A phrase can be at most 2 seconds (`VoicePlayer::kMaxDurationMs`). `phraseDurationMs()` tells the length.
- `renderVoiceNote()` and `renderPhrase()` in [src/VoiceSynth.h](src/VoiceSynth.h) only generate the samples, like `generateSine()`, so they can be used on a PC.

### Voices for the emotions

`emotionVoice()` makes the voice of a character for an emotion. The emotions are the same as the emotional expressions of [stackchan-display](../stackchan-display) (`Expression::kNeutral` to `kRelax`), so a face and its voice can be changed together.

```cpp
using namespace stackchan::voice;

VoiceNote notes[kMaxEmotionNotes];
size_t n = emotionVoice(Character::kCat, Emotion::kSmile, notes, kMaxEmotionNotes);
voice.playPhrase(notes, n);
```

The characters (`Character`):

| character | the sound |
|---|---|
| `kRobot` | electronic chirps, trills and blips, a little metallic (high, 600 to 3000 Hz) |
| `kCat` | meows: the pitch goes up to a peak and comes down |
| `kDog` | barks, growls and whines |
| `kBird` | a small bird: very high (about 2 to 3.7 kHz), almost pure sine waves, quick chirps and trills |
| `kOwl` | hoots: low (about 250 to 700 Hz), soft and round, with a slow fade in |
| `kFriendlyRobot` | a warm robot that almost talks: a voice-like sound in the middle range with a wobble, in slow syllables, not chirps |

The voice of each emotion:

| emotion | robot | cat | dog |
|---|---|---|---|
| `kNeutral` | "pi-po", two short blips | "mya", a short meow | "wan", one bark |
| `kAngry` | low, rough chirps falling | a long, low, rough meow | "grrr", a low growl |
| `kSad` | slow falling chirps, soft | a long, falling, dark meow | "kuuun", a slow whine |
| `kDoubt` | two blips and a rising chirp | "mya?", the pitch goes up at the end | "un?", the pitch goes up |
| `kSleepy` | slow, low, falling like a yawn | a low, soft, long "myuu..." | a low, soft, long sigh |
| `kSmile` | three happy rising chirps | a higher, cheerful meow | "wan wan", two happy barks |
| `kGrin` | fast rising chirps and trills | two quick rising meows | "wan wan wan", three quick yips |
| `kSurprised` | a sudden high chirp and a blip | a sharp high "nya!" | "kyan!", a short high yelp |
| `kUpset` | fast, high, shaky trills | two shaky high meows | "kuun kuun", two shaky whines |
| `kRelax` | two gentle slow chirps | a gentle slow meow | "fuun", a soft low sound |

| emotion | bird | owl | friendly robot |
|---|---|---|---|
| `kNeutral` | "pi", one short chirp | "hoo", one hoot | "wo", one syllable |
| `kAngry` | a scolding chatter: four low, harsh chirps | short, low, rough "hoh! hoh!" | gruff, low "gah! gah!" |
| `kSad` | slow falling, drooping chirps | long, sinking hoots | low, drooping syllables |
| `kDoubt` | a blip, then a chirp going up | "hoo? hoo?", the second goes up | "uh-huh?", the second goes up |
| `kSleepy` | slow, low and soft, fading away | one very low, slow hoot | powering down: low, slow, falling |
| `kSmile` | a cheerful song of four rising chirps | "ho-hoo", two higher hoots | "wa-ow", two rising syllables |
| `kGrin` | a lively song with a trill | "ho-ho-hoo", three hoots | a giggle of three syllables |
| `kSurprised` | a sudden high chirp | a sharp "hoh!" | a quick "wow!" going up |
| `kUpset` | a frantic chatter of trills | restless, trembling hoots | a worried stutter |
| `kRelax` | a gentle two-note song | one slow, gentle hoot | one slow, soft syllable |

- The voices are made of `VoiceNote` (see above), so they can be tuned by changing the notes before playing. The notes of each voice are written one by one in [src/EmotionVoices.h](src/EmotionVoices.h).
- The emotions are sorted by the way a voice shows them: happy and surprised voices are higher and quicker, sad and sleepy voices are lower and slower, angry voices are rough, and a question (`kDoubt`) goes up at the end.
- The robot voice changes the pitch of the notes a little with the `seed`, so the voice is not exactly the same every time. The same seed gives the same voice.
- `Emotion` is the library's own enum, so that this library does not depend on stackchan-display. For the 10 emotional expressions the value is the same, and `static_cast<stackchan::voice::Emotion>(expression)` converts it.

### Files

| file | what it has | depends on Arduino / M5Unified |
|---|---|---|
| [src/SineWave.h](src/SineWave.h) | `generateSine()`, `generateSineMix()`, `harmonicSeries()`, `Partial` | no |
| [src/VoiceSynth.h](src/VoiceSynth.h) | `VoiceNote`, `renderVoiceNote()`, `renderPhrase()`, `phraseDurationMs()` | no |
| [src/VoicePresets.h](src/VoicePresets.h) | `presets::robot()`, `presets::catMeow()`, `presets::puppyWhine()` | no |
| [src/EmotionVoices.h](src/EmotionVoices.h) | `Emotion`, `Character`, `emotionVoice()`, `emotionName()`, `characterName()` | no |
| [src/VoicePlayer.h](src/VoicePlayer.h) | `VoicePlayer`, which plays the sounds with `M5.Speaker` | yes |
| [src/StackChanVoice.h](src/StackChanVoice.h) | includes all of the above | yes |

The files that do not depend on Arduino / M5Unified generate the samples only, so they are tested on a PC (see Development).

### Limits

- One sound is at most 2 seconds (`VoicePlayer::kMaxDurationMs`), and a phrase made of notes is also at most 2 seconds. `playSine()`, `playSineMix()` and `playPhrase()` return false for a longer sound.
- At most 16 sine waves are added at once (`kMaxPartials`), and an emotion voice has at most 8 notes (`kMaxEmotionNotes`).
- The sounds are made at 16 kHz by default, so the highest frequency is 8 kHz (the Nyquist frequency). A higher sampling rate can be given to the constructor, `VoicePlayer voice(22050);`, but it is not tested on a device. The sine waves above the Nyquist frequency are left out.
- The sound is generated when it is played, and the samples are kept in memory while they are played. A 2 second sound at 16 kHz is 64 KB, and two buffers are used.

## Demo

[examples/SineWave](examples/SineWave/SineWave.ino) has nine modes:

| mode | sound |
|---|---|
| single sine | a C major scale up and down, one sine wave at a time |
| chord: 3 sines | the chords C - F - G - C, each made of 3 added sine waves |
| harmonics: 6 sines | a C major scale, each note is a fundamental sine wave plus 5 harmonics |
| robot emotions | the robot-like voice for each of the 10 emotions, one after another |
| cat emotions | the cat-like voice for each emotion |
| dog emotions | the dog-like voice for each emotion |
| bird emotions | the bird-like voice for each emotion |
| owl emotions | the owl-like voice for each emotion |
| friendly robot emotions | the friendly-robot-like voice for each emotion |

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
g++ -std=c++17 -Wall -Isrc test/test_emotion.cpp -o test_emotion && ./test_emotion
```

## Notes

- The speaker and the microphone share the I2S port of the CoreS3. Starting the speaker stops the microphone (and the other way around), so the loud sound detection of [stackchan-agent](../stackchan-agent) cannot run while sounds are played. This has to be solved when the voice is used in the agent.
