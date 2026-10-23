// Demo: play sine waves, sums of sine waves, and character voices.
//   mode 0: single sine waves (C major scale)
//   mode 1: chords, each note is a sine wave and they are added (C - F - G - C)
//   mode 2: harmonics, a fundamental sine wave and 5 harmonics are added (C major scale)
//   mode 3: robot-like voice for each emotion (neutral, angry, sad, doubt, sleepy, smile, grin, surprised, upset, relax)
//   mode 4: cat-like voice for each emotion
//   mode 5: dog-like voice for each emotion
//   mode 6: bird-like voice for each emotion
//   mode 7: owl-like voice for each emotion
//   mode 8: friendly-robot-like voice for each emotion
//
// The emotions are the same as the emotional expressions of stackchan-display.
//
// CoreS3: tap the upper half of the screen to change the volume, and the lower half to change the mode.
// Core2:  button A changes the volume, and the button B changes the mode.
#include <M5Unified.h>
#include <StackChanVoice.h>

using stackchan::voice::Partial;

stackchan::voice::VoicePlayer voice;

enum Mode
{
  kSingle = 0,
  kChord,
  kHarmonics,
  kEmotionRobot,
  kEmotionCat,
  kEmotionDog,
  kEmotionBird,
  kEmotionOwl,
  kEmotionFriendlyRobot,
  kNumModes
};
const char *mode_names[] = {"single sine", "chord: 3 sines", "harmonics: 6 sines",
                            "robot emotions", "cat emotions", "dog emotions",
                            "bird emotions", "owl emotions", "friendly robot emotions"};

// C4 - C5
const float notes[] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 493.88f, 523.25f};
const int num_notes = sizeof(notes) / sizeof(notes[0]);

// C major, F major, G major, C major
const float chords[][3] = {
    {261.63f, 329.63f, 392.00f}, // C4 E4 G4
    {349.23f, 440.00f, 523.25f}, // F4 A4 C5
    {196.00f, 246.94f, 293.66f}, // G3 B3 D4
    {261.63f, 329.63f, 392.00f}, // C4 E4 G4
};
const int num_chords = sizeof(chords) / sizeof(chords[0]);

const uint8_t volumes[] = {64, 128, 200};
const int num_volumes = sizeof(volumes) / sizeof(volumes[0]);

int mode = kSingle;
int note_idx = 0;
int emotion_idx = 0; // the emotion of the next voice
uint32_t phrase_count = 0; // how many voices were played, used as the seed of the robot voice
int direction = 1; // 1: up, -1: down
int volume_idx = 1;
unsigned long next_note_ms = 0;

void showStatus(const char *sound)
{
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 10);
  M5.Display.println("StackChan voice");
  M5.Display.setCursor(10, 50);
  M5.Display.println(mode_names[mode]);
  M5.Display.setCursor(10, 100);
  M5.Display.println(sound);
  M5.Display.setCursor(10, 140);
  M5.Display.printf("volume %d", volumes[volume_idx]);
  M5.Display.setCursor(10, 190);
  M5.Display.println("upper tap/A: volume");
  M5.Display.setCursor(10, 215);
  M5.Display.println("lower tap/B: mode");
}

// play the next sound of the current mode
// returns how long to wait before the next sound [msec]
uint32_t playNext()
{
  char text[48];
  uint32_t wait_ms = 600; // the sound is 500 msec

  if (mode == kSingle)
  {
    float f = notes[note_idx];
    voice.playSine(f, 500, 0.5f);
    snprintf(text, sizeof(text), "%.1f Hz", f);

    if (note_idx + direction < 0 || note_idx + direction >= num_notes)
    {
      direction = -direction; // turn back at both ends
    }
    note_idx += direction;
  }
  else if (mode == kChord)
  {
    // the 3 sine waves are added
    Partial partials[3];
    for (int k = 0; k < 3; k++)
    {
      partials[k].frequency = chords[note_idx % num_chords][k];
      partials[k].amplitude = 1.0f;
    }
    voice.playSineMix(partials, 3, 500, 0.5f);
    snprintf(text, sizeof(text), "%.0f+%.0f+%.0f Hz", partials[0].frequency, partials[1].frequency,
             partials[2].frequency);
    note_idx = (note_idx + 1) % num_chords;
  }
  else if (mode >= kEmotionRobot)
  {
    // the voice of a character for each emotion, one after another.
    // The modes kEmotionRobot ... are in the same order as Character.
    using namespace stackchan::voice;
    Character character = static_cast<Character>(mode - kEmotionRobot);
    Emotion emotion = static_cast<Emotion>(emotion_idx);

    VoiceNote voice_notes[kMaxEmotionNotes];
    size_t n = emotionVoice(character, emotion, voice_notes, kMaxEmotionNotes, phrase_count++);
    voice.playPhrase(voice_notes, n, 0.6f);

    uint32_t duration = phraseDurationMs(voice_notes, n);
    snprintf(text, sizeof(text), "%s, %d ms", emotionName(emotion), (int)duration);
    wait_ms = duration + 800; // listen to the whole voice, and a short silence
    emotion_idx = (emotion_idx + 1) % static_cast<int>(Emotion::kCount);
  }
  else
  {
    // a fundamental sine wave and its harmonics are added, the higher the softer
    Partial partials[6];
    float f0 = notes[note_idx];
    size_t n = stackchan::voice::harmonicSeries(f0, 6, 1.0f, partials, 6);
    voice.playSineMix(partials, n, 500, 0.5f);
    snprintf(text, sizeof(text), "f0 %.1f Hz x%d", f0, (int)n);

    if (note_idx + direction < 0 || note_idx + direction >= num_notes)
    {
      direction = -direction;
    }
    note_idx += direction;
  }

  showStatus(text);
  Serial.printf("%s: %s\n", mode_names[mode], text);
  return wait_ms;
}

void setup()
{
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  if (!voice.begin())
  {
    M5.Display.println("No speaker");
    Serial.println("No speaker");
    return;
  }
  voice.setVolume(volumes[volume_idx]);
  Serial.println("StackChan voice: sine wave demo");
}

void loop()
{
  M5.update();

  // button A / the upper half of the screen: volume, button B / the lower half: mode
  bool change_volume = M5.BtnA.wasPressed();
  bool change_mode = M5.BtnB.wasPressed();
  if (M5.Touch.isEnabled())
  {
    auto t = M5.Touch.getDetail();
    if (t.wasPressed())
    {
      if (t.y < M5.Display.height() / 2)
      {
        change_volume = true;
      }
      else
      {
        change_mode = true;
      }
    }
  }

  if (change_volume)
  {
    volume_idx = (volume_idx + 1) % num_volumes;
    voice.setVolume(volumes[volume_idx]);
    Serial.printf("volume: %d\n", volumes[volume_idx]);
  }
  if (change_mode)
  {
    mode = (mode + 1) % kNumModes;
    note_idx = 0;
    emotion_idx = 0;
    direction = 1;
    next_note_ms = 0; // play right away
  }

  // play the next sound every 600 msec (the sound is 500 msec)
  if (millis() >= next_note_ms)
  {
    next_note_ms = millis() + playNext();
  }

  M5.delay(10);
}
