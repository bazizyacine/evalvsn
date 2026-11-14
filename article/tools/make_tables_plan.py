#!/usr/bin/env python3
"""Tables of the experimental plan of the Sensors revision, French and English: tab:fr-plan / tab:en-plan, one row per series of an
experiment (the experiment, its question, counts and sections written once over
its series), and tab:fr-equite / tab:en-equite, what each comparison changes
besides its factor.

The plan: six experiments, E0 to
E5, and two measures named without a code, the agents' traces and the overheads
for a node. An experiment holds one series or several (the levels or groups of
variants of its factor); each series keeps its own Holm family.

No count is written by hand: the configurations, the simulations, the stacks and
the GOPs of each experiment are read from the set lists of studies 09 and 10
(experiments/studies/*/sets.txt), from sets.conf (protocol, label, video
and sims columns) and from the encoder file of each label (ipFiles/, gopCoef).
A configuration is one set: one stack, one fire, one GOP and one level of the
factor, simulated sims times.

Usage: python3 tools/make_tables_plan.py [OUT_DIR]   (default: src/tables)
PHD_EXPERIMENTS=DIR overrides the experiments directory (a copy run elsewhere).
"""
import os
import re
import sys

import table_cells

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
EXP = os.environ.get('PHD_EXPERIMENTS', os.path.join(ROOT, 'experiments'))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'src', 'tables')

# The experiment and the series each study-10 set belongs to, by its name. The
# order matters: the first pattern that matches wins. A single-series experiment
# and the traces have the series None.
RULES = (
    ('E3', 'frozen', r'-frozen$'),
    ('E1', 'ctl', r'-pq-ctl-s10$'),
    ('E1', 'tuned', r'-pq-tuned-s10$'),
    ('traces', None, r'-tr-s10$'),
    ('E3', 'edec', r'-edec-s10$'),
    ('E2', None, r'-(only[A-Z]|no[A-Z])-s10$'),
    ('E4', 'rate', r'_p9r(65|32)-'),
    ('E5', 'load', r'_p(7|12|18|30)-'),
    ('E5', 'fire', r'-(az015|az05|az165|pf0|pf025|pf075|fd1)-s10$'),
    ('E5', 'scenario', r'-(forest|smoke|j8|swap|v15)-s10$'),
    # the coding series at 0.1 was first run at 165 s (tranche 9); the article
    # reads its replay at 168 s (tranche 10); the 165 s sets were purged
    # and are commented out in sets.txt
    ('E4', 'coding', r'_GC-QC-0p1_.*-t168-s10$'),
)
ORDER = ('E0', 'E1', 'E2', 'E3', 'E4', 'E5', 'traces', 'costs')
MEASURES = ('traces', 'costs')      # named in full, no code in the table
SERIES_ORDER = [s for _, s, _ in RULES if s is not None]
# The Holm family of each series, said once in the method (section 2.7), not in
# the table. The coding series: its tests were fixed before reading.
FAMILY = {
    'E0': '9 cells, or 3 GOPs',
    'ctl': '3 GOPs', 'tuned': '3 GOPs',
    'E2': '7, then 6; 5 for the declarations, 6 for the interaction',
    'frozen': '3 GOPs', 'edec': '2 stacks; 3 comparisons',
    'rate': '2 stacks x 2 GOPs; 2 GOPs at equal rate',
    'coding': '2 stacks against the rules; 4 stacks, coefficient 0.1 against 0.01',
    'load': '5 periods', 'fire': '7 variants', 'scenario': '5 variants',
    'traces': 'none', 'costs': 'none',
}
# the stacks in the order of every table: AOMDV, rules, agents, hybrid
STACKS = (('pq', 'AOMDV', 'AOMDV'), ('rules', 'règles', 'rules'),
          ('agents', 'agents', 'agents'), ('hybrid', 'hybride', 'hybrid'))
assert tuple(x for x, _, _ in STACKS) == table_cells.STACKS
# GOP of a set: the GOP coefficient (gopCoef) of its encoder file; the other
# videos are coded at the coefficient that gives them nine M frames (9 M and 3 D).
GOP = {'0.01597': '3-9', '0.02525': '6-6', '0.03': '9-3'}
GOPS = table_cells.GOPS

