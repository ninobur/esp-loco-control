#!/bin/sh
# Decision 0070's harness.
#
# NOTHING IN firmware/ IS MODIFIED OR COMPILED IN PLACE. This assembles two
# build trees under a temporary directory --
#
#   actual/    firmware/test-programs/NAVI_ONE, verbatim
#   proposed/  the same, with tools/interrupted_replay/proposed/ overlaid
#
# -- runs the existing eleven gates against BOTH with the SAME unmodified
# runner, diffs the two outputs byte for byte to show the ordinary path is
# untouched, and then runs gate 12, which only exists in the proposed tree.
#
# The trees are laid out at the same depth as the real one, with field-records
# symlinked, because run_tests.sh finds the repository by walking up from
# itself and the replay gates read real captures from it.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../.." && pwd)
src="$repo/firmware/test-programs/NAVI_ONE"
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

for tree in actual proposed; do
  mkdir -p "$tmp/$tree/firmware/test-programs"
  cp -R "$src" "$tmp/$tree/firmware/test-programs/NAVI_ONE"
  ln -s "$repo/field-records" "$tmp/$tree/field-records"
done
p="$tmp/proposed/firmware/test-programs/NAVI_ONE"
cp "$here/proposed/"*.h "$here/proposed/"*.ino "$p/"
cp "$here/proposed/tests/"* "$p/tests/"
# The eleven existing gates are run in the proposed tree by the UNMODIFIED
# runner, so the comparison below is gate for gate with nothing new in the way.
cp "$src/tests/run_tests.sh" "$p/tests/run_eleven.sh"

echo "=============================================================================="
echo " 0. the working tree is untouched"
echo "=============================================================================="
git -C "$repo" status --short firmware/test-programs/NAVI_ONE || true
echo "(no lines above: no firmware file has been modified)"

echo ""
echo "=============================================================================="
echo " 1. the eleven existing gates, against the UNMODIFIED firmware"
echo "=============================================================================="
sh "$tmp/actual/firmware/test-programs/NAVI_ONE/tests/run_tests.sh" > "$tmp/actual.log" 2>&1 && a=0 || a=$?
tail -n 4 "$tmp/actual.log"; echo "[exit $a]"

echo ""
echo "=============================================================================="
echo " 2. the same eleven, against the PROPOSED firmware"
echo "=============================================================================="
sh "$p/tests/run_eleven.sh" > "$tmp/proposed.log" 2>&1 && b=0 || b=$?
tail -n 4 "$tmp/proposed.log"; echo "[exit $b]"

echo ""
echo "=============================================================================="
echo " 3. equivalence on the ordinary path"
echo "=============================================================================="
echo "The eleven include the 2026-08-28 survey replay -- 187 real passages and"
echo "their residuals -- the 2026-08-29 lap replay, the polarity survey and the"
echo "two baseline gates. Every line of all eleven, both trees, compared:"
d=0
if diff -u "$tmp/actual.log" "$tmp/proposed.log" > "$tmp/gates.diff" 2>&1; then
  echo "  IDENTICAL. The proposed change alters no verdict, residual or ruling"
  echo "  anywhere the existing gates reach."
else
  d=1
  echo "  *** THE TWO TREES DISAGREE:"
  sed -n '1,60p' "$tmp/gates.diff"
fi

echo ""
echo "=============================================================================="
echo " 4. gate 12 -- the interrupted-traversal rule, adversarially"
echo "=============================================================================="
c++ -std=c++17 -O1 -Wall -Wextra "$p/tests/gate_interrupted.cpp" -o "$tmp/g12"
"$tmp/g12" && g=0 || g=$?

echo ""
echo "=============================================================================="
[ "$a" = 0 ] && [ "$b" = 0 ] && [ "$d" = 0 ] && [ "$g" = 0 ] \
  && echo " ALL PASS" || echo " *** SOMETHING FAILED (eleven:$a/$b diff:$d gate12:$g)"
echo "=============================================================================="
[ "$a" = 0 ] && [ "$b" = 0 ] && [ "$d" = 0 ] && [ "$g" = 0 ]
