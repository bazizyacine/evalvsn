#!/usr/bin/env python3
"""Every derived number of the results section that the other tools of tools/
do not print (precision pass of the revision, 8 Nov 2026; labels P02 to P36): correlations per simulation, the slope of
the route requests on the declarations, the pooled p of the AOMDV control, the
mechanism at T = 7 s, the oracle, the energy, the HELLO counts and the rest.

One line per number: the block, what is computed, the value as the proposed
text writes it, the value computed here, and OK or DIFFERS. The last lines
count them. Pairs share their seeds (fire, realisation): mean difference, 95 %
Student interval, p, Holm within the family the text names. Rounding is half
up, as everywhere in the chain. Reads src/data/ only: comparative.tsv,
revision.tsv, agent-trace.tsv, break-agent.tsv, overhead.tsv, frames.tsv,
oracle-s10.tsv and codec-frames.tsv; a missing file turns its lines into
MISSING. P38 and P40 compute from the radio and capture constants they name.

Usage: python3 tools/analyse_claims.py
"""
import math
import os
import sys
from decimal import ROUND_HALF_UP, Decimal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'metrics')))
import stats  # noqa: E402

DATA = os.path.join(HERE, '..', 'src', 'data')
SPLITS = ('3M9D', '6M6D', '9M3D')
FIRES = ('F1', 'F2', 'F3')
PERIODS = ('p7', 'p9', 'p12', 'p18', 'p30')
SCENARIO = ('-forest', '-smoke', '-j8', '-swap', '-v15')   # the scenario series of E5, outside these blocks
MAIN8 = (2, 3, 4, 5, 6, 8, 10, 12)                      # main frames after the first image, GOP 9-3


def table(name):
    path = os.path.join(DATA, name)
    if not os.path.exists(path):
        return None
    lines = [l for l in open(path) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:]]


COMP = table('comparative.tsv')
REV = table('revision.tsv')
TRACE = table('agent-trace.tsv')
BRK = table('break-agent.tsv')
OVER = table('overhead.tsv')
FRAMES = table('frames.tsv')
ORACLE = table('oracle-s10.tsv')
CODEC = table('codec-frames.tsv')


# ------------------------------------------------------------------ statistics

def mean(x):
    x = list(x)
    return sum(x) / len(x)


def paired(a, b):
    """Mean of a - b over the shared seeds, its 95 % interval and p."""
    d = [a[k] - b[k] for k in a if k in b]
    n, m = len(d), mean(d)
    s = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1))
    if s == 0:
        return dict(n=n, d=m, lo=m, hi=m, p=1.0 if m == 0 else 0.0)
    h = stats.t_quantile(0.95, n - 1) * s / math.sqrt(n)
    return dict(n=n, d=m, lo=m - h, hi=m + h, p=stats.t_sf2(m / (s / math.sqrt(n)), n - 1))


def holm(ps):
    return [x[-1] for x in stats.holm([(i, '', p) for i, p in enumerate(ps)])]


def pearson(x, y):
    mx, my = mean(x), mean(y)
    sxy = sum((a - mx) * (b - my) for a, b in zip(x, y))
    return sxy / math.sqrt(sum((a - mx) ** 2 for a in x) * sum((b - my) ** 2 for b in y))


def slope(x, y):
    mx, my = mean(x), mean(y)
    return sum((a - mx) * (b - my) for a, b in zip(x, y)) / sum((a - mx) ** 2 for a in x)


def ci_mean(v):
    v = list(v)
    n, m = len(v), mean(v)
    h = stats.t_quantile(0.95, n - 1) * math.sqrt(sum((x - m) ** 2 for x in v) / (n - 1)) / math.sqrt(n)
    return m - h, m + h


# ------------------------------------------------------------------ data access

def rev(arm, split='9M3D', period='p9', m='psnr'):
    return {(r['fire'], r['realisation']): float(r[m]) for r in REV
            if r['arm'] == arm and r['split'] == split and r['period'] == period and r['dist'] == '40' and r[m] != ''}


def pooled(arm, m='psnr'):
    """Per seed, the mean over the three GOPs."""
    ds = [rev(arm, s, m=m) for s in SPLITS]
    return {k: mean(d[k] for d in ds) for k in ds[0] if all(k in d for d in ds)}


def comp(stack, split, fire=None, m='psnr'):
    return {(r['fire'], r['realisation']): float(r[m]) for r in COMP
            if r['stack'] == stack and r['split'] == split and (fire is None or r['fire'] == fire)}


def comp_pooled(stack, m='psnr'):
    ds = [comp(stack, s, m=m) for s in SPLITS]
    return {k: mean(d[k] for d in ds) for k in ds[0]}


def cell_means(stack, m='psnr'):
    return [mean(comp(stack, s, f, m).values()) for s in SPLITS for f in FIRES]


def brk(arm, cols):
    return {(r['fire'], r['realisation']): sum(float(r[c]) for c in cols) for r in BRK
            if r['arm'] == arm and all(r[c] != '' for c in cols)}


def trace(arm, agent, t, col):
    return float(next(r[col] for r in TRACE if r['arm'] == arm and r['agent'] == agent and r['t'] == str(t)))


def frames(arm, period):
    """Per simulation, its twelve image rows."""
    out = {}
    for r in FRAMES:
        if r['arm'] == arm and r['period'] == period:
            out.setdefault((r['fire'], r['realisation']), []).append(r)
    return out


def per_sim(arm, period, f):
    return {k: f(v) for k, v in frames(arm, period).items()}


# ------------------------------------------------------------------ the lines

LINES = []


def fr(x, nd):
    """Half-up rounding, French decimal comma."""
    s = str(Decimal(repr(x)).quantize(Decimal(1).scaleb(-nd), rounding=ROUND_HALF_UP))
    return s.replace('.', ',').replace('-', '−')


def claim(block, label, written, checks):
    """checks: (computed, op, expected, nd); op '=' compares the half-up rounding
    to nd decimals, '<' and '>' compare the raw value, '<=' and '>=' the rounded
    one. A computed None means the source file is missing."""
    if any(c[0] is None for c in checks):
        LINES.append((block, label, written, 'MISSING', 'MISSING'))
        return
    ok, shown = True, []
    for computed, op, expected, nd in checks:
        if isinstance(computed, bool):
            good = computed == expected
            shown.append(str(computed))
        else:
            r = float(Decimal(repr(computed)).quantize(Decimal(1).scaleb(-nd), rounding=ROUND_HALF_UP))
            good = {'=': r == expected, '<': computed < expected, '>': computed > expected,
                    '<=': r <= expected, '>=': r >= expected}[op]
            shown.append(fr(computed, nd + 2) if op in ('<', '>') else fr(computed, nd))
        ok = ok and good
    LINES.append((block, label, written, ' ; '.join(shown), 'OK' if ok else 'DIFFERS'))


def pair_checks(r, d, lo, hi, nd=2, sign=1):
    """The difference and its interval as the text writes them (sign=-1: the
    text writes the magnitude of a negative difference and its bounds swapped)."""
    if sign > 0:
        return [(r['d'], '=', d, nd), (r['lo'], '=', lo, nd), (r['hi'], '=', hi, nd)]
    return [(-r['d'], '=', d, nd), (-r['hi'], '=', lo, nd), (-r['lo'], '=', hi, nd)]