# Results subsection of each experiment (the labels of the article's Results
# section), one label per subsection: sensibilite and generalisation sit in 3.8.
SECTION = {'E0': ('oracle', 'aomdv', 'learning', 'm1', 'gop'), 'E1': ('aomdv',),
           'E2': ('ablation',), 'E3': ('ablation', 'break', 'learning'), 'E4': ('gop',),
           'E5': ('charge',), 'traces': ('ablation', 'break'), 'costs': ('costs',)}

TEXT = {
    'fr': {
        'head': ('Expérience', 'Question', 'Facteur et niveaux', 'Piles', 'GOP',
                 'Configurations (simulations)', 'Section'),
        'E0': ('Effet de la pile, du feu et du GOP', 'pile~; feu F1 à F3~; GOP'),
        'E1': ("Implémentation d'AOMDV", {'ctl': "couche d'accès modifiée et son chien de garde (AOMDV de contrôle)",
                                          'tuned': "temporisations et file de route des piles inter-couches (AOMDV aux temporisations inter-couches)"}),
        'E2': ('Contribution de chaque décision', "un agent seul sur la pile à règles (7), un agent retiré de la pile hybride (6)"),
        'E3': ('Adaptation des agents', {'frozen': r"apprentissage et exploration mis à 0 (figée), contre $\varepsilon = 0{,}1$ dans E0",
                                         'edec': r"$\varepsilon$ décroissant jusqu'à 0 entre 60 et \SI{114}{\second} ($\varepsilon$ décroissant)"}),
        'E4': ('Codage de la vidéo, volume du GOP et coefficient de qualité', {'rate': r"débit moyen du GOP 9-3, $T = 9{,}32$ et \SI{9,65}{\second} (débit égal)",
                                                                               'coding': "coefficient de qualité de 0,1 au lieu de 0,01 (coefficient~0,1)"}),
        'E5': ("Portée de l'avance de la pile hybride", {'load': r"$T = 7$, 12, 18 et \SI{30}{\second} (charge)",
                                                         'fire': "AZ (3~valeurs), PF (3~valeurs) ou loi des pannes (1), 7 variantes (feu)",
                                                         'scenario': r"vidéo forêt ou fumée, grille de \SI{30}{\metre}, caméra et puits échangés, vitesse de tête~$\times 1{,}5$, 5 variantes (scénario)"}),
        'traces': ('Traces des agents', r"aucun~: simulations de E0, choix et valeurs des agents écrits toutes les \SI{20}{\second}"),
        'costs': ('Coûts pour un nœud capteur', 'aucune simulation, mesures de E0 à E3, E5 et des traces'),
        'all': 'les quatre', 'none': '--', 'measure': '--',
    },
    'en': {
        'head': ('Experiment', 'Question', 'Factor and levels', 'Stacks', 'GOP',
                 'Configurations (simulations)', 'Section'),
        'E0': ('Effect of the stack, the fire and the GOP', 'stack; fire F1 to F3; GOP'),
        'E1': ('Implementation of AOMDV', {'ctl': 'derived access layer and its watchdog (control AOMDV)',
                                          'tuned': 'timers and route queue of the cross-layer stacks (AOMDV with cross-layer timers)'}),
        'E2': ('Contribution of each decision', 'one agent alone on the rule stack (7), one agent removed from the hybrid stack (6)'),
        'E3': ('Adaptation of the agents', {'frozen': r'learning and exploration set to 0 (frozen), against $\varepsilon = 0.1$ in E0',
                                      'edec': r'$\varepsilon$ decaying to 0 between 60 and \SI{114}{\second} (decaying $\varepsilon$)'}),
        'E4': ('Video coding, GOP volume and quality coefficient', {'rate': r'mean rate of GOP 9-3, $T = 9.32$ and \SI{9.65}{\second} (equal rate)',
                                                                    'coding': 'quality coefficient of 0.1 instead of 0.01 (coefficient~0.1)'}),
        'E5': ('Reach of the lead of the hybrid stack', {'load': r'$T = 7$, 12, 18 and \SI{30}{\second} (load)',
                                                    'fire': 'AZ (3~values), PF (3~values) or distribution of the failure instants (1), 7 variants (fire)',
                                                    'scenario': r'forest or smoke video, \SI{30}{\metre} grid, camera and sink swapped, head spread rate~$\times 1.5$, 5 variants (scenario)'}),
        'traces': ('Traces of the agents', r'none: simulations of E0, the choices and values of the agents written every \SI{20}{\second}'),
        'costs': ('Costs for a sensor node', 'no simulation, measurements of E0 to E3, E5 and the traces'),
        'all': 'all four', 'none': '--', 'measure': '--',
    },
}

