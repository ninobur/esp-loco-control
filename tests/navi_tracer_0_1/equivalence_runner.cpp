// Runs the ported C++ recognizer (NaviTracerCore.h) over the same fixture
// CSV format run_python_reference.py consumes, and writes its event stream
// in the same CSV format, for a direct diff against the Python reference.
//
// Usage: equivalence_runner FIXTURE.csv LAST_CONFIRMED_MM DIRECTION DURATION_S OUT.csv
//
// Build: c++ -std=c++17 -O2 -Wall -Wextra -Werror
//   -I ../../firmware/test-programs/NAVI_TRACER_0_1 equivalence_runner.cpp -o equivalence_runner
#include "NaviTracerCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using navi_tracer::TargetAcquisition;
using navi_tracer::Event;
using navi_tracer::EventKind;

static const char* kindName(EventKind k) {
  switch (k) {
    case EventKind::OPEN_CANDIDATE: return "OPEN_CANDIDATE";
    case EventKind::ACQUIRED: return "ACQUIRED";
    case EventKind::CONTRADICTION: return "CONTRADICTION";
    case EventKind::DISPOSED: return "DISPOSED";
    case EventKind::CLOSE_AND_ADVANCE: return "CLOSE_AND_ADVANCE";
  }
  return "UNKNOWN";
}

struct Row { double t; double hall; int pwm; };

static std::vector<Row> readFixture(const std::string& path) {
  std::ifstream f(path);
  if (!f) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); std::exit(2); }
  std::string line;
  std::getline(f, line);  // header
  std::vector<Row> rows;
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    std::stringstream ss(line);
    std::string tStr, hStr, pStr;
    std::getline(ss, tStr, ','); std::getline(ss, hStr, ','); std::getline(ss, pStr, ',');
    Row r; r.t = std::stod(tStr);
    r.hall = hStr.empty() ? NAN : std::stod(hStr);
    r.pwm = std::stoi(pStr);
    rows.push_back(r);
  }
  return rows;
}

int main(int argc, char** argv) {
  if (argc != 6) {
    std::fprintf(stderr, "usage: %s FIXTURE.csv LAST_CONFIRMED_MM DIRECTION DURATION_S OUT.csv\n", argv[0]);
    return 2;
  }
  const std::string fixturePath = argv[1];
  const int lastMm = std::atoi(argv[2]);
  const int direction = std::atoi(argv[3]);
  const double duration = std::atof(argv[4]);
  const std::string outPath = argv[5];

  TargetAcquisition ta(uint8_t(lastMm), int8_t(direction), duration);
  const auto rows = readFixture(fixturePath);

  std::ofstream out(outPath);
  out << "kind,t,target_mm,sign,matches_target,start_s,acquired_s,duration_s,acquired,peak_departure\n";
  int count = 0;
  for (const auto& r : rows) {
    Event evs[2];
    const int n = ta.observe(r.t, r.hall, r.pwm, evs);
    for (int i = 0; i < n; ++i) {
      const Event& e = evs[i];
      const bool isOpen = e.kind == EventKind::OPEN_CANDIDATE;
      const bool isAcquired = e.kind == EventKind::ACQUIRED;
      const bool isClose = e.kind == EventKind::CLOSE_AND_ADVANCE;
      const bool isDisposal = e.kind == EventKind::CONTRADICTION || e.kind == EventKind::DISPOSED;
      // Field population mirrors target_acquisition.py's per-kind Event.detail
      // dict exactly: a field absent from the Python dict for this kind is
      // left blank here too.
      out << kindName(e.kind) << "," << std::fixed;
      out.precision(6);
      out << e.t << "," << int(e.target_mm) << ",";
      if (isOpen || isDisposal) out << int(e.sign);
      out << ",";
      if (isOpen) out << int(e.matches_target);
      out << ",";
      if (isAcquired || isClose || isDisposal) out << e.start_s;
      out << ",";
      if (isClose) out << e.acquired_s;
      out << ",";
      if (isAcquired || isClose || isDisposal) out << e.duration_s;
      out << ",";
      if (isDisposal) out << int(e.acquired);
      out << ",";
      if (isDisposal) out << e.peak_departure;
      out << "\n";
      ++count;
    }
  }
  std::printf("cpp: %d events -> %s\n", count, outPath.c_str());
  return 0;
}
