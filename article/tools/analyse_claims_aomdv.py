#!/usr/bin/env python3
"""Every number of the tuned AOMDV series of E1 (study 10, tranche 8; labels A01 to A11), in the
style of analyse_claims.py and analyse_claims_generalisation.py: section 3.2,
the sentences of the introduction, the discussion and the conclusions, the
reply letters, and the revision's notes (A10 and A11: the seeds, the hellos
and the costs of the tuned sets).

One line per number: the block, what is computed, the value as the text
writes it, the value computed here, and OK or DIFFERS. Pairs share their seeds
(fire, realisation): mean difference, 95 % Student interval, p, Holm over the
three GOPs of one comparison, as the control AOMDV claims of analyse_claims.py (P03). A
pooled value is, per seed, the mean over the three GOPs. Rounding is half up.
Reads src/data/revision.tsv and src/data/overhead.tsv; the energy of the
tuned sets and their seeds are read from experiments/sets (graphs and
PROGRESS.txt), as make_tables_overhead.py --compile does; a missing file turns
its lines into MISSING, which does not fail the run.

Usage: python3 tools/analyse_claims_aomdv.py (also run by analyse_claims.py).
"""
import math
import os
import re
import sys
from decimal import ROUND_HALF_UP, Decimal

HERE = os.path.dirname(os.path.abspath(__file__))
PAPER = os.path.normpath(os.path.join(HERE, '..'))
sys.path.insert(0, os.path.normpath(os.path.join(PAPER, '..', 'experiments', 'metrics')))
import stats  # noqa: E402

DATA = os.path.join(PAPER, 'src', 'data')
SETS = os.path.normpath(os.path.join(PAPER, '..', 'experiments', 'sets'))
SPLITS = ('3M9D', '6M6D', '9M3D')
FIRES = ('F1', 'F2', 'F3')
GC = {'3M9D': '0p01597', '6M6D': '0p02525', '9M3D': '0p03'}
FIRE_SET = {'F1': 'fire', 'F2': 'fire2', 'F3': 'fire3'}


def table(path):
    if not os.path.exists(path):
        return None
    lines = [l for l in open(path) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:] if l.strip()]


REV = table(os.path.join(DATA, 'revision.tsv'))
OVER = table(os.path.join(DATA, 'overhead.tsv'))


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


def pearson_p(r, n):
    return stats.t_sf2(r * math.sqrt((n - 2) / (1 - r * r)), n - 2)


# ------------------------------------------------------------------ data access

def rev(arm, split='9M3D', m='psnr', fire=None):
    return {(r['fire'], r['realisation']): float(r[m]) for r in REV
            if r['arm'] == arm and r['split'] == split and r['period'] == 'p9' and r['dist'] == '40'
            and r[m] != '' and (fire is None or r['fire'] == fire)}


def pooled(arm, m='psnr'):
    ds = [rev(arm, s, m=m) for s in SPLITS]
    return {k: mean(d[k] for d in ds) for k in ds[0] if all(k in d for d in ds)}


def per_gop(a, b, m='psnr'):
    r = {g: paired(rev(a, g, m), rev(b, g, m)) for g in SPLITS}
    for g, p in zip(SPLITS, holm([r[g]['p'] for g in SPLITS])):
        r[g]['holm'] = p
    return r


def cells(arm, m):
    return [mean(rev(arm, g, m, f).values()) for g in SPLITS for f in FIRES]


def set_dir(arm, split, fire):
    return os.path.join(SETS, 'set_AOMDV_GC-PN-%s-13_p9-n100-d40-%s-%s-s10' % (GC[split], FIRE_SET[fire], arm))


def seeds(path):
    p = os.path.join(path, 'PROGRESS.txt')
    if not os.path.exists(p):
        return None
    return dict(re.findall(r'\((r\d\d), seed (\d+)\)', open(p, errors='replace').read()))


def tuned_energy():
    """The pq-tuned rows overhead.tsv would hold, from the sets' graphs (GOP 9-3)."""
    rows = []
    for f in FIRES:
        g = os.path.join(set_dir('pq-tuned', '9M3D', f), 'graphs')
        try:
            vals = {}
            for k, name in (('e_node', 'Energy Consumption'), ('e_total', 'Power Consumption'), ('lifetime', 'Life Time')):
                v = open(os.path.join(g, name, name + '.dat')).read().split()
                vals[k], vals['n'] = float(v[1]), int(v[3])
        except (OSError, IndexError, ValueError):
            return None
        rows.append(vals)
    n = sum(r['n'] for r in rows)
    e0 = 8658.0   # initial energy, header of overhead.tsv
    return {'e_total': sum(r['e_total'] * r['n'] for r in rows) / n, 'e_node': sum(r['e_node'] * r['n'] for r in rows) / n,
            'e_max': sum((1 - r['lifetime'] / 100) * e0 * r['n'] for r in rows) / n,
            'cells': {f: r['e_node'] for f, r in zip(FIRES, rows)}}


