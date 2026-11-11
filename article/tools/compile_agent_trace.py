#!/usr/bin/env python3
"""Compile the agents' traces of study 10 into src/data/agent-trace.tsv: for the
hybrid and agents stacks at GOP 9-3, each agent, each 20 s snapshot of the 60
traced simulations and each node's last snapshot (t = 999) give the nodes that decided, the decisions, the median per
deciding node, the share of node-states whose best action left the initial one,
and the decisions per action. Means are per simulation. The power agent writes
counts only, so it has no share. The traces are purged at the end of the
campaign; this file keeps what the text needs from them.

Usage:
    python3 tools/compile_agent_trace.py
"""
import collections
import glob
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
SETS = os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'sets'))
OUT = os.path.join(HERE, '..', 'src', 'data', 'agent-trace.tsv')
BASE = 'set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40-'
ARMS = (('hybrid-tr-s10', 'hybrid'), ('agents-tr-s10', 'agents'))
FIRES = ('fire', 'fire2', 'fire3')
SHAPE = {'P': (3, 5), 'C': (3, 4), 'B': (2, 4), 'D': (2, 3), 'E': (1, 5), 'A0': (3, 9), 'A1': (3, 9), 'A2': (3, 9)}
NAME = {'P': 'path', 'C': 'congestion', 'B': 'break', 'D': 'discovery', 'E': 'discard', 'A': 'access', 'T': 'power'}
ACTIONS = 9
END = 999  # the row of each node's last snapshot


def snapshots(path):
    """(t, node, line kind, [(counts of a state, values of a state)])."""
    for line in open(path, errors='ignore'):
        if not line.startswith('Q '):
            continue
        p = line.split()
        if p[3] == 'T':
            yield float(p[1]), p[2], 'T', [([int(x) for x in p[4:10]], None)]
            continue
        states, actions = SHAPE[p[3]]
        cells = [c.split(':') for c in p[4:4 + states * actions]]
        yield float(p[1]), p[2], p[3], [([int(c[0]) for c in cells[s * actions:(s + 1) * actions]],
                                         [float(c[1]) for c in cells[s * actions:(s + 1) * actions]])
                                        for s in range(states)]


def best(values):
    return values.index(max(values))


def main():
    rows = []
    for suffix, arm in ARMS:
        runs = 0
        decisions = collections.Counter()
        per_action = collections.defaultdict(lambda: [0] * ACTIONS)
        per_node = collections.defaultdict(list)
        changed = collections.Counter()
        seen = collections.Counter()
        for fire in FIRES:
            for tr in sorted(glob.glob(os.path.join(SETS, BASE + fire + '-' + suffix, 'r[0-9][0-9]', '*', 'aomdv_qpv.tr'))):
                runs += 1
                initial = {}
                total = collections.Counter()
                last = {}

                def tally(key, node, kind, states):
                    for s, (n, q) in enumerate(states):
                        decisions[key] += sum(n)
                        for a, c in enumerate(n):
                            per_action[key][a] += c
                        total[key + (node,)] += sum(n)
                        if q is None:
                            continue
                        first = initial.setdefault((node, kind, s), best(q))
                        if sum(n):
                            seen[key] += 1
                            changed[key] += best(q) != first

                for t, node, kind, states in snapshots(tr):
                    tally((NAME[kind[0]], int(t // 20) * 20), node, kind, states)
                    last[(node, kind)] = states
                # A burnt node stops writing: its last line is its total.
                for (node, kind), states in last.items():
                    tally((NAME[kind[0]], END), node, kind, states)
                for (agent, t, node), n in total.items():
                    if n:
                        per_node[(agent, t)].append(n)
        for key in sorted(decisions):
            nodes = per_node[key]
            rows.append([arm, key[0], key[1], '%.1f' % (len(nodes) / runs), '%.1f' % (decisions[key] / runs),
                         '%g' % statistics.median(nodes) if nodes else 0,
                         '%.1f' % (100.0 * changed[key] / seen[key]) if seen[key] else '']
                        + ['%.1f' % (c / runs) for c in per_action[key]])
    with open(OUT, 'w') as f:
        f.write('# compiled by tools/compile_agent_trace.py from the traced sets of study 10 (GOP 9-3, 60 simulations an arm)\n')
        f.write('\t'.join(('arm', 'agent', 't', 'nodes', 'decisions', 'median', 'changed') + tuple('a%d' % a for a in range(ACTIONS))) + '\n')
        for r in rows:
            f.write('\t'.join(str(x) for x in r) + '\n')
    print('%s: %d rows' % (os.path.relpath(OUT), len(rows)))


if __name__ == '__main__':
    main()