# What differs besides the factor, and what handles it (one row per comparison).
EQUITE = {
    'fr': (('Comparaison', 'Ce qui diffère en plus du facteur', 'Traitement'), (
        ('Règles, hybride, agents', r"rien : seules les décisions comparées changent, avec la structure qu'elles portent (section~\ref{sec:fr-methods-protocol})", 'sans objet'),
        ('AOMDV : la pile',
         r"routage, file ordonnée par rang, mesures de la couche physique dérivée, puissance en boucle fermée, jusqu'à 26 chemins (2 par niveau de priorité) contre 3, délai de rediffusion des demandes de route de 50 contre \SI{10}{\milli\second}",
         'facteur comparé'),
        ("AOMDV : la couche d'accès", "couche d'accès d'origine, sans chien de garde d'émission", 'E1, AOMDV de contrôle'),
        ('AOMDV : les temporisations',
         r"messages de voisinage chaque seconde contre toutes les \SI{5}{\second}, délai maximal avant de renouveler une demande de route de 1 contre \SI{10}{\second}, route active 10 contre \SI{35}{\second}, 64 contre 16 paquets en attente de route",
         'E1, AOMDV aux temporisations inter-couches'),
        ('E3 : pile figée face aux règles',
         r"puissance fixe à \SI{+3}{dBm} contre boucle fermée, seuil de congestion fixe à la moitié de la file, coût de chemin sans l'occupation de la file du voisin, poids du coût de lien sans $\kappa(P)$ et profil des frames M pour le contrôle, le niveau 1 des frames D et la découverte",
         'la série figée de E3 oppose la pile figée à la même pile qui apprend'),
        ('E4, débit égal : face au GOP 9-3',
         r"paquets d'une image espacés de 15,5 et \SI{16,1}{\milli\second} au lieu de 15, dernière image capturée 3,5 et \SI{7,2}{\second} plus tard, durée simulée de 173 et \SI{177}{\second} contre 169, nœuds hors service avant la fin de 5,8 à 6,5 contre 4,5 à 6,0",
         'débit moyen égalisé, non le débit instantané'),
        ("E5, charge : d'une période à l'autre",
         r"paquets espacés de $T/600$, durée simulée de 145, 204, 274 et \SI{415}{\second} contre 168, nœuds hors service avant la fin de 3,4 à 17,0 contre 4,5 à 6,0",
         'piles comparées à $T$ égal'),
        ('E5, scénario : vidéos forêt et fumée',
         r"coefficient de GOP qui leur donne neuf frames M, 0,01597 et 0,15671 ; 5\,704 et 3\,925 paquets contre 5\,352",
         'piles comparées à variante égale'),
        (r'E5, scénario : grille de \SI{30}{\metre}',
         r"nœuds déplacés de \SI{8}{\metre} au plus sur chaque axe, le même déploiement à chaque simulation ; liens de \SI{30}{\metre} reçus \SI{4,0}{\deci\bel} plus fort, liens diagonaux à portée, dix sauts au moins de la caméra au puits contre dix-huit ; allumages ramenés à 0,75 fois leur position ; vitesse, vent et zone chauffée inchangés ; nœuds hors service avant la fin de 9,0 à 11,4 contre 4,5 à 6,0",
         'piles comparées à variante égale'),
        ('E5, scénario : caméra et puits échangés', 'allumages inchangés : F2 referme le coin du puits, F3 celui de la caméra', 'piles comparées à variante égale'),
        (r'E5, scénario : vitesse de tête $\times 1{,}5$',
         r"vent inchangé, F2 et F3 à \SI{15}{\percent} du vent ouvert au lieu de 10 ; zone chauffée et fenêtre des pannes de \SI{60}{\second}, donc 1,5 fois plus profondes en mètres ; nœuds hors service avant la fin de 9,4 à 13,0 contre 4,5 à 6,0",
         'piles comparées à variante égale'),
        ('Point de fonctionnement', r"$T$ et les trois feux calibrés sur la pile à règles, pour 40 à \SI{60}{\percent} de pertes au GOP 9-3", 'E5, charge'),
    )),
    'en': (('Comparison', 'What differs besides the factor', 'Handled by'), (
        ('Rules, hybrid, agents', r'nothing: only the compared decisions change, with the structure they carry (Section~\ref{sec:methods-protocol})', 'not applicable'),
        ('AOMDV: the stack',
         r'routing, queue ordered by rank, measurements of the derived physical layer, closed-loop power, up to 26 paths (2 per priority level) against 3, route-request rebroadcast delay of 50 against \SI{10}{\milli\second}',
         'compared factor'),
        ('AOMDV: the access layer', 'original access layer, without the transmission watchdog', 'E1, control AOMDV'),
        ('AOMDV: the timers',
         r'HELLO messages every second against every \SI{5}{\second}, longest delay before a new route request 1 against \SI{10}{\second}, active route 10 against \SI{35}{\second}, 64 against 16 packets waiting for a route',
         'E1, AOMDV with cross-layer timers'),
        ('E3: frozen stack against the rules',
         r"fixed power at \SI{+3}{dBm} against the closed loop, congestion threshold fixed at half the queue, path cost without the queue occupancy of the neighbor, link-cost weights without $\kappa(P)$ and the M-frame profile for control, level 1 of the D frames and discovery",
         'the frozen series of E3 compares the frozen stack with the same stack learning'),
        ('E4, equal rate: against GOP 9-3',
         r'packets of an image spaced 15.5 and \SI{16.1}{\milli\second} apart instead of 15, last image captured 3.5 and \SI{7.2}{\second} later, simulated duration of 173 and \SI{177}{\second} against 169, nodes out of service before the end 5.8 to 6.5 against 4.5 to 6.0',
         'mean rate equalized, not the instantaneous rate'),
        ('E5, load: from one period to the next',
         r'packets spaced $T/600$ apart, simulated time 145, 204, 274 and \SI{415}{\second} against 168, nodes out of service before the end from 3.4 to 17.0 against 4.5 to 6.0',
         'stacks compared at equal $T$'),
        ('E5, scenario: forest and smoke videos',
         r'GOP coefficient giving them nine M frames, 0.01597 and 0.15671; 5704 and 3925 packets against 5352',
         'stacks compared within a variant'),
        (r'E5, scenario: \SI{30}{\metre} grid',
         r'nodes moved by at most \SI{8}{\metre} on each axis, the same deployment in every run; \SI{30}{\metre} links received \SI{4.0}{\deci\bel} stronger, diagonal links in range, at least ten hops from the camera to the sink against eighteen; ignitions brought to 0.75 times their position; speed, wind and heated zone unchanged; nodes out of service before the end from 9.0 to 11.4 against 4.5 to 6.0',
         'stacks compared within a variant'),
        ('E5, scenario: camera and sink swapped', 'ignitions unchanged: F2 closes the corner of the sink, F3 that of the camera', 'stacks compared within a variant'),
        (r'E5, scenario: head spread rate $\times 1.5$',
         r'wind unchanged, F2 and F3 at \SI{15}{\percent} of the open wind instead of 10; heated zone and failure window of \SI{60}{\second}, hence 1.5 times deeper in meters; nodes out of service before the end from 9.4 to 13.0 against 4.5 to 6.0',
         'stacks compared within a variant'),
        ('Operating point', r'$T$ and the three fires calibrated on the rule stack, for 40 to \SI{60}{\percent} loss at GOP 9-3', 'E5, load'),
    )),
}


