#!/usr/bin/env python3
"""Connectivity oracle of the fire scenario (decisions d23 and d24, 10 Oct 2026).

Splits the video packets a run lost into recoverable and unrecoverable ones.
A lost packet is recoverable when, at the instant it left the camera, a path
of live nodes joined the camera to the sink, each hop received above the
radio's floor at full power: some routing, MAC or power decision could have
delivered it. A packet lost while the fire had cut every such path is beyond
any mechanism's reach.

The link budget replays the simulator's own: ns-2's log-normal shadowing
(mobile/shadowing.cc) over Friis at the reference distance, the floor
max(CSThresh_, RXThresh_) under which WirelessPhy::sendUp() drops a frame,
and the heated zone of mac/fire-propagationEvalVSN.cc. With the campaign's
shadowing deviation of 0 the budget is exact; with a non-zero deviation the
oracle reads the mean path loss and says so. evalvsn.tcl writes the inputs
in fire-nodes.dat: the radio constants, then each node's position, front
arrival and death time.

Usage:
  fire_oracle.py RUN_DIR [RUN_DIR ...]     one line per realisation
  fire_oracle.py --set SET_DIR [...]        the realisations of a set, and their mean

A RUN_DIR is any directory holding, somewhere below it, fire-nodes.dat,
st-packet.dat (or ns2_st-packet.dat) and the sink's rt-packet.dat.
"""
import glob
import math
import os
import sys
from collections import deque

SPEED_OF_LIGHT = 300000000.0  # ns-2's constant (mac/wireless-phy.h)
GRID = 0.1                     # seconds between two connectivity snapshots


def read_fire_nodes(path):
    params, nodes = {}, []
    with open(path) as f:
        for line in f:
            if line.startswith('#'):
                tok = line[1:].split()
                if tok and tok[0] in ('Pt_', 'prop'):
                    for k, v in zip(tok[0::2], tok[1::2]):
                        params[k] = v
                continue
            if line.strip():
                i, x, y, front, death = line.split()
                nodes.append((int(i), float(x), float(y), float(front), float(death)))
    nodes.sort()
    return params, nodes


def read_packets(path):
    """seqNb -> (time, frameType, prioNb) from an evsn trace."""
    out = {}
    with open(path) as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            v = line.split('\t')
            out[int(v[1])] = (float(v[0]), v[4], int(v[5]))
    return out


class Oracle:
    def __init__(self, params, nodes):
        self.nodes = nodes
        pt = float(params['Pt_'])
        L = float(params['L_'])
        lam = SPEED_OF_LIGHT / float(params['freq_'])
        gt, gr = float(params['Gt_']), float(params['Gr_'])
        self.d0 = float(params.get('dist0', 1.0))
        self.beta = float(params['plexp'])
        self.std = float(params.get('pstd', 0))
        self.alpha = float(params.get('firealpha', 0))
        self.zone = float(params.get('firezone', 0))
        self.camera = int(params.get('camera', 0))
        self.sink = int(params['sink'])
        floor = max(float(params['CSThresh_']), float(params['RXThresh_']))
        pr0 = pt * gt * gr * lam * lam / ((4 * math.pi) ** 2 * self.d0 ** 2 * L)
        self.margin0 = 10 * math.log10(pr0 / floor)  # dB above the floor at d0
        if 'TwoRay' in params.get('prop', ''):
            raise SystemExit('fire_oracle: two-ray ground is not modelled, only shadowing')
        n = len(nodes)
        self.dist = [[0.0] * n for _ in range(n)]
        self.pathloss = [[0.0] * n for _ in range(n)]
        for a in range(n):
            for b in range(n):
                if a != b:
                    d = math.hypot(nodes[a][1] - nodes[b][1], nodes[a][2] - nodes[b][2])
                    self.dist[a][b] = d
                    self.pathloss[a][b] = 10 * self.beta * math.log10(d / self.d0)
        self.cache = {}

    def depth(self, i, t):
        if self.alpha <= 0 or self.zone <= 0:
            return 0.0
        tau = self.nodes[i][3] - t
        if tau >= self.zone:
            return 0.0
        if tau <= 0:
            return 1.0
        return 1.0 - tau / self.zone

    def reachable(self, t):
        k = int(t / GRID)
        if k in self.cache:
            return self.cache[k]
        t = k * GRID
        alive = [nd[4] > t for nd in self.nodes]
        ok = False
        if alive[self.camera] and alive[self.sink]:
            n = len(self.nodes)
            dep = [self.depth(i, t) for i in range(n)]
            seen = [False] * n
            seen[self.camera] = True
            q = deque([self.camera])
            while q and not ok:
                a = q.popleft()
                for b in range(n):
                    if seen[b] or not alive[b] or a == b:
                        continue
                    loss = self.pathloss[a][b]
                    if dep[a] or dep[b]:
                        loss += self.alpha * self.dist[a][b] * 0.5 * (dep[a] + dep[b])
                    if self.margin0 - loss >= 0:
                        if b == self.sink:
                            ok = True
                            break
                        seen[b] = True
                        q.append(b)
        self.cache[k] = ok
        return ok


