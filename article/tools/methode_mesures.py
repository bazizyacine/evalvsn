#!/usr/bin/env python3
"""The numbers the methods of the Sensors revision quote from the sets rather
than from src/data: compiled here into one TSV so that no number is read by hand.

  load      simulated time and nodes out of service (destroyed or failed) before
            the end of the simulation, per alarm period and fire, rules stack (E5, load)
  volume    simulated time and nodes out of service before the end at equal
            mean packet rate (E4), and at T = 9 s for the same GOP (E0)
  worst     camera-sink connectivity if every node failed TF = 60 s before its
            front, with the oracle's own link budget, heated zone included (E0)
  corr      correlation of each loss class with the PSNR (study 09)
  frames    mean frame size by type and reference PSNR range of the fire video
            per GOP, from the encodings
  calib     loss of the rules stack per fire and GOP (study 09)
  packets   packets of the fire video per GOP (E0)
  calendar  failure schedules per seed over the 720 runs of E0
  videos    packets of the other videos of E5 (scenario)
  variants  nodes out of service before the end per scenario variant of E5 and fire,
            rules stack
  topo      links above the radio floor, fewest hops from camera to sink and
            link margins of the 40 m grid and of the 30 m grid of E5 (scenario)

Usage: python3 tools/methode_mesures.py [OUT.tsv]   (default: src/data/methode-mesures.tsv)
"""
import csv
import glob
import math
import os
import re
import statistics as st
import sys
from collections import deque

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SETS = os.path.join(ROOT, 'experiments', 'sets')
DATA = os.path.join(HERE, '..', 'src', 'data', 'comparative.tsv')
sys.path.insert(0, os.path.join(ROOT, 'experiments', 'metrics'))
import fire_oracle as fo  # noqa: E402

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'src', 'data', 'methode-mesures.tsv')
FIRES = (('fire', 'F1'), ('fire2', 'F2'), ('fire3', 'F3'))
GC = (('0p01597', '3-9'), ('0p02525', '6-6'), ('0p03', '9-3'))
rows = []


def add(kind, key, value, source):
    # a file under experiments/sets is named from there: an absolute path is this machine's
    if os.path.isabs(source):
        source = os.path.relpath(source, SETS)
    rows.append((kind, key, value, source))


def fire_runs(pattern):
    return sorted(glob.glob(os.path.join(SETS, pattern, 'r[0-9][0-9]', '*', 'fire-nodes.dat')))


# load: simulated time and nodes out of service before the end, per T and fire
for per in ('p7', 'p9', 'p12', 'p18', 'p30'):
    for f, F in FIRES:
        name = 'set_AOMDV_QPV_GC-PN-0p03-13_%s-n100-d40-%s-rules%s' % (per, f, '' if per == 'p9' else '-s10')
        runs = fire_runs(name)
        simts, out = set(), []
        for path in runs:
            params, nodes = fo.read_fire_nodes(path)
            simt = float(params['simt'])
            simts.add(simt)
            out.append(sum(1 for nd in nodes if nd[4] < simt))
        add('load', '%s %s simt' % (per, F), '/'.join('%g' % s for s in sorted(simts)), name)
        add('load', '%s %s hors-service' % (per, F), '%.1f' % st.mean(out), '%s, %d runs' % (name, len(runs)))

# volume: simulated time and nodes out of service before the end at equal mean
# packet rate (E4, T = 9.32 and 9.65 s), against the same GOP at T = 9 s (E0)
for gc, g, per in (('0p02525', '6M6D', 'p9r32'), ('0p01597', '3M9D', 'p9r65')):
    for tag, suffix in (('p9', ''), (per, '-s10')):
        for f, F in FIRES:
            name = 'set_AOMDV_QPV_GC-PN-%s-13_%s-n100-d40-%s-rules%s' % (gc, tag, f, suffix)
            runs = fire_runs(name)
            simts, out = set(), []
            for path in runs:
                params, nodes = fo.read_fire_nodes(path)
                simt = float(params['simt'])
                simts.add(simt)
                out.append(sum(1 for nd in nodes if nd[4] < simt))
            add('volume', '%s %s %s simt' % (g, tag, F), '/'.join('%g' % s for s in sorted(simts)), name)
            add('volume', '%s %s %s hors-service' % (g, tag, F), '%.1f' % st.mean(out), '%s, %d runs' % (name, len(runs)))

