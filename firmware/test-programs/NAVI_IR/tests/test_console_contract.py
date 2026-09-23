#!/usr/bin/env python3
"""Exercise firmware states against the existing console's actual bookkeeping.

Only the constant and pure bookkeeping function are loaded from its AST; this
does not import/start Flask or MQTT, write the server, or send control commands.
Pass the compiled console_states.cpp executable as the sole argument.
"""
import ast
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
tree = ast.parse((ROOT / "server/ngr_app_v1_11_2.py").read_text())
selected = [node for node in tree.body if
            (isinstance(node, ast.FunctionDef) and node.name == "_apply_nav_state") or
            (isinstance(node, ast.Assign) and any(
                isinstance(target, ast.Name) and target.id == "USABLE_NAV"
                for target in node.targets))]
assert len(selected) == 2
state = {"nav": "NORMAL", "mm": "040", "nav_ready": "1", "start_interval": "040-041"}
scope = {"loco_state": {"test": state}, "_touch": lambda *args: None}
exec(compile(ast.Module(body=selected, type_ignores=[]), "console_bookkeeping", "exec"), scope)
lines = subprocess.run([sys.argv[1]], check=True, capture_output=True, text=True).stdout.splitlines()
assert len(lines) == 5
expected = ["NORMAL", "EVALUATING", "NORMAL", "LOST", "UNSET"]
for i, (line, nav) in enumerate(zip(lines, expected)):
    packet = json.loads(line)
    assert packet["nav"] == packet["state"] == nav
    scope["_apply_nav_state"]("test", packet["state"], packet["mm"])
    state["nav_ready"] = str(packet["nav_ready"])
    if i < 3:
        assert state["mm"] == "040" and state["nav_ready"] == "1"
        assert state["start_interval"] == "040-041"
    elif i == 3:
        assert state["mm"] == "--" and state["start_interval"] == "040-041"
    else:
        assert state["mm"] == "--" and state["start_interval"] == "UNSET"
print("PASS 5 NAVI_IR state transitions through actual console bookkeeping")
