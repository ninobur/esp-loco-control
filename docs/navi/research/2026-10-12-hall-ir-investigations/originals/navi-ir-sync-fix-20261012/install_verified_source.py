from pathlib import Path
import hashlib
import shutil

work=Path('/Users/davidbrown/NGR/NGR-Files/navi-ir-sync-fix-20261012')
repo=Path('/Users/davidbrown/esp-loco-control')
rel=Path('firmware/programs/NAVI_COHERENCE/variants')
name='NAVI_COHERENCE_0_6_IR_SYNC_TEST'
src=work/rel/name
dst=repo/rel/name
assert not dst.exists(), 'Refuse to replace an existing variant directory'
for p in (repo/rel/'NAVI_COHERENCE_0_6_IR_HEALTH').glob('*.h'):
    assert p.read_bytes()==(src/p.name).read_bytes(), 'Historical dependency changed: '+p.name
for filename in (name+'.ino',name+'_README.md'):
    assert (repo/rel/filename).read_bytes()==(work/'rollback'/filename).read_bytes(), 'Submitted file changed: '+filename
shutil.copytree(src,dst)
# Keep both user-supplied discovery paths pointing at the single corrected source.
for old,new in [(name+'.ino',name+'.ino'),(name+'_README.md','README.md')]:
    p=repo/rel/old
    p.unlink()
    p.symlink_to(Path(name)/new)
catalog=repo/'firmware/README.md'
text=catalog.read_text()
entry='''**2026-10-12 IR synchronization diagnostic R1:**
`programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST/`
identifies as **NAVI_COHERENCE_0_6_IR_SYNC_TEST_R1**, Otto 9950011 profile.
Experimental observation-only Type-5 / Type-6 Hall receiver-arrival comparison
on the historical 20Q3/NSR1 control baseline. Separate Type-6 queue/loss state,
causal diagnostic history and visible post-gap measurements. **Built with ESP32
3.3.12; all ten inherited host suites, JSON checks and new Type-6 isolation
regression pass.** Original navigation headers/control flow retained. Not field
accepted; no production promotion. Controlling records: variant `README.md` and
`VALIDATION.md` (including exact deployment status and rollback evidence).

'''
assert 'IR synchronization diagnostic R1' not in text
text=text.replace('## Current catalog\n\n','## Current catalog\n\n'+entry,1)
catalog.write_text(text)
print('Installed verified variant, preserved flat-path aliases, updated firmware catalog.')
print('Sketch SHA-256:',hashlib.sha256((dst/(name+'.ino')).read_bytes()).hexdigest())