def main():
    if COMP is None or REV is None:
        sys.exit('src/data/comparative.tsv and revision.tsv are needed')

    # P02, AOMDV and the rules
    am, ap, rm = cell_means('aomdv', 'm1loss'), cell_means('aomdv'), cell_means('rules', 'm1loss')
    claim('P02', 'AOMDV, perte de M1 par cellule, min. et max. (%)', '62 à 70', [(min(am), '=', 62, 0), (max(am), '=', 70, 0)])
    claim('P02', 'AOMDV, PSNR par cellule, min. et max. (dB)', '18,0 et 19,1', [(min(ap), '=', 18.0, 1), (max(ap), '=', 19.1, 1)])
    claim('P02', 'pile à règles, perte de M1 par cellule, min. et max. (%)', '14 à 32', [(min(rm), '=', 14, 0), (max(rm), '=', 32, 0)])
    rs = []
    for s in ('aomdv', 'rules'):
        for g in SPLITS:
            p, m = comp(s, g), comp(s, g, m='m1loss')
            k = sorted(p)
            rs.append(pearson([p[x] for x in k], [m[x] for x in k]))
    claim('P02', 'corrélation PSNR et perte de M1 par simulation, AOMDV et règles, six GOP-piles', '−0,87 à −0,99',
          [(max(rs), '=', -0.87, 2), (min(rs), '=', -0.99, 2)])

    # P03, AOMDV control
    r = paired(pooled('pq-ctl'), pooled('pq'))
    claim('P03', 'AOMDV de contrôle moins AOMDV, 60 graines', '0,07 (0,01 à 0,14), p = 0,03',
          pair_checks(r, 0.07, 0.01, 0.14) + [(r['p'], '=', 0.03, 2)])
    hp = holm([paired(rev('pq-ctl', g), rev('pq', g))['p'] for g in SPLITS])
    claim('P03', 'même écart par GOP, Holm sur trois GOP, min. de p', 'p ≥ 0,20', [(min(hp), '>=', 0.20, 2)])
    r = paired(pooled('rules'), pooled('pq-ctl'))
    claim('P03', 'pile à règles moins AOMDV de contrôle, 60 graines', '6,2 (5,7 à 6,6)', pair_checks(r, 6.2, 5.7, 6.6, 1))

    # P05, hybrid - rules per GOP
    for g, v in zip(SPLITS, (1.1, 1.5, 2.0)):
        r = paired(comp('hybrid', g), comp('rules', g))
        claim('P05', 'pile hybride moins règles, GOP ' + g, fr(v, 1), [(r['d'], '=', v, 1)])

    # P05b, agents - rules over the three GOP (E0), quoted with its interval in the
    # results, the introduction, the discussion and the conclusions
    r = paired(comp_pooled('agents'), comp_pooled('rules'))
    claim('P05', 'pile à agents moins règles, trois GOP, 60 graines', '−0,01 (−0,32 à 0,29), p = 0,94',
          pair_checks(r, -0.01, -0.32, 0.29) + [(r['p'], '=', 0.94, 2)])

    # P06, frozen hybrid - rules
    r = paired(pooled('hybrid-frozen'), pooled('rules'))
    claim('P06', 'pile hybride figée moins règles, 60 graines', '−0,04 (−0,41 à 0,33), p = 0,84',
          pair_checks(r, -0.04, -0.41, 0.33) + [(r['p'], '=', 0.84, 2)])
    per = [paired(rev('hybrid-frozen', g), rev('rules', g)) for g in SPLITS]
    claim('P06', 'même écart par GOP : Holm min., |écart| max.', 'aucun GOP ; 0,2 au plus',
          [(min(holm([x['p'] for x in per])), '>', 0.05, 2), (max(abs(x['d']) for x in per), '<=', 0.2, 1)])
    claim('P06', 'même écart par GOP : Holm min. ; le plus grand, GOP 6-6', 'p = 1,00 ; −0,20 (−0,75 à 0,36)',
          [(min(holm([x['p'] for x in per])), '=', 1.00, 2)] + pair_checks(per[SPLITS.index('6M6D')], -0.20, -0.75, 0.36))

    # P07
    r = paired(comp_pooled('hybrid'), comp_pooled('agents'))
    claim('P07', 'pile hybride moins pile à agents, 60 graines', '1,53 (1,20 à 1,87), p < 0,001',
          pair_checks(r, 1.53, 1.20, 1.87) + [(r['p'], '<', 0.001, 3)])

    # P08, upper bounds of the 9-3 cell intervals
    hi_r = [ci_mean(comp('rules', '9M3D', f).values())[1] for f in FIRES]
    hi_a = [ci_mean(comp('agents', '9M3D', f).values())[1] for f in FIRES]
    claim('P08', 'règles, bornes hautes au GOP 9-3, min. et max. (dB)', '28,0 à 28,3', [(min(hi_r), '=', 28.0, 1), (max(hi_r), '=', 28.3, 1)])
    claim('P08', 'pile à agents sous F3, borne haute (dB)', '28,5', [(hi_a[2], '=', 28.5, 1)])
    claim('P08', 'pile à agents sous F1 et F2, bornes hautes sous 28 dB', 'sauf F1 et F2', [(max(hi_a[:2]), '<', 28.0, 2)])

    # P09, P10, break agent and declarations
    if BRK is None:
        claim('P09', 'traces de l\'agent de rupture', '', [(None, '=', 0, 0)])
    else:
        b3, b1 = mean(brk('agents', ['best3']).values()), mean(brk('agents', ['best1']).values())
        tot = mean(brk('agents', ['best1', 'best2', 'best3', 'best5']).values())
        claim('P09', 'couples dont trois échecs ont la plus grande valeur, sur le total', '36 des 41', [(b3, '=', 36, 0), (tot, '=', 41, 0)])
        claim('P09', 'couples dont un seul échec a la plus grande valeur, et part (%)', '0,17 couple, 0,4 %', [(b1, '=', 0.17, 2), (100 * b1 / tot, '=', 0.4, 1)])
        alive = mean(brk('agents', ['alive']).values()); d1 = mean(brk('agents', ['decl1']).values())
        other = mean(brk('agents', ['decl2', 'decl3', 'decl5']).values())
        claim('P10', 'déclarations d\'un seul échec contre un voisin vivant, borne basse (%)', 'au moins 95', [(100 * (alive - other) / d1, '>=', 95, 0)])
    share = lambda a: 100 * mean(rev(a, m='false_declared').values()) / mean(rev(a, m='declared').values())
    sa = {k: 100 * v / rev('agents-tr', m='declared')[k] for k, v in rev('agents-tr', m='false_declared').items() if rev('agents-tr', m='declared')[k] > 0}
    sr = {k: 100 * v / rev('rules', m='declared')[k] for k, v in rev('rules', m='false_declared').items() if rev('rules', m='declared')[k] > 0}
    # the shares the test compares: the mean over the simulations of each one's share
    claim('P10', 'déclarations à tort, moyenne par simulation, pile à agents tracée et règles (%)', '97,6 et 97,7',
          [(mean(sa.values()), '=', 97.6, 1), (mean(sr.values()), '=', 97.7, 1)])
    r = paired(sa, sr)
    claim('P10', 'part à tort, agents moins règles, par graine (points)', '−0,1 (−1,3 à 1,1), p = 0,91',
          pair_checks(r, -0.1, -1.3, 1.1, 1) + [(r['p'], '=', 0.91, 2)])
    claim('P10', 'déclarations par simulation, agents et hybride tracées', '19,9 contre 7,3',
          [(mean(rev('agents-tr', m='declared').values()), '=', 19.9, 1), (mean(rev('hybrid-tr', m='declared').values()), '=', 7.3, 1)])
    rq, re_ = paired(rev('agents-tr', m='rreq_all'), rev('hybrid-tr', m='rreq_all')), paired(rev('agents-tr', m='rerr'), rev('hybrid-tr', m='rerr'))
    claim('P10', 'RREQ et RERR, agents contre hybride tracées, et p', '2 195 contre 1 034 ; 174 contre 61 ; p < 0,001',
          [(mean(rev('agents-tr', m='rreq_all').values()), '=', 2195, 0), (mean(rev('hybrid-tr', m='rreq_all').values()), '=', 1034, 0),
           (mean(rev('agents-tr', m='rerr').values()), '=', 174, 0), (mean(rev('hybrid-tr', m='rerr').values()), '=', 61, 0),
           (max(rq['p'], re_['p']), '<', 0.001, 3)])

    # P11, decay at GOP 9-3
    ra, rh = paired(rev('agents-edec'), rev('agents')), paired(rev('hybrid-edec'), rev('hybrid'))
    h2 = holm([ra['p'], rh['p']])
    claim('P11', 'pile hybride sous décroissance moins pile hybride, Holm sur deux piles', '−0,34 (−0,84 à 0,17), p = 0,19',
          pair_checks(rh, -0.34, -0.84, 0.17) + [(h2[1], '=', 0.19, 2)])
    r = paired(rev('hybrid-edec', m='loss'), rev('hybrid', m='loss'))
    claim('P11', 'même écart, perte globale (points)', '3,1 (1,8 à 4,4), p < 0,001', pair_checks(r, 3.1, 1.8, 4.4, 1) + [(r['p'], '<', 0.001, 3)])

    # P12
    x, y, z = paired(rev('agents-edec'), rev('rules')), paired(rev('agents-edec'), rev('hybrid-edec')), paired(rev('hybrid'), rev('agents-edec'))
    h3 = holm([x['p'], y['p'], z['p']])
    claim('P12', 'agents sous décroissance moins règles, Holm sur trois', '1,49 (0,89 à 2,10), p < 0,001', pair_checks(x, 1.49, 0.89, 2.10) + [(h3[0], '<', 0.001, 3)])
    claim('P12', 'agents sous décroissance moins hybride sous décroissance, Holm sur trois', '−0,19 (−0,67 à 0,29), p = 0,43', pair_checks(y, -0.19, -0.67, 0.29) + [(h3[1], '=', 0.43, 2)])
    claim('P12', 'hybride moins agents sous décroissance, Holm sur trois ; PSNR de la pile hybride', '0,53, p = 0,036 ; 29,31',
          [(z['d'], '=', 0.53, 2), (h3[2], '=', 0.036, 3), (mean(rev('hybrid').values()), '=', 29.31, 2)])
    if BRK is not None:
        e = lambda a, rng: {k: v / 6 for k, v in brk(a, ['img%d' % i for i in rng]).items()}
        r1 = paired(e('hybrid', range(1, 7)), e('agents-edec', range(1, 7)))
        claim('P12', 'images 1 à 6, hybride moins agents sous décroissance', '1,09 (0,44 à 1,74), p = 0,001', pair_checks(r1, 1.09, 0.44, 1.74) + [(r1['p'], '=', 0.001, 3)])
        la, lh = e('agents-edec', range(7, 13)), e('hybrid', range(7, 13))
        claim('P12', 'images 7 à 12, agents sous décroissance et hybride ; p', '29,39 et 29,36 ; p = 0,90',
              [(mean(la.values()), '=', 29.39, 2), (mean(lh.values()), '=', 29.36, 2), (paired(lh, la)['p'], '=', 0.90, 2)])
        da, dh = brk('agents-edec', ['t114', 't132', 't150']), brk('hybrid-edec', ['t114', 't132', 't150'])
        r = paired(da, dh)
        claim('P12', 'déclarations après 114 s, agents et hybride sous décroissance, écart', '1,1 et 3,1 ; −2,6 à −1,4 ; p < 0,001',
              [(mean(da.values()), '=', 1.1, 1), (mean(dh.values()), '=', 3.1, 1), (r['lo'], '=', -2.6, 1), (r['hi'], '=', -1.4, 1), (r['p'], '<', 0.001, 3)])

    # P13 to P15, P35, the agents' traces
    if TRACE is None:
        claim('P13', 'traces des agents', '', [(None, '=', 0, 0)])
    else:
        nodes = [trace('hybrid', a, 999, 'nodes') for a in ('path', 'congestion', 'access')]
        med = [trace('hybrid', a, 999, 'median') for a in ('path', 'congestion', 'access')]
        claim('P13', 'nœuds qui décident, chemin, congestion, accès, min. et max.', '69 à 70', [(min(nodes), '=', 69, 0), (max(nodes), '=', 70, 0)])
        claim('P13', 'médiane des décisions par nœud, min. et max.', '529 à 541', [(min(med), '=', 529, 0), (max(med), '=', 541, 0)])
        before = [trace('hybrid', a, 40, 'median') for a in ('path', 'congestion', 'access')]
        chg = [trace('hybrid', a, t, 'changed') for a in ('path', 'congestion', 'access') for t in (20, 40)]
        claim('P13', 'avant la détection : médiane des décisions, part hors du choix initial (%)', '2 à 3 ; 0,1',
              [(min(before), '=', 2, 0), (max(before), '=', 3, 0), (max(chg), '=', 0.1, 1)])
        claim('P13', 'chemin hors du choix initial, 120 puis 140 s (%)', '17,6 à 19,4',
              [(trace('hybrid', 'path', 120, 'changed'), '=', 17.6, 1), (trace('hybrid', 'path', 140, 'changed'), '=', 19.4, 1)])
        claim('P14', 'découverte, états hors du choix initial (%)', '38,6', [(trace('hybrid', 'discovery', 999, 'changed'), '=', 38.6, 1)])
        # the action columns are rounded to 0.1 in the file: the decisions at another
        # step are their sum, not decisions - a0 (50 680.3 - 50 680.1 = 0.2)
        dec, a0 = trace('hybrid', 'power', 999, 'decisions'), trace('hybrid', 'power', 999, 'a0')
        other = sum(trace('hybrid', 'power', 999, 'a%d' % i) for i in range(1, 9))
        claim('P14', 'puissance : part à +3 dBm (%), décisions à un autre pas, total', 'plus de 99,99 ; 0,1 sur 50 680',
              [(100 * a0 / dec, '>', 99.99, 2), (other, '=', 0.1, 1), (dec, '=', 50680, 0)])
        br = [trace('agents', 'break', t, 'changed') for t in (60, 80, 100, 120, 140)]
        claim('P15', 'détection de rupture hors du choix initial, 60 à 160 s, min. et max. (%)', '8,4 à 10,9', [(min(br), '=', 8.4, 1), (max(br), '=', 10.9, 1)])
        mono = all(all(trace('hybrid', a, t, 'changed') < trace('hybrid', a, t + 20, 'changed') for t in (60, 80, 100, 120))
                   for a in ('path', 'congestion', 'access', 'discard'))
        claim('P15', 'quatre autres agents, hausse d\'un intervalle au suivant de 60 à 160 s', 'augmente', [(mono, '=', True, 0)])
        claim('P35', 'chemin : médiane des décisions avant la détection ; par seconde après', '2 ; cinq par seconde',
              [(trace('hybrid', 'path', 40, 'median'), '=', 2, 0), (trace('hybrid', 'path', 999, 'median') / 108, '=', 5, 0)])
        claim('P35', 'médianes : rejet anticipé, détection de rupture, découverte', '32 ; 2 ; 11',
              [(trace('hybrid', 'discard', 999, 'median'), '=', 32, 0), (trace('agents', 'break', 999, 'median'), '=', 2, 0),
               (trace('hybrid', 'discovery', 999, 'median'), '=', 11, 0)])

    # P16, P17, ablations
    r = paired(rev('rules-onlyA', m='m1loss'), rev('rules', m='m1loss'))
    claim('P16', 'perte de M1, règles avec l\'agent d\'accès moins règles (points)', '5,1 de moins (2,5 à 7,7), p < 0,001',
          pair_checks(r, 5.1, 2.5, 7.7, 1, sign=-1) + [(r['p'], '<', 0.001, 3)])
    a, b, am_, bm_ = rev('rules-onlyA'), rev('rules'), rev('rules-onlyA', m='m1loss'), rev('rules', m='m1loss')
    k = sorted(a)
    claim('P16', 'corrélation des écarts de PSNR et de perte de M1 par graine', '−0,88', [(pearson([a[x] - b[x] for x in k], [am_[x] - bm_[x] for x in k]), '=', -0.88, 2)])
    r = paired(rev('hybrid-frozen', m='declared'), rev('rules', m='declared'))
    claim('P16', 'déclarations, pile hybride figée, et écart aux règles', '22,4 ; +0,35 (−1,26 à 1,96), p = 0,67',
          [(mean(rev('hybrid-frozen', m='declared').values()), '=', 22.4, 1)] + pair_checks(r, 0.35, -1.26, 1.96) + [(r['p'], '=', 0.67, 2)])
    nd = [paired(rev('hybrid-no' + x, m='declared'), rev('hybrid', m='declared')) for x in 'PCDET']
    claim('P16', 'déclarations, retrait de chacun des cinq autres agents : Holm min., écarts min. et max.', 'non significatif ; −0,03 à +0,73',
          [(min(holm([q['p'] for q in nd])), '>', 0.05, 2), (min(q['d'] for q in nd), '=', -0.03, 2), (max(q['d'] for q in nd), '=', 0.73, 2)])
    only = [paired(rev('rules-only' + x), rev('rules')) for x in 'PCBDAET']
    no = [paired(rev('hybrid'), rev('hybrid-no' + x)) for x in 'PCDAET']
    ho, hn = holm([q['p'] for q in only]), holm([q['p'] for q in no])
    others = [ho[i] for i, x in enumerate('PCBDAET') if x in 'PCDET'] + [hn[i] for i, x in enumerate('PCDAET') if x in 'PCDET']
    claim('P17', 'cinq autres agents, seuls et dans la pile hybride, Holm min.', 'p ≥ 0,136', [(min(others), '>=', 0.136, 3)])
    r = paired(rev('hybrid'), rev('rules-onlyA'))
    claim('P17', 'pile hybride moins règles avec le seul agent d\'accès', '+0,02 (−0,49 à 0,54), p = 0,93', pair_checks(r, 0.02, -0.49, 0.54) + [(r['p'], '=', 0.93, 2)])
    same = all(rev('rules-onlyT')[k] == rev('rules')[k] and rev('rules-onlyT', m='loss')[k] == rev('rules', m='loss')[k] for k in rev('rules'))
    claim('P17', 'agent de puissance seul : 60 simulations identiques aux règles, PSNR et pertes', 'identiques', [(same, '=', True, 0)])
    six = ['rules-only' + x for x in 'PCDATE']
    base = rev('rules')
    inter = {k: (rev('hybrid')[k] - base[k]) - sum(rev(s)[k] - base[k] for s in six) for k in base}
    r = paired(inter, {k: 0.0 for k in inter})
    claim('P17', 'interaction, intervalle', '−2,03 à +1,30', [(r['lo'], '=', -2.03, 2), (r['hi'], '=', 1.30, 2)])
    dif = []
    for x in 'PCDATE':
        e_in = {s: rev('hybrid')[s] - rev('hybrid-no' + x)[s] for s in base}
        e_al = {s: rev('rules-only' + x)[s] - base[s] for s in base}
        dif.append(paired(e_in, e_al))
    claim('P17', 'effet dans la pile hybride moins effet seul, six agents : Holm min., écarts', 'non significatif ; −0,27 à +0,33',
          [(min(holm([q['p'] for q in dif])), '>', 0.05, 2), (min(q['d'] for q in dif), '=', -0.27, 2), (max(q['d'] for q in dif), '=', 0.33, 2)])
    claim('P17', 'agent d\'accès seul ; six agents ; agent de rupture dans la pile hybride (dB)', '2,00 ; 2,02 ; 2,07',
          [(paired(rev('rules-onlyA'), base)['d'], '=', 2.00, 2), (paired(rev('hybrid'), base)['d'], '=', 2.02, 2),
           (-paired(rev('agents'), rev('hybrid'))['d'], '=', 2.07, 2)])
    claim('P17', 'déclarations : règles, avec l\'agent d\'accès, avec l\'agent de rupture', '22,0 ; 7,4 ; 28,1',
          [(mean(rev('rules', m='declared').values()), '=', 22.0, 1), (mean(rev('rules-onlyA', m='declared').values()), '=', 7.4, 1),
           (mean(rev('rules-onlyB', m='declared').values()), '=', 28.1, 1)])

    # P18, level M1
    rr = []
    for g in SPLITS:
        a, b, am_, bm_ = comp('hybrid', g), comp('rules', g), comp('hybrid', g, m='m1loss'), comp('rules', g, m='m1loss')
        k = sorted(a)
        rr.append(pearson([a[x] - b[x] for x in k], [am_[x] - bm_[x] for x in k]))
    claim('P18', 'corrélation des écarts de PSNR et de perte de M1, hybride et règles, par GOP', '−0,84 à −0,93', [(max(rr), '=', -0.84, 2), (min(rr), '=', -0.93, 2)])
    hm = holm([paired(comp('hybrid', g, m='m1loss'), comp('rules', g, m='m1loss'))['p'] for g in SPLITS])
    claim('P18', 'perte de M1, hybride moins règles par GOP, Holm max.', 'p ≤ 0,032', [(max(hm), '<=', 0.032, 3)])
    r = paired(comp_pooled('hybrid', 'loss'), comp_pooled('rules', 'loss'))
    claim('P18', 'perte globale, hybride moins règles, 60 graines (points)', '8,9 de moins (7,3 à 10,5), p < 0,001',
          pair_checks(r, 8.9, 7.3, 10.5, 1, sign=-1) + [(r['p'], '<', 0.001, 3)])

    # P19, the gallery frame
    if FRAMES is None:
        claim('P19', 'frame 8 de la galerie', '', [(None, '=', 0, 0)])
    else:
        f8 = {r['arm']: r for r in FRAMES if r['period'] == 'p9' and r['fire'] == 'F1' and r['realisation'] == 'r01' and r['image'] == '8'}
        g = lambda arm, c: float(f8[arm][c]) if arm in f8 else None
        claim('P19', 'frame 8 sous AOMDV : perte, perte de M1 (%)', '100 ; 100', [(g('pq', 'loss'), '=', 100, 0), (g('pq', 'l1loss'), '=', 100, 0)])
        claim('P19', 'frame 8, règles et hybride : perte (%), perte de M1 (%), PSNR (dB)', '40 et 36 ; 0 ; 33,2 et 32,5',
              [(g('rules', 'loss'), '=', 40, 0), (g('hybrid', 'loss'), '=', 36, 0), (g('rules', 'l1loss'), '=', 0, 0), (g('hybrid', 'l1loss'), '=', 0, 0),
               (g('rules', 'psnr'), '=', 33.2, 1), (g('hybrid', 'psnr'), '=', 32.5, 1)])
        claim('P19', 'frame 8, pile à agents : perte de M1 (%), PSNR (dB)', '19 ; 22,8', [(g('agents', 'l1loss'), '=', 19, 0), (g('agents', 'psnr'), '=', 22.8, 1)])

    # P21, P23
    hd = holm([paired(rev('hybrid', g, m='delay_mean'), rev('rules', g, m='delay_mean'))['p'] for g in SPLITS])
    claim('P21', 'délai moyen, hybride moins règles, Holm par GOP (3-9, 6-6, 9-3)', '0,17 ; < 0,001 ; < 0,001',
          [(hd[0], '=', 0.17, 2), (hd[1], '<', 0.001, 3), (hd[2], '<', 0.001, 3)])
    hl = holm([paired(rev('hybrid', g, m='late'), rev('rules', g, m='late'))['p'] for g in SPLITS])
    claim('P23', 'échéances manquées, hybride moins règles, Holm min.', 'p ≥ 0,13', [(min(hl), '>=', 0.13, 4)])
    al = cell_means('aomdv', 'loss')
    claim('P23', 'AOMDV, perte par cellule, min. et max. (%)', '74 à 79', [(min(al), '=', 74, 0), (max(al), '=', 79, 0)])

    # P25, ranking per cell
    first = last = agents_ahead = 0
    for s in SPLITS:
        for f in FIRES:
            m = {st: mean(comp(st, s, f).values()) for st in ('aomdv', 'rules', 'agents', 'hybrid')}
            first += max(m, key=m.get) == 'hybrid'
            last += min(m, key=m.get) == 'aomdv'
            agents_ahead += m['agents'] > m['rules']
    ha = holm([paired(comp('agents', s, f), comp('rules', s, f))['p'] for s in SPLITS for f in FIRES])
    claim('P25', 'cellules : hybride en tête, AOMDV dernière, agents devant les règles', '9 ; 9 ; 4',
          [(first, '=', 9, 0), (last, '=', 9, 0), (agents_ahead, '=', 4, 0)])
    claim('P25', 'pile à agents moins règles, Holm sur neuf cellules, min.', 'aucune cellule', [(min(ha), '>', 0.05, 2)])

    # P26, P27, volume (packet counts of the methods: 5 738, 5 544 and 5 352)
    claim('P26', 'GOP 3-9 à 9 s : paquets par seconde ; excès sur le GOP 9-3 (%) ; paquets de plus par frame D', '53,1 ; 7 ; 64',
          [(5738 / (12 * 9), '=', 53.1, 1), (100 * (5738 / 5352 - 1), '=', 7, 0), ((5738 - 5352) / 6, '=', 64, 0)])
    sh = {}
    for s in ('rules', 'hybrid'):
        sh[s, '3M9D'] = paired(rev(s, '3M9D', 'p9r65'), rev(s, '3M9D'))
        sh[s, '6M6D'] = paired(rev(s, '6M6D', 'p9r32'), rev(s, '6M6D'))
    keys = [('rules', '3M9D'), ('hybrid', '3M9D'), ('rules', '6M6D'), ('hybrid', '6M6D')]
    h4 = dict(zip(keys, holm([sh[k]['p'] for k in keys])))
    full = {s: paired(rev(s, '9M3D'), rev(s, '3M9D')) for s in ('rules', 'hybrid')}
    claim('P26', 'part du volume, règles au GOP 3-9 : dB, % de l\'effet, Holm sur quatre', '0,61 ; 11 ; p = 0,19',
          [(sh['rules', '3M9D']['d'], '=', 0.61, 2), (100 * sh['rules', '3M9D']['d'] / full['rules']['d'], '=', 11, 0), (h4['rules', '3M9D'], '=', 0.19, 2)])
    r1, r2 = paired(rev('rules', '9M3D'), rev('rules', '6M6D', 'p9r32')), paired(rev('hybrid', '9M3D'), rev('hybrid', '6M6D', 'p9r32'))
    claim('P26', 'GOP 9-3 moins GOP 6-6 à débit égal, règles et hybride', '1,82 (1,15 à 2,49) ; 2,00 (1,47 à 2,54)',
          pair_checks(r1, 1.82, 1.15, 2.49) + pair_checks(r2, 2.00, 1.47, 2.54))
    claim('P26', 'même écart à 9 s ; part du volume au GOP 6-6, Holm min.', '1,92 et 2,46 ; p ≥ 0,12',
          [(paired(rev('rules', '9M3D'), rev('rules', '6M6D'))['d'], '=', 1.92, 2), (paired(rev('hybrid', '9M3D'), rev('hybrid', '6M6D'))['d'], '=', 2.46, 2),
           (min(h4['rules', '6M6D'], h4['hybrid', '6M6D']), '>=', 0.12, 2)])
    eqd = {s: paired(rev(s, '9M3D'), rev(s, '3M9D', 'p9r65')) for s in ('rules', 'hybrid')}
    claim('P26', 'GOP 9-3 moins GOP 3-9 à débit égal, règles et hybride', '5,12 (4,39 à 5,85) ; 5,63 (5,11 à 6,14)',
          pair_checks(eqd['rules'], 5.12, 4.39, 5.85) + pair_checks(eqd['hybrid'], 5.63, 5.11, 6.14))
    claim('P26', 'même écart à 9 s, règles et hybride', '5,72 et 6,66', [(full['rules']['d'], '=', 5.72, 2), (full['hybrid']['d'], '=', 6.66, 2)])
    claim('P26', 'part du volume, hybride au GOP 3-9 : dB, % de l\'effet, Holm sur quatre', '1,04 ; 16 ; p = 0,009',
          [(sh['hybrid', '3M9D']['d'], '=', 1.04, 2), (100 * sh['hybrid', '3M9D']['d'] / full['hybrid']['d'], '=', 16, 0), (h4['hybrid', '3M9D'], '=', 0.009, 3)])
    sr_ = {k: rev('rules', '3M9D', 'p9r65')[k] - rev('rules', '3M9D')[k] for k in rev('rules', '3M9D')}
    sh_ = {k: rev('hybrid', '3M9D', 'p9r65')[k] - rev('hybrid', '3M9D')[k] for k in rev('hybrid', '3M9D')}
    claim('P26', 'parts du volume, hybride contre règles, par graine', 'p = 0,41', [(paired(sh_, sr_)['p'], '=', 0.41, 2)])
    kept = [100 * paired(rev(s, '9M3D'), rev(s, '3M9D', 'p9r65'))['d'] / full[s]['d'] for s in ('rules', 'hybrid')]
    claim('P27', 'effet du GOP gardé à débit égal, règles et hybride (%)', '89 et 84', [(kept[0], '=', 89, 0), (kept[1], '=', 84, 0)])
    m1 = [paired(rev(s, '3M9D', 'p9r65', 'm1loss'), rev(s, '9M3D', m='m1loss')) for s in ('rules', 'hybrid')]
    claim('P27', 'perte de M1, GOP 3-9 à débit égal moins GOP 9-3, règles et hybride (points)', '12,3 (8,4 à 16,3) ; 11,6 (8,9 à 14,2)',
          pair_checks(m1[0], 12.3, 8.4, 16.3, 1) + pair_checks(m1[1], 11.6, 8.9, 14.2, 1))
    eq = [paired(rev('hybrid', '6M6D', 'p9r32'), rev('rules', '6M6D', 'p9r32')), paired(rev('hybrid', '3M9D', 'p9r65'), rev('rules', '3M9D', 'p9r65'))]
    claim('P27', 'hybride moins règles à débit égal, GOP 6-6 et 3-9, Holm sur deux', '1,84 (1,22 à 2,45) ; 1,51 (0,80 à 2,22) ; p < 0,001',
          pair_checks(eq[0], 1.84, 1.22, 2.45) + pair_checks(eq[1], 1.51, 0.80, 2.22) + [(max(holm([q['p'] for q in eq])), '<', 0.001, 3)])

    # P37, the quality coefficient 0.1, second series of E4 (decisions d83 and d84),
    # its sets of tranche 10, simulated for 168 s like their counterparts:
    # the two families fixed before the results, then the losses and the usable
    # images described without a test (the rate stays in the table, d84).
    stk = ('pq', 'rules', 'agents', 'hybrid')
    qc = [paired(rev(s + '-q01'), rev(s)) for s in stk]
    hq = holm([q['p'] for q in qc])
    claim('P37', 'PSNR au coefficient 0,1 moins 0,01 : AOMDV, règles, agents, hybride (dB) ; Holm sur quatre, max.', '1,45 ; 1,27 ; 1,25 ; 0,96 ; p < 0,001',
          [(q['d'], '=', v, 2) for q, v in zip(qc, (1.45, 1.27, 1.25, 0.96))] + [(max(hq), '<', 0.001, 3)])
    q1 = [paired(rev('hybrid-q01'), rev('rules-q01')), paired(rev('agents-q01'), rev('rules-q01'))]
    h1 = holm([q['p'] for q in q1])
    claim('P37', 'coefficient 0,1 : hybride moins règles (dB), Holm sur deux', '1,71 ; p < 0,001', [(q1[0]['d'], '=', 1.71, 2), (h1[0], '<', 0.001, 3)])
    claim('P37', 'coefficient 0,1 : agents moins règles (dB), Holm sur deux', '−0,08 ; p = 0,79', [(q1[1]['d'], '=', -0.08, 2), (h1[1], '=', 0.79, 2)])
    cm = lambda arm, m: mean(rev(arm, m=m).values())
    claim('P37', 'coefficient 0,1 : la perte globale baisse avec les quatre piles', 'baisse',
          [(all(cm(s + '-q01', 'loss') < cm(s, 'loss') for s in stk), '=', True, 0)])
    claim('P37', 'coefficient 0,1 : les images exploitables augmentent avec les trois piles inter-couches', 'augmentent',
          [(all(cm(s + '-q01', 'usable') > cm(s, 'usable') for s in stk[1:]), '=', True, 0)])

    # P28 to P30, load
    xl = [mean(rev(s, period=p).values()) for s in ('rules', 'hybrid', 'agents') for p in ('p18', 'p30')]
    claim('P28', 'piles inter-couches à 18 et 30 s, PSNR min. et max. (dB)', '59,00 à 62,74', [(min(xl), '=', 59.00, 2), (max(xl), '=', 62.74, 2)])
    hy = [paired(rev('hybrid', period=p), rev('rules', period=p)) for p in PERIODS]
    h5 = holm([q['p'] for q in hy])
    claim('P28', 'hybride moins règles à 9 et 12 s, Holm sur cinq périodes', '2,02 ; 2,42 (1,82 à 3,02) ; p < 0,001',
          [(hy[1]['d'], '=', 2.02, 2)] + pair_checks(hy[2], 2.42, 1.82, 3.02) + [(max(h5[1], h5[2]), '<', 0.001, 3)])
    u7 = [mean(rev(s, period='p7', m='usable').values()) for s in ('hybrid', 'rules')]
    claim('P28', '7 s : hybride moins règles (dB), Holm ; images exploitables, hybride et règles', '0,92 de moins (0,57 à 1,27), p < 0,001 ; 0,35 contre 2,33',
          pair_checks(hy[0], 0.92, 0.57, 1.27, sign=-1) + [(h5[0], '<', 0.001, 3), (u7[0], '=', 0.35, 2), (u7[1], '=', 2.33, 2)])
    claim('P28', 'hybride moins règles à 18 s, Holm sur cinq périodes', '+0,89 (−0,18 à 1,97), p = 0,205', pair_checks(hy[3], 0.89, -0.18, 1.97) + [(h5[3], '=', 0.205, 3)])
    claim('P28', 'hybride moins règles à 30 s, Holm sur cinq périodes', '−0,53 (−1,44 à 0,38), p = 0,247', pair_checks(hy[4], -0.53, -1.44, 0.38) + [(h5[4], '=', 0.247, 3)])
    lo7, de7 = paired(rev('hybrid', period='p7', m='loss'), rev('rules', period='p7', m='loss')), paired(rev('hybrid', period='p7', m='declared'), rev('rules', period='p7', m='declared'))
    claim('P29', '7 s : perte (%) et déclarations, règles et hybride ; p', '63,5 et 58,1 ; 17,3 et 7,1 ; p < 0,001',
          [(mean(rev('rules', period='p7', m='loss').values()), '=', 63.5, 1), (mean(rev('hybrid', period='p7', m='loss').values()), '=', 58.1, 1),
           (mean(rev('rules', period='p7', m='declared').values()), '=', 17.3, 1), (mean(rev('hybrid', period='p7', m='declared').values()), '=', 7.1, 1),
           (max(lo7['p'], de7['p']), '<', 0.001, 3)])
    r = paired(rev('hybrid', period='p7', m='m1loss'), rev('rules', period='p7', m='m1loss'))
    claim('P29', '7 s : perte de M1, hybride moins règles (points)', '+1,3 (−0,7 à 3,4), p = 0,20', pair_checks(r, 1.3, -0.7, 3.4, 1) + [(r['p'], '=', 0.20, 2)])
    # 12 s: the M1 loss and the overall loss, hybrid minus rules (single tests, section 3.10 and 4.1)
    r = paired(rev('hybrid', period='p12', m='m1loss'), rev('rules', period='p12', m='m1loss'))
    claim('P29', '12 s : perte de M1, hybride moins règles (points)', '−1,1 (−2,9 à 0,8), p = 0,25', pair_checks(r, -1.1, -2.9, 0.8, 1) + [(r['p'], '=', 0.25, 2)])
    r = paired(rev('hybrid', period='p12', m='loss'), rev('rules', period='p12', m='loss'))
    claim('P29', '12 s : perte globale, hybride moins règles (points)', '10,6 de moins (8,4 à 12,8), p < 0,001', pair_checks(r, 10.6, 8.4, 12.8, 1, sign=-1) + [(r['p'], '<', 0.001, 4)])
    if FRAMES is None:
        claim('P29', 'mécanisme à 7 s', '', [(None, '=', 0, 0)])
    else:
        le5 = lambda arm: per_sim(arm, 'p7', lambda v: sum(1 for i in v if int(i['image']) in MAIN8 and float(i['l1loss']) <= 5))
        a, b = le5('hybrid'), le5('rules')
        r = paired(a, b)
        claim('P29', 'frames principales 2 à 12 à au plus 5 % de perte de M1, règles et hybride ; écart', '2,77 ; 0,47 ; −2,30 (−2,68 à −1,92), p < 0,001',
              [(mean(b.values()), '=', 2.77, 2), (mean(a.values()), '=', 0.47, 2)] + pair_checks(r, -2.30, -2.68, -1.92) + [(r['p'], '<', 0.001, 3)])
        main = lambda arm: [i for v in frames(arm, 'p7').values() for i in v if int(i['image']) in MAIN8]
        cls = lambda arm, lo, hi: [float(i['psnr']) for i in main(arm) if lo <= float(i['l1loss']) < hi]
        claim('P29', 'PSNR d\'une frame principale sans perte de M1, règles et hybride (dB)', '31,1', [(mean(cls('rules', 0, 2)), '=', 31.1, 1), (mean(cls('hybrid', 0, 2)), '=', 31.1, 1)])
        claim('P29', 'PSNR pour 10 à 15 % de perte de M1, règles et hybride (dB)', '24,5', [(mean(cls('rules', 10, 15)), '=', 24.5, 1), (mean(cls('hybrid', 10, 15)), '=', 24.5, 1)])
        claim('P29', 'frames principales dans la classe de 10 à 15 %, hybride et règles (%)', '57 et 16',
              [(100 * len(cls('hybrid', 10, 15)) / len(main('hybrid')), '=', 57, 0), (100 * len(cls('rules', 10, 15)) / len(main('rules')), '=', 16, 0)])
        out8 = sum(1 for arm in ('rules', 'hybrid') for v in frames(arm, 'p7').values() for i in v
                   if float(i['psnr']) >= 28 and int(i['image']) not in MAIN8)
        claim('P29', 'images exploitables à 7 s hors des huit frames principales', '0 (toutes viennent de ces huit frames)', [(out8, '=', 0, 0)])
        bins = ((0, 2), (2, 5), (5, 10), (10, 15), (15, 20), (20, 30), (30, 50), (50, 101))
        which = lambda x: next(j for j, (lo, hi) in enumerate(bins) if lo <= x < hi)
        pool = {}
        for arm in ('rules', 'hybrid'):
            for v in frames(arm, 'p7').values():
                for i in v:
                    pool.setdefault((i['type'], i['image'] == '1', which(float(i['l1loss']))), []).append(float(i['psnr']))
        mp = {key: mean(v) for key, v in pool.items()}
        pred = lambda arm: per_sim(arm, 'p7', lambda v: mean(mp[(i['type'], i['image'] == '1', which(float(i['l1loss'])))] for i in v))
        r, o = paired(pred('hybrid'), pred('rules')), paired(rev('hybrid', period='p7'), rev('rules', period='p7'))
        claim('P29', 'option : écart prédit par la classe de perte de M1, et écart mesuré (dB)', '−0,75 (−1,02 à −0,48) ; −0,92',
              pair_checks(r, -0.75, -1.02, -0.48) + [(o['d'], '=', -0.92, 2)])
    ag = [paired(rev('agents', period=p), rev('rules', period=p)) for p in PERIODS]
    hg = holm([q['p'] for q in ag])
    claim('P29', 'pile à agents moins règles, Holm sur cinq périodes : 7 s, autres périodes', '< 0,001 ; non significatif',
          [(hg[0], '<', 0.001, 3), (min(hg[1:]), '>', 0.05, 2)])
    r = paired(rev('agents', period='p7', m='m1loss'), rev('rules', period='p7', m='m1loss'))
    claim('P29', '7 s : pile à agents moins règles, PSNR (dB) et perte de M1 (points)', '1,76 de moins (1,41 à 2,11) ; +7,3 (5,0 à 9,6)',
          pair_checks(ag[0], 1.76, 1.41, 2.11, sign=-1) + pair_checks(r, 7.3, 5.0, 9.6, 1))
    gaps = [mean(rev('rules', period=p).values()) - mean(rev('pq', period=p).values()) for p in PERIODS]
    lastp = all(min(('pq', 'rules', 'hybrid', 'agents'), key=lambda s: mean(rev(s, period=p).values())) == 'pq' for p in PERIODS)
    claim('P29', 'AOMDV dernière à chaque période ; écart aux règles min. et max. (dB) ; perte à 30 s (%)', 'oui ; 5,72 à 23,12 ; 30,9',
          [(lastp, '=', True, 0), (min(gaps), '=', 5.72, 2), (max(gaps), '=', 23.12, 2), (mean(rev('pq', period='p30', m='loss').values()), '=', 30.9, 1)])
    if ORACLE is None:
        claim('P29', 'oracle des quatre nouvelles périodes : simulations, paquets perdus sans chemin', '960 ; 0', [(None, '=', 0, 0)])
        claim('P32', 'oracle des sept variantes : simulations, paquets perdus sans chemin', '840 ; 0', [(None, '=', 0, 0)])
    else:
        load = [r for r in ORACLE if r['period'] in ('p7', 'p12', 'p18', 'p30')]
        sens = [r for r in ORACLE if r['period'] == 'p9' and any(t in r['arm'] for t in ('-az', '-pf', '-fd1'))]
        claim('P29', 'oracle des quatre nouvelles périodes : simulations, paquets perdus sans chemin', '960 ; 0',
              [(len(load), '=', 960, 0), (sum(int(r['unreachable']) for r in load), '=', 0, 0)])
        claim('P32', 'oracle des sept variantes : simulations, paquets perdus sans chemin', '840 ; 0',
              [(len(sens), '=', 840, 0), (sum(int(r['unreachable']) for r in sens), '=', 0, 0)])
    claim('P30', 'perte des règles à 9, 12, 18 et 30 s (%)', '56,2 ; 35,6 ; 1,5 ; 0,5',
          [(mean(rev('rules', period=p, m='loss').values()), '=', v, 1) for p, v in (('p9', 56.2), ('p12', 35.6), ('p18', 1.5), ('p30', 0.5))])

    # P32, sensitivity
    sl = [mean(rev('rules-' + v, m='loss').values()) for v in ('az015', 'az05', 'az165', 'pf0', 'pf025', 'pf075', 'fd1')]
    claim('P32', 'perte des règles sur les sept variantes, min. et max. ; référence (%)', '55,5 à 56,5 ; 56,2',
          [(min(sl), '=', 55.5, 1), (max(sl), '=', 56.5, 1), (mean(rev('rules', m='loss').values()), '=', 56.2, 1)])

    # P33, P36, energy (radio powers of radio-params.tcl: 37 and 42 mW)
    if OVER is None:
        claim('P33', 'énergie', '', [(None, '=', 0, 0)])
    else:
        s3 = s5 = 0.0
        for g in SPLITS:
            for f in FIRES:
                e = {r['stack']: float(r['e_node']) for r in OVER if r['split'] == g and r['fire'] == f}
                x = [e[s] for s in ('rules', 'agents', 'hybrid')]
                s3, s5 = max(s3, (max(x) - min(x)) / min(x)), max(s5, (max(e.values()) - min(e.values())) / min(e.values()))
        claim('P33', 'écart d\'énergie maximal par cellule, piles inter-couches et cinq piles (%)', '0,09 ; 0,45', [(100 * s3, '=', 0.09, 2), (100 * s5, '=', 0.45, 2)])
        claim('P36', 'écart d\'énergie entre piles inter-couches, sous 0,1 %', 'moins de 0,1', [(100 * s3, '<', 0.1, 2)])
    claim('P33', 'émission continue face à l\'écoute, 42 contre 37 mW (%)', '14', [(100 * (42 / 37 - 1), '=', 14, 0)])
    claim('P36', 'opérations par paquet : 20 comparaisons + 4 × 3 mises à jour + 8 tirages', '40', [(20 + 4 * 3 + 8, '=', 40, 0)])

    # P34, control traffic
    hel = [mean(rev(a, m='hello').values()) for a in ('rules', 'hybrid-tr', 'agents-tr')]
    claim('P34', 'HELLO par simulation, piles inter-couches min. et max. ; AOMDV', '3 212 à 3 214 ; 16 153',
          [(min(hel), '=', 3212, 0), (max(hel), '=', 3214, 0), (mean(rev('pq', m='hello').values()), '=', 16153, 0)])
    fs = [share(a) for a in ('rules', 'hybrid-frozen', 'agents-frozen', 'agents-tr', 'hybrid-tr')]
    claim('P34', 'déclarations à tort, cinq piles du tableau des contrôles, min. et max. (%)', '90 à 98', [(min(fs), '=', 90, 0), (max(fs), '=', 98, 0)])
    # every cross-layer set at GOP 9-3 and T = 9 s, the scenario variants of E5 included
    # (the 30 m grid too, hence the dist of each arm rather than rev()'s 40 m)
    arms = sorted({(r['arm'], r['dist']) for r in REV
                   if not r['arm'].startswith('pq') and r['split'] == '9M3D' and r['period'] == 'p9'})
    xs, ys, n = [], [], 0
    for a, dist in arms:
        d, q = ({(r['fire'], r['realisation']): float(r[m]) for r in REV if r['arm'] == a and r['dist'] == dist
                 and r['split'] == '9M3D' and r['period'] == 'p9' and r[m] != ''} for m in ('declared', 'rreq_all'))
        k = [x for x in d if x in q]
        if len(k) == 60:
            n += 1
            xs += [d[x] for x in k]
            ys += [q[x] for x in k]
    claim('P34', 'RREQ par déclaration : configurations, simulations, pente, corrélation', '49 ; 2 940 ; 83 ; 0,95',
          [(n, '=', 49, 0), (len(xs), '=', 2940, 0), (slope(xs, ys), '=', 83, 0), (pearson(xs, ys), '=', 0.95, 2)])

    # P38 to P41, final audit of 12 Nov 2026 (findings M1, I9, I5 and M18)
    # P38, the margin of a 40 m link (section 3.4): the shadowing model with
    # sigma 0 (log-distance) and the radio of experiments/run/radio-params.tcl,
    # Friis at 1 m (Pt 0.002 W, 2.4 GHz, unit gains, L = 1), exponent 3.2,
    # against CSThresh_ 7.94e-13 W (-91 dBm); the step down needs 1 + 1.7 dB
    lam = 299792458 / 2.4e9
    pr1 = 0.002 * lam ** 2 / (4 * math.pi) ** 2
    marge = 10 * math.log10(pr1 / 7.94e-13) - 10 * 3.2 * math.log10(40)
    claim('P38', 'marge d\'un lien de 40 m au-dessus de P_CCA, sans interférence (dB) ; sous les 2,7 exigés', '2,69 ; oui',
          [(marge, '=', 2.69, 2), (marge < 1 + (3 - 1.3), '=', True, 0)])
    # P39, the coded frames against the raw image (section 2.6)
    if CODEC is None:
        claim('P39', 'image brute (octets) ; tailles moyennes M et D au-dessus, trois GOP', '25 344 ; oui', [(None, '=', 0, 0)])
    else:
        cv = {r['key']: r['value'] for r in CODEC}
        sizes = [float(cv['%s %s mean bytes' % (g, t)]) for g in ('3-9', '6-6', '9-3') for t in 'MD']
        raw = float(cv['raw image'])
        claim('P39', 'image brute (octets) ; tailles moyennes M et D au-dessus, trois GOP', '25 344 ; oui',
              [(raw, '=', 25344, 0), (min(sizes) > raw, '=', True, 0)])
    # P40, the equal-rate periods (5 352 packets of GOP 9-3 against 5 544 and
    # 5 738) and the delay of the twelfth capture, at 60 + 11 T s (sets.conf,
    # capture period:ipt:detect), against 159 s at 9 s (section 4.4)
    t66, t39 = 9 * 5544 / 5352, 9 * 5738 / 5352
    claim('P40', 'périodes à débit égal, GOP 6-6 et 3-9 (s)', '9,32 et 9,65', [(t66, '=', 9.32, 2), (t39, '=', 9.65, 2)])
    claim('P40', 'dernière capture retardée à 9,32 et 9,65 s, 11 × (T − 9) (s)', '3,5 et 7,2',
          [(11 * (9.32 - 9), '=', 3.5, 1), (11 * (9.65 - 9), '=', 7.2, 1)])
    # P41, hybrid - rules at GOP 9-3 (section 3.3), Holm over the three GOPs
    hg = [paired(comp('hybrid', g), comp('rules', g)) for g in SPLITS]
    claim('P41', 'pile hybride moins règles au GOP 9-3, Holm sur trois GOP', '2,02 ; p < 0,001',
          [(hg[2]['d'], '=', 2.02, 2), (holm([x['p'] for x in hg])[2], '<', 0.001, 3)])

    width = max(len(l[1]) for l in LINES)
    for block, label, written, computed, verdict in LINES:
        print('%-4s  %-*s  texte : %-48s  calcul : %-42s  %s' % (block, width, label, written, computed, verdict))
    ok = sum(1 for l in LINES if l[4] == 'OK')
    bad = [l for l in LINES if l[4] == 'DIFFERS']
    miss = [l for l in LINES if l[4] == 'MISSING']
    print('\n%d lignes : %d OK, %d DIFFERS, %d MISSING' % (len(LINES), ok, len(bad), len(miss)))
    for l in bad + miss:
        print('  %s %s : %s' % (l[4], l[0], l[1]))
    # the scenario variants of E5 (G01 to G09) have their own module
    print('\n--- E5, variantes du scénario (analyse_claims_generalisation.py) ---')
    sys.path.insert(0, HERE)
    import analyse_claims_generalisation as gen
    gen_bad = gen.main()
    # the tuned AOMDV series of E1 (A01 to A11) has its own module too
    print('\n--- E1, AOMDV réglé (analyse_claims_aomdv.py) ---')
    import analyse_claims_aomdv as aomdv
    return 1 if bad or gen_bad or aomdv.main() else 0


if __name__ == '__main__':
    sys.exit(main())
