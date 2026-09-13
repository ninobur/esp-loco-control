// Otto 9950011, 2026-09-13 11:20:59-11:21:05 PDT, CCW at cruise PWM 90,
// between Northpoint (MM098) and MM086 -- the six passages published on
// diag/waveform during the acquisition-latch incident that stopped him.
//
// GENERATED from the live MQTT capture of ngr/loco/9950011/# -- do not edit.
// Verbatim as the firmware published them: ORIENTED (negated at close when the
// pole read S) and ENTRY-BASELINE-RELATIVE. `decimation` says how many
// milliseconds separate one stored sample from the next; the readings in
// between were never transmitted and cannot be recovered (decision 0070).
//
// NOT PRESENT: the 1569 ms passage accepted as MM092 at 11:20:59. It was
// ACCEPTED, and this build dumps a waveform only for a refusal or a strike.
// The two latched records below are from the same episode and carry the same
// failure.
//
// entryBaseline is the frozen reference the samples were measured against,
// read from the base_open field of the matching mm/marker record.
#pragma once
#include <stdint.h>

struct NorthpointRecord {
  const char* tag; const char* what;
  uint8_t  polarity;        // 1 = N, 0 = S (samples were negated at close)
  uint16_t decimation, peak, gain;
  float    ratio;
  uint32_t gapMs, openedAtMs, closedAtMs;
  int32_t  entryBaseline;
  bool     accepted;
  uint16_t n; const int16_t* v;
};

// FRAG_95 -- 95 ms fragment, refused TOO_SOON (gap 52 ms)
// published 2026-09-13T11:20:59-0700  pol=N peak=112 gain=175 ratio=0.640 dec=1 n=108 dur=95 ms
static const int16_t NP_FRAG_95[] = {
  48,46,51,52,52,54,60,58,60,62,62,66,72,73,74,78,
  80,80,83,83,83,85,84,89,86,92,90,90,86,96,98,98,
  98,99,97,102,100,99,100,101,102,103,104,103,101,102,101,102,
  102,102,101,99,112,113,109,109,108,104,104,105,100,100,97,98,
  96,96,94,92,90,88,86,84,83,83,82,78,76,76,73,73,
  65,63,58,58,54,52,51,51,51,46,46,42,39,37,34,33,
  32,30,28,23,20,20,19,18,17,14,13,10,
};
static const NorthpointRecord NP_REC_FRAG_95 = { "FRAG_95", "95 ms fragment, refused TOO_SOON (gap 52 ms)", 1, 1, 112, 175, 0.6400f, 52u, 3848555u, 3848650u, 1981, false, 108, NP_FRAG_95 };

// LATCH_1200 -- 1200 ms latched passage, refused TOO_SOON (gap 215 ms)
// published 2026-09-13T11:21:01-0700  pol=S peak=76 gain=175 ratio=0.434 dec=4 n=304 dur=1200 ms
static const int16_t NP_LATCH_1200[] = {
  62,62,66,70,67,71,71,72,73,75,75,72,74,75,76,76,
  75,75,77,76,75,74,63,66,66,66,66,64,65,64,65,64,
  66,66,65,65,64,64,66,63,65,64,64,64,77,75,75,72,
  61,61,62,61,61,62,62,62,61,62,62,61,62,62,62,61,
  62,61,61,61,61,61,74,61,61,62,61,61,61,61,61,73,
  73,71,70,68,71,69,71,70,70,71,68,71,70,68,70,68,
  71,69,68,59,59,60,59,61,61,61,60,61,61,61,61,61,
  61,60,61,61,61,61,61,61,59,73,68,72,70,67,59,59,
  60,61,61,60,60,60,60,61,60,61,61,61,61,61,60,61,
  61,60,61,70,60,60,60,63,60,60,61,62,60,60,60,61,
  61,60,59,61,61,61,61,61,61,61,61,61,72,73,68,69,
  72,58,60,61,61,59,60,59,61,60,61,59,61,60,61,61,
  61,61,62,61,61,60,70,70,72,60,61,60,61,61,61,61,
  61,61,61,61,61,61,61,61,61,61,61,62,62,62,61,74,
  72,72,74,70,61,60,61,61,61,61,61,61,61,63,62,62,
  62,62,61,62,61,61,61,62,62,75,61,61,62,62,63,62,
  62,62,62,62,62,63,61,64,63,62,62,62,62,63,61,61,
  61,62,62,72,60,61,61,61,60,60,57,60,60,59,56,55,
  52,51,50,49,46,47,45,43,42,38,35,31,42,24,18,13,
};
static const NorthpointRecord NP_REC_LATCH_1200 = { "LATCH_1200", "1200 ms latched passage, refused TOO_SOON (gap 215 ms)", 0, 4, 76, 175, 0.4343f, 215u, 3848718u, 3849918u, 1981, false, 304, NP_LATCH_1200 };

