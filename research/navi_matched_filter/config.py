"""Configurations frozen from the dev-partition sweep (results/sweep_dev.json).

Rule fixed before reading validation/transfer: among dev configurations with
recall >= 99.5 %, take the fewest false + duplicate events; break ties toward
the simpler window (level-only) and lower thresholds.
"""
import mf

FROZEN = mf.Config(detrend='mean', pad=1.0, r_min=0.7, a_min=70.0)
# Best level+slope (line) configuration on dev at the same recall, for comparison.
LINE_ALT = mf.Config(detrend='line', pad=1.0, r_min=0.7, a_min=70.0)
# Muse-style configuration as reported (0.6 threshold, 70-count floor).
MUSE_LIKE = mf.Config(detrend='mean', pad=0.5, r_min=0.6, a_min=70.0)
