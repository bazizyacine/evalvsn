#!/usr/bin/env python3
"""stats.py - the statistical protocol of the comparative study.

Reads computed sets under experiments/sets/ and applies, on one metric, the
tests the thesis audit's rubric etude-comparative prescribes:

  ci        per set: n, mean, standard deviation, 95 % Student interval,
            minimum and maximum (the minimum carries the 28 dB criterion)
  compare   two sets: Welch's t test, Mann-Whitney U as a control,
            Cohen's d, the difference of means with its 95 % interval
  anova     k sets as k levels of one factor: one-way ANOVA (F, p, eta
            squared), Kruskal-Wallis as a control, pairwise Welch tests
            with Holm's correction
  factorial a design file mapping sets to factor levels: balanced N-way
            ANOVA with all interactions, partial eta squared per term,
            cell means for the interaction plots

Where the metric is a column of sim/mt.dat (psnr, refPSNR, ssim, refSSIM,
pktLoss, pktLossM, pktLossD, pktlossM1..M13, pktlossD1..D13), the values
of the realisations are read one by one and every test runs on raw data.
Any other metric of the catalogue (Delay, Jitter, Throughput, Energy
Consumption, Life Time, ...) is read from graphs/<Metric>/<Metric>.dat,
which holds mean, standard deviation and count: ci, compare and anova then
run on those summary statistics (Welch and F from moments; no rank test),
and the output says so.

The distributions (Student, F, chi-square, normal) are computed here from
the regularized incomplete beta and gamma functions, so the script needs
nothing beyond the standard library and numpy.

    python3 stats.py ci set_A set_B --metric psnr
    python3 stats.py compare set_AOMDV_X set_AOMDV_QPV_X --metric psnr
    python3 stats.py anova set_L set_M set_H --metric psnr --labels 3/9,6/6,9/3
    python3 stats.py factorial --design design.tsv --metric psnr
    ... --latex          tabular rows instead of text

A design file is tab- or space-separated, one header line naming the
factors and a last column `set`, then one row per cell:

    codec   protocol   set
    3/9     AOMDV      set_AOMDV_GC-PN-0p01597-13_p30-n100-d200
    3/9     AOMDV_QPV  set_AOMDV_QPV_GC-PN-0p01597-13_p30-n100-d200
    ...
"""

import argparse
import glob
import math
import os
import sys
from itertools import combinations

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SETS = os.path.normpath(os.path.join(HERE, '..', 'sets'))

MT_COLUMNS = None   # header of sim/mt.dat, read once


# ---------------------------------------------------------------- distributions

def _betacf(a, b, x):
    """Continued fraction for the incomplete beta function (Lentz)."""
    tiny = 1e-300
    qab, qap, qam = a + b, a + 1.0, a - 1.0
    c, d = 1.0, 1.0 - qab * x / qap
    d = 1.0 / (d if abs(d) > tiny else tiny)
    h = d
    for m in range(1, 300):
        m2 = 2 * m
        aa = m * (b - m) * x / ((qam + m2) * (a + m2))
        d = 1.0 + aa * d
        d = 1.0 / (d if abs(d) > tiny else tiny)
        c = 1.0 + aa / (c if abs(c) > tiny else tiny)
        h *= d * c
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2))
        d = 1.0 + aa * d
        d = 1.0 / (d if abs(d) > tiny else tiny)
        c = 1.0 + aa / (c if abs(c) > tiny else tiny)
        de = d * c
        h *= de
        if abs(de - 1.0) < 3e-16:
            break
    return h


def betainc(a, b, x):
    """Regularized incomplete beta I_x(a, b)."""
    if x <= 0.0:
        return 0.0
    if x >= 1.0:
        return 1.0
    lbeta = math.lgamma(a + b) - math.lgamma(a) - math.lgamma(b)
    front = math.exp(lbeta + a * math.log(x) + b * math.log(1.0 - x))
    if x < (a + 1.0) / (a + b + 2.0):
        return front * _betacf(a, b, x) / a
    return 1.0 - front * _betacf(b, a, 1.0 - x) / b