// MM91_96 -- 96 ms fragment, ACCEPTED as MM091
// published 2026-09-13T11:21:05-0700  pol=N peak=117 gain=175 ratio=0.669 dec=1 n=109 dur=96 ms
static const int16_t NP_MM91_96[] = {
  48,49,48,51,51,52,58,58,61,63,64,65,72,74,78,80,
  83,80,83,83,84,84,88,104,103,104,105,106,109,108,109,110,
  114,113,115,115,114,115,115,115,116,115,115,115,116,105,109,107,
  108,108,117,118,116,116,116,115,115,115,115,115,115,113,114,96,
  93,95,94,86,89,90,90,86,86,86,83,81,82,79,81,76,
  77,72,64,64,64,57,56,53,52,51,51,48,44,42,42,42,
  34,34,34,28,24,22,23,19,19,14,14,13,13,
};
static const NorthpointRecord NP_REC_MM91_96 = { "MM91_96", "96 ms fragment, ACCEPTED as MM091", 1, 1, 117, 175, 0.6686f, 1472u, 3849975u, 3850071u, 1981, true, 109, NP_MM91_96 };

// LATCH_2446 -- 2446 ms latched passage, refused TOO_SOON (gap 62 ms) -- ratio 1.477
// published 2026-09-13T11:21:03-0700  pol=S peak=257 gain=174 ratio=1.477 dec=8 n=308 dur=2446 ms
static const int16_t NP_LATCH_2446[] = {
  61,66,72,77,66,68,72,72,71,72,72,73,74,70,70,70,
  67,70,70,67,67,66,66,66,65,63,63,63,64,64,62,61,
  62,61,62,62,61,76,71,70,60,61,61,61,60,61,61,62,
  61,61,71,61,71,67,70,68,71,66,66,68,67,66,70,64,
  67,59,59,60,58,60,60,57,57,61,59,59,70,68,57,59,
  58,58,58,58,59,58,58,59,57,55,57,56,58,59,57,59,
  58,59,58,59,58,70,63,56,56,56,58,56,58,58,59,57,
  59,68,64,64,64,63,63,63,63,63,52,54,54,55,54,64,
  55,55,54,55,54,54,54,51,54,51,54,54,63,62,62,51,
  50,54,55,59,55,61,62,66,72,77,82,94,106,118,134,149,
  170,188,207,226,239,266,257,256,249,237,217,196,173,151,132,111,
  99,84,82,75,67,61,59,58,54,54,54,54,54,52,53,51,
  54,45,45,46,47,45,47,48,50,49,49,51,61,51,50,54,
  54,52,52,54,54,52,55,53,54,54,55,54,56,56,55,56,
  60,56,56,57,56,67,55,58,56,56,57,56,56,58,67,66,
  64,68,64,66,67,68,65,67,67,66,66,64,66,64,55,67,
  56,57,58,58,61,58,59,59,57,59,58,61,71,57,59,59,
  58,59,60,59,61,59,61,58,72,70,67,57,60,61,60,61,
  61,60,61,60,60,71,70,61,61,61,62,61,61,62,62,61,
  62,62,72,61,
};
static const NorthpointRecord NP_REC_LATCH_2446 = { "LATCH_2446", "2446 ms latched passage, refused TOO_SOON (gap 62 ms) -- ratio 1.477", 0, 8, 257, 174, 1.4770f, 62u, 3850133u, 3852579u, 1981, false, 308, NP_LATCH_2446 };

