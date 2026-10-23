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

int main() {
  test_sample_count();
  test_frequency();
  test_amplitude();
  test_fade();
  test_invalid_arguments();
  if (failures == 0) printf("all tests passed\n");
  return failures == 0 ? 0 : 1;
}