def gammainc(s, x):
    """Regularized lower incomplete gamma P(s, x)."""
    if x <= 0.0:
        return 0.0
    if x < s + 1.0:
        term = total = 1.0 / s
        for n in range(1, 500):
            term *= x / (s + n)
            total += term
            if abs(term) < abs(total) * 1e-16:
                break
        return total * math.exp(-x + s * math.log(x) - math.lgamma(s))
    tiny = 1e-300
    b = x + 1.0 - s
    c, d = 1.0 / tiny, 1.0 / b
    h = d
    for i in range(1, 500):
        an = -i * (i - s)
        b += 2.0
        d = an * d + b
        d = 1.0 / (d if abs(d) > tiny else tiny)
        c = b + an / (c if abs(c) > tiny else tiny)
        de = d * c
        h *= de
        if abs(de - 1.0) < 1e-16:
            break
    return 1.0 - math.exp(-x + s * math.log(x) - math.lgamma(s)) * h


def t_sf2(t, df):
    """Two-sided p-value of Student's t."""
    x = df / (df + t * t)
    return betainc(df / 2.0, 0.5, x)


def t_quantile(p, df):
    """Two-sided quantile: t such that P(|T| > t) = 1 - p."""
    lo, hi = 0.0, 100.0
    for _ in range(200):
        mid = (lo + hi) / 2.0
        if t_sf2(mid, df) > 1.0 - p:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2.0


def f_sf(f, d1, d2):
    """Upper tail of the F distribution."""
    if f <= 0.0:
        return 1.0
    return betainc(d2 / 2.0, d1 / 2.0, d2 / (d2 + d1 * f))


def chi2_sf(x, df):
    return 1.0 - gammainc(df / 2.0, x / 2.0)


def norm_sf2(z):
    return math.erfc(abs(z) / math.sqrt(2.0))


# ---------------------------------------------------------------------- reading

def set_dir(name):
    d = name if os.path.isdir(name) else os.path.join(SETS, name)
    if not os.path.isdir(d):
        raise SystemExit(f"stats.py: no set {name} under {SETS}")
    return d


def mt_columns():
    global MT_COLUMNS
    if MT_COLUMNS is None:
        for f in glob.glob(os.path.join(SETS, '*', 'r01', '*', 'sim', 'mt.dat')):
            with open(f, encoding='utf8') as fh:
                MT_COLUMNS = fh.readline().lstrip('#').split()
            break
        MT_COLUMNS = MT_COLUMNS or []
    return MT_COLUMNS


class Sample:
    """A metric on one set: raw values when available, else summary moments."""

    def __init__(self, name, values=None, mean=None, sd=None, n=None):
        self.name = name
        self.values = None if values is None else np.asarray(values, dtype=float)
        if self.values is not None:
            self.n = len(self.values)
            self.mean = float(self.values.mean())
            self.sd = float(self.values.std(ddof=1)) if self.n > 1 else 0.0
        else:
            self.n, self.mean, self.sd = n, mean, sd

    @property
    def raw(self):
        return self.values is not None

    @property
    def var(self):
        return self.sd ** 2

    def ci95(self):
        if self.n < 2:
            return (self.mean, self.mean)
        h = t_quantile(0.95, self.n - 1) * self.sd / math.sqrt(self.n)
        return (self.mean - h, self.mean + h)


def read_metric(name, metric):
    """Sample of a metric on a set: mt.dat per realisation, else graphs summary."""
    d = set_dir(name)
    cols = mt_columns()
    if metric in cols:
        idx = cols.index(metric)
        vals = []
        for f in sorted(glob.glob(os.path.join(d, 'r[0-9]*', '*', 'sim', 'mt.dat'))):
            with open(f, encoding='utf8') as fh:
                for line in fh:
                    if line.startswith('#') or not line.strip():
                        continue
                    vals.append(float(line.split()[idx]))
                    break
        if not vals:
            raise SystemExit(f"stats.py: {name}: no realisation carries sim/mt.dat")
        return Sample(name, values=vals)
    f = os.path.join(d, 'graphs', metric, metric + '.dat')
    if not os.path.isfile(f):
        raise SystemExit(f"stats.py: {name}: neither an mt.dat column nor graphs/{metric}/{metric}.dat")
    with open(f, encoding='utf8') as fh:
        for line in fh:
            if line.startswith('#') or not line.strip():
                continue
            _, mean, sd, n = line.split()[:4]
            return Sample(name, mean=float(mean), sd=float(sd), n=int(n))
    raise SystemExit(f"stats.py: {name}: {f} is empty")


# ------------------------------------------------------------------------ tests

