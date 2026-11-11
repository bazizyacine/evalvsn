#!/usr/bin/env python3
"""Generalisation of the comparison (study 10, tranche T5, experiment E5, scenario series)
from src/data/revision.tsv, GOP 9-3, alarm period 9 s.

Five variants of the reference scenario, each under the three fires, for the
four stacks (AOMDV, arm prefix pq; rules; hybrid; agents), the 60 seeds of
study 09:

  forest, smoke   two other videos, at the GOP coefficient that gives them nine
                  M frames out of twelve (0.01597 and 0.15671)
  j8              30 m grid (dist = 30), every node moved by at most 8 m on each
                  axis by one fixed draw, ignitions at 0.75 times their position
  swap            camera and sink exchanged, ignitions unchanged
  v15             head speed times 1.5 for the three fires, wind unchanged

The reference cells are the four stacks of study 09 at 9M3D, p9, 40 m.

Every contrast pairs the 60 seeds (fire, realisation):
  gap        <a>-<v> - <b>-<v>: the lead of stack a over stack b under variant
             v, for the five contrasts hybrid-rules, agents-rules, rules-pq,
             hybrid-pq and hybrid-agents (the two stacks differ only by the
             neighbour-death decision); Holm over the five variants within each
             contrast
  change     (<a>-<v> - <b>-<v>) - (<a> - <b>): how much the lead moves with
             the variant; Holm over the five variants within each contrast
  reference  <a> - <b> in the reference scenario, a single test

Usage: analyse_generalisation.py [--json OUT]
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
STACKS = ('pq', 'rules', 'hybrid', 'agents')
VARIANTS = ('forest', 'smoke', 'j8', 'swap', 'v15')
FIRES = ('F1', 'F2', 'F3')
CONTRASTS = {'hybrid-rules': ('hybrid', 'rules'), 'agents-rules': ('agents', 'rules'),
             'rules-pq': ('rules', 'pq'), 'hybrid-pq': ('hybrid', 'pq'), 'hybrid-agents': ('hybrid', 'agents')}
METRICS = ('psnr', 'ssim', 'loss', 'm1loss', 'usable', 'usable25', 'usable31',
           'declared', 'false_declared', 'rreq_all')


def table():
    lines = [l for l in open(DATA) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:]]


REV = table()


def arm(stack, v):
    return stack if v is None else '%s-%s' % (stack, v)


def dist(a):
    # The 30 m grid is the only variant whose sets carry d30.
    return '30' if a.endswith('-j8') else '40'


def values(a, metric, fire=None):
    return {(r['fire'], r['realisation']): float(r[metric]) for r in REV
            if r['arm'] == a and r['split'] == '9M3D' and r['period'] == 'p9' and r['dist'] == dist(a)
            and r[metric] != '' and (fire is None or r['fire'] == fire)}


def summary(d):
    n = len(d)
    m = sum(d) / n
    s = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1))
    if s == 0:
        return dict(n=n, mean=m, lo=m, hi=m, p=1.0 if m == 0 else 0.0)
    se = s / math.sqrt(n)
    h = stats.t_quantile(0.95, n - 1) * se
    return dict(n=n, mean=m, lo=m - h, hi=m + h, p=stats.t_sf2(m / se, n - 1))


def diff(a, b):
    return {k: a[k] - b[k] for k in a if k in b}


def gap(contrast, v, metric):
    a, b = CONTRASTS[contrast]
    return diff(values(arm(a, v), metric), values(arm(b, v), metric))


def change(contrast, v, metric):
    return diff(gap(contrast, v, metric), gap(contrast, None, metric))


def family(kind, contrast, metric):
    f = gap if kind == 'gap' else change
    out = {}
    for v in VARIANTS:
        d = f(contrast, v, metric)
        out[v] = summary(list(d.values())) if len(d) == SEEDS else None
    full = [v for v in VARIANTS if out[v]]
    if full:
        for v, _, _, p in stats.holm([(v, '', out[v]['p']) for v in full]):
            out[v]['holm'] = p
    return out


def reference(contrast, metric):
    d = gap(contrast, None, metric)
    return summary(list(d.values())) if len(d) == SEEDS else None


def mean_of(a, metric, fire=None):
    x = values(a, metric, fire)
    want = SEEDS if fire is None else SEEDS // 3
    return sum(x.values()) / len(x) if len(x) == want else None


def means():
    return {arm(s, v): {m: mean_of(arm(s, v), m) for m in METRICS} for v in (None,) + VARIANTS for s in STACKS}


def fire_means():
    """Per fire, the mean of each arm (20 realisations): the loss regime of each variant."""
    return {arm(s, v): {f: {m: mean_of(arm(s, v), m, f) for m in ('psnr', 'loss', 'm1loss')} for f in FIRES}
            for v in (None,) + VARIANTS for s in STACKS}


def ranks(mn, fm):
    """Order of the four stacks by mean PSNR, best first, per variant and per (variant, fire) cell."""
    out = {}
    for v in (None,) + VARIANTS:
        key = 'reference' if v is None else v
        out[key] = {'all': sorted(STACKS, key=lambda s: -mn[arm(s, v)]['psnr'])}
        for f in FIRES:
            out[key][f] = sorted(STACKS, key=lambda s: -fm[arm(s, v)][f]['psnr'])
    return out


def compute():
    mn, fm = means(), fire_means()
    res = {'means': mn, 'fire_means': fm, 'ranks': ranks(mn, fm), 'families': {}, 'reference': {}}
    for kind in ('gap', 'change'):
        res['families'][kind] = {c: {m: family(kind, c, m) for m in METRICS} for c in CONTRASTS}
    res['reference'] = {c: {m: reference(c, m) for m in METRICS} for c in CONTRASTS}
    return res


def main():
    res = compute()
    if '--json' in sys.argv:
        json.dump(res, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=1)
    show = ('psnr', 'loss', 'm1loss', 'usable', 'declared', 'false_declared')
    print('Means over the 60 seeds, GOP 9-3, T = 9 s')
    print('  %-16s' % 'arm' + ''.join('%10s' % m[:9] for m in show))
    for a, row in res['means'].items():
        print('  %-16s' % a + ''.join('%10s' % ('%.2f' % row[m] if row[m] is not None else '--') for m in show))
    print('\nRules loss per fire (%)')
    for v in (None,) + VARIANTS:
        print('  %-10s' % (v or 'reference') + ''.join('%8.1f' % res['fire_means'][arm('rules', v)][f]['loss'] for f in FIRES))
    print('\nRanking by mean PSNR (best first)')
    for k, r in res['ranks'].items():
        print('  %-10s all %-30s ' % (k, ' '.join(r['all'])) + '  '.join('%s %s' % (f, ' '.join(r[f])) for f in FIRES))
    for c in CONTRASTS:
        for m in ('psnr', 'm1loss', 'loss', 'usable', 'declared'):
            ref = res['reference'][c][m]
            if ref is None:
                continue
            print('\n%s, %s: reference %+.2f [%+.2f, %+.2f] p=%.3g' % (c, m, ref['mean'], ref['lo'], ref['hi'], ref['p']))
            for kind in ('gap', 'change'):
                for v in VARIANTS:
                    t = res['families'][kind][c][m][v]
                    if t is None:
                        continue
                    print('  %-6s %-6s n=%d %+7.2f [%+6.2f, %+6.2f] p=%-9.3g Holm %.3g'
                          % (kind, v, t['n'], t['mean'], t['lo'], t['hi'], t['p'], t['holm']))


if __name__ == '__main__':
    main()
