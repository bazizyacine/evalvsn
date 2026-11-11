#!/usr/bin/env python3
"""Every number of the scenario series of E5 (study 10, tranche T5; labels G01 to
G09), in the style of tools/analyse_claims.py: the new results subsection
(scenario variants), its methods sentences, the limits, the conclusions,
the abstract sentence and the reply letters,
and the numbers of the revision's notes (G09).

One line per number: the block, what is computed, the value as the draft
writes it, the value computed here, and OK or DIFFERS. Pairs share their seeds
(fire, realisation): mean difference, 95 % Student interval, p, Holm over the
five variants within each family, as analyse_generalisation.py. Rounding is
half up. Reads src/data/revision.tsv, the oracle from src/data/oracle-s10.tsv
(tools/compile_oracle.py), the topology, the nodes out of service and the
videos' packets from src/data/methode-mesures.tsv (tools/methode_mesures.py).
A missing file turns its lines
into MISSING.

Usage: python3 tools/analyse_claims_generalisation.py   (also run by tools/analyse_claims.py)
"""
import math
import os
import re
import sys
from decimal import ROUND_HALF_UP, Decimal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'metrics')))
import stats  # noqa: E402

DATA = os.path.join(HERE, '..', 'src', 'data')
VARIANTS = ('forest', 'smoke', 'j8', 'swap', 'v15')
FIRES = ('F1', 'F2', 'F3')
STACKS = ('pq', 'rules', 'hybrid', 'agents')


def table(path):
    if not os.path.exists(path):
        return None
    lines = [l for l in open(path) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:]]


REV = table(os.path.join(DATA, 'revision.tsv'))
ORACLE = None
_o = table(os.path.join(DATA, 'oracle-s10.tsv'))
if _o:
    ORACLE = [r for r in _o if r['arm'].endswith(tuple('-' + v for v in VARIANTS))] or None
_m = table(os.path.join(DATA, 'methode-mesures.tsv')) or []
MM = {(r['kind'], r['key']): r['value'] for r in _m}


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


def diff(a, b):
    return {k: a[k] - b[k] for k in a if k in b}


# ------------------------------------------------------------------ data access

def arm(stack, v=None):
    return stack if v is None else '%s-%s' % (stack, v)


def rev10(a, m='psnr', fire=None):
    """GOP 9-3, T = 9 s; the 30 m grid's sets carry d30, the others d40."""
    dist = '30' if a.endswith('-j8') else '40'
    return {(r['fire'], r['realisation']): float(r[m]) for r in REV
            if r['arm'] == a and r['split'] == '9M3D' and r['period'] == 'p9' and r['dist'] == dist
            and r[m] != '' and (fire is None or r['fire'] == fire)}


def gap(a, b, v=None, m='psnr'):
    return paired(rev10(arm(a, v), m), rev10(arm(b, v), m))


def gaps(a, b, m='psnr'):
    """The five variants of one family, with their Holm p."""
    r = {v: gap(a, b, v, m) for v in VARIANTS}
    for v, p in zip(VARIANTS, holm([r[v]['p'] for v in VARIANTS])):
        r[v]['holm'] = p
    return r


def changes(a, b, m='psnr'):
    g0 = diff(rev10(a, m), rev10(b, m))
    r = {}
    for v in VARIANTS:
        g = diff(rev10(arm(a, v), m), rev10(arm(b, v), m))
        r[v] = paired(g, g0)
    for v, p in zip(VARIANTS, holm([r[v]['p'] for v in VARIANTS])):
        r[v]['holm'] = p
    return r


def cell_mean(a, m, fire=None):
    return mean(rev10(a, m, fire).values())


def mm(kind, key, pattern=r'([-+0-9.]+)', cast=float):
    # a value of src/data/methode-mesures.tsv (tools/methode_mesures.py)
    v = MM.get((kind, key))
    m = re.search(pattern, v) if v is not None else None
    return cast(m.group(1)) if m else None


def out_of_service(variant, fire):
    if variant == 'reference':
        return mm('load', 'p9 %s hors-service' % fire)
    return mm('variants', '%s %s hors-service' % (variant, fire))


# ------------------------------------------------------------------ the lines

LINES = []


def fr(x, nd):
    s = str(Decimal(repr(x)).quantize(Decimal(1).scaleb(-nd), rounding=ROUND_HALF_UP))
    return s.replace('.', ',').replace('-', '−')


