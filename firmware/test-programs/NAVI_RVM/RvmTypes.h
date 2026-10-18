#pragma once
#include <cstdint>
namespace rvm {
struct Cell { int16_t mean4; uint16_t scale4; };
struct Model {
 const Cell* cells; const uint16_t* peaks; const uint16_t* seams;
 const int16_t* baselines4; const uint16_t* speeds;
 int size, lapMm, gridMm, magnets; uint32_t version;
};
struct Block {
 uint32_t firstSerial=0,lastSerial=0; uint64_t firstUs=0,lastUs=0;
 float hall=0; uint8_t pwm=0; int8_t direction=1;
 bool gap=false,irValid=false; float irSpeed=0;
};
struct Token {
 uint64_t boot=0,firstUs=0,lastUs=0,commitUs=0;
 uint32_t run=0,ordinal=0,firstSerial=0,lastSerial=0,model=0;
 int expected=0,committed=0,trusted=0; uint8_t pwm=0,mismatches=0;
 bool irValid=false,atypical=false; float evidence=0,quality=0,phase=0;
 bool finalQuality=false, stopDecision=false;
};
}
