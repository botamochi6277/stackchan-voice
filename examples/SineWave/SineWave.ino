// Demo: play a C major scale with sine waves.
// Tap the screen (or press the button A) to change the volume.
#include <M5Unified.h>
#include <StackChanVoice.h>

stackchan::voice::VoicePlayer voice;

// C4 - C5
const float notes[] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 493.88f, 523.25f};
const int num_notes = sizeof(notes) / sizeof(notes[0]);

const uint8_t volumes[] = {64, 128, 200};
const int num_volumes = sizeof(volumes) / sizeof(volumes[0]);

int note_idx = 0;
int direction = 1; // 1: up, -1: down
int volume_idx = 1;
unsigned long next_note_ms = 0;

void showStatus(float frequency)
{
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 10);
  M5.Display.println("StackChan voice");
  M5.Display.setCursor(10, 50);
  M5.Display.println("sine wave demo");
  M5.Display.setCursor(10, 100);
  M5.Display.printf("%.2f Hz", frequency);
  M5.Display.setCursor(10, 140);
  M5.Display.printf("volume %d", volumes[volume_idx]);
  M5.Display.setCursor(10, 200);
  M5.Display.println("tap: change volume");
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

  // change the volume with a tap or the button A
  bool tapped = M5.BtnA.wasPressed();
  if (M5.Touch.isEnabled())
  {
    tapped = tapped || M5.Touch.getDetail().wasPressed();
  }
  if (tapped)
  {
    volume_idx = (volume_idx + 1) % num_volumes;
    voice.setVolume(volumes[volume_idx]);
    Serial.printf("volume: %d\n", volumes[volume_idx]);
  }

  // play the next note every 600 msec (the sound is 500 msec)
  if (millis() >= next_note_ms)
  {
    float f = notes[note_idx];
    voice.playSine(f, 500, 0.5f);
    showStatus(f);
    Serial.printf("%.2f Hz\n", f);

    if (note_idx + direction < 0 || note_idx + direction >= num_notes)
    {
      direction = -direction; // turn back at both ends
    }
    note_idx += direction;
    next_note_ms = millis() + 600;
  }

  M5.delay(10);
}
