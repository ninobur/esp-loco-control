// Host-only design gate for a bounded post-stop passage resolver.
//
// This does not alter or stand in for the production firmware. It encodes the
// three field records that any proposed integration must satisfy first.
#include <cstdio>
#include "post_stop_resolver_model.h"

using namespace post_stop_model;

static int checks = 0;
static int failures = 0;
static void ck(bool condition, const char* label) {
  ++checks;
  if (!condition) { ++failures; std::printf("  FAIL  %s\n", label); }
}

static Candidate c(uint32_t open, uint32_t close, uint16_t peak,
                   uint16_t gain, uint8_t polarity) {
  return Candidate{open, close, peak, gain, polarity};
}

int main() {
  std::printf("NAVI_ONE experimental post-stop resolver gate\n");

  std::printf("\nBamboo X14: retain the genuine strong MM161 passage\n");
  {
    Resolver r;
    r.arm();
    r.acceptedAnchor(c(1637629, 1637893, 84, 160, 1));       // MM160 N
    const Resolution mm161 = r.examine(
        c(1638029, 1640525, 239, 158, 0), 0);                // expected S
    ck(mm161 == Resolution::AcceptStrongClose,
       "MM161 earns the bounded strong/expected bypass");
    const Resolution mm162 = r.examine(
        c(1641515, 1641649, 155, 158, 1), 1);                // expected N
    ck(mm162 == Resolution::AcceptOrdinary,
       "MM162 remains an ordinary passage");
  }

  std::printf("\nGrillers X15: retain the successful MM64 departure\n");
  {
    Resolver r;
    r.arm();
    r.acceptedAnchor(c(1000, 1140, 170, 175, 1));            // MM63 N
    const Resolution mm64 = r.examine(c(1253, 1450, 252, 182, 0), 0);
    ck(mm64 == Resolution::AcceptStrongClose,
       "MM64 earns the bounded strong/expected bypass");
    ck(r.examine(c(2654, 2794, 180, 182, 1), 1) ==
           Resolution::AcceptOrdinary,
       "MM65 remains an ordinary passage");
  }

  std::printf("\nArches X15: quarantine N lobe, retain expected S lobe\n");
  {
    Resolver r;
    r.arm();
    r.acceptedAnchor(c(1608385, 1608683, 84, 179, 0));       // MM106 S
    const Resolution north = r.examine(
        c(1609001, 1610963, 74, 179, 1), 0);                 // wrong N
    ck(north == Resolution::Quarantine,
       "wrong/weak North component is quarantined");
    const Resolution south = r.examine(
        c(1611021, 1611141, 113, 179, 0), 0);                // expected S
    ck(south == Resolution::ReplaceQuarantined,
       "expected South component replaces it without re-anchoring");
  }

  std::printf("\nAdversarial bounds\n");
  {
    Resolver r;
    r.arm();
    r.acceptedAnchor(c(1000, 1200, 180, 180, 0));
    ck(r.examine(c(1500, 1560, 44, 180, 1), 1) == Resolution::Quarantine,
       "0052-strength matching re-read cannot advance");
  }
  {
    Resolver r;
    r.arm();
    r.acceptedAnchor(c(1000, 1200, 180, 180, 0));
    ck(r.examine(c(1500, 1700, 220, 180, 0), 1) == Resolution::Quarantine,
       "strong wrong-polarity component cannot advance");
  }
  {
    Resolver r;
    r.arm();
    r.acceptedAnchor(c(1000, 1200, 180, 180, 0));
    ck(r.examine(c(1700, 1880, 80, 180, 1), 1) ==
           Resolution::AcceptOrdinary,
       "500 ms boundary remains ordinary and unconditional");
  }

  std::printf("\n%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
