"""Run applicable 0.1 assertions against 0.3 headers; only redirect includes."""
from pathlib import Path
import re
import subprocess
import tempfile

HERE = Path(__file__).resolve().parents[1]
OLD = HERE.parent / 'NAVI_EWO_0_1_MM045_STOP' / 'tests'
cases = [
    ('map_adapter', ['-DHALL_POLARITY_INVERTED=true']),
    ('map_adapter', ['-DHALL_POLARITY_INVERTED=false']),
    ('recorder', []),
    ('stop_overlay', []),
    ('ir_configuration', []),
]
with tempfile.TemporaryDirectory() as tmp:
    for index, (name, profile) in enumerate(cases):
        original = OLD / f'test_{name}.cpp'
        def include(match):
            target = (original.parent / match[1]).resolve()
            if target.parent == OLD.parent and (HERE / target.name).exists():
                target = HERE / target.name
            return f'#include "{target}"'
        source = Path(tmp) / f'{index}.cpp'
        source.write_text(re.sub(r'#include "([^"]+)"', include, original.read_text()))
        binary = Path(tmp) / str(index)
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', *profile, str(source),
                        '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
        print(f'PASS unchanged 0.1 assertions against 0.3: {name} {profile}', flush=True)
