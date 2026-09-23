// Test-only replay of Otto's 2026-09-12 Bamboo CCW transient.
// No production header includes this file and this gate changes no firmware.
#include <algorithm>
#include <cstdio>
#include <vector>
#include "../NAVIFieldConfig.h"
#include "fixtures_bamboo_20260912.h"

static int checks = 0, failures = 0;
static void ck(bool c, const char* what) {
  ++checks;
  if (!c) { ++failures; std::printf("  FAIL  %s\n", what); }
}

static std::vector<short> medianWindow(const short* src, unsigned n,
                                       unsigned width) {
  std::vector<short> out(src, src + n), window;
  const unsigned half = width / 2;
  for (unsigned i = half; i + half < n; ++i) {
    window.assign(src + i - half, src + i + half + 1);
    std::sort(window.begin(), window.end());
    out[i] = window[half];
  }
  return out;
}

static short peak(const std::vector<short>& v) {
  return *std::max_element(v.begin(), v.end());
}

static unsigned longestEntryRun(const short* src, unsigned n, short entry) {
  unsigned run = 0, longest = 0;
  for (unsigned i = 0; i < n; ++i) {
    run = src[i] >= entry ? run + 1 : 0;
    longest = std::max(longest, run);
  }
  return longest;
}

int main() {
  std::printf("NAVI_ONE test-only gate -- 2026-09-12 Bamboo transient\n");
  const auto m3 = medianWindow(BAMBOO_20260912_TRANSIENT,
                               BAMBOO_20260912_TRANSIENT_N, 3);
  const auto m5 = medianWindow(BAMBOO_20260912_TRANSIENT,
                               BAMBOO_20260912_TRANSIENT_N, 5);
  const auto m7 = medianWindow(BAMBOO_20260912_TRANSIENT,
                               BAMBOO_20260912_TRANSIENT_N, 7);
  const unsigned entryRun = longestEntryRun(BAMBOO_20260912_TRANSIENT,
                                             BAMBOO_20260912_TRANSIENT_N,
                                             BAMBOO_20260912_ENTRY_COUNTS);

  std::printf("  raw/m3/m5/m7 peaks: %d / %d / %d / %d\n",
              (int)*std::max_element(BAMBOO_20260912_TRANSIENT,
                                     BAMBOO_20260912_TRANSIENT +
                                       BAMBOO_20260912_TRANSIENT_N),
              (int)peak(m3), (int)peak(m5), (int)peak(m7));
  std::printf("  duration: %u ms; longest raw entry run: %u ms\n",
              BAMBOO_20260912_DURATION_MS, entryRun);

  ck(peak(m3) == 86, "exact replay reproduces the field judgement peak");
  ck(peak(m5) == 78, "median-of-five still leaves an above-entry peak");
  ck(peak(m5) >= (short)BAMBOO_20260912_ENTRY_COUNTS,
     "median-of-five does not remove this transient");
  ck(peak(m7) < (short)BAMBOO_20260912_ENTRY_COUNTS,
     "median-of-seven removes it, but is not an approved production rule");
  ck(entryRun == 3, "the transient has exactly three consecutive entry samples");
  ck(BAMBOO_20260912_DURATION_MS < NAVI_PASSAGE_FLOOR_MS,
     "the active completed-passage floor rejects it");
  ck(entryRun < 4, "a test-only four-sample entry persistence rule rejects it");

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
