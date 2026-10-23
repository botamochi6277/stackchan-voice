// Host-side test of the sine wave generator. No hardware is needed.
// Build and run (from stackchan-voice/):
//   g++ -std=c++17 -Wall -Isrc test/test_sine.cpp -o test_sine && ./test_sine
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "SineWave.h"

using namespace stackchan::voice;

static int failures = 0;
#define CHECK(cond)                                          \
  do {                                                       \
    if (!(cond)) {                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                            \
    }                                                        \
  } while (0)

static int peakOf(const std::vector<int16_t>& buf) {
  int peak = 0;
  for (auto s : buf) peak = abs(s) > peak ? abs(s) : peak;
  return peak;
}

static void test_sample_count() {
  CHECK(sampleCount(1000, 16000) == 16000);
  CHECK(sampleCount(500, 16000) == 8000);
  CHECK(sampleCount(0, 16000) == 0);
  CHECK(sampleCount(2000, 48000) == 96000);
}

static void test_frequency() {
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate);  // 1 second
  generateSine(buf.data(), buf.size(), 440.0f, rate, 0.5f, 5);

  // count the upward zero crossings: 440 Hz gives about 440 in 1 second
  int crossings = 0;
  for (size_t i = 1; i < buf.size(); i++) {
    if (buf[i - 1] < 0 && buf[i] >= 0) crossings++;
  }
  printf("440 Hz: %d upward zero crossings in 1 s\n", crossings);
  CHECK(crossings >= 438 && crossings <= 441);
}

static void test_amplitude() {
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate);
  generateSine(buf.data(), buf.size(), 440.0f, rate, 0.5f, 5);
  int peak = peakOf(buf);
  printf("amplitude 0.5: peak %d\n", peak);
  CHECK(peak > 16000 && peak <= 16384);

  // amplitude is clamped to 0..1, so it never overflows
  generateSine(buf.data(), buf.size(), 440.0f, rate, 5.0f, 5);
  peak = peakOf(buf);
  CHECK(peak > 32000 && peak <= 32767);

  generateSine(buf.data(), buf.size(), 440.0f, rate, -1.0f, 5);
  CHECK(peakOf(buf) == 0);
}

static void test_fade() {
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate / 2);
  generateSine(buf.data(), buf.size(), 1000.0f, rate, 1.0f, 10);

  // the first and the last samples are silent, so there are no clicks
  CHECK(buf.front() == 0);
  CHECK(abs(buf.back()) < 200);

  // the beginning is quieter than the middle
  int head = 0, mid = 0;
  for (size_t i = 0; i < 16; i++) head = abs(buf[i]) > head ? abs(buf[i]) : head;
  for (size_t i = buf.size() / 2; i < buf.size() / 2 + 32; i++) mid = abs(buf[i]) > mid ? abs(buf[i]) : mid;
  CHECK(head < mid / 2);

  // fades longer than half of the sound are shortened, without crashing
  std::vector<int16_t> tiny(10);
  generateSine(tiny.data(), tiny.size(), 440.0f, rate, 1.0f, 1000);
}

static void test_invalid_arguments() {
  generateSine(nullptr, 10, 440.0f, 16000);  // must not crash
  int16_t buf[4] = {1, 2, 3, 4};
  generateSine(buf, 0, 440.0f, 16000);
  generateSine(buf, 4, 440.0f, 0);
  CHECK(buf[0] == 1 && buf[3] == 4);  // untouched
}

// strength of one frequency in the samples (Goertzel algorithm), as an amplitude of a sine wave
static double strengthOf(const std::vector<int16_t>& buf, double frequency, uint32_t rate) {
  double w = 2.0 * M_PI * frequency / rate;
  double coeff = 2.0 * cos(w);
  double s1 = 0.0, s2 = 0.0;
  for (auto x : buf) {
    double s0 = x + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
  return 2.0 * sqrt(power) / buf.size();
}

static void test_mix_contains_each_frequency() {
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate);  // 1 second, so the frequency resolution is 1 Hz
  Partial p[3];
  p[0].frequency = 261.63f;
  p[1].frequency = 329.63f;
  p[2].frequency = 392.0f;
  generateSineMix(buf.data(), buf.size(), p, 3, rate, 0.9f, 5);

  // each of the 3 frequencies has about the same strength, and the other frequencies are almost silent
  double a = strengthOf(buf, 261.63, rate);
  double b = strengthOf(buf, 329.63, rate);
  double c = strengthOf(buf, 392.0, rate);
  double other = strengthOf(buf, 500.0, rate);
  printf("chord: %.0f %.0f %.0f (other %.0f)\n", a, b, c, other);
  CHECK(a > 8000 && b > 8000 && c > 8000);
  CHECK(fabs(a - b) < 600 && fabs(b - c) < 600);
  CHECK(other < 300);
}

static void test_mix_amplitude_ratio() {
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate);
  Partial p[2];
  p[0].frequency = 440.0f;
  p[0].amplitude = 3.0f;
  p[1].frequency = 880.0f;
  p[1].amplitude = 1.0f;
  generateSineMix(buf.data(), buf.size(), p, 2, rate, 1.0f, 5);

  // only the ratio matters: 3 : 1, and the total is 1.0 of the full scale (0.75 + 0.25)
  double low = strengthOf(buf, 440.0, rate);
  double high = strengthOf(buf, 880.0, rate);
  printf("ratio: %.0f : %.0f\n", low, high);
  CHECK(fabs(low / high - 3.0) < 0.1);
  CHECK(low > 23000 && low < 25000);
}