# worst: every node fails TF = 60 s before its front
for f, F in FIRES:
    for gc, g in GC:
        path = fire_runs('set_AOMDV_QPV_GC-PN-%s-13_p9-n100-d40-%s-rules' % (gc, f))[0]
        params, nodes = fo.read_fire_nodes(path)
        simt = float(params['simt'])
        worst = [(i, x, y, fr, min(fr - 60.0, d)) for (i, x, y, fr, d) in nodes]
        o = fo.Oracle(params, worst)
        t, cut = 0.0, None
        while t <= simt + 1e-9:
            if not o.reachable(t):
                cut = t
                break
            t += fo.GRID
        add('worst', '%s %s' % (F, g), 'connected to %g s' % simt if cut is None else 'cut at %.1f s' % cut, path)

# corr and calib, from comparative.tsv
data = [r for r in csv.DictReader((l for l in open(DATA) if not l.startswith('#')), delimiter='\t')]


def corr(a, b):
    ma, mb = st.mean(a), st.mean(b)
    return sum((x - ma) * (y - mb) for x, y in zip(a, b)) / math.sqrt(
        sum((x - ma) ** 2 for x in a) * sum((y - mb) ** 2 for y in b))


for label, sub in (('720', data), ('540 cross-layer', [r for r in data if r['stack'] != 'aomdv'])):
    ps = [float(r['psnr']) for r in sub]
    for k in ('m1loss', 'm1to3loss', 'mloss', 'dloss', 'loss'):
        add('corr', '%s %s' % (label, k), '%.2f' % corr([float(r[k]) for r in sub], ps), 'comparative.tsv')
for _, g in GC:
    sp = {'3-9': '3M9D', '6-6': '6M6D', '9-3': '9M3D'}[g]
    for _, F in FIRES:
        v = st.mean(float(r['loss']) for r in data if r['stack'] == 'rules' and r['split'] == sp and r['fire'] == F)
        add('calib', 'rules %s %s loss' % (g, F), '%.2f' % v, 'comparative.tsv')
    for stack in ('aomdv',):
        for k in ('m1loss', 'mloss', 'dloss'):
            v = st.mean(float(r[k]) for r in data if r['stack'] == stack and r['split'] == sp)
            add('calib', '%s %s %s' % (stack, g, k), '%.1f' % v, 'comparative.tsv')

# frames: sizes by type and reference PSNR, fire video
for gc, g in GC:
    path = glob.glob(os.path.join(SETS, 'set_AOMDV_QPV_GC-PN-%s-13_p9-n100-d40-fire-rules' % gc, 'encoding', '*', 'st-frame.dat'))[0]
    size = {'M': [], 'D': []}
    ref = []
    for l in open(path):
        if l.startswith('#') or not l.strip():
            continue
        v = l.rstrip('\n').split('\t')
        size[v[1]].append(int(v[2]))
        ref.append(float(v[3]))
    add('frames', '%s M' % g, '%d frames, %.0f bytes' % (len(size['M']), st.mean(size['M'])), path)
    add('frames', '%s D' % g, '%d frames, %.0f bytes' % (len(size['D']), st.mean(size['D'])), path)
    add('frames', '%s refPSNR' % g, '%.1f to %.1f dB' % (min(ref), max(ref)), path)

# packets: packets of the fire video per GOP, from the encodings of E0
for gc, g in GC:
    path = glob.glob(os.path.join(SETS, 'set_AOMDV_QPV_GC-PN-%s-13_p9-n100-d40-fire-rules' % gc, 'encoding', '*', 'st-priority.dat'))[0]
    n = sum(1 for l in open(path) if l.strip() and not l.startswith('#'))
    add('packets', g, '%d packets' % n, path)

# calendar: failure schedules (front and failure instant of every node) per
# seed (fire, realisation) over the four stacks and three GOPs of E0
cals, nruns = {}, 0
for f, F in FIRES:
    for gc, g in GC:
        for stack in ('AOMDV_GC-PN-%s-13_p9-n100-d40-%s-pq', 'AOMDV_QPV_GC-PN-%s-13_p9-n100-d40-%s-rules',
                      'AOMDV_QPV_GC-PN-%s-13_p9-n100-d40-%s-hybrid', 'AOMDV_QPV_GC-PN-%s-13_p9-n100-d40-%s-agents'):
            for path in fire_runs('set_' + stack % (gc, f)):
                r = os.path.basename(os.path.dirname(os.path.dirname(path)))
                params, nodes = fo.read_fire_nodes(path)
                cals.setdefault((F, r), set()).add(tuple((nd[0], nd[3], nd[4]) for nd in nodes))
                nruns += 1
