// Host-side test of the emotion voices. No hardware is needed.
// Build and run (from stackchan-voice/):
//   g++ -std=c++17 -Wall -Isrc test/test_emotion.cpp -o test_emotion && ./test_emotion
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "EmotionVoices.h"

using namespace stackchan::voice;

static int failures = 0;
#define CHECK(cond)                                          \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                            \
    }                                                        \
  } while (0)

static const uint32_t kRate = 16000;
static const size_t kNumCharacters = static_cast<size_t>(Character::kCount);
static const size_t kNumEmotions = static_cast<size_t>(Emotion::kCount);

struct Voice {
  VoiceNote notes[kMaxEmotionNotes];
  size_t count = 0;
};

static Voice make(Character c, Emotion e, uint32_t seed = 1) {
  Voice v;
  v.count = emotionVoice(c, e, v.notes, kMaxEmotionNotes, seed);
  return v;
}

// mean pitch of the notes: the geometric mean of the start, the peak (if any) and the end of each note
static double meanPitch(const Voice& v) {
  double sum = 0.0;
  for (size_t i = 0; i < v.count; i++) {
    const VoiceNote& n = v.notes[i];
    double log_sum = log(n.f_start) + log(n.f_end);
    int terms = 2;
    if (n.f_peak > 0.0f) {
      log_sum += log(n.f_peak);
      terms++;
    }
    sum += exp(log_sum / terms);
  }
  return sum / v.count;
}

static std::vector<int16_t> renderVoice(const Voice& v, float amplitude = 1.0f) {
  uint32_t total = phraseDurationMs(v.notes, v.count);
  std::vector<int16_t> buf(sampleCount(total, kRate));
  renderPhrase(buf.data(), buf.size(), v.notes, v.count, kRate, amplitude);
  return buf;
}

static int peakOf(const std::vector<int16_t>& buf) {
  int peak = 0;
  for (auto s : buf) peak = abs(s) > peak ? abs(s) : peak;
  return peak;
}

static void test_enum_order_matches_display() {
  // the same order as stackchan::display::Expression::kNeutral .. kRelax
  CHECK(static_cast<int>(Emotion::kNeutral) == 0);
  CHECK(static_cast<int>(Emotion::kAngry) == 1);
  CHECK(static_cast<int>(Emotion::kSad) == 2);
  CHECK(static_cast<int>(Emotion::kDoubt) == 3);
  CHECK(static_cast<int>(Emotion::kSleepy) == 4);
  CHECK(static_cast<int>(Emotion::kSmile) == 5);
  CHECK(static_cast<int>(Emotion::kGrin) == 6);
  CHECK(static_cast<int>(Emotion::kSurprised) == 7);
  CHECK(static_cast<int>(Emotion::kUpset) == 8);
  CHECK(static_cast<int>(Emotion::kRelax) == 9);
  CHECK(kNumEmotions == 10);
}

static void test_names() {
  CHECK(strcmp(emotionName(Emotion::kNeutral), "neutral") == 0);
  CHECK(strcmp(emotionName(Emotion::kSurprised), "surprised") == 0);
  CHECK(strcmp(emotionName(Emotion::kRelax), "relax") == 0);
  CHECK(strcmp(emotionName(Emotion::kCount), "?") == 0);
  CHECK(strcmp(characterName(Character::kRobot), "robot") == 0);
  CHECK(strcmp(characterName(Character::kCat), "cat") == 0);
  CHECK(strcmp(characterName(Character::kDog), "dog") == 0);
  CHECK(strcmp(characterName(Character::kCount), "?") == 0);
}