def claim(block, label, written, checks):
    """checks: (computed, op, expected, nd), as tools/analyse_claims.py."""
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
    if sign > 0:
        return [(r['d'], '=', d, nd), (r['lo'], '=', lo, nd), (r['hi'], '=', hi, nd)]
    return [(-r['d'], '=', d, nd), (-r['hi'], '=', lo, nd), (-r['lo'], '=', hi, nd)]


def main():
    if REV is None:
        sys.exit('src/data/revision.tsv is needed')
    four = ('forest', 'smoke', 'swap', 'v15')
    three = ('forest', 'smoke', 'v15')
    hr, ar, rp = gaps('hybrid', 'rules'), gaps('agents', 'rules'), gaps('rules', 'pq')
    chr_, car = changes('hybrid', 'rules'), changes('agents', 'rules')

    # G01, the hybrid stack's lead (first paragraph, abstract, letters, conclusions)
    claim('G01', 'cellules par variante : 60 graines chacune, 20 cellules', '60 ; 20',
          [(min(len(rev10(arm(s, v))) for s in STACKS for v in VARIANTS), '=', 60, 0),
           (sum(1 for s in STACKS for v in VARIANTS if len(rev10(arm(s, v))) == 60), '=', 20, 0)])
    claim('G01', 'hybride − règles, quatre variantes : min., max. (dB) ; Holm max.', '1,18 à 1,90 ; < 0,001',
          [(min(hr[v]['d'] for v in four), '=', 1.18, 2), (max(hr[v]['d'] for v in four), '=', 1.90, 2),
           (max(hr[v]['holm'] for v in four), '<', 0.001, 3)])
    claim('G01', 'hybride − règles, référence (dB)', '2,02', [(gap('hybrid', 'rules')['d'], '=', 2.02, 2)])
    claim('G01', 'variantes où l\'avance est significative (Holm < 0,05) ; la grille de 30 m n\'en est pas',
          'quatre sur cinq ; non', [(sum(hr[v]['holm'] < 0.05 for v in VARIANTS), '=', 4, 0), (hr['j8']['holm'] < 0.05, '=', False, 0)])
    claim('G01', 'baisse de l\'écart, quatre variantes : toutes < 0 ; min., max. (dB) ; Holm min.', 'oui ; 0,12 à 0,84 ; ≥ 0,085',
          [(all(chr_[v]['d'] < 0 for v in four), '=', True, 0), (min(-chr_[v]['d'] for v in four), '=', 0.12, 2),
           (max(-chr_[v]['d'] for v in four), '=', 0.84, 2), (min(chr_[v]['holm'] for v in four), '>=', 0.085, 3)])
    big = sorted(four, key=lambda v: chr_[v]['d'])[:2]
    claim('G01', 'les deux plus fortes baisses : forêt et échange', 'forêt, échange', [(set(big) == {'forest', 'swap'}, '=', True, 0)])
    claim('G01', 'baisse, vidéo forêt : intervalle (dB)', '−1,56 à −0,13', [(chr_['forest']['lo'], '=', -1.56, 2), (chr_['forest']['hi'], '=', -0.13, 2)])
    claim('G01', 'baisse, échange : intervalle (dB)', '−1,53 à −0,02', [(chr_['swap']['lo'], '=', -1.53, 2), (chr_['swap']['hi'], '=', -0.02, 2)])
    claim('G01', 'grille de 30 m, hybride − règles (dB) ; Holm', '+0,40 (−0,11 à 0,91) ; 0,124',
          pair_checks(hr['j8'], 0.40, -0.11, 0.91) + [(hr['j8']['holm'], '=', 0.124, 3)])
    claim('G01', 'grille de 30 m, baisse de l\'écart (dB) ; Holm', '1,62 de moins (0,95 à 2,30) ; < 0,001',
          pair_checks(chr_['j8'], 1.62, 0.95, 2.30, sign=-1) + [(chr_['j8']['holm'], '<', 0.001, 3)])

    # G02, ranking, AOMDV and the agents stack
    best_worst = True
    for v in VARIANTS:
        for f in FIRES:
            ms = {s: cell_mean(arm(s, v), 'psnr', f) for s in STACKS}
            best_worst &= max(ms, key=ms.get) == 'hybrid' and min(ms, key=ms.get) == 'pq'
    claim('G02', 'quinze cellules (5 × 3) : hybride en tête, AOMDV dernière', 'oui', [(best_worst, '=', True, 0)])
    claim('G02', 'règles − AOMDV, cinq variantes : min., max. (dB) ; Holm max. ; référence', '7,98 à 10,87 ; < 0,001 ; 8,48',
          [(min(rp[v]['d'] for v in VARIANTS), '=', 7.98, 2), (max(rp[v]['d'] for v in VARIANTS), '=', 10.87, 2),
           (max(rp[v]['holm'] for v in VARIANTS), '<', 0.001, 3), (gap('rules', 'pq')['d'], '=', 8.48, 2)])
    claim('G02', 'agents − règles, forêt, fumée, vitesse : min., max. (dB) ; Holm min.', '−0,59 à −0,09 ; ≥ 0,33',
          [(min(ar[v]['d'] for v in three), '=', -0.59, 2), (max(ar[v]['d'] for v in three), '=', -0.09, 2),
           (min(ar[v]['holm'] for v in three), '>=', 0.33, 2)])
    claim('G02', 'le plus grand des trois : fumée ; intervalle (dB)', 'fumée ; −1,32 à 0,14',
          [(min(three, key=lambda v: ar[v]['d']) == 'smoke', '=', True, 0), (ar['smoke']['lo'], '=', -1.32, 2), (ar['smoke']['hi'], '=', 0.14, 2)])
    claim('G02', 'échange, agents − règles (dB) ; Holm', '0,80 de moins (0,19 à 1,41) ; 0,046',
          pair_checks(ar['swap'], 0.80, 0.19, 1.41, sign=-1) + [(ar['swap']['holm'], '=', 0.046, 3)])
    claim('G02', 'grille de 30 m, agents − règles (dB) ; Holm', '1,93 de moins (1,45 à 2,41) ; < 0,001',
          pair_checks(ar['j8'], 1.93, 1.45, 2.41, sign=-1) + [(ar['j8']['holm'], '<', 0.001, 3)])
    claim('G02', 'agents − règles, référence (dB)', '−0,06', [(gap('agents', 'rules')['d'], '=', -0.06, 2)])
    claim('G02', 'variation de l\'écart agents − règles, grille de 30 m (dB) ; Holm', '1,88 de plus (1,08 à 2,67) ; < 0,001',
          pair_checks(car['j8'], 1.88, 1.08, 2.67, sign=-1) + [(car['j8']['holm'], '<', 0.001, 3)])
    claim('G02', 'seule la grille de 30 m change l\'écart agents − règles (Holm < 0,05)', 'oui',
          [(all(car[v]['holm'] >= 0.05 for v in VARIANTS if v != 'j8'), '=', True, 0)])
    claim('G02', 'variation de l\'écart agents − règles, échange (dB) ; Holm', '−0,74 (−1,68 à 0,19) ; 0,47',
          pair_checks(car['swap'], -0.74, -1.68, 0.19) + [(car['swap']['holm'], '=', 0.47, 2)])

    ha, cha = gaps('hybrid', 'agents'), changes('hybrid', 'agents')
    claim('G02', 'hybride − agents, cinq variantes : min., max. (dB) ; Holm max. ; référence', '1,48 à 2,33 ; < 0,001 ; 2,07',
          [(min(ha[v]['d'] for v in VARIANTS), '=', 1.48, 2), (max(ha[v]['d'] for v in VARIANTS), '=', 2.33, 2),
           (max(ha[v]['holm'] for v in VARIANTS), '<', 0.001, 3), (gap('hybrid', 'agents')['d'], '=', 2.07, 2)])
    claim('G02', 'variation de hybride − agents : min., max. (dB) ; Holm min.', '−0,59 à +0,25 ; ≥ 0,65',
          [(min(cha[v]['d'] for v in VARIANTS), '=', -0.59, 2), (max(cha[v]['d'] for v in VARIANTS), '=', 0.25, 2),
           (min(cha[v]['holm'] for v in VARIANTS), '>=', 0.65, 4)])
    claim('G02', 'la plus grande variation : forêt ; intervalle (dB)', 'forêt ; −1,37 à 0,18',
          [(max(VARIANTS, key=lambda v: abs(cha[v]['d'])) == 'forest', '=', True, 0), (cha['forest']['lo'], '=', -1.37, 2), (cha['forest']['hi'], '=', 0.18, 2)])

    # G03, the 30 m grid
    claim('G03', 'perte des règles, grille de 30 m ; référence (%)', '44,7 ; 56,2',
          [(cell_mean('rules-j8', 'loss'), '=', 44.7, 1), (cell_mean('rules', 'loss'), '=', 56.2, 1)])
    r, h = gaps('hybrid', 'rules', 'loss')['j8'], gaps('hybrid', 'rules', 'm1loss')['j8']
    claim('G03', 'grille de 30 m, hybride − règles, perte (points) ; Holm', '−0,39 (−2,04 à 1,25) ; 0,64',
          pair_checks(r, -0.39, -2.04, 1.25) + [(r['holm'], '=', 0.64, 2)])
    claim('G03', 'grille de 30 m, hybride − règles, perte de M1 (points) ; Holm', '−1,01 (−3,27 à 1,25) ; 0,37',
          pair_checks(h, -1.01, -3.27, 1.25) + [(h['holm'], '=', 0.37, 2)])
    claim('G03', 'déclarations par simulation : règles 30 m ; règles référence ; hybride 30 m ; agents 30 m', '9,1 ; 22,0 ; 7,5 ; 23,1',
          [(cell_mean('rules-j8', 'declared'), '=', 9.1, 1), (cell_mean('rules', 'declared'), '=', 22.0, 1),
           (cell_mean('hybrid-j8', 'declared'), '=', 7.5, 1), (cell_mean('agents-j8', 'declared'), '=', 23.1, 1)])
    claim('G03', 'liens diagonaux à portée sur 162 ; sauts minimaux 40 m et 30 m', '139 ; 18 et 10',
          [(mm('topo', '30 m diagonal links', r'(\d+) of 162', int), '=', 139, 0),
           (mm('topo', '40 m fewest hops', r'(\d+)', int), '=', 18, 0),
           (mm('topo', '30 m fewest hops', r'(\d+)', int), '=', 10, 0)])

    # G04, loss regime, severity, oracle
    per = {v: [cell_mean(arm('rules', v), 'loss', f) for f in FIRES] for v in (None,) + VARIANTS}
    e10 = [x for v in VARIANTS for x in per[v]]
    claim('G04', 'perte des règles par cellule, quinze cellules : min., max. ; référence (%)', '43,0 à 62,3 ; 54,1 à 57,3',
          [(min(e10), '=', 43.0, 1), (max(e10), '=', 62.3, 1), (min(per[None]), '=', 54.1, 1), (max(per[None]), '=', 57.3, 1)])
    vs = per['forest'] + per['smoke'] + per['swap']
    claim('G04', 'vidéos et échange ; grille de 30 m ; vitesse × 1,5 : min., max. (%)', '52,1 à 59,0 ; 43,0 à 46,4 ; 55,4 à 62,3',
          [(min(vs), '=', 52.1, 1), (max(vs), '=', 59.0, 1), (min(per['j8']), '=', 43.0, 1), (max(per['j8']), '=', 46.4, 1),
           (min(per['v15']), '=', 55.4, 1), (max(per['v15']), '=', 62.3, 1)])
    v15 = [out_of_service('v15', f) for f in FIRES]
    ref = [out_of_service('reference', f) for f in FIRES]
    j8 = [out_of_service('j8', f) for f in FIRES]
    claim('G04', 'nœuds hors service avant la fin, vitesse × 1,5 ; référence', '9,4 à 13,0 ; 4,5 à 6,0',
          [(None if None in v15 else min(v15), '=', 9.4, 1), (None if None in v15 else max(v15), '=', 13.0, 1),
           (None if None in ref else min(ref), '=', 4.5, 1), (None if None in ref else max(ref), '=', 6.0, 1)])
    if ORACLE is None:
        claim('G04', 'oracle des cinq variantes : simulations, paquets perdus sans chemin', '1 200 ; 0', [(None, '=', 0, 0)])
    else:
        claim('G04', 'oracle des cinq variantes : simulations, paquets perdus sans chemin', '1 200 ; 0',
              [(len(ORACLE), '=', 1200, 0), (sum(int(r['unreachable']) for r in ORACLE), '=', 0, 0)])

    # G05, level M1, usable images, the 28 dB threshold
    m1, us = gaps('hybrid', 'rules', 'm1loss'), gaps('hybrid', 'rules', 'usable')
    claim('G05', 'hybride − règles, perte de M1, forêt, fumée, vitesse : min., max. (points) ; Holm max. ; référence',
          '2,61 à 4,16 de moins ; ≤ 0,032 ; 4,93',
          [(min(-m1[v]['d'] for v in three), '=', 2.61, 2), (max(-m1[v]['d'] for v in three), '=', 4.16, 2),
           (max(m1[v]['holm'] for v in three), '<=', 0.032, 4), (-gap('hybrid', 'rules', m='m1loss')['d'], '=', 4.93, 2)])
    claim('G05', 'échange, perte de M1 (points) ; Holm ; PSNR (dB)', '−1,90 (−3,82 à 0,01) ; 0,10 ; 1,25',
          pair_checks(m1['swap'], -1.90, -3.82, 0.01) + [(m1['swap']['holm'], '=', 0.10, 2), (hr['swap']['d'], '=', 1.25, 2)])
    three_u = ('smoke', 'swap', 'v15')
    claim('G05', 'images exploitables, fumée, échange, vitesse : min., max. ; Holm max. ; référence', '0,98 à 1,85 ; ≤ 0,002 ; 1,93',
          [(min(us[v]['d'] for v in three_u), '=', 0.98, 2), (max(us[v]['d'] for v in three_u), '=', 1.85, 2),
           (max(us[v]['holm'] for v in three_u), '<=', 0.002, 3), (gap('hybrid', 'rules', m='usable')['d'], '=', 1.93, 2)])
    claim('G05', 'images, forêt et 30 m : écarts, intervalles ; Holm', '0,53 (0,04 à 1,03), 0,60 (0,01 à 1,19) ; 0,071 et 0,071',
          pair_checks(us['forest'], 0.53, 0.04, 1.03) + pair_checks(us['j8'], 0.60, 0.01, 1.19)
          + [(us['forest']['holm'], '=', 0.071, 3), (us['j8']['holm'], '=', 0.071, 3)])
    fo = {s: cell_mean(arm(s, 'forest'), 'psnr') for s in STACKS}
    sm = [cell_mean(arm(s, 'smoke'), 'psnr') for s in ('rules', 'hybrid', 'agents')]
    claim('G05', 'forêt : aucune pile ≥ 28 dB, la meilleure hybride (dB)', 'aucune ; 24,02',
          [(max(fo.values()) < 28, '=', True, 0), (max(fo, key=fo.get) == 'hybrid', '=', True, 0), (fo['hybrid'], '=', 24.02, 2)])
    claim('G05', 'fumée : trois piles inter-couches ≥ 28 dB, min., max. (dB)', '31,53 à 33,77',
          [(min(sm) >= 28, '=', True, 0), (min(sm), '=', 31.53, 2), (max(sm), '=', 33.77, 2)])
    claim('G05', 'grille de 30 m : règles au-dessus de 28 dB (dB)', '28,47', [(cell_mean('rules-j8', 'psnr'), '=', 28.47, 2)])
    alone = all(cell_mean(arm('hybrid', v), 'psnr') >= 28 and all(cell_mean(arm(s, v), 'psnr') < 28 for s in ('pq', 'rules', 'agents'))
                for v in ('swap', 'v15'))
    claim('G05', 'échange et vitesse : hybride seule ≥ 28 dB (dB)', 'oui ; 28,69 et 29,03',
          [(alone, '=', True, 0), (cell_mean('hybrid-swap', 'psnr'), '=', 28.69, 2), (cell_mean('hybrid-v15', 'psnr'), '=', 29.03, 2)])

    # G06, closing paragraph
    claim('G06', 'pile à agents au-dessus des règles sous aucune variante', 'aucune',
          [(any(ar[v]['d'] > 0 and ar[v]['holm'] < 0.05 for v in VARIANTS), '=', False, 0)])
    claim('G06', 'AOMDV dernière sous chaque variante (moyenne des 60 graines)', 'oui',
          [(all(min(STACKS, key=lambda s: cell_mean(arm(s, v), 'psnr')) == 'pq' for v in VARIANTS), '=', True, 0)])

    # G07, methods sentences and the fairness table
    claim('G07', 'grille de 30 m : liens de la grille, deux pas ; un seul déploiement sur 240 simulations', '180 ; 5 ; oui',
          [(mm('topo', '30 m grid links', r'(\d+) of 180', int), '=', 180, 0), (mm('topo', '30 m two-step links', r'(\d+)', int), '=', 5, 0),
           (MM.get(('topo', '30 m deployments')) == '1 over 240 runs', '=', True, 0)])
    claim('G07', 'déplacement maximal par axe (m), au plus 8', '8 au plus',
          [(mm('topo', '30 m largest displacement', r'([0-9.]+) m on x'), '<', 8.0, 0),
           (mm('topo', '30 m largest displacement', r'on x, ([0-9.]+) m on y'), '<', 8.0, 0)])
    claim('G07', 'marges au-dessus du seuil : 42,4 m ; 30 m ; 30 contre 40 m (dB)', '1,9 ; 6,7 ; 4,0',
          [(mm('topo', 'margin 42.4 m link'), '=', 1.9, 1), (mm('topo', 'margin 30.0 m link'), '=', 6.7, 1),
           (mm('topo', '30 m against 40 m'), '=', 4.0, 1)])
    claim('G07', 'nœuds hors service avant la fin, grille de 30 m', '9,0 à 11,4',
          [(None if None in j8 else min(j8), '=', 9.0, 1), (None if None in j8 else max(j8), '=', 11.4, 1)])

    claim('G07', 'paquets des vidéos forêt et fumée', '5 704 ; 3 925',
          [(mm('videos', 'forest', r'(\d+) packets', int), '=', 5704, 0), (mm('videos', 'smoke', r'(\d+) packets', int), '=', 3925, 0)])

    # G08, the two letters' counts
    claim('G08', 'configurations et simulations de la série scénario de E5 : 4 piles × 5 variantes × 3 feux, × 20', '60 ; 1 200',
          [(4 * 5 * 3, '=', 60, 0), (sum(len(rev10(arm(s, v))) for s in STACKS for v in VARIANTS), '=', 1200, 0)])

    # G09, numbers of the revision's notes not already above
    hu = {v: {s: cell_mean(arm(s, v), 'usable') for s in STACKS} for v in VARIANTS}
    claim('G09', 'C-14 : hybride, le plus d\'images exploitables sous chaque variante ; min., max.', 'oui ; 5,05 à 9,55',
          [(all(max(hu[v], key=hu[v].get) == 'hybrid' for v in VARIANTS), '=', True, 0),
           (min(hu[v]['hybrid'] for v in VARIANTS), '=', 5.05, 2), (max(hu[v]['hybrid'] for v in VARIANTS), '=', 9.55, 2)])
    claim('G09', 'C-22 : vitesse × 1,5, variation de l\'avance (dB) ; Holm', '−0,12 (−0,87 à 0,62) ; 0,84',
          pair_checks(chr_['v15'], -0.12, -0.87, 0.62) + [(chr_['v15']['holm'], '=', 0.84, 2)])
    xs, ys, n = [], [], 0
    for a in sorted({r['arm'] for r in REV if not r['arm'].startswith('pq')}):
        dd, q = rev10(a, 'declared'), rev10(a, 'rreq_all')
        k = [x for x in dd if x in q]
        if len(k) == 60:
            n += 1
            xs += [dd[x] for x in k]
            ys += [q[x] for x in k]
    mx, my = mean(xs), mean(ys)
    sxy = sum((u - mx) * (w - my) for u, w in zip(xs, ys))
    sxx, syy = sum((u - mx) ** 2 for u in xs), sum((w - my) ** 2 for w in ys)
    claim('G09', 'C-16 : avec la série scénario de E5, ensembles, simulations, pente, corrélation', '49 ; 2 940 ; 83 ; 0,95',
          [(n, '=', 49, 0), (len(xs), '=', 2940, 0), (sxy / sxx, '=', 83, 0), (sxy / math.sqrt(sxx * syy), '=', 0.95, 2)])

    width = max(len(l[1]) for l in LINES)
    for block, label, written, computed, verdict in LINES:
        print('%-4s  %-*s  texte : %-44s  calcul : %-44s  %s' % (block, width, label, written, computed, verdict))
    ok = sum(1 for l in LINES if l[4] == 'OK')
    bad = [l for l in LINES if l[4] == 'DIFFERS']
    miss = [l for l in LINES if l[4] == 'MISSING']
    print('\n%d lignes : %d OK, %d DIFFERS, %d MISSING' % (len(LINES), ok, len(bad), len(miss)))
    for l in bad + miss:
        print('  %s %s : %s' % (l[4], l[0], l[1]))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