add('calendar', 'E0', '%d seeds over %d runs, at most %d schedule(s) per seed'
    % (len(cals), nruns, max(len(v) for v in cals.values())),
    'set_*_p9-n100-d40-{fire,fire2,fire3}-{pq,rules,hybrid,agents}, fire-nodes.dat')

# videos: packets of the other videos of E5 (scenario)
for video, gc in (('forest', '0p01597'), ('smoke', '0p15671')):
    path = glob.glob(os.path.join(SETS, 'set_AOMDV_QPV_GC-PN-%s-13_p9-n100-d40-fire-rules-%s-s10' % (gc, video), 'encoding', '*', 'st-priority.dat'))[0]
    n = sum(1 for l in open(path) if l.strip() and not l.startswith('#'))
    frames = glob.glob(os.path.join(os.path.dirname(path), 'st-frame.dat'))[0]
    types = [l.split('\t')[1] for l in open(frames) if l.strip() and not l.startswith('#')]
    add('videos', video, '%d packets, %d M and %d D frames' % (n, types.count('M'), types.count('D')), path)

# variants: nodes out of service before the end, per scenario variant of E5 and fire, rules stack
for suffix, d in (('j8', '30'), ('v15', '40')):
    for f, F in FIRES:
        name = 'set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d%s-%s-rules-%s-s10' % (d, f, suffix)
        runs = fire_runs(name)
        out = []
        for path in runs:
            params, nodes = fo.read_fire_nodes(path)
            out.append(sum(1 for nd in nodes if nd[4] < float(params['simt'])))
        add('variants', '%s %s hors-service' % (suffix, F), '%.1f' % st.mean(out), '%s, %d runs' % (name, len(runs)))


# topo: links above the floor at full power (the oracle's budget, no fire), by
# grid offset, and the fewest hops from camera to sink
def hops(adj, s, t):
    seen, q = {s: 0}, deque([s])
    while q:
        a = q.popleft()
        for b in adj[a]:
            if b not in seen:
                seen[b] = seen[a] + 1
                q.append(b)
    return seen.get(t)


for label, pattern in (('40 m', 'set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-fire-rules'),
                       ('30 m', 'set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d30-fire-rules-j8-s10')):
    files = fire_runs(pattern.replace('AOMDV_QPV_', 'AOMDV*_').replace('-fire-', '-fire*-').replace('-rules', '-*', 1))
    deployments = {tuple((x, y) for _, x, y, _, _ in fo.read_fire_nodes(f)[1]) for f in files}
    params, nodes = fo.read_fire_nodes(files[0])
    orc = fo.Oracle(params, nodes)
    n = len(nodes)
    side = int(round(math.sqrt(n)))
    adj = [[b for b in range(n) if b != a and orc.margin0 - orc.pathloss[a][b] >= 0] for a in range(n)]
    cls = {}
    for a in range(n):
        for b in adj[a]:
            if a < b:
                k = tuple(sorted((abs(a // side - b // side), abs(a % side - b % side))))
                cls[k] = cls.get(k, 0) + 1
    add('topo', '%s deployments' % label, '%d over %d runs' % (len(deployments), len(files)), pattern)
    add('topo', '%s grid links' % label, '%d of %d' % (cls.get((0, 1), 0), 2 * side * (side - 1)), pattern)
    add('topo', '%s diagonal links' % label, '%d of %d' % (cls.get((1, 1), 0), 2 * (side - 1) ** 2), pattern)
    add('topo', '%s two-step links' % label, '%d' % cls.get((0, 2), 0), pattern)
    add('topo', '%s fewest hops' % label, '%d' % hops(adj, orc.camera, orc.sink), pattern)
    step = float(label.split()[0])
    add('topo', '%s largest displacement' % label, '%.2f m on x, %.2f m on y' % (
        max(abs(x - (i // side) * step) for i, x, _, _, _ in nodes),
        max(abs(y - (i % side) * step) for i, _, y, _, _ in nodes)), pattern)
beta = float(params['plexp'])
for d in (30.0, 40.0, 30.0 * math.sqrt(2)):
    add('topo', 'margin %.1f m link' % d, '%+.1f dB' % (orc.margin0 - 10 * beta * math.log10(d)), 'oracle link budget')
add('topo', '30 m against 40 m', '%.1f dB stronger' % (10 * beta * math.log10(40.0 / 30.0)), 'oracle link budget')

with open(OUT, 'w') as f:
    f.write('# compiled by methode_mesures.py from experiments/sets and src/data/comparative.tsv\n')
    f.write('kind\tkey\tvalue\tsource\n')
    for r in rows:
        f.write('\t'.join(r) + '\n')
for r in rows:
    print('%-7s %-32s %s' % r[:3])