def welch(a, b):
    """Welch's t test from moments: t, df, p, difference and its 95 % CI."""
    va, vb = a.var / a.n, b.var / b.n
    se = math.sqrt(va + vb)
    diff = a.mean - b.mean
    if se == 0.0:
        return 0.0, a.n + b.n - 2, 1.0, diff, (diff, diff)
    t = diff / se
    df = (va + vb) ** 2 / (va ** 2 / (a.n - 1) + vb ** 2 / (b.n - 1))
    p = t_sf2(t, df)
    h = t_quantile(0.95, df) * se
    return t, df, p, diff, (diff - h, diff + h)


def cohen_d(a, b):
    sp = math.sqrt(((a.n - 1) * a.var + (b.n - 1) * b.var) / (a.n + b.n - 2))
    return (a.mean - b.mean) / sp if sp > 0 else 0.0


def ranks(values):
    """Average ranks, ties shared."""
    order = np.argsort(values, kind='mergesort')
    r = np.empty(len(values))
    i = 0
    while i < len(values):
        j = i
        while j + 1 < len(values) and values[order[j + 1]] == values[order[i]]:
            j += 1
        r[order[i:j + 1]] = (i + j) / 2.0 + 1.0
        i = j + 1
    return r


def mann_whitney(a, b):
    """U statistic and its normal-approximation two-sided p (ties corrected)."""
    x = np.concatenate([a.values, b.values])
    r = ranks(x)
    n1, n2 = a.n, b.n
    u1 = r[:n1].sum() - n1 * (n1 + 1) / 2.0
    u = min(u1, n1 * n2 - u1)
    mu = n1 * n2 / 2.0
    n = n1 + n2
    _, counts = np.unique(x, return_counts=True)
    tie = (counts ** 3 - counts).sum()
    sigma = math.sqrt(n1 * n2 / 12.0 * ((n + 1) - tie / (n * (n - 1))))
    if sigma == 0.0:
        return u, 1.0
    z = (u - mu + 0.5) / sigma
    return u, norm_sf2(z)


def anova_oneway(samples):
    """F, df1, df2, p, eta squared, from moments (exact on raw data too)."""
    k = len(samples)
    n_tot = sum(s.n for s in samples)
    grand = sum(s.mean * s.n for s in samples) / n_tot
    ss_b = sum(s.n * (s.mean - grand) ** 2 for s in samples)
    ss_w = sum((s.n - 1) * s.var for s in samples)
    df1, df2 = k - 1, n_tot - k
    if ss_w == 0.0 or df2 <= 0:
        return float('inf'), df1, df2, 0.0, 1.0
    f = (ss_b / df1) / (ss_w / df2)
    return f, df1, df2, f_sf(f, df1, df2), ss_b / (ss_b + ss_w)


def kruskal(samples):
    """H statistic and chi-square p, ties corrected."""
    x = np.concatenate([s.values for s in samples])
    r = ranks(x)
    n = len(x)
    h, i = 0.0, 0
    for s in samples:
        h += r[i:i + s.n].sum() ** 2 / s.n
        i += s.n
    h = 12.0 / (n * (n + 1)) * h - 3 * (n + 1)
    _, counts = np.unique(x, return_counts=True)
    corr = 1.0 - (counts ** 3 - counts).sum() / (n ** 3 - n)
    if corr > 0:
        h /= corr
    return h, chi2_sf(h, len(samples) - 1)


def holm(pairs):
    """Holm's step-down correction: [(i, j, p_raw, p_adj)]."""
    m = len(pairs)
    order = sorted(range(m), key=lambda q: pairs[q][2])
    adj, running = [None] * m, 0.0
    for rank, q in enumerate(order):
        running = max(running, (m - rank) * pairs[q][2])
        adj[q] = min(1.0, running)
    return [(i, j, p, adj[q]) for q, (i, j, p) in enumerate(pairs)]


# ------------------------------------------------------------- factorial ANOVA

def read_design(path):
    with open(path, encoding='utf8') as fh:
        lines = [l.split() for l in fh if l.strip() and not l.startswith('#')]
    header = lines[0]
    if header[-1] != 'set':
        raise SystemExit("stats.py: the design's last column must be named set")
    factors = header[:-1]
    rows = [(dict(zip(factors, r[:-1])), r[-1]) for r in lines[1:]]
    return factors, rows