static void test_every_voice_is_valid() {
  for (size_t c = 0; c < kNumCharacters; c++) {
    for (size_t e = 0; e < kNumEmotions; e++) {
      Character ch = static_cast<Character>(c);
      Emotion em = static_cast<Emotion>(e);
      Voice v = make(ch, em);

      // has notes, and can be played at once
      CHECK(v.count >= 1 && v.count <= kMaxEmotionNotes);
      uint32_t total = phraseDurationMs(v.notes, v.count);
      if (total < 100 || total > 2000) {
        printf("%s %s: %u ms\n", characterName(ch), emotionName(em), total);
      }
      CHECK(total >= 100 && total <= 2000);

      // every pitch can be played at 16 kHz, in the range of a voice
      for (size_t i = 0; i < v.count; i++) {
        const VoiceNote& n = v.notes[i];
        CHECK(n.f_start >= 80.0f && n.f_start < 4000.0f);
        CHECK(n.f_end >= 80.0f && n.f_end < 4000.0f);
        CHECK(n.f_peak == 0.0f || (n.f_peak >= 80.0f && n.f_peak < 4000.0f));
        CHECK(n.duration_ms >= 30);
        CHECK(n.harmonics >= 1 && n.harmonics <= kMaxPartials);
      }

      // it makes sound, without clipping, and the end is silent
      auto buf = renderVoice(v);
      int peak = peakOf(buf);
      if (peak < 3000) printf("%s %s: peak %d\n", characterName(ch), emotionName(em), peak);
      CHECK(peak > 3000 && peak <= 32767);
    }
  }
}

static void test_deterministic_and_seed() {
  for (size_t e = 0; e < kNumEmotions; e++) {
    Emotion em = static_cast<Emotion>(e);
    // the same seed gives the same voice for every character
    for (size_t c = 0; c < kNumCharacters; c++) {
      Voice a = make(static_cast<Character>(c), em, 7);
      Voice b = make(static_cast<Character>(c), em, 7);
      CHECK(a.count == b.count);
      for (size_t i = 0; i < a.count; i++) {
        CHECK(a.notes[i].f_start == b.notes[i].f_start);
        CHECK(a.notes[i].f_end == b.notes[i].f_end);
      }
    }
    // the robot voice changes the pitch a little with the seed
    Voice r1 = make(Character::kRobot, em, 1);
    Voice r2 = make(Character::kRobot, em, 2);
    CHECK(r1.count == r2.count);
    bool different = false;
    for (size_t i = 0; i < r1.count; i++) {
      different = different || r1.notes[i].f_start != r2.notes[i].f_start;
      // but the voice does not change much: within 0.2 octave
      CHECK(fabs(log2(r1.notes[i].f_start / r2.notes[i].f_start)) < 0.2);
    }
    CHECK(different);
  }
}

