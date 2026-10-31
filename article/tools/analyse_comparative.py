#!/usr/bin/env python3
"""Statistics of the comparative study (study 09) from src/data/comparative.tsv:
the cell means, the paired t-tests between stacks in each cell (fire x split;
the four stacks of a fire share their seeds, so each test pairs the same
simulations), Holm-corrected within each comparison over the nine cells, the
same differences pooled over the 180 pairs (mean, 95 % confidence interval,
share of positive pairs), and the three-way ANOVA stack x fire x split on a metric (effect coding,
sequential sums of squares; the design is balanced).

Usage: analyse_comparative.py [metric]   (default psnr; also loss, m1loss...)
"""
import math
import os
import sys
from itertools import combinations

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'metrics')))
import stats  # noqa: E402

DATA = os.path.join(HERE, '..', 'src', 'data', 'comparative.tsv')
STACKS = ('aomdv', 'rules', 'agents', 'hybrid')
PAIRS = (('hybrid', 'rules'), ('agents', 'rules'), ('hybrid', 'agents'), ('rules', 'aomdv'))
FACTORS = ('stack', 'fire', 'split')


def load():
    rows = []
    for line in open(DATA):
        if line.startswith('#') or line.startswith('fire\t'):
            continue
        v = line.rstrip('\n').split('\t')
        rows.append(dict(zip(('fire', 'split', 'stack', 'real', 'psnr', 'loss', 'mloss', 'm1loss', 'm1to3loss',
                              'dloss', 'recoverable'), v)))
    return rows


def paired(a, b):
    k = sorted(set(a) & set(b))
    d = [a[x] - b[x] for x in k]
    m = sum(d) / len(d)
    sd = math.sqrt(sum((y - m) ** 2 for y in d) / (len(d) - 1)) if len(d) > 1 else 0.0
    p = stats.t_sf2(m / (sd / math.sqrt(len(d))), len(d) - 1) if sd else 1.0
    return m, p, len(d)


def pooled(d):
    n = len(d)
    m = sum(d) / n
    sd = math.sqrt(sum((y - m) ** 2 for y in d) / (n - 1))
    h = stats.t_quantile(0.975, n - 1) * sd / math.sqrt(n)
    p = stats.t_sf2(m / (sd / math.sqrt(n)), n - 1) if sd else 1.0
    return m, m - h, m + h, p, sum(y > 0 for y in d) / n, n


def anova(rows, metric):
    data = [r for r in rows if r['stack'] in STACKS]
    y = np.array([float(r[metric]) for r in data])
    levels = {f: sorted({r[f] for r in data}) for f in FACTORS}
    main = {f: stats.effect_columns(levels[f], [r[f] for r in data]) for f in FACTORS}
    X = np.ones((len(y), 1))
    prev = float(((y - y.mean()) ** 2).sum())
    out = []
    for n in range(1, len(FACTORS) + 1):
        for combo in combinations(FACTORS, n):
            cols = main[combo[0]]
            for f in combo[1:]:
                cols = np.hstack([cols[:, i:i + 1] * main[f] for i in range(cols.shape[1])])
            X = np.hstack([X, cols])
            beta, *_ = np.linalg.lstsq(X, y, rcond=None)
            res = float(((y - X @ beta) ** 2).sum())
            out.append((' x '.join(combo), prev - res, cols.shape[1]))
            prev = res
    dfe = len(y) - X.shape[1]
    mse = prev / dfe
    return [(t, ss, df, (ss / df) / mse, stats.f_sf((ss / df) / mse, df, dfe), ss / (ss + prev)) for t, ss, df in out], dfe


def main():
    metric = sys.argv[1] if len(sys.argv) > 1 else 'psnr'
    rows = load()
    cell = {}
    for r in rows:
        cell.setdefault((r['fire'], r['split'], r['stack']), {})[r['real']] = float(r[metric])
    fires = sorted({r['fire'] for r in rows})
    splits = sorted({r['split'] for r in rows})
    print('%s, mean over the realisations' % metric)
    print('%-4s %-5s ' % ('fire', 'split') + ' '.join('%9s' % s for s in STACKS))
    for f in fires:
        for s in splits:
            vals = [cell.get((f, s, st)) for st in STACKS]
            print('%-4s %-5s ' % (f, s) + ' '.join('%9.2f' % (sum(v.values()) / len(v)) if v else '%9s' % '-' for v in vals))
    print('\npaired differences (Holm over the cells of each comparison)')
    for a, b in PAIRS:
        tests = []
        for f in fires:
            for s in splits:
                if (f, s, a) in cell and (f, s, b) in cell:
                    m, p, n = paired(cell[(f, s, a)], cell[(f, s, b)])
                    tests.append(((f, s), m, p, n))
        if not tests:
            continue
        adj = stats.holm([(t[0], None, t[2]) for t in tests])
        print('  %s - %s' % (a, b))
        for (key, m, p, n), (_, _, _, pa) in zip(tests, adj):
            print('    %s %s  %+7.2f  p = %.4f  p_Holm = %.4f  (n = %d)' % (key[0], key[1], m, p, pa, n))
        d = [cell[(f, s, a)][k] - cell[(f, s, b)][k] for (f, s), _, _, _ in tests
             for k in sorted(set(cell[(f, s, a)]) & set(cell[(f, s, b)]))]
        m, lo, hi, p, pos, n = pooled(d)
        print('    pooled   %+7.2f  95%% CI [%+.2f, %+.2f]  p = %.3g  positive %.1f%%  (n = %d)'
              % (m, lo, hi, p, 100 * pos, n))
    complete = all((f, s, st) in cell for f in fires for s in splits for st in STACKS)
    if complete and len(fires) > 1 and len(splits) > 1:
        res, dfe = anova(rows, metric)
        print('\nANOVA %s (residual df %d)' % (metric, dfe))
        for t, ss, df, F, p, eta in res:
            print('  %-22s df %2d  F %9.2f  p %.4g  partial eta2 %.3f' % (t, df, F, p, eta))
    else:
        print('\nANOVA waits for the complete design')


if __name__ == '__main__':
    main()