def effect_columns(levels, values):
    """Sum-to-zero contrast columns of one factor for a vector of level labels."""
    k = len(levels)
    cols = np.zeros((len(values), k - 1))
    for i, v in enumerate(values):
        j = levels.index(v)
        if j < k - 1:
            cols[i, j] = 1.0
        else:
            cols[i, :] = -1.0
    return cols


def factorial_anova(factors, rows, metric):
    """Balanced N-way ANOVA by sequential model comparison on effect coding."""
    data, cells = [], {}
    for lv, name in rows:
        s = read_metric(name, metric)
        if not s.raw:
            raise SystemExit(f"stats.py: factorial needs per-realisation values; {metric} on {name} is a summary")
        for v in s.values:
            data.append((lv, v))
        cells[tuple(lv[f] for f in factors)] = s
    y = np.array([v for _, v in data])
    levels = {f: sorted({lv[f] for lv, _ in data}) for f in factors}
    main = {f: effect_columns(levels[f], [lv[f] for lv, _ in data]) for f in factors}
    terms = []
    for r in range(1, len(factors) + 1):
        for combo in combinations(factors, r):
            cols = main[combo[0]]
            for f in combo[1:]:
                cols = np.hstack([cols[:, i:i + 1] * main[f] for i in range(cols.shape[1])])
            terms.append((combo, cols))
    X = np.ones((len(y), 1))
    ss_res_prev = float(((y - y.mean()) ** 2).sum())
    results = []
    for combo, cols in terms:
        X = np.hstack([X, cols])
        beta, *_ = np.linalg.lstsq(X, y, rcond=None)
        ss_res = float(((y - X @ beta) ** 2).sum())
        results.append((combo, ss_res_prev - ss_res, cols.shape[1]))
        ss_res_prev = ss_res
    df_err = len(y) - X.shape[1]
    ms_err = ss_res_prev / df_err if df_err > 0 else float('nan')
    out = []
    for combo, ss, df in results:
        f = (ss / df) / ms_err if ms_err > 0 else float('inf')
        p = f_sf(f, df, df_err) if ms_err > 0 else 0.0
        out.append((' x '.join(combo), ss, df, f, p, ss / (ss + ss_res_prev)))
    return out, ss_res_prev, df_err, cells, levels


# ----------------------------------------------------------------------- output

def fmt(x, nd=2):
    return f"{x:.{nd}f}"


def cmd_ci(a):
    rows = [read_metric(s, a.metric) for s in a.sets]
    if a.latex:
        for s in rows:
            lo, hi = s.ci95()
            mn = fmt(s.values.min()) if s.raw else '--'
            print(f"{s.name} & {s.n} & {fmt(s.mean)} & {fmt(s.sd)} & [{fmt(lo)}, {fmt(hi)}] & {mn} \\\\")
        return
    print(f"{a.metric}: {'set':50} {'n':>3} {'mean':>9} {'sd':>8} {'95 % CI':>20} {'min':>8} {'max':>8}")
    for s in rows:
        lo, hi = s.ci95()
        mn = fmt(s.values.min()) if s.raw else '--'
        mx = fmt(s.values.max()) if s.raw else '--'
        print(f"{'':6} {s.name:50} {s.n:3d} {s.mean:9.3f} {s.sd:8.3f} [{lo:8.3f}, {hi:8.3f}] {mn:>8} {mx:>8}")
    if not all(s.raw for s in rows):
        print("  (summary statistics from graphs/, no per-realisation values)")


def cmd_compare(a):
    x, y = read_metric(a.sets[0], a.metric), read_metric(a.sets[1], a.metric)
    t, df, p, diff, (lo, hi) = welch(x, y)
    d = cohen_d(x, y)
    if a.latex:
        print(f"{x.name} vs {y.name} & {fmt(diff)} & [{fmt(lo)}, {fmt(hi)}] & {fmt(t)} & {fmt(df,1)} & {p:.3g} & {fmt(d)} \\\\")
        return
    print(f"{a.metric}: {x.name} ({x.mean:.3f} +- {x.sd:.3f}, n={x.n}) vs {y.name} ({y.mean:.3f} +- {y.sd:.3f}, n={y.n})")
    print(f"  difference of means {diff:+.3f}, 95 % CI [{lo:.3f}, {hi:.3f}]")
    print(f"  Welch t = {t:.3f}, df = {df:.1f}, p = {p:.4g}")
    print(f"  Cohen d = {d:.2f}")
    if x.raw and y.raw:
        u, pu = mann_whitney(x, y)
        print(f"  Mann-Whitney U = {u:.1f}, p = {pu:.4g}")
    else:
        print("  (summary statistics: no rank test)")


