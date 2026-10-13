from pathlib import Path
import hashlib
import json
import subprocess
import time
import serial

root=Path('/Users/davidbrown/NGR/NGR-Files/navi-ir-sync-fix-20261012')
rel=Path('firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST')
manifest=json.loads((root/'manifest.json').read_text())
assert all(v=='PASS' for v in manifest['gates'].values())
repo=Path(manifest['repository'])
for name,digest in manifest['source_sha256'].items():
    assert hashlib.sha256((root/rel/name).read_bytes()).hexdigest()==digest
    assert hashlib.sha256((repo/rel/name).read_bytes()).hexdigest()==digest
binary=root/'build/NAVI_COHERENCE_0_6_IR_SYNC_TEST.ino.bin'
assert hashlib.sha256(binary.read_bytes()).hexdigest()==manifest['application_sha256']
rollback=json.loads((root/'rollback/manifest.json').read_text())
assert rollback['mac']==manifest['mac']
backup=Path(rollback['file'])
assert backup.stat().st_size==rollback['bytes']
assert hashlib.sha256(backup.read_bytes()).hexdigest()==rollback['sha256']
for region in rollback['upload_regions']:
    assert int(region['erase_end'],16)<=rollback['bytes']
    assert (root/'build'/region['file']).stat().st_size==region['bytes']
manifest['rollback_sha256']=rollback['sha256']
manifest['rollback_coverage']=rollback['coverage']

# Independently check the target immediately before upload. This reads identity;
# no firmware write occurs unless the expected device is connected.
tool='/Users/davidbrown/Library/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool'
identity=subprocess.run([tool,'--chip','esp32','--port',manifest['port'],'--baud','115200','--after','no-reset','read-mac'],capture_output=True,text=True)
(root/'preupload-identity.log').write_text(identity.stdout+identity.stderr)
assert identity.returncode==0 and manifest['mac'] in identity.stdout.lower()

cmd=['arduino-cli','upload','--verify','--fqbn','esp32:esp32:esp32','--port',manifest['port'],
     '--upload-property','upload.speed=115200','--input-dir',str(root/'build'),str(root/rel)]
manifest['upload_command']=cmd
with (root/'upload.log').open('w') as log:
    result=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
manifest['upload_exit_code']=result.returncode
manifest['flash_status']='UPLOAD_PASS' if result.returncode==0 else 'UPLOAD_FAILED'
(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
if result.returncode:
    raise SystemExit('Upload failed; inspect upload.log. Rollback image preserved.')
print('Upload passed; capturing the boot identity.',flush=True)

p=serial.Serial(port=None,baudrate=115200,timeout=.25)
p.dtr=False;p.rts=False;p.port=manifest['port'];p.open()
p.rts=True;time.sleep(.1);p.rts=False
end=time.monotonic()+15;data=bytearray()
while time.monotonic()<end:data.extend(p.read(4096))
p.close()
text=data.decode('utf-8','replace')
(root/'postflash-boot.log').write_text(text)
ok=manifest['sketch'] in text and '9950011' in text and 'FATAL' not in text and 'Guru Meditation' not in text
manifest['boot_identity_verified']=ok
manifest['flash_status']='UPLOAD_AND_BOOT_VERIFIED' if ok else 'UPLOAD_PASS_BOOT_REVIEW_REQUIRED'
(root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(text[:14000])
if not ok:raise SystemExit('Boot needs review; do not claim complete boot verification.')
