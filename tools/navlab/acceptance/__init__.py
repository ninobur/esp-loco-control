"""Frozen host acceptance harness for the autonomous acquisition design.

Written BEFORE the replacement navigator, per
docs/AUTONOMOUS_ACQUISITION_ACCEPTANCE_TESTS.md and decision 0042.

This package contains tests and test doubles only. It contains no navigator,
and none is to be added here: a navigator implementation belongs in its own
module and is loaded through navapi.load_navigator().
"""