def names(path):
    return [l.split()[0] for l in open(path) if l.strip() and not l.startswith('#')]


def conf():
    out = {}
    for l in open(os.path.join(EXP, 'sets.conf')):
        if l.startswith('set_'):
            v = l.split()
            out[v[0]] = {'proto': v[1], 'label': v[2], 'video': v[7], 'sims': int(v[12])}
    return out


def experiment(name):
    """The (experiment, series) of a study-10 set, or None outside the plan."""
    for e, series, pat in RULES:
        if re.search(pat, name):
            return e, series
    return None


def stack_of(name):
    if name.startswith('set_AOMDV_GC'):
        return 'pq'
    for s in ('rules', 'hybrid', 'agents'):
        if re.search(r'-fire[23]?-%s(-|$)' % s, name):
            return s
    raise SystemExit('no stack in ' + name)


def gop_coef(label):
    """gopCoef of the encoder file ipFiles/<label>.dat (fifth column)."""
    path = os.path.join(EXP, 'ipFiles', label + '.dat')
    rows = [l.split() for l in open(path) if l.strip() and not l.startswith('#')]
    if len(rows) != 1:
        raise SystemExit('%s: one encoder line expected' % path)
    return rows[0][4]


def gop_of(name, c):
    if c['video'] != 'fire_qcif':
        return '9-3'
    coef = gop_coef(c['label'])
    if coef not in GOP:
        raise SystemExit('%s: gopCoef %s outside the three GOPs' % (name, coef))
    return GOP[coef]


