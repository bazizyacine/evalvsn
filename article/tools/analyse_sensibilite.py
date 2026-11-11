#!/usr/bin/env python3
"""Sensitivity of the fire scenario (study 10, tranche T4, experiment E5, fire series)
from src/data/revision.tsv, GOP 9-3, alarm period 9 s, 40 m grid.

Seven variants, each changing one element of the reference scenario
(AZ = 1 dB/m, PF = 0.5, failure instant uniform in the TF = 60 s before the
front), for the rule and hybrid stacks, three fires, the 60 seeds of study 09:

  az015, az05, az165   heated-zone attenuation AZ = 0.15, 0.5, 1.65 dB/m
  pf0, pf025, pf075    early-failure probability PF = 0, 0.25, 0.75
  fd1                  failure instant TF * u^2 before the front instead of
                       TF * u (u uniform): closer to the front

Every contrast pairs the 60 seeds (fire, realisation):
  gain       hybrid-<v> - rules-<v>: the hybrid stack's lead under variant v;
             Holm over the seven variants
  shift      <stack>-<v> - <stack>: the variant's effect on one stack; Holm
             over the seven variants of that stack
  change     (hybrid-<v> - rules-<v>) - (hybrid - rules): how much the lead
             moves with the variant; Holm over the seven variants

Usage: python3 tools/analyse_sensibilite.py [--json OUT]
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'metrics')))
import stats  # noqa: E402

DATA = os.path.join(HERE, '..', 'src', 'data', 'revision.tsv')
SEEDS = 60
VARIANTS = ('az015', 'az05', 'az165', 'pf0', 'pf025', 'pf075', 'fd1')
METRICS = ('psnr', 'm1loss', 'loss', 'usable', 'declared', 'false_declared')


def table():
    lines = [l for l in open(DATA) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:]]


REV = table()


def values(arm, metric):
    return {(r['fire'], r['realisation']): float(r[metric]) for r in REV
            if r['arm'] == arm and r['split'] == '9M3D' and r['period'] == 'p9' and r['dist'] == '40'
            and r[metric] != ''}


def arm(stack, v):
    return stack if v is None else '%s-%s' % (stack, v)


def summary(d):
    n = len(d)
    m = sum(d) / n
    s = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1))
    if s == 0:
        return dict(n=n, mean=m, lo=m, hi=m, p=1.0)
    se = s / math.sqrt(n)
    h = stats.t_quantile(0.95, n - 1) * se
    return dict(n=n, mean=m, lo=m - h, hi=m + h, p=stats.t_sf2(m / se, n - 1))


def diff(a, b):
    return {k: a[k] - b[k] for k in a if k in b}


def contrast(kind, v, metric):
    if kind == 'gain':
        return diff(values(arm('hybrid', v), metric), values(arm('rules', v), metric))
    if kind in ('shift-rules', 'shift-hybrid'):
        s = kind.split('-')[1]
        return diff(values(arm(s, v), metric), values(s, metric))
    if kind == 'change':
        g = contrast('gain', v, metric)
        g0 = contrast('gain', None, metric)
        return diff(g, g0)
    raise ValueError(kind)


def family(kind, metric):
    out = {}
    for v in VARIANTS:
        d = contrast(kind, v, metric)
        out[v] = summary(list(d.values())) if len(d) == SEEDS else None
    full = [v for v in VARIANTS if out[v]]
    adj = stats.holm([(v, '', out[v]['p']) for v in full])
    for v, _, _, p in adj:
        out[v]['holm'] = p
    return out


def means():
    out = {}
    for s in ('rules', 'hybrid'):
        for v in (None,) + VARIANTS:
            a = arm(s, v)
            out[a] = {}
            for m in METRICS:
                x = values(a, m)
                out[a][m] = sum(x.values()) / len(x) if len(x) == SEEDS else None
    return out


def main():
    res = {'means': means(), 'families': {}}
    for kind in ('gain', 'shift-rules', 'shift-hybrid', 'change'):
        res['families'][kind] = {m: family(kind, m) for m in METRICS}
    res['gain_reference'] = {m: summary(list(contrast('gain', None, m).values())) for m in METRICS}
    if '--json' in sys.argv:
        json.dump(res, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=1)
    print('Means over the 60 seeds, GOP 9-3')
    print('  %-14s' % 'arm' + ''.join('%10s' % m[:9] for m in METRICS))
    for a, row in res['means'].items():
        print('  %-14s' % a + ''.join('%10s' % ('%.2f' % row[m] if row[m] is not None else '--') for m in METRICS))
    g0 = res['gain_reference']['psnr']
    print('\nReference gain hybrid - rules, PSNR: %+.2f [%+.2f, %+.2f] p=%.3g' % (g0['mean'], g0['lo'], g0['hi'], g0['p']))
    for kind in ('gain', 'shift-rules', 'shift-hybrid', 'change'):
        for m in ('psnr', 'm1loss', 'usable', 'declared'):
            print('\n%s, %s' % (kind, m))
            for v in VARIANTS:
                t = res['families'][kind][m][v]
                print('  %-6s n=%d %+7.2f [%+6.2f, %+6.2f] p=%-9.3g Holm %.3g'
                      % (v, t['n'], t['mean'], t['lo'], t['hi'], t['p'], t['holm']))


if __name__ == '__main__':
    main()
