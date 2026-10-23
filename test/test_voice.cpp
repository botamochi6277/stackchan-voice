// Host-side test of the voice synthesis (VoiceNote, phrases and presets). No hardware is needed.
// Build and run (from stackchan-voice/):
//   g++ -std=c++17 -Wall -Isrc test/test_voice.cpp -o test_voice && ./test_voice
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "VoicePresets.h"
#include "VoiceSynth.h"

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

static std::vector<int16_t> render(const VoiceNote& note, float amplitude = 0.5f) {
  std::vector<int16_t> buf(sampleCount(note.duration_ms, kRate));
  renderVoiceNote(buf.data(), buf.size(), note, kRate, amplitude);
  return buf;
}

static int peakOf(const std::vector<int16_t>& buf) {
  int peak = 0;
  for (auto s : buf) peak = abs(s) > peak ? abs(s) : peak;
  return peak;
}

// frequency of a part of the samples, from the upward zero crossings. Use it for single sine waves.
static double frequencyOf(const std::vector<int16_t>& buf, size_t begin, size_t end) {
  int crossings = 0;
  for (size_t i = begin + 1; i < end; i++) {
    if (buf[i - 1] < 0 && buf[i] >= 0) crossings++;
  }
  return crossings * static_cast<double>(kRate) / (end - begin);
}

