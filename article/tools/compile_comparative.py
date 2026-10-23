#!/usr/bin/env python3
"""Compile the comparative study (study 09, decisions d42 and d45) into
src/data/comparative.tsv, one line per realisation, so that the article's
tables are generated from data the repository keeps (the sets are gitignored
machine-local artifacts).

Columns: fire (F1-F3), split (3M9D, 6M6D, 9M3D), stack (aomdv, rules,
agents, hybrid), realisation, psnr (dB), loss, mloss, m1loss, m1to3loss, dloss
(%), recoverable (% of the packets sent, from metrics/fire_oracle.py).

Also writes src/data/fire-map-F<k>.tsv, each fire's node schedule in the
first simulation of the rule stack at 9M/3D, for the scenario figure.
"""
import argparse
import glob
import os

from compile_bench import HERE, SETS, fire_oracle, level_losses

FIRES = (('F1', 'fire', '180 180', '1.3', '6', '315'),
         ('F2', 'fire2', '120 40', '0.83', '4.17', '135'),
         ('F3', 'fire3', '240 320', '1.11', '5.56', '315'))
SPLITS = (('3M9D', 'GC-PN-0p01597-13'), ('6M6D', 'GC-PN-0p02525-13'), ('9M3D', 'GC-PN-0p03-13'))
STACKS = (('aomdv', 'set_AOMDV_{c}_p9-n100-d40-{f}-pq'), ('rules', 'set_AOMDV_QPV_{c}_p9-n100-d40-{f}-rules'),
          ('agents', 'set_AOMDV_QPV_{c}_p9-n100-d40-{f}-agents'), ('hybrid', 'set_AOMDV_QPV_{c}_p9-n100-d40-{f}-hybrid'))


def fire_map(out_dir, label, tok, ign, head, wind, wdir):
    src = glob.glob(os.path.join(SETS, 'set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-%s-rules' % tok, 'r01', '*', 'fire-nodes.dat'))
    if not src:
        return
    rows = [l.split() for l in open(src[0]) if not l.startswith('#') and l.strip()]
    simt = [l for l in open(src[0]) if l.startswith('# prop')][0].split('simt')[1].split()[0]
    path = os.path.join(out_dir, 'fire-map-%s.tsv' % label)
    with open(path, 'w') as f:
        f.write('# compiled by tools/compile_comparative.py from the rule stack, 9M/3D, r01\n')
        f.write('# ignition %s t0 0 head_speed %s wind %s wind_dir %s zone_s 60 detection 60 simt %s camera 0 sink 99\n'
                % (ign, head, wind, wdir, simt))
        f.write('id\tx\ty\tfront\tdeath\n')
        for r in rows:
            f.write('%s\t%s\t%s\t%.3f\t%.3f\n' % (r[0], r[1], r[2], float(r[3]), float(r[4])))
    print('fire map written to %s' % os.path.relpath(path))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.parse_args()
    out = os.path.join(HERE, '..', 'src', 'data', 'comparative.tsv')
    rows, missing = [], []
    for label, tok, *_ in FIRES:
        for split, codec in SPLITS:
            for stack, pat in STACKS:
                name = pat.format(c=codec, f=tok)
                reals = sorted(glob.glob(os.path.join(SETS, name, 'r[0-9][0-9]')))
                # a set still running is left out: every realisation must be done
                if not reals or any(not glob.glob(os.path.join(r, '*', 'sim', 'mt.dat')) for r in reals):
                    missing.append(name)
                    continue
                for r in reals:
                    mt = glob.glob(os.path.join(r, '*', 'sim', 'mt.dat'))
                    v = [l for l in open(mt[0]) if not l.startswith('#') and l.strip()][0].split('\t')
                    m1, m13, dl = level_losses(r)
                    orc = fire_oracle.analyse(r)
                    rec = 100.0 * orc['rec'] / orc['sent'] if orc else float('nan')
                    rows.append((label, split, stack, os.path.basename(r), float(v[0]), float(v[4]), float(v[5]),
                                 m1, m13, dl, rec))
    with open(out, 'w') as f:
        f.write('# compiled by tools/compile_comparative.py from experiments/sets (study 09)\n')
        f.write('fire\tsplit\tstack\trealisation\tpsnr\tloss\tmloss\tm1loss\tm1to3loss\tdloss\trecoverable\n')
        for row in rows:
            f.write('%s\t%s\t%s\t%s\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\n' % row)
    print('%d realisations written to %s; %d sets not complete' % (len(rows), os.path.relpath(out), len(missing)))
    for label, tok, ign, head, wind, wdir in FIRES:
        fire_map(os.path.dirname(out), label, tok, ign, head, wind, wdir)


if __name__ == '__main__':
    main()
