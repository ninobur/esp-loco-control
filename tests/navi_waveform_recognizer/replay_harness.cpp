// replay_harness.cpp — feed recorded 1 ms tick medians through the portable
// recognizer core and print the recognized events as CSV.
//
// usage: replay_harness <ticks.bin>
//   ticks.bin: records of (uint64 t_us, int16 median), little-endian, packed
//   (as written by make_tick_medians.py). Output CSV on stdout:
//   passage_us,recognized_us,score_r,amplitude_adc,polarity
// No Arduino dependencies; includes the sketch's core headers directly.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "NaviMatchedFilter.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <ticks.bin>\n", argv[0]);
    return 2;
  }
  FILE* f = std::fopen(argv[1], "rb");
  if (!f) {
    std::fprintf(stderr, "cannot open %s\n", argv[1]);
    return 2;
  }
  NaviMatchedFilter core;
  uint64_t tUs;
  int16_t median;
  uint64_t n = 0;
  std::printf("passage_us,recognized_us,score_r,amplitude_adc,polarity\n");
  while (std::fread(&tUs, sizeof(tUs), 1, f) == 1 &&
         std::fread(&median, sizeof(median), 1, f) == 1) {
    ++n;
    core.pushTickMedian(median, tUs);
    while (core.hasEvent()) {
      NaviRecognizedEvent e = core.nextEvent();
      std::printf("%llu,%llu,%.4f,%.1f,%+d\n",
                  (unsigned long long)e.tPassageUs,
                  (unsigned long long)e.tRecognizedUs, (double)e.r,
                  (double)e.amplitude, (int)e.polarity);
    }
  }
  // Drain any events declared after the last tick.
  while (core.hasEvent()) {
    NaviRecognizedEvent e = core.nextEvent();
    std::printf("%llu,%llu,%.4f,%.1f,%+d\n",
                (unsigned long long)e.tPassageUs,
                (unsigned long long)e.tRecognizedUs, (double)e.r,
                (double)e.amplitude, (int)e.polarity);
  }
  std::fclose(f);
  std::fprintf(stderr, "ticks fed: %llu\n", (unsigned long long)n);
  return 0;
}
