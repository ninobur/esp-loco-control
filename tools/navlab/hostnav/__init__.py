"""Replacement autonomous-acquisition host navigator.

    NGR_NAVIGATOR=tools.navlab.hostnav.navigator:Navigator \
        python3 -m tools.navlab.acceptance.run_acceptance --require-navigator

This package is deliberately NOT inside `tools/navlab/acceptance/`, which
remains the independent frozen test harness.
"""
from .navigator import Navigator            # noqa: F401