def collect():
    """rows[experiment][series] = configurations, simulations, stacks and GOPs."""
    c = conf()
    exp = {e: {} for e in ORDER}
    for n in names(os.path.join(EXP, 'studies', '09-comparative', 'sets.txt')):
        exp['E0'].setdefault(None, []).append(n)
    for n in names(os.path.join(EXP, 'studies', '10-revision', 'sets.txt')):
        r = experiment(n)
        if r is None:
            raise SystemExit('set outside the plan: ' + n)
        exp[r[0]].setdefault(r[1], []).append(n)
    rows = {}
    for e in ORDER:
        rows[e] = {}
        for series in sorted(exp[e], key=lambda x: -1 if x is None else SERIES_ORDER.index(x)):
            sets = exp[e][series]
            missing = [n for n in sets if n not in c]
            if missing:
                raise SystemExit('%s: not in sets.conf: %s' % (e, missing[0]))
            rows[e][series] = {'n': len(sets), 'sims': sum(c[n]['sims'] for n in sets),
                               'stacks': {stack_of(n) for n in sets},
                               'gops': {gop_of(n, c[n]) for n in sets}}
    # the overheads have no simulation of their own: they are measured on E0 and
    # the traces at GOP 9-3, with the four stacks
    rows['costs'] = {None: {'n': 0, 'sims': 0, 'stacks': {s for s, _, _ in STACKS}, 'gops': {'9-3'}}}
    return rows


def num(n, lang):
    s = '{:,}'.format(n)
    if lang == 'fr':
        return s.replace(',', '\\,')
    # English (MDPI): no separator at four digits, a comma from five
    return s if n >= 10000 else str(n)


def stacks_text(st, lang):
    t = TEXT[lang]
    if st == {s for s, _, _ in STACKS}:
        return t['all']
    return ', '.join((fr if lang == 'fr' else en) for s, fr, en in STACKS if s in st)


def gops_text(g, lang):
    return ', '.join(x for x in GOPS if x in g) if g else TEXT[lang]['none']


# Column widths (they sum to 7) and the space between columns: the factor and
# its levels, the longest cells, get the width the short cells of the stacks,
# the GOP and the counts do not need; the question keeps its longest word
# ("Implémentation", 71 pt), the stacks hold "agents, hybride" (68.3 pt) and
# the GOP "3-9, 6-6, 9-3" (50.0 pt) on one line, "Configurations" (68.3 pt
# bold), "Section" (33.9 pt) and the first header, "Experiment" (52.8 pt bold,
# the explanatory heading MDPI asks for instead of "Exp."), hold on one line;
# the question of E4 keeps its four lines in French (81 pt). Measured on the
# MDPI class, 12 Nov 2026: one width unit is 69.0 pt at the full length,
# 525.2 pt, less 3 pt on each side of the seven columns.
PLAN_WIDTHS = ('0.78', '1.18', '1.80', '1.00', '0.74', '1.00', '0.50')
PLAN_SEP = '3pt'
# The series of an experiment, one per line of a nested tabular that spans the
# factor, stacks and GOP columns (no
# partial rule cuts the row of an experiment). Its columns take the widths of
# the columns they sit under: an X column of this table is w/7 of the full
# length less the 14 column separations, and two inner columns stay apart by
# the two separations of the outer table. The series are set apart by this
# space, the experiments by a \midrule; the experiment, its question, counts
# and sections, plain cells of the outer row, are centered on all its series
# (the X columns of the MDPI class are m columns).
SERIES_GAP = '3pt'  # as the groups of text rows of Table 1 (13 Nov 2026)


