#!/usr/bin/env python3
"""Compile the per-image results of the load sweep into frames.tsv, one row per
image: AOMDV and the rules, hybrid and agents stacks at GOP 9-3 and alarm periods of 7, 9
and 12 s, three fires, twenty realisations each (study 09 for T = 9 s, study 10,
tranche T3, for 7 and 12 s). Each row gives the image's type (M or D, from the
set's encoding/*/st-frame.dat), its PSNR and SSIM at the sink, its packet loss
and the loss of its first priority level (M1 for a main frame, D1 for a
differential one), all read from the realisation's sim/rt-frame.dat. A set or a
realisation missing from disk is reported and left out.

Usage:
    python3 tools/compile_frames.py [OUT]     (default: src/data/frames.tsv)
"""
import glob
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SETS = os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'sets'))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'src', 'data', 'frames.tsv')
FIRES = (('fire', 'F1'), ('fire2', 'F2'), ('fire3', 'F3'))
STACKS = ('pq', 'rules', 'hybrid', 'agents')
PERIODS = ('p7', 'p9', 'p12')


def set_dir(period, fire, stack):
    suffix = '' if period == 'p9' else '-s10'
    proto = 'AOMDV' if stack == 'pq' else 'AOMDV_QPV'
    return '%s/set_%s_GC-PN-0p03-13_%s-n100-d40-%s-%s%s' % (SETS, proto, period, fire, stack, suffix)


def frame_types(d):
    st = glob.glob(d + '/encoding/*/st-frame.dat')
    if not st:
        return None
    rows = [l.rstrip('\n').split('\t') for l in open(st[0]) if not l.startswith('#') and l.strip()]
    return {int(r[0]): r[1] for r in rows}


def main():
    out = ['# compiled by compile_frames.py from experiments/sets (studies 09 and 10, GOP 9-3, T = 7, 9 and 12 s)',
           '\t'.join(('arm', 'fire', 'period', 'realisation', 'image', 'type', 'psnr', 'ssim', 'loss', 'l1loss'))]
    missing = 0
    for period in PERIODS:
        for stack in STACKS:
            for fk, fv in FIRES:
                d = set_dir(period, fk, stack)
                types = frame_types(d)
                if types is None:
                    print('missing set or encoding:', d, file=sys.stderr)
                    missing += 20
                    continue
                for r in range(1, 21):
                    g = glob.glob('%s/r%02d/*/sim/rt-frame.dat' % (d, r))
                    if not g:
                        print('missing realisation:', d, r, file=sys.stderr)
                        missing += 1
                        continue
                    for line in open(g[0]):
                        if line.startswith('#') or not line.strip():
                            continue
                        x = line.rstrip('\n').split('\t')
                        img = int(x[0])
                        out.append('\t'.join((stack, fv, period, 'r%02d' % r, str(img), types[img], x[1], x[2], x[3], x[4])))
    with open(OUT, 'w') as f:
        f.write('\n'.join(out) + '\n')
    print('%s: %d images, %d realisations missing' % (OUT, len(out) - 2, missing))


if __name__ == '__main__':
    main()