def find_one(run_dir, names):
    for name in names:
        hits = sorted(glob.glob(os.path.join(run_dir, '**', name), recursive=True))
        if hits:
            return hits[0]
    return None


def analyse(run_dir):
    fn = find_one(run_dir, ['fire-nodes.dat'])
    st = find_one(run_dir, ['ns2_st-packet.dat', 'st-packet.dat'])
    rt = find_one(run_dir, ['rt-packet.dat'])
    if not (fn and st and rt):
        return None
    params, nodes = read_fire_nodes(fn)
    orc = Oracle(params, nodes)
    sent = read_packets(st)
    got = read_packets(rt)
    r = dict(sent=0, lost=0, rec=0, unrec=0, sentM=0, lostM=0, recM=0, std=orc.std)
    for seq, (t, ftype, prio) in sent.items():
        crit = ftype == 'M'
        r['sent'] += 1
        r['sentM'] += crit
        if seq in got:
            continue
        r['lost'] += 1
        r['lostM'] += crit
        if orc.reachable(t):
            r['rec'] += 1
            r['recM'] += crit
        else:
            r['unrec'] += 1
    return r


def pct(a, b):
    return 100.0 * a / b if b else float('nan')


def line(label, r):
    return ('%-40s sent %5d  loss %5.1f %%  recoverable %5.1f %%  unrecoverable %5.1f %%  '
            'M loss %5.1f %% (recoverable %5.1f %%)' %
            (label, r['sent'], pct(r['lost'], r['sent']), pct(r['rec'], r['sent']),
             pct(r['unrec'], r['sent']), pct(r['lostM'], r['sentM']), pct(r['recM'], r['sentM'])))


def main(argv):
    if not argv:
        print(__doc__)
        return 2
    if argv[0] == '--set':
        for s in argv[1:]:
            rows = []
            for r in sorted(glob.glob(os.path.join(s, 'r[0-9][0-9]'))):
                res = analyse(r)
                if res:
                    rows.append(res)
                    print(line('  ' + os.path.basename(r), res))
            if rows:
                m = {k: sum(v) / len(v) for k, v in (
                    ('loss', [pct(x['lost'], x['sent']) for x in rows]),
                    ('rec', [pct(x['rec'], x['sent']) for x in rows]),
                    ('unrec', [pct(x['unrec'], x['sent']) for x in rows]),
                    ('lossM', [pct(x['lostM'], x['sentM']) for x in rows]),
                    ('recM', [pct(x['recM'], x['sentM']) for x in rows]))}
                print('%-40s %d realisations: loss %5.1f %%, recoverable %5.1f %%, unrecoverable %5.1f %%, '
                      'M loss %5.1f %% (recoverable %5.1f %%)' %
                      (os.path.basename(s.rstrip('/')), len(rows), m['loss'], m['rec'], m['unrec'],
                       m['lossM'], m['recM']))
                if rows[0]['std'] > 0:
                    print('  (shadowing deviation %.1f dB: the oracle reads the mean path loss)' % rows[0]['std'])
            else:
                print('%s: no realisation with fire-nodes.dat' % s)
        return 0
    for d in argv:
        res = analyse(d)
        print(line(d[-40:], res) if res else '%s: inputs missing' % d)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