static void test_mix_never_clips() {
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate);

  // many partials with large amplitudes: normalized, so the peak stays within the full scale
  Partial p[kMaxPartials];
  for (size_t k = 0; k < kMaxPartials; k++) {
    p[k].frequency = 200.0f * (k + 1);
    p[k].amplitude = 50.0f;
  }
  generateSineMix(buf.data(), buf.size(), p, kMaxPartials, rate, 1.0f, 5);
  int peak = peakOf(buf);
  printf("16 partials: peak %d\n", peak);
  CHECK(peak <= 32767);
  CHECK(peak > 8000);

  // the peak never exceeds the requested amplitude
  generateSineMix(buf.data(), buf.size(), p, kMaxPartials, rate, 0.25f, 5);
  CHECK(peakOf(buf) <= 8192);
}

static void test_mix_invalid_partials() {
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate / 2);

  // partials at or above the Nyquist frequency (8000 Hz), zero or negative, are skipped
  Partial p[4];
  p[0].frequency = 440.0f;
  p[1].frequency = 9000.0f;   // above Nyquist
  p[2].frequency = -100.0f;   // negative
  p[3].frequency = 600.0f;
  p[3].amplitude = 0.0f;      // silent
  generateSineMix(buf.data(), buf.size(), p, 4, rate, 1.0f, 5);

  std::vector<int16_t> ref(rate / 2);
  generateSine(ref.data(), ref.size(), 440.0f, rate, 1.0f, 5);
  CHECK(buf == ref);  // the same as the 440 Hz sine wave alone

  // no valid partial: silent, and no crash
  p[0].frequency = 20000.0f;
  generateSineMix(buf.data(), buf.size(), p, 4, rate, 1.0f, 5);
  CHECK(peakOf(buf) == 0);

  generateSineMix(buf.data(), buf.size(), nullptr, 3, rate, 1.0f, 5);
  CHECK(peakOf(buf) == 0);
  generateSineMix(buf.data(), buf.size(), p, 0, rate, 1.0f, 5);
  CHECK(peakOf(buf) == 0);

  CHECK(isValidPartial(Partial{440.0f, 1.0f}, rate));
  CHECK(!isValidPartial(Partial{8000.0f, 1.0f}, rate));
  CHECK(!isValidPartial(Partial{440.0f, 0.0f}, rate));
}

static void test_mix_more_than_max_partials() {
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate);
  Partial p[kMaxPartials + 4];
  for (size_t k = 0; k < kMaxPartials + 4; k++) {
    p[k].frequency = 200.0f * (k + 1) > 7900.0f ? 300.0f : 200.0f * (k + 1);
  }
  // must not overrun the internal arrays
  generateSineMix(buf.data(), buf.size(), p, kMaxPartials + 4, rate, 0.5f, 5);
  CHECK(peakOf(buf) <= 16384);
}

static void test_mix_fade() {
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate / 2);
  Partial p[2];
  p[0].frequency = 440.0f;
  p[1].frequency = 1320.0f;
  generateSineMix(buf.data(), buf.size(), p, 2, rate, 1.0f, 10);
  CHECK(buf.front() == 0);
  CHECK(abs(buf.back()) < 300);
}

static void test_harmonic_series() {
  Partial p[8];
  size_t n = harmonicSeries(220.0f, 5, 1.0f, p, 8);
  CHECK(n == 5);
  for (size_t k = 0; k < n; k++) {
    CHECK(fabs(p[k].frequency - 220.0f * (k + 1)) < 0.01f);
    CHECK(fabs(p[k].amplitude - 1.0f / (k + 1)) < 0.001f);
  }

  // rolloff 0: all harmonics have the same amplitude
  n = harmonicSeries(100.0f, 4, 0.0f, p, 8);
  for (size_t k = 0; k < n; k++) CHECK(fabs(p[k].amplitude - 1.0f) < 0.001f);

  // limited by the capacity, and by the null pointer
  CHECK(harmonicSeries(100.0f, 10, 1.0f, p, 3) == 3);
  CHECK(harmonicSeries(100.0f, 10, 1.0f, nullptr, 3) == 0);

  // the sound of the harmonic series has each harmonic, softer as it goes up
  const uint32_t rate = 16000;
  std::vector<int16_t> buf(rate);
  n = harmonicSeries(220.0f, 6, 1.0f, p, 8);
  generateSineMix(buf.data(), buf.size(), p, n, rate, 1.0f, 5);
  double h1 = strengthOf(buf, 220.0, rate);
  double h2 = strengthOf(buf, 440.0, rate);
  double h3 = strengthOf(buf, 660.0, rate);
  double between = strengthOf(buf, 330.0, rate);  // not a harmonic
  printf("harmonics: %.0f %.0f %.0f (between %.0f)\n", h1, h2, h3, between);
  CHECK(h1 > h2 && h2 > h3);
  CHECK(fabs(h1 / h2 - 2.0) < 0.1);
  CHECK(between < 300);
}

int main() {
  test_sample_count();
  test_frequency();
  test_amplitude();
  test_fade();
  test_invalid_arguments();
  test_mix_contains_each_frequency();
  test_mix_amplitude_ratio();
  test_mix_never_clips();
  test_mix_invalid_partials();
  test_mix_more_than_max_partials();
  test_mix_fade();
  test_harmonic_series();
  if (failures == 0) printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
