"""Join two consecutive boots of one run into a single block tracing on the first boot's clock
(the reboot interval becomes gap blocks: Hall NaN, PWM 0, commanded 0), so the run can be replayed
as if Otto had not rebooted. Analysis only.
  u08_stitch.py <boots_dir> <bootA> <bootB> <name> <decl_us_bootA> <last> <next> <dir>"""
import sys, pathlib, numpy as np
BD = pathlib.Path(sys.argv[1]); A, Bn, NAME = sys.argv[2], sys.argv[3], sys.argv[4]
decl_us, last, nxt, dr = int(sys.argv[5]), int(sys.argv[6]), int(sys.argv[7]), int(sys.argv[8])
a = dict(np.load(BD / f'boot_{A}.npz')); b = dict(np.load(BD / f'boot_{Bn}.npz'))
offA, offB = float(a['pi_offset_s']), float(b['pi_offset_s'])
tb_on_a = b['t_us'] + int(round((offB - offA) * 1e6))                 # boot-B block centres on boot-A's clock
t0 = int(a['t_us'][0]); nb = int(round((tb_on_a[-1] - t0) / 20000)) + 1
t = t0 + np.arange(nb) * 20000
out = dict(t_us=t, hall=np.full(nb, np.nan), n=np.zeros(nb, int), pwm=np.zeros(nb, np.int16), cmd=np.zeros(nb, np.int16), dirb=np.zeros(nb, np.int8))
ia = np.round((a['t_us'] - t0) / 20000).astype(int); ib = np.round((tb_on_a - t0) / 20000).astype(int)
for k in ('hall', 'n', 'pwm', 'cmd', 'dirb'): out[k][ia] = a[k]; out[k][ib] = b[k]
gap = (ib[0] - ia[-1] - 1) * 0.02
np.savez_compressed(BD / f'boot_{NAME}.npz', **out, pi_offset_s=offA, first_us=t0,
                    decl_idx=np.array([int(np.argmin(np.abs(t - decl_us)))]), decl_last=np.array([last]), decl_next=np.array([nxt]), decl_dir=np.array([dr]))
print(f'{NAME}: {nb} blocks ({nb*0.02:.1f} s); boot A {len(ia)} blocks, reboot gap {gap:.2f} s, boot B {len(ib)} blocks')