def series_block(cells):
    """The series of one experiment (factor, stacks, GOP per series) as one
    cell spanning the three columns, a series per line."""
    spec = '@{\\hspace{2\\tabcolsep}}'.join(
        '>{\\centering\\arraybackslash}m{\\dimexpr(\\fulllength-14\\tabcolsep)*%d/700\\relax}' % round(float(w) * 100)
        for w in PLAN_WIDTHS[2:5])
    body = (' \\\\ \\addlinespace[%s]\n' % SERIES_GAP).join(' & '.join(c) for c in cells)
    return '\\multicolumn{3}{c}{\\begin{tabular}[c]{@{}%s@{}}\n%s\n\\end{tabular}}' % (spec, body)


def write_plan(lang, rows):
    t = TEXT[lang]
    out = ['% generated by make_tables_plan.py from studies/*/sets.txt, sets.conf and ipFiles/',
           table_cells.setup(lang), '\\setlength{\\tabcolsep}{%s}' % PLAN_SEP,
           '\\begin{tabularx}{\\fulllength}{%s}'
           % ''.join('>{\\hsize=%s\\hsize\\centering\\arraybackslash}X' % w for w in PLAN_WIDTHS),
           '\\toprule', ' & '.join('\\textbf{%s}' % h for h in t['head']) + ' \\\\', '\\midrule']
    # The two measurements (agents' traces, costs for a node) have no factor of
    # their own: the choice of 12 Nov 2026 moves them to the text of
    # section 2.7; the table lists the experiments E0 to E5 only.
    exps = [e for e in ORDER if e not in MEASURES]
    for i, e in enumerate(exps):
        series = rows[e]
        # the Holm family of each series is said once, in the method (section 2.7)
        q, f = t[e]
        n = sum(r['n'] for r in series.values())
        sims = sum(r['sims'] for r in series.values())
        conf_cell = t['none'] if n == 0 else '%s (%s)' % (num(n, lang), num(sims, lang))
        # the English labels are the French ones without the fr- infix
        sec = ', '.join('\\ref{sec:%sresults-%s}' % ('fr-' if lang == 'fr' else '', s) for s in SECTION[e])
        code = t['measure'] if e in MEASURES else e
        # one line per series, its factor and levels, stacks and GOPs; the
        # experiment, its question, counts and sections written once, centered
        # on its series; no rule between the series of an experiment, a
        # \\midrule between experiments
        cells = [(f[name] if isinstance(f, dict) else f, stacks_text(r['stacks'], lang), gops_text(r['gops'], lang))
                 for name, r in series.items()]
        middle = ' & '.join(cells[0]) if len(cells) == 1 else series_block(cells)
        out.append(' & '.join((code, q, middle, conf_cell, sec)) + ' \\\\')
        out.append('\\bottomrule' if i == len(exps) - 1 else '\\midrule')
    out.append('\\end{tabularx}')
    path = os.path.join(OUT, '%s_revision-plan.tex' % lang)
    open(path, 'w').write('\n'.join(out) + '\n')
    return path


def write_equite(lang):
    head, rows = EQUITE[lang]
    out = ['% generated by make_tables_plan.py',
           '\\begin{tabularx}{\\fulllength}{>{\\hsize=0.8\\hsize\\centering\\arraybackslash}X'
           '>{\\hsize=1.65\\hsize\\centering\\arraybackslash}X>{\\hsize=0.55\\hsize\\centering\\arraybackslash}X}',
           '\\toprule', ' & '.join('\\textbf{%s}' % h for h in head) + ' \\\\', '\\midrule']
    for i, r in enumerate(rows):
        out.append(' & '.join(r) + ' \\\\')
        out.append('\\bottomrule' if i == len(rows) - 1 else '\\midrule')
    out.append('\\end{tabularx}')
    path = os.path.join(OUT, '%s_revision-equite.tex' % lang)
    open(path, 'w').write('\n'.join(out) + '\n')
    return path


def main():
    rows = collect()
    for e in ORDER:
        for series, r in rows[e].items():
            print('%-6s %-8s %3d configurations, %5d simulations, stacks %s, GOP %s, Holm %s'
                  % (e, series or '-', r['n'], r['sims'], sorted(r['stacks']), sorted(r['gops']),
                     FAMILY[series or e]))
    print('total study 10: %d configurations'
          % sum(r['n'] for e in ORDER if e != 'E0' for r in rows[e].values()))
    for lang in ('fr', 'en'):
        print(write_plan(lang, rows))
        print(write_equite(lang))


if __name__ == '__main__':
    main()
