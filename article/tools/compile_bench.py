#!/usr/bin/env python3
"""Compile study 08 (the learning agents on three use cases) into feu-agents.tsv, one line per realisation, so
that its tables are generated from data a repository keeps (the
sets are gitignored machine-local artifacts). Its trace readers serve
compile_comparative.py too.

Columns: level, arm, realisation, psnr (dB), loss, mloss, m1loss, m1to3loss
(levels M1 to M3 together), dloss (%), recoverable (% of the packets sent,
from metrics/fire_oracle.py). Six cells: levels 0 to 3, u1 and u2. The arm is
the set's suffix in the nomenclature of 15 October 2026: pq, base (the
rules without early discard), rules, agents, hybrid, the ag<letters>
ablations, and aomdv, AOMDV on its own seeds.

Level 0 is the agent-by-agent bench (one image every 10 s, no early discard):
each agent alone against the rule stack, the other decisions keeping their
rule.
"""
import argparse
import glob
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EXP = os.path.normpath(os.path.join(HERE, '..', '..', 'experiments'))
sys.path.insert(0, os.path.join(EXP, 'metrics'))
import fire_oracle  # noqa: E402

SETS = os.path.join(EXP, 'sets')  # study 08 (the bench sets, adopted on 11 Oct 2026)
# level -> cell: 0 the agent-by-agent level, 1 to 3 the fire scenario with
# headroom, u1 the intact base network, u2 S1 at half destruction (study 08's
# use cases 1 and 2).
LEVELS = (('0', 'GC-PN-0p03-13_p10-n100-d40-fsev'), ('1', 'GC-PN-0p03-13_p9-n100-d40-fire'),
          ('2', 'GC-PN-0p03-13_p8-n100-d40-fire'), ('3', 'GC-PN-0p03-13_p7-n100-d40-fire'),
          ('u1', 'GC-PN-0p03-13_p30-n100-d200'), ('u2', 'GC-PN-0p03-13_p30-n100-d40-m50'))
ARMS = (('aomdv', 'set_AOMDV_{c}'), ('base', 'set_AOMDV_QPV_{c}'),
        ('rules', 'set_AOMDV_QPV_{c}-rules'), ('agPCBDAT', 'set_AOMDV_QPV_{c}-agPCBDAT'),
        ('agents', 'set_AOMDV_QPV_{c}-agents'), ('hybrid', 'set_AOMDV_QPV_{c}-hybrid'),
        ('agPCDAT-er', 'set_AOMDV_QPV_{c}-agPCDAT-er'), ('agB', 'set_AOMDV_QPV_{c}-agB'),
        ('agP', 'set_AOMDV_QPV_{c}-agP'), ('agC', 'set_AOMDV_QPV_{c}-agC'),
        ('agD', 'set_AOMDV_QPV_{c}-agD'), ('agT', 'set_AOMDV_QPV_{c}-agT'),
        ('agA', 'set_AOMDV_QPV_{c}-agA'),
        # the agents without the early discard, AOMDV on
        # the cross-layer seeds; the proposed stack without the channel-access
        # agent (the level-3 diagnosis)
        ('agPCDAT', 'set_AOMDV_QPV_{c}-agPCDAT'), ('pq', 'set_AOMDV_{c}-pq'),
        ('agPCDTE', 'set_AOMDV_QPV_{c}-agPCDTE'))


def load_trace(path):
    out = {}
    for line in open(path):
        if line.startswith('#') or not line.strip():
            continue
        v = line.split('\t')
        out[int(v[1])] = (v[4], int(v[5]))
    return out


def level_losses(r):
    st = glob.glob(os.path.join(r, '*', 'st-packet.dat'))
    rt = glob.glob(os.path.join(r, '*', 'sim', 'rt-packet.dat'))
    s, g = load_trace(st[0]), load_trace(rt[0])

    def loss(pred):
        ks = [q for q, (ft, pr) in s.items() if pred(ft, pr)]
        return 100.0 * sum(1 for q in ks if q not in g) / len(ks)
    return (loss(lambda ft, pr: ft == 'M' and pr == 1),
            loss(lambda ft, pr: ft == 'M' and pr <= 3),
            loss(lambda ft, pr: ft == 'D'))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.parse_args()
    out = os.path.normpath(os.path.join(HERE, '..', '..', 'thesis', 'data', 'feu-agents.tsv'))
    rows = []
    for lvl, cell in LEVELS:
        for arm, pat in ARMS:
            d = os.path.join(SETS, pat.format(c=cell))
            reals = sorted(glob.glob(os.path.join(d, 'r[0-9][0-9]')))
            # a set still running is left out: every realisation must be done
            if not reals or any(not glob.glob(os.path.join(r, '*', 'sim', 'mt.dat')) for r in reals):
                continue
            for r in reals:
                mt = glob.glob(os.path.join(r, '*', 'sim', 'mt.dat'))
                if not mt:
                    continue
                v = [l for l in open(mt[0]) if not l.startswith('#') and l.strip()][0].split('\t')
                m1, m13, dl = level_losses(r)
                orc = fire_oracle.analyse(r)   # None without a fire (use case 1)
                rec = 100.0 * orc['rec'] / orc['sent'] if orc else float('nan')
                rows.append((lvl, arm, os.path.basename(r), float(v[0]), float(v[4]), float(v[5]), m1, m13, dl, rec))
    with open(out, 'w') as f:
        f.write('# compiled by tools/compile_bench.py from experiments/sets (study 08)\n')
        f.write('level\tarm\trealisation\tpsnr\tloss\tmloss\tm1loss\tm1to3loss\tdloss\trecoverable\n')
        for row in rows:
            f.write('%s\t%s\t%s\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\n' % row)
    print('%d realisations written to %s' % (len(rows), os.path.relpath(out)))


if __name__ == '__main__':
    main()