def tuned_energy_all():
    """Per-node energy of the nine tuned sets, per (split, fire)."""
    out = {}
    for g in SPLITS:
        for f in FIRES:
            p = os.path.join(set_dir('pq-tuned', g, f), 'graphs', 'Energy Consumption', 'Energy Consumption.dat')
            if not os.path.exists(p):
                return None
            out[g, f] = float(open(p).read().split()[1])
    return out


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

    # A01, the control AOMDV series of E1 as section 3.2 and the reply letters write it (reproduction of P03)
    e2 = paired(pooled('pq-ctl'), pooled('pq'))
    claim('A01', 'AOMDV de contrôle − AOMDV, 60 graines (dB) ; p', '0,07 (0,01 à 0,14) ; p = 0,03',
          pair_checks(e2, 0.07, 0.01, 0.14) + [(e2['p'], '=', 0.03, 2)])
    g2 = per_gop('pq-ctl', 'pq')
    claim('A01', 'même écart par GOP : aucun GOP après Holm ; Holm min. ; écarts min. et max. (dB)', 'aucun ; p ≥ 0,20 ; 0,07 à 0,08',
          [(all(g2[g]['holm'] >= 0.05 for g in SPLITS), '=', True, 0), (min(g2[g]['holm'] for g in SPLITS), '>=', 0.20, 2),
           (min(g2[g]['d'] for g in SPLITS), '=', 0.07, 2), (max(g2[g]['d'] for g in SPLITS), '=', 0.08, 2)])
    r = paired(pooled('rules'), pooled('pq-ctl'))
    claim('A01', 'pile à règles − AOMDV de contrôle, 60 graines (dB)', '6,2 (5,7 à 6,6)', pair_checks(r, 6.2, 5.7, 6.6, 1))
    rp = paired(pooled('rules'), pooled('pq'))
    claim('A01', 'pile à règles − AOMDV, 60 graines (dB) : premier paragraphe de 3.2, introduction, lettres', '6,2 (5,8 à 6,7)',
          pair_checks(rp, 6.2, 5.8, 6.7, 1))

    # A02, the share of the timers (pq-tuned - pq-ctl)
    t = paired(pooled('pq-tuned'), pooled('pq-ctl'))
    claim('A02', 'AOMDV réglé − AOMDV de contrôle, 60 graines (dB) ; p', '2,04 (1,73 à 2,34) ; p < 0,001',
          pair_checks(t, 2.04, 1.73, 2.34) + [(t['p'], '<', 0.001, 3)])
    gt = per_gop('pq-tuned', 'pq-ctl')
    claim('A02', 'même écart par GOP : 3-9 et 9-3 (dB), bornes de l\'étendue, Holm max.', '1,60 à 2,35 ; p < 0,001',
          [(gt['3M9D']['d'], '=', 1.60, 2), (gt['9M3D']['d'], '=', 2.35, 2),
           (min(gt[g]['d'] for g in SPLITS) == gt['3M9D']['d'] and max(gt[g]['d'] for g in SPLITS) == gt['9M3D']['d'], '=', True, 0),
           (max(gt[g]['holm'] for g in SPLITS), '<', 0.001, 3)])

    # A03, control messages of tuned AOMDV and the control AOMDV (180 runs each)
    h = paired(pooled('pq-tuned', 'hello'), pooled('pq-ctl', 'hello'))
    q = paired(pooled('pq-tuned', 'rreq_all'), pooled('pq-ctl', 'rreq_all'))
    claim('A03', 'HELLO par simulation, AOMDV réglé et AOMDV de contrôle ; p', '3 226 contre 16 215 ; p < 0,001',
          [(mean(pooled('pq-tuned', 'hello').values()), '=', 3226, 0), (mean(pooled('pq-ctl', 'hello').values()), '=', 16215, 0),
           (h['p'], '<', 0.001, 3)])
    claim('A03', 'RREQ par simulation, AOMDV réglé et AOMDV de contrôle ; p', '578 contre 1 341 ; p < 0,001',
          [(mean(pooled('pq-tuned', 'rreq_all').values()), '=', 578, 0), (mean(pooled('pq-ctl', 'rreq_all').values()), '=', 1341, 0),
           (q['p'], '<', 0.001, 3)])

    # A04, losses of tuned AOMDV and the control AOMDV
    lo, m1 = paired(pooled('pq-tuned', 'loss'), pooled('pq-ctl', 'loss')), paired(pooled('pq-tuned', 'm1loss'), pooled('pq-ctl', 'm1loss'))
    claim('A04', 'perte (%), AOMDV réglé et AOMDV de contrôle ; p', '44,0 contre 74,7 ; p < 0,001',
          [(mean(pooled('pq-tuned', 'loss').values()), '=', 44.0, 1), (mean(pooled('pq-ctl', 'loss').values()), '=', 74.7, 1),
           (lo['p'], '<', 0.001, 3)])
    claim('A04', 'perte du niveau M1 (%), AOMDV réglé et AOMDV de contrôle ; p', '37,0 contre 66,2 ; p < 0,001',
          [(mean(pooled('pq-tuned', 'm1loss').values()), '=', 37.0, 1), (mean(pooled('pq-ctl', 'm1loss').values()), '=', 66.2, 1),
           (m1['p'], '<', 0.001, 3)])
    claim('A04', 'simulations d\'AOMDV réglé ; moyennes sur 60 graines = moyennes sur 180 simulations', '180',
          [(sum(len(rev('pq-tuned', g)) for g in SPLITS), '=', 180, 0), (len(pooled('pq-tuned')), '=', 60, 0)])

    # A05, the gap that remains (rules - pq-tuned)
    rt = paired(pooled('rules'), pooled('pq-tuned'))
    claim('A05', 'pile à règles − AOMDV réglé, 60 graines (dB) ; p', '4,13 (3,83 à 4,42) ; p < 0,001',
          pair_checks(rt, 4.13, 3.83, 4.42) + [(rt['p'], '<', 0.001, 3)])
    gr = per_gop('rules', 'pq-tuned')
    claim('A05', 'même écart par GOP : 3-9 et 9-3 (dB), bornes de l\'étendue, Holm max.', '1,84 à 6,06 ; p < 0,001',
          [(gr['3M9D']['d'], '=', 1.84, 2), (gr['9M3D']['d'], '=', 6.06, 2),
           (min(gr[g]['d'] for g in SPLITS) == gr['3M9D']['d'] and max(gr[g]['d'] for g in SPLITS) == gr['9M3D']['d'], '=', True, 0),
           (max(gr[g]['holm'] for g in SPLITS), '<', 0.001, 3)])

    # A06, losses of tuned AOMDV and the rules, per cell
    tl, rl, tm, rm = cells('pq-tuned', 'loss'), cells('rules', 'loss'), cells('pq-tuned', 'm1loss'), cells('rules', 'm1loss')
    gl, gm = per_gop('rules', 'pq-tuned', 'loss'), per_gop('rules', 'pq-tuned', 'm1loss')
    claim('A06', 'perte par cellule (%), AOMDV réglé ; pile à règles', '42 à 48 ; 54 à 63',
          [(min(tl), '=', 42, 0), (max(tl), '=', 48, 0), (min(rl), '=', 54, 0), (max(rl), '=', 63, 0)])
    claim('A06', 'perte, règles − AOMDV réglé : positive à chaque GOP ; Holm max.', 'oui ; p < 0,001 à chaque GOP',
          [(all(gl[g]['d'] > 0 for g in SPLITS), '=', True, 0), (max(gl[g]['holm'] for g in SPLITS), '<', 0.001, 3)])
    claim('A06', 'perte du niveau M1 par cellule (%), AOMDV réglé ; pile à règles', '32 à 45 ; 14 à 32',
          [(min(tm), '=', 32, 0), (max(tm), '=', 45, 0), (min(rm), '=', 14, 0), (max(rm), '=', 32, 0)])
    claim('A06', 'perte du niveau M1, règles − AOMDV réglé : négative à chaque GOP ; Holm max.', 'oui ; p < 0,001 à chaque GOP',
          [(all(gm[g]['d'] < 0 for g in SPLITS), '=', True, 0), (max(gm[g]['holm'] for g in SPLITS), '<', 0.001, 3)])
    claim('A06', 'neuf cellules : AOMDV réglé perd moins que les règles ; plus du niveau M1', 'oui ; oui',
          [(all(x < y for x, y in zip(tl, rl)), '=', True, 0), (all(x > y for x, y in zip(tm, rm)), '=', True, 0)])
    claim('A06', 'images exploitables d\'AOMDV réglé, 180 simulations', 'aucune',
          [(sum(sum(rev('pq-tuned', g, 'usable').values()) for g in SPLITS), '=', 0, 0)])

    # A07, per-seed link of the remaining gap with the M1 loss
    rs, ps = [], []
    for g in SPLITS:
        a, b, am, bm = rev('rules', g), rev('pq-tuned', g), rev('rules', g, 'm1loss'), rev('pq-tuned', g, 'm1loss')
        k = sorted(a)
        rs.append(pearson([a[x] - b[x] for x in k], [am[x] - bm[x] for x in k]))
        ps.append(pearson_p(rs[-1], len(k)))
    claim('A07', 'corrélation des écarts de PSNR et de perte du niveau M1, règles − AOMDV réglé, par GOP ; p max.', '−0,72 à −0,83 ; p < 0,001',
          [(max(rs), '=', -0.72, 2), (min(rs), '=', -0.83, 2), (max(ps), '<', 0.001, 3)])

    # A08, the decomposition the closing sentences write (intro, results, conclusions, letters)
    claim('A08', '0,07 + 2,04 + 4,13 = règles − AOMDV, à 1e-9 près (identité des moyennes appariées) ; arrondi', 'oui ; 6,2',
          [(abs(e2['d'] + t['d'] + rt['d'] - rp['d']) < 1e-9, '=', True, 0), (e2['d'] + t['d'] + rt['d'], '=', 6.2, 1)])
    claim('A08', 'conclusions : AOMDV gagne 0,07 puis 2,04 dB (les deux positifs, p ≤ 0,03)', '0,07 puis 2,04',
          [(e2['d'], '=', 0.07, 2), (t['d'], '=', 2.04, 2), (max(e2['p'], t['p']), '<=', 0.03, 2)])

    # A09, the letters: the size of the tuned AOMDV series of E1
    n_sets = sum(1 for g in SPLITS for f in FIRES if os.path.isdir(set_dir('pq-tuned', g, f))) if os.path.isdir(SETS) else None
    claim('A09', 'E1, AOMDV réglé : configurations et simulations (lettres)', '9 ; 180',
          [(n_sets, '=', 9, 0), (sum(len(rev('pq-tuned', g)) for g in SPLITS), '=', 180, 0)])

    # A10, the revision's notes: the costs table and the other numbers it quotes
    hel = paired(rev('pq-tuned', '9M3D', 'hello'), rev('rules', '9M3D', 'hello'))
    claim('A10', 'GOP 9-3 : HELLO, RREQ, RERR, RREP d\'AOMDV réglé (ligne proposée du tableau des coûts)', '3 213 ; 558 ; 48 ; 7',
          [(mean(rev('pq-tuned', '9M3D', m).values()), '=', v, 0) for m, v in (('hello', 3213), ('rreq_all', 558), ('rerr', 48), ('rrep', 7))])
    claim('A10', 'GOP 9-3 : HELLO, AOMDV réglé − règles (écart, intervalle) ; p', '+1,02 (−1,67 à 3,70) ; p = 0,45',
          pair_checks(hel, 1.02, -1.67, 3.70) + [(hel['p'], '=', 0.45, 2)])
    claim('A10', 'GOP 9-3 : RREQ de la pile à règles et d\'AOMDV de contrôle (tableau des coûts)', '2 334 ; 1 290',
          [(mean(rev('rules', '9M3D', 'rreq_all').values()), '=', 2334, 0), (mean(rev('pq-ctl', '9M3D', 'rreq_all').values()), '=', 1290, 0)])
    en = tuned_energy()
    claim('A10', 'GOP 9-3 : énergie d\'AOMDV réglé, réseau, par nœud, nœud le plus sollicité (J)', '601,7 ; 6,10 ; 6,42',
          [(None, '=', 0, 0)] if en is None else
          [(en['e_total'], '=', 601.7, 1), (en['e_node'], '=', 6.10, 2), (en['e_max'], '=', 6.42, 2)])
    ea = tuned_energy_all()
    if OVER is None or ea is None:
        claim('A10', 'écart d\'énergie maximal par cellule, cinq piles puis six (%)', '0,45 ; 0,46', [(None, '=', 0, 0)])
    else:
        s5 = s6 = 0.0
        for g in SPLITS:
            for f in FIRES:
                e = {x['stack']: float(x['e_node']) for x in OVER if x['split'] == g and x['fire'] == f}
                five = list(e.values())
                six = five + [ea[g, f]]
                s5, s6 = max(s5, (max(five) - min(five)) / min(five)), max(s6, (max(six) - min(six)) / min(six))
        claim('A10', 'écart d\'énergie maximal par cellule, cinq piles puis six avec AOMDV réglé (%)', '0,45 ; 0,46',
              [(100 * s5, '=', 0.45, 2), (100 * s6, '=', 0.46, 2)])
    claim('A10', 'part des temporisations dans règles − AOMDV (%)', '33', [(100 * t['d'] / rp['d'], '=', 33, 0)])
    claim('A10', 'GOP 9-3 : HELLO de la pile à règles (tableau des coûts)', '3 212',
          [(mean(rev('rules', '9M3D', 'hello').values()), '=', 3212, 0)])
    if OVER is None or en is None:
        claim('A10', 'lignes d\'overhead.tsv ; nœud le plus sollicité, piles inter-couches et AOMDV réglé (J)', '45 ; 6,35 ; 6,42', [(None, '=', 0, 0)])
    else:
        def emax(stack):
            sel = [x for x in OVER if x['stack'] == stack and x['split'] == '9M3D']
            return sum(float(x['e_max']) * int(x['n']) for x in sel) / sum(int(x['n']) for x in sel)
        cross = [emax(x) for x in ('rules', 'agents', 'hybrid')]
        claim('A10', 'lignes d\'overhead.tsv ; nœud le plus sollicité, piles inter-couches et AOMDV réglé (J)', '45 ; 6,35 ; 6,42',
              [(len(OVER), '=', 45, 0), (max(cross), '=', 6.35, 2), (en['e_max'], '=', 6.42, 2), (en['e_max'] > max(cross), '=', True, 0)])
    a, b = pooled('pq-tuned'), pooled('pq-ctl')
    c, d = pooled('rules'), pooled('pq-tuned')
    claim('A10', 'graines où AOMDV réglé dépasse AOMDV de contrôle ; où les règles dépassent AOMDV réglé', '57 sur 60 ; 60 sur 60',
          [(sum(a[k] > b[k] for k in a), '=', 57, 0), (sum(c[k] > d[k] for k in c), '=', 60, 0)])
    claim('A10', 'PSNR d\'AOMDV réglé par cellule (dB), min. et max.', '19,6 à 21,4',
          [(min(cells('pq-tuned', 'psnr')), '=', 19.6, 1), (max(cells('pq-tuned', 'psnr')), '=', 21.4, 1)])

    # A11, the verification of point 1
    ht, hc = [], []
    for g in SPLITS:
        ht += list(rev('pq-tuned', g, 'hello').values())
        hc += list(rev('pq-ctl', g, 'hello').values())
    claim('A11', 'HELLO par simulation, AOMDV réglé min. et max. ; AOMDV de contrôle min. et max.', '3 185 à 3 269 ; 16 037 à 16 365',
          [(min(ht), '=', 3185, 0), (max(ht), '=', 3269, 0), (min(hc), '=', 16037, 0), (max(hc), '=', 16365, 0)])
    one = lambda arm: rev(arm, '9M3D', 'hello').get(('F1', 'r01'))
    claim('A11', 'simulation de vérification de ns-2 (GOP 9-3, F1, r01) : HELLO réglé, contrôle', '3 199 ; 16 141',
          [(one('pq-tuned'), '=', 3199, 0), (one('pq-ctl'), '=', 16141, 0)])
    same = None
    if os.path.isdir(SETS):
        same = True
        for g in SPLITS:
            for f in FIRES:
                st, sc = seeds(set_dir('pq-tuned', g, f)), seeds(set_dir('pq-ctl', g, f))
                same = same and st is not None and st == sc and len(st) == 20
    claim('A11', 'neuf jeux de 20 simulations, graines identiques à AOMDV de contrôle (PROGRESS.txt)', 'oui',
          [(same, '=', True, 0)] if same is not None else [(None, '=', 0, 0)])
    claim('A11', 'HELLO moyens par simulation, AOMDV réglé et AOMDV de contrôle, GOP 9-3', '3 213 ; 16 155',
          [(mean(rev('pq-tuned', '9M3D', 'hello').values()), '=', 3213, 0), (mean(rev('pq-ctl', '9M3D', 'hello').values()), '=', 16155, 0)])

    width = max(len(l[1]) for l in LINES)
    for block, label, written, computed, verdict in LINES:
        print('%-4s  %-*s  texte : %-36s  calcul : %-40s  %s' % (block, width, label, written, computed, verdict))
    ok = sum(1 for l in LINES if l[4] == 'OK')
    bad = [l for l in LINES if l[4] == 'DIFFERS']
    miss = [l for l in LINES if l[4] == 'MISSING']
    print('\n%d lignes : %d OK, %d DIFFERS, %d MISSING' % (len(LINES), ok, len(bad), len(miss)))
    for l in bad + miss:
        print('  %s %s : %s' % (l[4], l[0], l[1]))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
