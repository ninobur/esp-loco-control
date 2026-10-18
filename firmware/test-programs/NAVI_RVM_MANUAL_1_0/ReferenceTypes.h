#pragma once
#include <cstdint>
namespace manual_rvm {
struct Cell { int16_t mean4; uint16_t scale4; };
struct Model {
  const Cell* cells;
  const uint16_t* peaks;
  const uint16_t* seams;
  const int16_t* baselines4;
  const uint16_t* speeds;
  int size, lapMm, gridMm, magnets;
  uint32_t version;
};
}
