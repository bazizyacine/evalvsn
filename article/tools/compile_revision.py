#!/usr/bin/env python3
"""Compile the simulations of studies 09 and 10 into src/data/revision.tsv, one
row per simulation, with what comparative.tsv does not hold: the quality seen by
an operator, the delay, the neighbour-death declarations and the control traffic.
The usable images are counted at 28 dB, and at 25 and 31 dB, the bounds of the
range EvalVid calls fair, to check that the ranking does not hang on the threshold.

A set still running is left out. A column that a slimmed set no longer provides
stays empty. The false declarations follow metrics/fire_oracle.py: the neighbour
was alive and the link above the floor at the instant it was declared dead.

The sets of the quality coefficient 0.1 (ipFiles/GC-QC-0p1.dat, decisions d82
to d84) take the arm of their stack with the suffix -q01, at GOP 9-3; they are
compiled after the others, so that their rows follow. Only the sets of tranche
10 (-t168-s10) are read: they simulate the 168 s of their study 09
counterparts, whereas those of tranche 9 (-s10) stopped at 165 s.

Usage:
    python3 tools/compile_revision.py
"""
import csv
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
EXP = os.path.normpath(os.path.join(HERE, '..', '..', 'experiments'))
SETS = os.path.join(EXP, 'sets')
sys.path.insert(0, os.path.join(EXP, 'metrics'))
import fire_oracle  # noqa: E402

OUT = os.path.join(HERE, '..', 'src', 'data', 'revision.tsv')
NAME = re.compile(r'set_(AOMDV_QPV|AOMDV)_GC-PN-(0p\d+)-13_(p[0-9r]+)-n100-d(\d+)-(fire[23]?)-(.+?)(?:-s10)?$')
QUALITY = re.compile(r'set_(AOMDV_QPV|AOMDV)_GC-QC-0p1_(p9)-n100-d(40)-(fire[23]?)-(rules|hybrid|agents|pq)-t168-s10$')
SPLIT = {'0p03': '9M3D', '0p02525': '6M6D', '0p01597': '3M9D'}
FIRE = {'fire': 'F1', 'fire2': 'F2', 'fire3': 'F3'}
FLOOR_DB = 28.0
DEATH = re.compile(r'^A (\d+\.\d+) _(\d+)_ deleting LL hop to (\d+)')
COLS = ('arm', 'fire', 'split', 'period', 'dist', 'realisation', 'psnr', 'ssim', 'loss', 'm1loss', 'usable',
        'delay_mean', 'delay_p95', 'late', 'declared', 'false_declared', 'rreq_all', 'rerr', 'rrep', 'hello',
        'usable25', 'usable31')


def metrics(rdir):
    mt = glob.glob(os.path.join(rdir, '*', 'sim', 'mt.dat'))
    if not mt:
        return None
    lines = open(mt[0]).read().split('\n')
    head = lines[0].lstrip('#').split()
    return dict(zip(head, lines[1].split()))


def usable(rdir, floor=FLOOR_DB):
    n = 0
    for f in glob.glob(os.path.join(rdir, '*', 'sim', 'rt-frame.dat')):
        for line in open(f):
            if line.startswith('#') or not line.strip():
                continue
            if float(line.split('\t')[1]) >= floor:
                n += 1
    return n


def packets(path):
    out = {}
    for line in open(path, errors='ignore'):
        if line.startswith('#') or not line.strip():
            continue
        v = line.split('\t')
        out[int(v[1])] = (float(v[0]), int(v[3]))
    return out


def timing(rdir):
    """Mean and 95th percentile of the delay, and the share of the received
    packets that arrive after the next image was captured."""
    st = glob.glob(os.path.join(rdir, '*', 'ns2_st-packet.dat')) or glob.glob(os.path.join(rdir, '*', 'st-packet.dat'))
    rt = glob.glob(os.path.join(rdir, '*', 'sim', 'rt-packet.dat')) or glob.glob(os.path.join(rdir, '*', 'rt-packet.dat'))
    if not st or not rt:
        return '', '', ''
    sent, got = packets(st[0]), packets(rt[0])
    first = {}
    for t, f in sent.values():
        first[f] = min(t, first.get(f, t))
    frames = sorted(first)
    period = (first[frames[-1]] - first[frames[0]]) / (len(frames) - 1) if len(frames) > 1 else 0.0
    delays, late = [], 0
    for seq, (t, _) in got.items():
        if seq not in sent:
            continue
        ts, f = sent[seq]
        delays.append(t - ts)
        if t > first.get(f + 1, first[f] + period):
            late += 1
    if not delays:
        return '', '', ''
    delays.sort()
    return (sum(delays) / len(delays), delays[int(0.95 * (len(delays) - 1))], 100.0 * late / len(delays))


