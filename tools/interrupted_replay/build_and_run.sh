#!/bin/sh
# Decision 0070's harness, run against the FIELD-TEST BUILD.
#
# The change has landed in firmware/. This no longer compares a proposal
# against the tree -- it compares the WORKING TREE against the last accepted
# firmware commit, so the equivalence claim keeps its meaning after the fact:
#
#   base/      the historical firmware/programs/NAVI_ONE path at $NAVI_ONE_BASE
#              (NAVI_ONE 0.9,
#              the last accepted build), extracted from git, never the checkout
#   fieldtest/ the working tree as it stands now
#
# It runs the ELEVEN pre-existing gates against both with the SAME unmodified
# runner -- the one from the base commit -- and diffs the two outputs byte for
# byte. Anything but IDENTICAL means the pause/resume path has reached into the
# ordinary one. Then it runs gate 12, which only exists in the field-test tree.
#
# The trees are laid out at the same depth as the real one, with field-records
# symlinked, because run_tests.sh finds the repository by walking up from
# itself and the replay gates read real captures from it.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/../.." && pwd)
# The current sketch is grouped with the other NAVI_ONE variants.  The base
# commit predates that grouping, so its historical archive path is retained
# for the comparison fixture.
src="$repo/firmware/programs/NAVI_ONE/variants/NAVI_ONE"
rel="firmware/programs/NAVI_ONE"
# NAVI_ONE 0.9 -- the last firmware commit before decision 0070, and the build
# Toby last ran. Override to compare against something else.
base=${NAVI_ONE_BASE:-1b8b828}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/base/firmware/programs" "$tmp/fieldtest/firmware/programs"
git -C "$repo" archive "$base" "$rel" | tar -x -C "$tmp/base"
cp -R "$src" "$tmp/fieldtest/$rel"
for tree in base fieldtest; do ln -s "$repo/field-records" "$tmp/$tree/field-records"; done
b="$tmp/base/$rel"
f="$tmp/fieldtest/$rel"
# credentials.h is git-ignored, so it is not in the archive. The gates include
# it through LL_LocoConfig_9950012.h and will not compile without it. It never
# leaves this machine: $tmp is removed on exit.
[ -f "$src/credentials.h" ] && cp "$src/credentials.h" "$b/credentials.h"
# The eleven are run in the field-test tree by the BASE COMMIT'S runner, so the
# comparison below is gate for gate with nothing new in the way.
cp "$b/tests/run_tests.sh" "$f/tests/run_eleven.sh"

echo "=============================================================================="
echo " 0. what changed in firmware/"
echo "=============================================================================="
git -C "$repo" diff --stat "$base" -- "$rel" || true
echo ""
git -C "$repo" status --short "$rel" || true

echo ""
echo "=============================================================================="
echo " 1. the eleven existing gates, at $base (NAVI_ONE 0.9)"
echo "=============================================================================="
sh "$b/tests/run_tests.sh" > "$tmp/base.log" 2>&1 && a=0 || a=$?
tail -n 4 "$tmp/base.log"; echo "[exit $a]"

echo ""
echo "=============================================================================="
echo " 2. the same eleven, against the FIELD-TEST BUILD"
echo "=============================================================================="
sh "$f/tests/run_eleven.sh" > "$tmp/field.log" 2>&1 && c=0 || c=$?
tail -n 4 "$tmp/field.log"; echo "[exit $c]"

echo ""
echo "=============================================================================="
echo " 3. equivalence on the ordinary path"
echo "=============================================================================="
echo "The eleven include the 2026-08-28 survey replay -- 187 real passages and"
echo "their residuals -- the 2026-08-29 lap replay, the polarity survey and the"
echo "two baseline gates. Every line of all eleven, both trees, compared:"
d=0
if diff -u "$tmp/base.log" "$tmp/field.log" > "$tmp/gates.diff" 2>&1; then
  echo "  IDENTICAL. Nothing in the field-test build alters a verdict, residual"
  echo "  or ruling anywhere the pre-existing gates reach."
else
  d=1
  echo "  *** THE TWO TREES DISAGREE:"
  sed -n '1,60p' "$tmp/gates.diff"
fi

echo ""
echo "=============================================================================="
echo " 4. gate 12 -- the interrupted-traversal rule, adversarially"
echo "=============================================================================="
c++ -std=c++17 -O1 -Wall -Wextra "$f/tests/gate_interrupted.cpp" -o "$tmp/g12"
"$tmp/g12" && g=0 || g=$?

echo ""
echo "=============================================================================="
[ "$a" = 0 ] && [ "$c" = 0 ] && [ "$d" = 0 ] && [ "$g" = 0 ] \
  && echo " ALL PASS -- and gate 12 carries a known risk it does not close. Read it." \
  || echo " *** SOMETHING FAILED (eleven:$a/$c diff:$d gate12:$g)"
echo "=============================================================================="
[ "$a" = 0 ] && [ "$c" = 0 ] && [ "$d" = 0 ] && [ "$g" = 0 ]
