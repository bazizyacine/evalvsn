#!/usr/bin/env python3
"""Compile the connectivity oracle of metrics/fire_oracle.py over the sets of
study 10 into oracle-s10.tsv, one row per realisation: the packets lost and,
among them, those that left the camera while no path of live nodes joined it
to the sink. The default sets are the load sweep (T = 7, 12, 18 and 30 s, four
stacks), the sensitivity variants (AZ, PF, failure law; rules and hybrid) and
the generalisation variants (two videos, 30 m grid, camera and sink swapped,
head speed x 1.5; four stacks);
glob patterns on the command line choose others. A set still running (no
graphs/PSNR/PSNR.dat) is left out.

Usage:
    python3 tools/compile_oracle.py [--out FILE] [PATTERN ...]   (default: src/data/oracle-s10.tsv)
"""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EXP = os.path.normpath(os.path.join(HERE, '..', '..', 'experiments'))
sys.path.insert(0, os.path.join(EXP, 'metrics'))
import fire_oracle as fo  # noqa: E402

NAME = re.compile(r'set_(AOMDV_QPV|AOMDV)_GC-PN-(0p\d+)-13_(p[0-9r]+)-n100-d(\d+)-(fire[23]?)-(.+?)(?:-s10)?$')
SPLIT = {'0p03': '9M3D', '0p02525': '6M6D', '0p01597': '3M9D'}
FIRE = {'fire': 'F1', 'fire2': 'F2', 'fire3': 'F3'}
DEFAULT = ['set_*_p7-n100-d40-*-s10', 'set_*_p12-n100-d40-*-s10', 'set_*_p18-n100-d40-*-s10', 'set_*_p30-n100-d40-*-s10',
           'set_*_p9-n100-d40-*-az*-s10', 'set_*_p9-n100-d40-*-pf*-s10', 'set_*_p9-n100-d40-*-fd1-s10',
           'set_*_p9-n100-d40-*-forest-s10', 'set_*_p9-n100-d40-*-smoke-s10', 'set_*_p9-n100-d30-*-j8-s10',
           'set_*_p9-n100-d40-*-swap-s10', 'set_*_p9-n100-d40-*-v15-s10']


def main(argv):
    out = os.path.join(HERE, '..', 'src', 'data', 'oracle-s10.tsv')
    if argv[:1] == ['--out']:
        out, argv = argv[1], argv[2:]
    pats = argv or DEFAULT
    names = sorted({os.path.basename(d) for p in pats for d in glob.glob(os.path.join(EXP, 'sets', p))})
    rows = ['# compiled by compile_oracle.py from experiments/sets (study 10) with metrics/fire_oracle.py',
            '\t'.join(('arm', 'fire', 'split', 'period', 'dist', 'realisation', 'lost', 'unreachable'))]
    for name in names:
        m = NAME.match(name)
        d = os.path.join(EXP, 'sets', name)
        if not m or not os.path.exists(os.path.join(d, 'graphs', 'PSNR', 'PSNR.dat')):
            print('skipped:', name, file=sys.stderr)
            continue
        proto, gc, period, dist, fire, arm = m.groups()
        arm = 'pq' if proto == 'AOMDV' and arm == 'pq' else arm
        # the two other videos have their own GOP coefficient, both giving nine M frames
        split = '9M3D' if ('-forest' in arm or '-smoke' in arm) else SPLIT[gc]
        for r in sorted(glob.glob(os.path.join(d, 'r[0-9][0-9]'))):
            x = fo.analyse(r)
            if x is None:
                print('no oracle input:', r, file=sys.stderr)
                continue
            rows.append('\t'.join((arm, FIRE[fire], split, period, dist, os.path.basename(r), str(x['lost']), str(x['unrec']))))
        print(name, flush=True)
    with open(out, 'w') as f:
        f.write('\n'.join(rows) + '\n')
    print('%s: %d realisations' % (out, len(rows) - 2))


if __name__ == '__main__':
    main(sys.argv[1:])