def declarations(rdir):
    tr = glob.glob(os.path.join(rdir, '*', 'aomdv_qpv.tr'))
    fn = glob.glob(os.path.join(rdir, '*', 'fire-nodes.dat'))
    if not tr or not fn:
        return '', ''
    params, nodes = fire_oracle.read_fire_nodes(fn[0])
    orc = fire_oracle.Oracle(params, nodes)
    n = false = 0
    for line in open(tr[0], errors='ignore'):
        m = DEATH.match(line)
        if not m:
            continue
        t, a, b = float(m[1]), int(m[2]), int(m[3])
        # The periodic neighbour purge fires on whole seconds; a run of failures does not.
        if t < 60 or abs(t - round(t)) < 1e-6:
            continue
        n += 1
        if nodes[a][4] > t and nodes[b][4] > t:
            loss = orc.pathloss[a][b] + orc.alpha * orc.dist[a][b] * 0.5 * (orc.depth(a, t) + orc.depth(b, t))
            false += orc.margin0 - loss >= 0
    return n, false


def events(name):
    for path in (os.path.join(SETS, name, 'events.tsv'),
                 os.path.join(EXP, 'studies', '09-comparative', 'results', 'routing.tsv')):
        if os.path.exists(path):
            rows = [r for r in csv.DictReader((l for l in open(path) if not l.startswith('#')), delimiter='\t')
                    if r['set'] == name]
            if rows:
                return {r['realisation']: r for r in rows}
    return {}


def main():
    names = sorted(os.path.basename(p) for p in glob.glob(os.path.join(SETS, 'set_*_p*-fire*')))
    names.sort(key=lambda n: QUALITY.match(n) is not None)
    rows = []
    for name in names:
        m, q = NAME.match(name), QUALITY.match(name)
        if not (m or q) or not os.path.exists(os.path.join(SETS, name, 'graphs', 'PSNR', 'PSNR.dat')):
            continue
        if q:
            _, period, dist, fire, stack = q.groups()
            gc, arm = '0p03', stack + '-q01'
        else:
            _, gc, period, dist, fire, arm = m.groups()
        if period == 'p9' and dist == '40' and arm not in ('rules', 'agents', 'hybrid', 'pq') \
                and not name.endswith(('-s10', '-frozen')):
            continue
        if period != 'p9' and not name.endswith('-s10'):
            continue
        ev = events(name)
        split = '9M3D' if ('-forest' in arm or '-smoke' in arm) else SPLIT[gc]
        for r in sorted(glob.glob(os.path.join(SETS, name, 'r[0-9][0-9]'))):
            real = os.path.basename(r)
            mt = metrics(r)
            if mt is None:
                continue
            dm, dp, late = timing(r)
            dec, fal = declarations(r)
            e = ev.get(real, {})
            rows.append((arm, FIRE[fire], split, period, dist, real, mt['psnr'], mt['ssim'], mt['pktLoss'], mt['pktlossM1'],
                         usable(r), dm, dp, late, dec, fal, e.get('rreq_all', ''), e.get('rerr', ''),
                         e.get('rrep', ''), e.get('hello', ''), usable(r, 25.0), usable(r, 31.0)))
    # Written aside, then renamed: a reader never sees a half-written file.
    with open(OUT + '.tmp', 'w') as f:
        f.write('# compiled by tools/compile_revision.py from experiments/sets (studies 09 and 10)\n')
        f.write('\t'.join(COLS) + '\n')
        for row in rows:
            f.write('\t'.join('%.4f' % x if isinstance(x, float) else str(x) for x in row) + '\n')
    os.replace(OUT + '.tmp', OUT)
    print('%s: %d simulations' % (os.path.relpath(OUT), len(rows)))


if __name__ == '__main__':
    main()