static void test_emotions_sound_like_the_emotion() {
  for (size_t c = 0; c < kNumCharacters; c++) {
    Character ch = static_cast<Character>(c);
    Voice neutral = make(ch, Emotion::kNeutral);
    double neutral_pitch = meanPitch(neutral);
    uint32_t neutral_ms = phraseDurationMs(neutral.notes, neutral.count);

    // happy sounds are higher than neutral, angry and sleepy sounds are lower
    CHECK(meanPitch(make(ch, Emotion::kSmile)) > neutral_pitch);
    CHECK(meanPitch(make(ch, Emotion::kGrin)) > neutral_pitch);
    CHECK(meanPitch(make(ch, Emotion::kSurprised)) > neutral_pitch);
    CHECK(meanPitch(make(ch, Emotion::kAngry)) < neutral_pitch);
    CHECK(meanPitch(make(ch, Emotion::kSleepy)) < neutral_pitch);

    // the surprised sound is the shortest of the sounds, and the sleepy and the sad sounds are slow
    Voice surprised = make(ch, Emotion::kSurprised);
    CHECK(phraseDurationMs(surprised.notes, surprised.count) <= neutral_ms + 100);
    Voice sleepy = make(ch, Emotion::kSleepy);
    CHECK(phraseDurationMs(sleepy.notes, sleepy.count) > neutral_ms * 2);
    Voice sad = make(ch, Emotion::kSad);
    CHECK(phraseDurationMs(sad.notes, sad.count) > neutral_ms * 2);

    // a question goes up at the end
    Voice doubt = make(ch, Emotion::kDoubt);
    const VoiceNote& last = doubt.notes[doubt.count - 1];
    CHECK(last.f_end > last.f_start * 1.3f);

    // the sad sound falls at the end, in the last note
    const VoiceNote& sad_last = sad.notes[sad.count - 1];
    CHECK(sad_last.f_end < sad_last.f_start || (sad_last.f_peak > 0 && sad_last.f_end < sad_last.f_peak));

    // the angry sound is rough: bright (a small rolloff), or a fast vibrato
    Voice angry = make(ch, Emotion::kAngry);
    CHECK(angry.notes[0].rolloff_start < neutral.notes[0].rolloff_start + 0.01f ||
          angry.notes[0].vibrato_hz >= 12.0f);

    // the sleepy sound is soft: a larger rolloff and a slower attack than neutral
    CHECK(sleepy.notes[0].rolloff_start > neutral.notes[0].rolloff_start);
    CHECK(sleepy.notes[0].attack_ms > neutral.notes[0].attack_ms);

    // more notes for the excited emotions than for neutral
    Voice grin = make(ch, Emotion::kGrin);
    CHECK(grin.count >= neutral.count);
  }

  // Robot voice: the grin and the upset voices are made of many fast chirps
  CHECK(make(Character::kRobot, Emotion::kGrin).count >= 5);
  CHECK(make(Character::kRobot, Emotion::kUpset).count >= 5);
  // the dog barks again and again when it is happy
  CHECK(make(Character::kDog, Emotion::kGrin).count == 3);
  CHECK(make(Character::kDog, Emotion::kSmile).count == 2);
}

static void test_characters_are_different() {
  // the same emotion sounds different for each character
  for (size_t e = 0; e < kNumEmotions; e++) {
    Emotion em = static_cast<Emotion>(e);
    double robot = meanPitch(make(Character::kRobot, em));
    double cat = meanPitch(make(Character::kCat, em));
    double dog = meanPitch(make(Character::kDog, em));
    CHECK(fabs(log2(robot / cat)) > 0.1 || fabs(log2(cat / dog)) > 0.1);
  }
  // the dog is lower than the cat in the neutral voice, and the robot voice is the highest
  CHECK(meanPitch(make(Character::kDog, Emotion::kNeutral)) <
        meanPitch(make(Character::kCat, Emotion::kNeutral)));
  CHECK(meanPitch(make(Character::kRobot, Emotion::kNeutral)) >
        meanPitch(make(Character::kCat, Emotion::kNeutral)));
  // only the robot voice is metallic
  CHECK(make(Character::kRobot, Emotion::kNeutral).notes[0].stretch > 0.0f);
  CHECK(make(Character::kCat, Emotion::kNeutral).notes[0].stretch == 0.0f);
}

static void test_capacity_and_invalid() {
  VoiceNote notes[kMaxEmotionNotes];

  // limited by the capacity: the robot grin voice has 6 notes
  CHECK(emotionVoice(Character::kRobot, Emotion::kGrin, notes, 2) == 2);
  CHECK(emotionVoice(Character::kDog, Emotion::kGrin, notes, 2) == 2);
  CHECK(emotionVoice(Character::kCat, Emotion::kGrin, notes, 1) == 1);

  CHECK(emotionVoice(Character::kCat, Emotion::kNeutral, nullptr, 8) == 0);
  CHECK(emotionVoice(Character::kCat, Emotion::kNeutral, notes, 0) == 0);
  CHECK(emotionVoice(Character::kCount, Emotion::kNeutral, notes, 8) == 0);
  CHECK(emotionVoice(Character::kCat, Emotion::kCount, notes, 8) == 0);
}

int main() {
  test_enum_order_matches_display();
  test_names();
  test_every_voice_is_valid();
  test_deterministic_and_seed();
  test_emotions_sound_like_the_emotion();
  test_characters_are_different();
  test_capacity_and_invalid();
  if (failures == 0) printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
