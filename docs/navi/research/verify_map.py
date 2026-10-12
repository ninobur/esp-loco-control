"""Reproduce map-only historical claims; no firmware writes or new patterns."""
import collections
import hashlib
import json
from pathlib import Path
import re
import sys

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
    "firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h")
raw = path.read_bytes()
blob = hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()
text = raw.decode()
def array(name):
    values = re.search(name + r"\[ROUTE_N\] = \{(.*?)\}", text, re.S)[1]
    return [int(v) for v in values.split(",") if v.strip()]
polarity, spacing = array("ROUTE_POLARITY"), array("ROUTE_SPACING_MM")
result = {"map_blob": blob, "n": len(polarity), "loop_mm": sum(spacing),
          "min_mm": min(spacing), "max_mm": max(spacing), "tests": []}
assert len(polarity) == len(spacing) == 171
for direction in (1, -1):
    for width in range(7, 11):
        words = [tuple(polarity[(i + j * direction) % len(polarity)]
                       for j in range(width)) for i in range(len(polarity))]
        counts = collections.Counter(words)
        result["tests"].append({"direction": direction, "window": width,
            "distinct_words": len(counts),
            "positions_with_unique_word": sum(counts[word] == 1 for word in words),
            "max_ambiguity": max(counts.values())})
print(json.dumps(result, indent=2))