def cmd_anova(a):
    samples = [read_metric(s, a.metric) for s in a.sets]
    labels = a.labels.split(',') if a.labels else [s.name for s in samples]
    if len(labels) != len(samples):
        raise SystemExit("stats.py: --labels must name every set")
    f, df1, df2, p, eta = anova_oneway(samples)
    print(f"{a.metric}: one-way ANOVA over {len(samples)} levels")
    for lab, s in zip(labels, samples):
        lo, hi = s.ci95()
        print(f"  {lab:12} n={s.n:2d} mean={s.mean:9.3f} sd={s.sd:7.3f} CI95=[{lo:.3f}, {hi:.3f}]")
    print(f"  F({df1}, {df2}) = {f:.3f}, p = {p:.4g}, eta2 = {eta:.3f}")
    if all(s.raw for s in samples):
        h, ph = kruskal(samples)
        print(f"  Kruskal-Wallis H = {h:.3f}, p = {ph:.4g}")
    else:
        print("  (summary statistics: no rank test)")
    pairs = []
    for i, j in combinations(range(len(samples)), 2):
        _, _, pw, _, _ = welch(samples[i], samples[j])
        pairs.append((i, j, pw))
    print("  pairwise Welch, Holm-adjusted:")
    for i, j, praw, padj in holm(pairs):
        diff = samples[i].mean - samples[j].mean
        mark = '*' if padj < 0.05 else ' '
        print(f"   {mark} {labels[i]:12} vs {labels[j]:12} diff={diff:+9.3f} p={praw:.3g} adj={padj:.3g}")
    if a.latex:
        for lab, s in zip(labels, samples):
            lo, hi = s.ci95()
            print(f"{lab} & {s.n} & {fmt(s.mean)} & {fmt(s.sd)} & [{fmt(lo)}, {fmt(hi)}] \\\\")


def cmd_factorial(a):
    factors, rows = read_design(a.design)
    terms, ss_err, df_err, cells, levels = factorial_anova(factors, rows, a.metric)
    print(f"{a.metric}: {len(factors)}-way ANOVA, {len(rows)} cells, factors " +
          ', '.join(f"{f} ({len(levels[f])})" for f in factors))
    print(f"  {'term':28} {'SS':>12} {'df':>4} {'F':>10} {'p':>10} {'partial eta2':>13}")
    for term, ss, df, f, p, eta in terms:
        mark = '*' if p < 0.05 else ' '
        print(f" {mark}{term:28} {ss:12.3f} {df:4d} {f:10.3f} {p:10.3g} {eta:13.3f}")
    print(f"  {'residual':28} {ss_err:12.3f} {df_err:4d}")
    print("  cell means (for the interaction plots):")
    for key in sorted(cells):
        s = cells[key]
        print("   " + '  '.join(f"{f}={v}" for f, v in zip(factors, key)) +
              f"  mean={s.mean:.3f} sd={s.sd:.3f} n={s.n}")
    if a.latex:
        for term, ss, df, f, p, eta in terms:
            print(f"{term} & {df} & {fmt(f)} & {p:.3g} & {fmt(eta,3)} \\\\")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('command', choices=['ci', 'compare', 'anova', 'factorial'])
    ap.add_argument('sets', nargs='*', help='set names under experiments/sets/')
    ap.add_argument('--metric', required=True, help='an mt.dat column or a catalogue metric')
    ap.add_argument('--labels', help='anova: level labels, comma-separated, one per set')
    ap.add_argument('--design', help='factorial: the design file')
    ap.add_argument('--latex', action='store_true', help='also print tabular rows')
    a = ap.parse_args()
    if a.command == 'compare' and len(a.sets) != 2:
        ap.error('compare takes exactly two sets')
    if a.command == 'anova' and len(a.sets) < 2:
        ap.error('anova takes at least two sets')
    if a.command == 'factorial' and not a.design:
        ap.error('factorial needs --design')
    if a.command in ('ci',) and not a.sets:
        ap.error('name at least one set')
    {'ci': cmd_ci, 'compare': cmd_compare, 'anova': cmd_anova, 'factorial': cmd_factorial}[a.command](a)


if __name__ == '__main__':
    main()