// MM90_160 -- 160 ms passage, ACCEPTED as MM090
// published 2026-09-13T11:21:05-0700  pol=N peak=199 gain=174 ratio=1.144 dec=1 n=173 dur=160 ms
static const int16_t NP_MM90_160[] = {
  49,51,54,56,57,59,61,62,64,66,68,68,73,74,76,77,
  79,79,82,83,87,90,92,93,97,99,103,106,108,109,111,115,
  116,120,123,122,130,135,138,139,141,142,143,138,139,141,143,147,
  162,164,167,168,171,173,173,173,176,176,180,183,185,186,188,189,
  189,189,190,192,195,195,196,198,196,198,199,199,199,199,199,198,
  196,199,196,195,197,194,195,194,194,191,192,189,189,186,188,184,
  183,179,178,176,174,173,173,169,168,162,162,159,158,155,151,148,
  144,143,141,141,137,132,131,124,122,118,114,112,109,106,104,103,
  99,95,92,88,88,83,82,78,78,75,73,70,67,63,64,61,
  58,45,44,42,41,43,45,45,45,42,40,36,35,35,33,30,
  28,28,28,26,23,20,19,19,16,14,14,13,13,
};
static const NorthpointRecord NP_REC_MM90_160 = { "MM90_160", "160 ms passage, ACCEPTED as MM090", 1, 1, 199, 174, 1.1437f, 2669u, 3852740u, 3852900u, 1923, true, 173, NP_MM90_160 };

// STRIKE_156 -- 156 ms passage, the WRONG_MAGNET strike
// published 2026-09-13T11:21:05-0700  pol=N peak=188 gain=174 ratio=1.080 dec=1 n=169 dur=156 ms
static const int16_t NP_STRIKE_156[] = {
  48,50,51,54,47,48,49,49,51,65,66,69,75,75,75,80,
  80,80,83,85,86,91,92,95,97,101,102,106,93,95,100,106,
  109,110,112,114,116,119,122,125,127,127,137,140,142,144,144,147,
  151,154,156,159,161,164,165,165,167,171,173,175,175,176,176,179,
  181,181,182,181,185,182,185,186,187,185,188,186,187,187,189,188,
  185,186,185,186,183,182,182,179,179,176,176,176,176,171,172,170,
  168,162,162,158,158,154,152,151,147,145,143,144,141,137,134,141,
  140,135,131,127,123,119,114,112,111,109,106,102,100,96,93,91,
  89,84,82,80,79,77,73,70,67,64,64,62,58,58,55,53,
  51,48,48,47,46,42,39,38,37,35,34,33,33,31,30,25,
  21,22,21,22,19,17,17,16,16,
};
static const NorthpointRecord NP_REC_STRIKE_156 = { "STRIKE_156", "156 ms passage, the WRONG_MAGNET strike", 1, 1, 188, 174, 1.0805f, 1107u, 3854007u, 3854163u, 1920, false, 169, NP_STRIKE_156 };

static const NorthpointRecord* const NORTHPOINT_20260913[] = {
  &NP_REC_FRAG_95,
  &NP_REC_LATCH_1200,
  &NP_REC_MM91_96,
  &NP_REC_LATCH_2446,
  &NP_REC_MM90_160,
  &NP_REC_STRIKE_156,
};
static const unsigned NORTHPOINT_20260913_N = 6;

// The accepted MM092 passage that has no waveform, for the record:
//   opened 3846934  closed 3848503  dur 1569 ms  peak 251  ratio 1.443  gap 788  base_open 1980
static const uint32_t NP_MM092_OPENED_MS = 3846934u, NP_MM092_CLOSED_MS = 3848503u;
static const uint16_t NP_MM092_DUR_MS = 1569, NP_MM092_PEAK = 251;

// Measured cruise cadence over the seven markers before the episode:
// close-to-close spans 1121,1130,1160,1190,1220,1288,1289 ms at PWM 90.
static const uint16_t NP_CRUISE_SPAN_MS[] = {1121,1130,1160,1190,1220,1288,1289};
static const unsigned NP_CRUISE_SPAN_N = 7;
