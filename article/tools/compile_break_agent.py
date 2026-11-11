#!/usr/bin/env python3
"""Compile what the traces say about the neighbour-death decision into
src/data/break-agent.tsv, one row per simulation at GOP 9-3: the counts the
link-break agent picked and declared with (traced arms only), the declarations
per 18 s of the alarm, and the PSNR of the twelve images. The traces are purged
at the end of the campaign; this file keeps what the text needs from them.

Usage:
    python3 tools/compile_break_agent.py
"""
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SETS = os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'sets'))
OUT = os.path.join(HERE, '..', 'src', 'data', 'break-agent.tsv')
BASE = 'set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-'
FIRES = (('fire', 'F1'), ('fire2', 'F2'), ('fire3', 'F3'))
ARMS = (('rules', 'rules'), ('agents-frozen', 'agents-frozen'), ('hybrid-tr-s10', 'hybrid'), ('agents-tr-s10', 'agents'),
        ('hybrid-edec-s10', 'hybrid-edec'), ('agents-edec-s10', 'agents-edec'))
COUNTS = (1, 2, 3, 5)
EDGES = (60, 78, 96, 114, 132, 150, 169)
DEATH = re.compile(r'^A (\d+\.\d+) _(\d+)_ deleting LL hop to (\d+)')
COLS = (('arm', 'fire', 'realisation') + tuple('pick%d' % k for k in COUNTS) + tuple('decl%d' % k for k in COUNTS)
        + ('alive', 'silent') + tuple('best%d' % k for k in COUNTS) + tuple('t%d' % t for t in EDGES[:-1])
        + tuple('img%d' % i for i in range(1, 13)))


def agent_lines(path):
    """The last B line of each node: picks and values per state, declarations
    per count, frames that proved a declared neighbour alive, silent neighbours."""
    last = {}
    for line in open(path, errors='ignore'):
        if line.startswith('Q ') and line.split(None, 3)[2:3] and ' B ' in line[:24]:
            p = line.split()
            last[p[2]] = p
    return last.values()


def agent(path):
    pick, decl, best = [0] * 4, [0] * 4, [0] * 4
    alive = silent = 0
    lines = list(agent_lines(path))
    if not lines:
        return [''] * 14
    for p in lines:
        cells = [c.split(':') for c in p[4:12]]
        for s in (0, 1):
            n = [int(c[0]) for c in cells[4 * s:4 * s + 4]]
            q = [float(c[1]) for c in cells[4 * s:4 * s + 4]]
            for k in range(4):
                pick[k] += n[k]
            if sum(n):
                best[q.index(max(q))] += 1
        i = p.index('decl')
        for k in range(4):
            decl[k] += int(p[i + 1 + k])
        alive += int(p[p.index('alive') + 1])
        silent += int(p[p.index('silent') + 1])
    return pick + decl + [alive, silent] + best


def over_time(path):
    # The periodic neighbour purge fires on whole seconds; a run of failures does not.
    bins = [0] * (len(EDGES) - 1)
    for line in open(path, errors='ignore'):
        m = DEATH.match(line)
        if m:
            t = float(m[1])
            if t >= 60 and abs(t - round(t)) >= 1e-6:
                bins[max(i for i, e in enumerate(EDGES[:-1]) if t >= e)] += 1
    return bins


def images(rdir):
    f = glob.glob(os.path.join(rdir, '*', 'sim', 'rt-frame.dat'))[0]
    v = [line.split('\t')[1] for line in open(f) if line.strip() and not line.startswith('#')]
    return v[:12] + [''] * (12 - len(v))


def main():
    rows = []
    for suffix, arm in ARMS:
        for fire, label in FIRES:
            for r in sorted(glob.glob(os.path.join(SETS, BASE + fire + '-' + suffix, 'r[0-9][0-9]'))):
                tr = glob.glob(os.path.join(r, '*', 'aomdv_qpv.tr'))
                if tr:
                    rows.append([arm, label, os.path.basename(r)] + agent(tr[0]) + over_time(tr[0]) + images(r))
    with open(OUT, 'w') as f:
        f.write('# compiled by tools/compile_break_agent.py from the traces of experiments/sets (GOP 9-3)\n')
        f.write('\t'.join(COLS) + '\n')
        for row in rows:
            f.write('\t'.join(str(x) for x in row) + '\n')
    print('%s: %d simulations' % (os.path.relpath(OUT), len(rows)))


if __name__ == '__main__':
    main()
