#!/usr/bin/env python3
"""Single-agent ablations of the revision (study 10, experiment E2) from
src/data/revision.tsv, GOP 9-3, alarm period 9 s, 40 m grid.

Each arm changes one decision of a study 09 stack and keeps its seeds, so every
test pairs the 60 seeds (fire, realisation):

  alone       rules-only<X> - rules: agent X in place of its rule on the rule
              stack, for the seven agents (P path, C congestion, B link-break
              detection, D discovery, A channel access, T power, E early discard)
  in hybrid   hybrid - hybrid-no<X>: the PSNR with agent X minus the PSNR with
              its rule, the other agents of the hybrid stack present, for its
              six agents; the link-break agent, absent from the hybrid stack,
              takes agents - hybrid, the one switch between the two stacks

Each contrast gives the mean of the per-seed differences, its two-sided 95 %
Student interval and p, then Holm's correction within its family: the seven
effects alone, and the six removals from the hybrid stack. The link-break
agent's effect in the hybrid stack is one test outside that family (the same
comparison as study 09's hybrid - agents); a Holm over the seven is printed as
a check and changes no conclusion.
Additivity: the joint effect hybrid - rules against the sum of the six effects
alone, and against the sum of the six removals, seed by seed; then each agent's
effect in the hybrid minus its effect alone.

Usage: analyse_ablation.py
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'metrics')))
import stats  # noqa: E402

DATA = os.path.join(HERE, '..', 'src', 'data', 'revision.tsv')
SEEDS = 60
SOLO = 'PCBDATE'           # the seven agents, alone on the rule stack
HYBRID = 'PCDATE'          # the six agents of the hybrid stack
METRICS = (('psnr', 'PSNR (dB)'), ('m1loss', 'M1 loss (points)'), ('declared', 'declarations'),
           ('false_declared', 'declarations of a living neighbour'), ('loss', 'loss (points)'))


def table():
    lines = [l for l in open(DATA) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:]]


REV = table()


def values(arm, metric):
    """The metric of each seed (fire, realisation) of an arm at GOP 9-3."""
    return {(r['fire'], r['realisation']): float(r[metric]) for r in REV
            if r['arm'] == arm and r['split'] == '9M3D' and r['period'] == 'p9' and r['dist'] == '40'
            and r[metric] != ''}


def summary(d):
    """n, mean, 95 % interval and two-sided p of a list of per-seed differences."""
    n = len(d)
    m = sum(d) / n
    s = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1))
    if s == 0:
        return n, m, m, m, 1.0
    se = s / math.sqrt(n)
    # t_quantile is two-sided: 0.95 gives the 95 % interval.
    h = stats.t_quantile(0.95, n - 1) * se
    return n, m, m - h, m + h, stats.t_sf2(m / se, n - 1)


def diff(a, b):
    """Per-seed a - b, over the seeds both arms hold (a dict of seed: value)."""
    return {k: a[k] - b[k] for k in a if k in b}


def with_without(agent, metric, context):
    """Per-seed effect of an agent: the arm with it minus the arm with its rule."""
    if context == 'alone':
        return diff(values('rules-only' + agent, metric), values('rules', metric))
    if agent == 'B':
        return diff(values('agents', metric), values('hybrid', metric))
    return diff(values('hybrid', metric), values('hybrid-no' + agent, metric))


def family(effects):
    """effects: {label: per-seed dict}. Returns {label: (n, mean, lo, hi, p, p_holm)};
    a contrast backed by fewer than the 60 seeds is left out of the family."""
    full = {k: summary(list(v.values())) for k, v in effects.items() if len(v) == SEEDS}
    adj = stats.holm([(k, '', full[k][4]) for k in full])
    return {k: full[k] + (p,) for (k, _, _, p) in adj}


def effects(metric):
    """The two families of a metric: the seven effects alone, and the effects in
    the hybrid stack, Holm over its six removals; the link-break agent's effect
    there is a single test, its p left as it is."""
    alone = family({x: with_without(x, metric, 'alone') for x in SOLO})
    inside = family({x: with_without(x, metric, 'hybrid') for x in HYBRID})
    b = with_without('B', metric, 'hybrid')
    if len(b) == SEEDS:
        t = summary(list(b.values()))
        inside['B'] = t + (t[4],)
    return alone, inside


def sum_of(agents, metric, context):
    out = None
    for x in agents:
        e = with_without(x, metric, context)
        out = e if out is None else {k: out[k] + e[k] for k in out if k in e}
    return out


def fmt(t):
    n, m, lo, hi, p = t[:5]
    s = 'n=%d  %+7.2f [%+6.2f, %+6.2f]  p=%-9.3g' % (n, m, lo, hi, p)
    return s + ('  Holm %.3g' % t[5] if len(t) > 5 else '')


def main():
    print('Means over the 60 seeds, GOP 9-3')
    arms = ['rules', 'hybrid', 'agents'] + ['rules-only' + x for x in SOLO] + ['hybrid-no' + x for x in HYBRID]
    print('  %-14s' % 'arm' + ''.join('%10s' % m for m, _ in METRICS))
    for a in arms:
        row = []
        for m, _ in METRICS:
            v = values(a, m)
            row.append('%10.2f' % (sum(v.values()) / len(v)) if len(v) == SEEDS else '%10s' % '--')
        print('  %-14s' % a + ''.join(row))

    print('\nIdentical simulations (every column of the 60 seeds equal)')
    cols = ('psnr', 'ssim', 'loss', 'm1loss', 'usable', 'delay_mean', 'declared', 'false_declared')
    for a, b in (('rules-onlyT', 'rules'), ('hybrid-noT', 'hybrid')):
        same = sum(all(values(a, c)[k] == values(b, c)[k] for c in cols) for k in values(a, 'psnr'))
        print('  %s and %s: %d of %d seeds' % (a, b, same, SEEDS))

    for metric, label in METRICS[:4]:
        alone, inside = effects(metric)
        seven = family({x: with_without(x, metric, 'hybrid') for x in SOLO})
        print('\n%s: effect alone on the rule stack, onlyX - rules, Holm over 7' % label)
        for x in SOLO:
            print('  %s  %s' % (x, fmt(alone[x])))
        print('%s: effect in the hybrid stack, hybrid - noX, Holm over 6 (B: agents - hybrid, one test)' % label)
        for x in SOLO:
            print('  %s  %s  (Holm over 7 with B: %.3g)' % (x, fmt(inside[x]), seven[x][5]))

    print('\nAdditivity, PSNR, per seed')
    joint = diff(values('hybrid', 'psnr'), values('rules', 'psnr'))
    solos = sum_of(HYBRID, 'psnr', 'alone')
    removals = sum_of(HYBRID, 'psnr', 'hybrid')
    print('  joint, hybrid - rules          %s' % fmt(summary(list(joint.values()))))
    print('  sum of the six effects alone   %s' % fmt(summary(list(solos.values()))))
    print('  sum of the six removals        %s' % fmt(summary(list(removals.values()))))
    print('  joint - sum alone (interaction) %s' % fmt(summary([joint[k] - solos[k] for k in joint])))
    print('  joint - sum of removals         %s' % fmt(summary([joint[k] - removals[k] for k in joint])))
    print('  without A: sum alone of P, C, D, T, E %s'
          % fmt(summary(list(sum_of('PCDTE', 'psnr', 'alone').values()))))
    print('  without A: sum of removals of P, C, D, T, E %s'
          % fmt(summary(list(sum_of('PCDTE', 'psnr', 'hybrid').values()))))

    print('\nThe channel-access agent and the five others, PSNR (Holm over the 3)')
    tests = {'hybrid - onlyA (the five others with A)': diff(values('hybrid', 'psnr'), values('rules-onlyA', 'psnr')),
             'noA - rules (the five others without A)': diff(values('hybrid-noA', 'psnr'), values('rules', 'psnr')),
             'noE - onlyA (P, C, D, T with A, rule discard)': diff(values('hybrid-noE', 'psnr'), values('rules-onlyA', 'psnr'))}
    for k, t in family(tests).items():
        print('  %-46s %s' % (k, fmt(t)))

    print('\nInteraction of each agent: effect in the hybrid minus effect alone, PSNR (Holm over 7)')
    inter = family({x: diff(with_without(x, 'psnr', 'hybrid'), with_without(x, 'psnr', 'alone')) for x in SOLO})
    for x in SOLO:
        print('  %s  %s' % (x, fmt(inter[x])))


if __name__ == '__main__':
    main()
