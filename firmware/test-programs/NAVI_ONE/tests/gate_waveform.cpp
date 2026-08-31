// GATE 5 — WaveformWindow and the wire format that publishes it.
//
// Built 2026-08-31 on the operator's ruling, after two field events (MM110,
// MM147/145) in which a WRONG_SHAPE rejection produced a silent lag with no
// waveform evidence to explain it. This gate exists so the new capability is
// checked before it ever sees the railway: does the window actually hold six
// passages, does it evict the right one, does it preserve every sample
// unresampled, and does the chunked wire format round-trip exactly, including
// the case a passage does not fit in one message.
#include <cstdio>
#include <cstring>
#include <vector>
#include "../WaveformWindow.h"
#include "../WaveformDump.h"
using namespace navi_one;

static int checks=0, failures=0;
static void ck(bool c,const char* what){ checks++; if(!c){ failures++; printf("  FAIL  %s\n",what);} }
static void ckEq(long g,long w,const char* what){ checks++;
  if(g!=w){ failures++; printf("  FAIL  %s (got %ld want %ld)\n",what,g,w);} }

static Passage makePassage(uint32_t opened, uint16_t peak, uint8_t polarity,
                            const std::vector<int16_t>& samples, uint16_t decimation=1){
  static std::vector<std::vector<int16_t>> storage; // keep the backing arrays alive
  storage.push_back(samples);
  Passage p;
  p.openedAtMs = opened; p.closedAtMs = opened + 100;
  p.peakCounts = peak; p.polarity = polarity;
  p.oriented = storage.back().data();
  p.sampleCount = (uint16_t)samples.size();
  p.decimation = decimation;
  return p;
}

static Verdict makeVerdict(float ratio,float resid,Outcome outcome,bool isMagnet){
  Verdict v; v.amplitudeRatio=ratio; v.residual=resid; v.outcome=outcome;
  v.isMagnet=isMagnet; v.shapeTested=true; v.gapMs=1000; v.gain=200;
  return v;
}

int main(){
  setvbuf(stdout,nullptr,_IONBF,0);
  printf("NAVI_ONE gate 5 -- WaveformWindow + wire format\n");

  printf("\nP1  the window holds exactly DEPTH passages and evicts the oldest\n");
  {
    WaveformWindow<6> w;
    ck(w.count()==0,"starts empty");
    for (int i = 0; i < 4; ++i) {
      Passage p = makePassage(1000+i, 200+i, 1, {1,2,3});
      Verdict v = makeVerdict(1.0f,0.05f,Outcome::Magnet,true);
      w.push(p, v);
    }
    ckEq(w.count(),4,"count after 4 pushes");
    ckEq(w.at(0).peakCounts,203,"index 0 is the most recently pushed");
    ckEq(w.at(3).peakCounts,200,"index 3 is the first ever pushed, still held");

    for (int i = 4; i < 9; ++i) {  // 9 total pushes, depth 6 -> first 3 evicted
      Passage p = makePassage(1000+i, 200+i, 1, {1,2,3});
      Verdict v = makeVerdict(1.0f,0.05f,Outcome::Magnet,true);
      w.push(p, v);
    }
    ckEq(w.count(),6,"count caps at DEPTH");
    ckEq(w.at(0).peakCounts,208,"newest after 9 pushes is #8 (peak 208)");
    ckEq(w.at(5).peakCounts,203,"oldest surviving is #3 (peak 203) -- #0,#1,#2 evicted");
  }

  printf("\nP2  samples are preserved EXACTLY -- no resampling, no truncation within RING\n");
  {
    WaveformWindow<6> w;
    std::vector<int16_t> wave = {0,-5,12,88,201,240,199,90,10,-3,0,-1};
    Passage p = makePassage(5000, 240, 1, wave);
    Verdict v = makeVerdict(1.1f,0.06f,Outcome::Magnet,true);
    w.push(p, v);
    const auto& e = w.at(0);
    ckEq(e.sampleCount,(long)wave.size(),"sampleCount matches the passage exactly");
    bool identical = true;
    for (size_t i = 0; i < wave.size(); ++i)
      if (e.samples[i] != wave[i]) identical = false;
    ck(identical,"every sample byte-for-byte identical, in order");
    ck(e.residual > 0.059f && e.residual < 0.061f,"residual carried through from the Verdict");
    ck(e.outcome == (uint8_t)Outcome::Magnet,"outcome carried through");
  }

  printf("\nP3  a passage that does NOT fit one message chunks, and no sample is dropped\n");
  {
    // Build a passage bigger than one message can hold unchunked.
    uint16_t perChunk = wavChunkCapacity(704);
    ck(perChunk > 0 && perChunk < 512,"perChunk is sane for a 704-byte payload");
    std::vector<int16_t> wave(400);
    for (size_t i = 0; i < wave.size(); ++i) wave[i] = (int16_t)(i % 300 - 10);
    WaveformWindow<6> w;
    Passage p = makePassage(9000, 250, 0, wave);
    Verdict v = makeVerdict(0.9f,0.07f,Outcome::Magnet,true);
    w.push(p, v);
    const auto& e = w.at(0);
    uint8_t chunks = wavChunkCount(e.sampleCount, perChunk);
    ck(chunks > 1,"a 400-sample passage needs more than one chunk at this payload size");

    // Encode every chunk, then reassemble and compare against the original.
    std::vector<int16_t> reassembled(e.sampleCount, 0);
    bool sizesOk = true;
    for (uint8_t c = 0; c < chunks; ++c) {
      uint16_t offset = (uint16_t)(c * perChunk);
      uint16_t remain = (uint16_t)(e.sampleCount - offset);
      uint16_t n = remain < perChunk ? remain : perChunk;
      uint8_t buf[704];
      uint16_t bytes = wavEncodeChunk(buf, sizeof(buf), 0, 1, c, chunks,
        e.polarity, e.outcome, e.isMagnet, e.shapeTested,
        e.sampleCount, e.decimation, e.peakCounts, e.gain,
        e.amplitudeRatio, e.residual, e.gapMs, e.openedAtMs, e.closedAtMs,
        e.samples, offset, n);
      if (bytes == 0) { sizesOk = false; continue; }
      WavHeader h;
      if (!wavDecodeHeader(buf, bytes, h)) { sizesOk = false; continue; }
      if (h.chunkSampleCount != n || h.chunkOffset != offset || h.sampleCount != e.sampleCount)
        sizesOk = false;
      const int16_t* samplesIn = (const int16_t*)(buf + sizeof(WavHeader));
      for (uint16_t i = 0; i < n; ++i) reassembled[offset + i] = samplesIn[i];
    }
    ck(sizesOk,"every chunk's header round-trips its offsets and counts");
    bool allMatch = true;
    for (size_t i = 0; i < wave.size(); ++i)
      if (reassembled[i] != wave[i]) allMatch = false;
    ck(allMatch,"reassembled samples match the original waveform exactly -- nothing dropped");
  }

  printf("\nP4  an empty passage (sampleCount 0) still yields exactly one chunk\n");
  {
    ckEq(wavChunkCount(0, wavChunkCapacity(704)),1,"zero samples -> 1 chunk, not 0");
  }

  printf("\n%d checks, %d failures\n",checks,failures);
  return failures ? 1 : 0;
}
