#!/usr/bin/env python3
"""Statistics of the comparative study (study 09) from src/data/comparative.tsv:
the cell means, the paired t-tests between stacks in each cell (fire x split;
the four stacks of a fire share their seeds, so each test pairs the same
simulations), Holm-corrected within each comparison over the nine cells, the
same differences per seed (each of the 60 seeds averaged over its three splits:
mean, 95 % confidence interval; the share of positive pairs is over the 180
pairs), and the repeated-measures ANOVA of a metric: stack and split vary within
a seed, fire between seeds, and each term is tested against its own error
stratum (the design is balanced). anova() keeps the ANOVA that treats the
simulations as independent, for comparison.

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
    h = stats.t_quantile(0.95, n - 1) * sd / math.sqrt(n)
    p = stats.t_sf2(m / (sd / math.sqrt(n)), n - 1) if sd else 1.0
    return m, m - h, m + h, p, sum(y > 0 for y in d) / n, n


def anova_rm(rows, metric):
    """Split-plot ANOVA: subjects are the seeds (fire, realisation), fire is
    between subjects, stack and split are within. Returns (term, df, error df,
    F, p, partial eta2) for each term; partial eta2 = SS / (SS + SS of its
    error stratum)."""
    data = [(r['fire'], r['real'], r['stack'], r['split'], float(r[metric])) for r in rows]
    y = np.array([d[4] for d in data])
    gm = y.mean()

    def ss(*cols):
        groups = {}
        for d in data:
            groups.setdefault(tuple(d[c] for c in cols), []).append(d[4])
        return sum(len(v) * (sum(v) / len(v) - gm) ** 2 for v in groups.values())

    F_, R_, S_, G_ = 0, 1, 2, 3
    a = len({d[F_] for d in data})
    b = len({d[S_] for d in data})
    c = len({d[G_] for d in data})
    n = len({(d[F_], d[R_]) for d in data})
    fire, stack, split = ss(F_), ss(S_), ss(G_)
    subj = ss(F_, R_)
    fs = ss(F_, S_) - fire - stack
    fg = ss(F_, G_) - fire - split
    sg = ss(S_, G_) - stack - split
    fsg = ss(F_, S_, G_) - fire - stack - split - fs - fg - sg
    e0, d0 = subj - fire, n - a
    e1, d1 = ss(F_, R_, S_) - subj - stack - fs, d0 * (b - 1)
    e2, d2 = ss(F_, R_, G_) - subj - split - fg, d0 * (c - 1)
    e3 = float(((y - gm) ** 2).sum()) - subj - stack - split - fs - fg - sg - fsg - e1 - e2
    d3 = d0 * (b - 1) * (c - 1)
    terms = (('stack', stack, b - 1, e1, d1), ('fire', fire, a - 1, e0, d0), ('split', split, c - 1, e2, d2),
             ('stack x fire', fs, (a - 1) * (b - 1), e1, d1), ('stack x split', sg, (b - 1) * (c - 1), e3, d3),
             ('fire x split', fg, (a - 1) * (c - 1), e2, d2),
             ('stack x fire x split', fsg, (a - 1) * (b - 1) * (c - 1), e3, d3))
    out = []
    for name, s, df, e, de in terms:
        f = (s / df) / (e / de)
        out.append((name, df, de, f, stats.f_sf(f, df, de), s / (s + e)))
    return out


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
        seeds = {}
        for (f, s), _, _, _ in tests:
            for k in sorted(set(cell[(f, s, a)]) & set(cell[(f, s, b)])):
                seeds.setdefault((f, k), []).append(cell[(f, s, a)][k] - cell[(f, s, b)][k])
        pairs = [x for v in seeds.values() for x in v]
        m, lo, hi, p, _, n = pooled([sum(v) / len(v) for v in seeds.values()])
        print('    per seed %+7.2f  95%% CI [%+.2f, %+.2f]  p = %.3g  (n = %d seeds); positive in %.1f%% of the %d pairs'
              % (m, lo, hi, p, n, 100 * sum(x > 0 for x in pairs) / len(pairs), len(pairs)))
    complete = all((f, s, st) in cell for f in fires for s in splits for st in STACKS)
    if complete and len(fires) > 1 and len(splits) > 1:
        res, dfe = anova(rows, metric)
        print('\nANOVA %s, simulations taken as independent (residual df %d)' % (metric, dfe))
        for t, ss, df, F, p, eta in res:
            print('  %-22s df %2d  F %9.2f  p %.4g  partial eta2 %.3f' % (t, df, F, p, eta))
        print('\nrepeated-measures ANOVA %s, the three cross-layer stacks' % metric)
        for t, df, de, F, p, eta in anova_rm([r for r in rows if r['stack'] != 'aomdv'], metric):
            print('  %-22s df %2d, %3d  F %9.2f  p %.4g  partial eta2 %.3f' % (t, df, de, F, p, eta))
    else:
        print('\nANOVA waits for the complete design')


if __name__ == '__main__':
    main()