// strength of one frequency in a part of the samples (Goertzel algorithm), as an amplitude of a sine wave
static double strengthOf(const std::vector<int16_t>& buf, size_t begin, size_t end, double frequency) {
  double w = 2.0 * M_PI * frequency / kRate;
  double coeff = 2.0 * cos(w);
  double s1 = 0.0, s2 = 0.0;
  for (size_t i = begin; i < end; i++) {
    double s0 = buf[i] + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
  return 2.0 * sqrt(power) / (end - begin);
}

static VoiceNote sineNote(float f0) {
  VoiceNote n;
  n.f_start = f0;
  n.f_end = f0;
  n.harmonics = 1;
  n.duration_ms = 1000;
  n.attack_ms = 10;
  n.release_ms = 10;
  return n;
}

static void test_fixed_pitch_and_harmonics() {
  VoiceNote n = sineNote(440.0f);
  n.harmonics = 4;
  n.rolloff_start = n.rolloff_end = 1.0f;
  auto buf = render(n, 1.0f);

  // harmonics at the exact multiples, with the amplitude 1/k (normalized by the total 1 + 1/2 + 1/3 + 1/4)
  double total = 1.0 + 0.5 + 1.0 / 3 + 0.25;
  size_t b = 2000, e = 14000;
  for (int k = 1; k <= 4; k++) {
    double a = strengthOf(buf, b, e, 440.0 * k);
    double expected = 32767.0 / k / total;
    printf("harmonic %d: %.0f (expected %.0f)\n", k, a, expected);
    CHECK(fabs(a - expected) < expected * 0.05);
  }
  CHECK(strengthOf(buf, b, e, 550.0) < 300);  // not a harmonic
  CHECK(peakOf(buf) <= 32767);
}

static void test_pitch_glide() {
  VoiceNote n = sineNote(500.0f);
  n.f_end = 1000.0f;  // one octave up
  auto buf = render(n);
  size_t len = buf.size();

  double start = frequencyOf(buf, len / 20, len / 10);          // 5% .. 10%
  double middle = frequencyOf(buf, len * 45 / 100, len * 55 / 100);
  double end = frequencyOf(buf, len * 90 / 100, len * 95 / 100);
  printf("glide 500 -> 1000 Hz: %.0f %.0f %.0f\n", start, middle, end);
  CHECK(start > 500 && start < 580);
  // straight line in musical pitch: the middle is the geometric mean sqrt(500 * 1000) = 707
  CHECK(middle > 670 && middle < 745);
  CHECK(end > 920 && end < 1000);
}

static void test_pitch_peak() {
  VoiceNote n = sineNote(400.0f);
  n.f_peak = 800.0f;
  n.peak_position = 0.25f;
  n.f_end = 400.0f;
  auto buf = render(n);
  size_t len = buf.size();

  double start = frequencyOf(buf, 0, len / 20);
  double at_peak = frequencyOf(buf, len * 24 / 100, len * 26 / 100);
  double late = frequencyOf(buf, len * 95 / 100, len);
  printf("peak 400 -> 800 -> 400 Hz: %.0f %.0f %.0f\n", start, at_peak, late);
  CHECK(start < 480);
  CHECK(at_peak > 740 && at_peak < 860);
  CHECK(late < 470);
}

static void test_vibrato() {
  VoiceNote plain = sineNote(600.0f);
  VoiceNote vib = sineNote(600.0f);
  vib.vibrato_hz = 6.0f;
  vib.vibrato_semitones = 2.0f;  // +-2 semitones: 534 .. 673 Hz
  auto a = render(plain);
  auto b = render(vib);

  // the frequency of short windows: constant without vibrato, swinging with it
  auto spread = [](const std::vector<int16_t>& buf) {
    double lo = 1e9, hi = 0;
    size_t win = 800;  // 50 ms
    for (size_t i = 1600; i + win < buf.size() - 1600; i += win) {
      double f = frequencyOf(buf, i, i + win);
      lo = f < lo ? f : lo;
      hi = f > hi ? f : hi;
    }
    return hi - lo;
  };
  double plain_spread = spread(a), vib_spread = spread(b);
  printf("frequency spread: plain %.0f Hz, vibrato %.0f Hz\n", plain_spread, vib_spread);
  CHECK(plain_spread < 50);
  CHECK(vib_spread > 80);
}

static void test_timbre_glide() {
  // bright at the start (small rolloff), dark at the end (large rolloff): the 4th harmonic fades
  VoiceNote n = sineNote(300.0f);
  n.harmonics = 6;
  n.rolloff_start = 0.0f;
  n.rolloff_end = 3.0f;
  n.attack_ms = 0;
  n.release_ms = 0;
  auto buf = render(n, 1.0f);
  size_t len = buf.size();

  double h1_start = strengthOf(buf, 0, len / 5, 300.0);
  double h4_start = strengthOf(buf, 0, len / 5, 1200.0);
  double h1_end = strengthOf(buf, len * 4 / 5, len, 300.0);
  double h4_end = strengthOf(buf, len * 4 / 5, len, 1200.0);
  printf("4th / 1st harmonic: start %.2f, end %.2f\n", h4_start / h1_start, h4_end / h1_end);
  CHECK(h4_start / h1_start > 0.6);
  // the weights are mixed linearly from the start timbre to the end timbre, so the last 20 % is not fully dark yet
  CHECK(h4_end / h1_end < 0.2);
  CHECK(h4_end / h1_end < h4_start / h1_start / 3);
}

static void test_stretch() {
  VoiceNote n = sineNote(500.0f);
  n.harmonics = 2;
  n.stretch = 0.1f;  // the 2nd partial is at 500 * 2^1.1 = 1072 Hz, not 1000 Hz
  auto buf = render(n);
  size_t b = 2000, e = 14000;
  double exact = strengthOf(buf, b, e, 1000.0);
  double stretched = strengthOf(buf, b, e, 500.0 * pow(2.0, 1.1));
  printf("stretch: at 1000 Hz %.0f, at 1072 Hz %.0f\n", exact, stretched);
  CHECK(stretched > 5000);
  CHECK(exact < 1500);
}

static void test_envelope_and_amplitude() {
  VoiceNote n = sineNote(1000.0f);
  n.attack_ms = 50;
  n.release_ms = 100;
  auto buf = render(n, 1.0f);
  size_t len = buf.size();

  CHECK(buf.front() == 0);
  CHECK(abs(buf.back()) < 300);
  // quiet in the first 5 ms, full in the middle, quiet again in the last 5 ms
  int head = 0, mid = 0, tail = 0;
  for (size_t i = 0; i < 80; i++) head = abs(buf[i]) > head ? abs(buf[i]) : head;
  for (size_t i = len / 2; i < len / 2 + 32; i++) mid = abs(buf[i]) > mid ? abs(buf[i]) : mid;
  for (size_t i = len - 80; i < len; i++) tail = abs(buf[i]) > tail ? abs(buf[i]) : tail;
  CHECK(head < mid / 4);
  CHECK(tail < mid / 4);
  CHECK(mid > 30000);

  // the amplitude of the whole sound and of the note
  CHECK(peakOf(render(n, 0.5f)) <= 16384);
  n.amplitude = 0.5f;
  CHECK(peakOf(render(n, 0.5f)) <= 8192);
  CHECK(peakOf(render(n, 5.0f)) <= 16384);  // clamped to 1.0 x 0.5

  // attack and release longer than the sound are shortened without breaking it
  VoiceNote s = sineNote(1000.0f);
  s.duration_ms = 20;
  s.attack_ms = 500;
  s.release_ms = 500;
  auto tiny = render(s, 1.0f);
  CHECK(peakOf(tiny) <= 32767);
  CHECK(peakOf(tiny) > 1000);
}

static void test_nyquist_and_invalid() {
  // harmonics above the Nyquist frequency (8000 Hz) are left out, so the sound is the same as with fewer harmonics
  VoiceNote a = sineNote(3000.0f);
  a.harmonics = 2;
  VoiceNote b = a;
  b.harmonics = 8;  // 6000 Hz is allowed, but 9000 Hz and above are not
  auto buf_b = render(b);
  CHECK(peakOf(buf_b) <= 32767);
  CHECK(peakOf(buf_b) > 5000);

  // a pitch above the Nyquist frequency, and invalid pitches: silent
  VoiceNote hi = sineNote(9000.0f);
  CHECK(peakOf(render(hi)) == 0);
  VoiceNote zero = sineNote(0.0f);
  CHECK(peakOf(render(zero)) == 0);
  VoiceNote neg = sineNote(440.0f);
  neg.f_end = -1.0f;
  CHECK(peakOf(render(neg)) == 0);

  // vibrato that would reach the Nyquist frequency drops the harmonics instead of aliasing
  VoiceNote vib = sineNote(3500.0f);
  vib.harmonics = 4;
  vib.vibrato_hz = 20.0f;
  vib.vibrato_semitones = 6.0f;
  CHECK(peakOf(render(vib)) <= 32767);

  // null and empty
  renderVoiceNote(nullptr, 10, a, kRate);
  int16_t buf[4] = {1, 2, 3, 4};
  renderVoiceNote(buf, 0, a, kRate);
  renderVoiceNote(buf, 4, a, 0);
  CHECK(buf[0] == 1 && buf[3] == 4);
}

static void test_phrase() {
  VoiceNote notes[2];
  notes[0] = sineNote(500.0f);
  notes[0].duration_ms = 100;
  notes[0].gap_ms = 50;
  notes[1] = sineNote(800.0f);
  notes[1].duration_ms = 200;
  notes[1].gap_ms = 30;

  CHECK(phraseDurationMs(notes, 2) == 380);
  CHECK(phraseDurationMs(nullptr, 2) == 0);

  std::vector<int16_t> buf(sampleCount(380, kRate), 12345);
  size_t n = renderPhrase(buf.data(), buf.size(), notes, 2, kRate, 1.0f);
  CHECK(n == buf.size());

  // note 1, silence, note 2, silence
  size_t n1 = sampleCount(100, kRate), g1 = sampleCount(50, kRate), n2 = sampleCount(200, kRate);
  auto peak = [&](size_t b, size_t e) {
    int p = 0;
    for (size_t i = b; i < e; i++) p = abs(buf[i]) > p ? abs(buf[i]) : p;
    return p;
  };
  CHECK(peak(0, n1) > 20000);
  CHECK(peak(n1, n1 + g1) == 0);  // the gap is silent (and the old data is overwritten)
  CHECK(peak(n1 + g1, n1 + g1 + n2) > 20000);
  CHECK(peak(n1 + g1 + n2, n) == 0);

  // only the notes that fit are written
  std::vector<int16_t> small(sampleCount(200, kRate), 777);
  size_t m = renderPhrase(small.data(), small.size(), notes, 2, kRate, 1.0f);
  CHECK(m == n1 + g1);  // the 2nd note (200 ms) does not fit after the first 150 ms

  CHECK(renderPhrase(nullptr, 10, notes, 2, kRate) == 0);
  CHECK(renderPhrase(small.data(), small.size(), nullptr, 2, kRate) == 0);
  CHECK(renderPhrase(small.data(), small.size(), notes, 0, kRate) == 0);
}

static void test_robot() {
  VoiceNote a[presets::kMaxRobotNotes], b[presets::kMaxRobotNotes], c[presets::kMaxRobotNotes];
  size_t na = presets::robot(a, presets::kMaxRobotNotes, 1);
  size_t nb = presets::robot(b, presets::kMaxRobotNotes, 1);
  size_t nc = presets::robot(c, presets::kMaxRobotNotes, 2);
  CHECK(na >= 6 && na <= 8);

  // the same seed gives the same phrase, another seed gives another phrase
  CHECK(na == nb);
  bool same = true, other = false;
  for (size_t i = 0; i < na; i++) {
    same = same && a[i].f_start == b[i].f_start && a[i].duration_ms == b[i].duration_ms;
    if (i < nc && (a[i].f_start != c[i].f_start || a[i].duration_ms != c[i].duration_ms)) other = true;
  }
  CHECK(same);
  CHECK(other || na != nc);

  // for many seeds: 6-8 notes, short enough to be played at once, in the pitch range, and no clipping
  for (uint32_t seed = 1; seed <= 200; seed++) {
    VoiceNote notes[presets::kMaxRobotNotes];
    size_t n = presets::robot(notes, presets::kMaxRobotNotes, seed);
    CHECK(n >= 6 && n <= 8);
    uint32_t total = phraseDurationMs(notes, n);
    CHECK(total > 300 && total <= 2000);
    for (size_t i = 0; i < n; i++) {
      CHECK(notes[i].f_start >= 590 && notes[i].f_start <= 3300);
      CHECK(notes[i].f_end >= 590 && notes[i].f_end <= 3300);
      CHECK(notes[i].duration_ms >= 35);
    }
    if (seed % 40 == 1) {
      std::vector<int16_t> buf(sampleCount(total, kRate));
      size_t written = renderPhrase(buf.data(), buf.size(), notes, n, kRate, 1.0f);
      CHECK(written == buf.size());
      CHECK(peakOf(buf) <= 32767);
      CHECK(peakOf(buf) > 5000);
    }
  }

  // capacity and null
  VoiceNote few[3];
  CHECK(presets::robot(few, 3, 1) == 3);
  CHECK(presets::robot(nullptr, 3, 1) == 0);
  CHECK(presets::robot(few, 0, 1) == 0);
}

static void test_animals() {
  VoiceNote notes[2];

  size_t n = presets::catMeow(notes, 2);
  CHECK(n == 1);
  CHECK(notes[0].f_peak > notes[0].f_start && notes[0].f_peak > notes[0].f_end);
  CHECK(notes[0].duration_ms <= 2000);
  auto meow = render(notes[0], 1.0f);
  CHECK(peakOf(meow) > 5000 && peakOf(meow) <= 32767);
  // the pitch rises and then falls, so the peak is near 820 Hz at 30 %
  // (the sound has 12 harmonics, so check the fundamental with the Goertzel algorithm)
  size_t len = meow.size();
  double at_peak = strengthOf(meow, len * 28 / 100, len * 32 / 100, 820.0);
  double at_start = strengthOf(meow, len * 28 / 100, len * 32 / 100, 420.0);
  CHECK(at_peak > at_start);

  n = presets::puppyWhine(notes, 2);
  CHECK(n == 1);
  CHECK(notes[0].f_peak > notes[0].f_start && notes[0].f_peak > notes[0].f_end);
  CHECK(notes[0].duration_ms <= 2000);
  auto whine = render(notes[0], 1.0f);
  CHECK(peakOf(whine) > 5000 && peakOf(whine) <= 32767);
  CHECK(notes[0].vibrato_semitones > 0.0f);

  CHECK(presets::catMeow(notes, 0) == 0);
  CHECK(presets::puppyWhine(nullptr, 2) == 0);
}

int main() {
  test_fixed_pitch_and_harmonics();
  test_pitch_glide();
  test_pitch_peak();
  test_vibrato();
  test_timbre_glide();
  test_stretch();
  test_envelope_and_amplitude();
  test_nyquist_and_invalid();
  test_phrase();
  test_robot();
  test_animals();
  if (failures == 0) printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
