#!/usr/bin/env python3
"""Compile only. Never selects a serial port, uploads, or contacts a locomotive."""
from pathlib import Path
import argparse, hashlib, json, re, subprocess, tempfile

ROOT = Path(__file__).resolve().parents[2]
SKETCH = ROOT/'firmware/test-programs/NAVI_RVM'
OUT = ROOT/'tests/navi_rvm/results'
p = argparse.ArgumentParser()
p.add_argument('--credentials-dir', type=Path, help='Private directory containing credentials.h; otherwise use inert review placeholders.')
p.add_argument('--build-path', type=Path, default=Path(tempfile.gettempdir())/'navi-rvm-build')
args = p.parse_args()
OUT.mkdir(exist_ok=True)
def invoke(command):
    return subprocess.run(command, capture_output=True, text=True)
with tempfile.TemporaryDirectory(prefix='rvm-compile-credentials-') as temp:
    credentials = args.credentials_dir or Path(temp)
    if not args.credentials_dir:
        (credentials/'credentials.h').write_text((SKETCH/'credentials.example.h').read_text())
    command = ['arduino-cli', 'compile', '--fqbn', 'esp32:esp32:esp32',
               '--build-path', str(args.build_path), '--build-property',
               'compiler.cpp.extra_flags=-I'+str(credentials)+' -Werror=format', str(SKETCH)]
    result = invoke(command)
    output = result.stdout+result.stderr
    (OUT/'compile-final.txt').write_text(output)
    report = {'command': command, 'exit': result.returncode, 'uploaded': False,
              'credentials': 'private external include' if args.credentials_dir else 'inert review placeholders',
              'arduino_cli': invoke(['arduino-cli','version']).stdout.strip(),
              'core_list': invoke(['arduino-cli','core','list']).stdout,
              'libraries': invoke(['arduino-cli','lib','list']).stdout,
              'source_sha256': {f.name: hashlib.sha256(f.read_bytes()).hexdigest()
                                for f in sorted(SKETCH.iterdir()) if f.suffix in ('.h','.ino')},
              'artifacts_sha256': {}}
    flash = re.search(r'Sketch uses ([\d,]+) bytes', output)
    ram = re.search(r'Global variables use ([\d,]+) bytes', output)
    report['program_bytes'] = int(flash[1].replace(',','')) if flash else None
    report['static_ram_bytes'] = int(ram[1].replace(',','')) if ram else None
    if result.returncode == 0:
        for suffix in ('.ino.bin', '.ino.elf'):
            f = args.build_path/('NAVI_RVM'+suffix)
            report['artifacts_sha256'][f.name] = hashlib.sha256(f.read_bytes()).hexdigest()
    (OUT/'build.json').write_text(json.dumps(report, indent=2)+'\n')
    print(output, end='')
    raise SystemExit(result.returncode)
