from pathlib import Path
import hashlib
import json
import subprocess

root=Path('/Users/davidbrown/NGR/NGR-Files/navi-ir-sync-fix-20261012')
folder=root/'rollback'
tool='/Users/davidbrown/Library/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool'
expected='68:09:47:ac:4e:b4'
parts=[]
for offset in range(0,0x400000,0x80000):
    p=folder/f'chunk-{offset:06x}.bin'
    for attempt,baud in enumerate((230400,115200),1):
        cmd=[tool,'--chip','esp32','--port','/dev/cu.wchusbserial110','--baud',str(baud),'--after','no-reset',
             'read-flash','--no-progress',hex(offset),'0x80000',str(p)]
        result=subprocess.run(cmd,text=True,capture_output=True)
        (folder/f'chunk-{offset:06x}-attempt{attempt}.log').write_text(result.stdout+result.stderr)
        if result.returncode==0 and p.stat().st_size==0x80000:
            assert expected in result.stdout.lower(), 'Unexpected device identity'
            parts.append(p.read_bytes())
            print(f'Preserved chunk {len(parts)}/8 at {baud} baud',flush=True)
            break
    else:
        raise SystemExit(f'Backup failed at {offset:#x}; no flash authorized by this script')
full=b''.join(parts)
assert len(full)==0x400000
p=folder/'otto-9950011-pre-ir-sync.bin'
p.write_bytes(full)
manifest={'mac':expected,'profile':'Otto 9950011','installed_sketch':'NAVI_EWO_0_1_MM045_STOP',
          'bytes':len(full),'sha256':hashlib.sha256(full).hexdigest(),
          'source':'Eight successful esptool reads; application held in bootloader between chunks',
          'restore_command':f'{tool} --chip esp32 --port /dev/cu.wchusbserial110 --baud 115200 write-flash 0x0 {p}'}
(folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Full rollback image preserved: '+manifest['sha256'],flush=True)
