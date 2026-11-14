#!/usr/bin/env python3
"""The two tables of video coding
(study 10, tranches T3 and T10; experiment E4), French and English,
from src/data/revision.tsv; means over the
60 seeds, a cell without all 60 printing as a dash. One comparison per table:

- {lang}_revision-volume.tex, the equal-rate series: one row per GOP and alarm
  period, the GOPs in the order of every table (3-9, 6-6, 9-3), GOPs 3-9 and
  6-6 at T = 9 s and again at the mean packet rate of GOP 9-3 (T = 9.65 and
  9.32 s), then GOP 9-3 at T = 9 s; under each measure, the rules stack and
  the hybrid stack side by side;
- {lang}_revision-qualite-coef.tex, the coefficient 0.1 series: one row per
  stack, in the article's order, at GOP 9-3 and T = 9 s; under each measure,
  the quality coefficient 0.01 (E0) and 0.1 (arms -q01) side by side.

No note under either table: Images is defined in the metrics table
(tab:metriques, Section 2.5, 12 Nov 2026); the captions carry the rest (means
over 60 seeds; T = 9 s and equal rate, or GOP 9-3).

Usage: python3 tools/make_tables_volume.py [out_dir]   (default: src/tables)
"""
import os
import sys
from decimal import ROUND_HALF_UP, Decimal

import table_cells

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'src')
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(SRC, 'tables')
# Packets the camera sends per video (the methods' Protocol; the
# lines of each set's encoding/st-priority.dat) and the capture period of each
# set (sets.conf, capture column period:ipt:detect).
PACKETS = {'9M3D': 5352, '6M6D': 5544, '3M9D': 5738}
# At the quality coefficient 0.1, GOP 9-3: the lines of its sets' encoding/st-priority.dat.
PACKETS_Q01 = 2670
PERIOD = {'p9': 9.0, 'p9r32': 9.32, 'p9r65': 9.65}
GOP = {'9M3D': '9-3', '6M6D': '6-6', '3M9D': '3-9'}
# The equal-rate table: its rows, then the two stacks under each measure.
ROWS = (('3M9D', 'p9'), ('3M9D', 'p9r65'), ('6M6D', 'p9'), ('6M6D', 'p9r32'), ('9M3D', 'p9'))
assert tuple(GOP[s] for s, _ in ROWS if _ == 'p9') == table_cells.GOPS
PAIR = (('rules', 'Règles', 'Rules'), ('hybrid', 'Hybride', 'Hybrid'))
# The coefficient table: one row per stack, then the two coefficients.
STACKS = (('pq', 'AOMDV', 'AOMDV'), ('rules', 'Règles', 'Rules'), ('agents', 'Agents', 'Agents'), ('hybrid', 'Hybride', 'Hybrid'))
# the column headers name the coefficient of each column (MDPI: an explanatory
# heading for every column; the conformity check of 12 Nov 2026)
COEF = (('', 'à 0{,}01', 'at 0.01'), ('-q01', 'à 0{,}1', 'at 0.1'))
METRICS = (('psnr', 2), ('m1loss', 1), ('loss', 1), ('usable', 1))
MEASURE = {'fr': ('PSNR (dB)', 'Perte M1 (\\%)', 'Perte (\\%)', 'Images'),
           'en': ('PSNR (dB)', 'M1 loss (\\%)', 'Loss (\\%)', 'Images')}
RATE = {'fr': 'Débit moyen (paquets/s)', 'en': 'Mean rate (packets/s)'}
STACK = {'fr': 'Pile', 'en': 'Stack'}
# Shares of the free width (tabularx), measured on the MDPI class at the full
# width of the page (12 Nov 2026): in the equal-rate table, with a column
# padding of 4 pt as in the scenario-variants table, each "Hybride" on one
# line and the mean rate on two; in the coefficient table, the mean rate's
# header on one line over its two columns.
W_VOLUME = (0.64, 0.62, 1.50) + (1.03,) * 8
W_COEF = (1.48,) * 2 + (0.88,) * 8


def load():
    lines = [l for l in open(os.path.join(SRC, 'data', 'revision.tsv')) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:]]


def mean(rows, arm, split, period, metric):
    v = [float(r[metric]) for r in rows if r['arm'] == arm and r['split'] == split
         and r['period'] == period and r['dist'] == '40' and r[metric] != '']
    return sum(v) / len(v) if len(v) == 60 else None


def num(x, nd, lang):
    if x is None:
        return '--'
    s = str(Decimal(repr(x)).quantize(Decimal(1).scaleb(-nd), rounding=ROUND_HALF_UP))
    return s.replace('.', '{,}') if lang == 'fr' else s


def colspec(widths, stub):
    """A left-aligned stub if asked, then centered X columns."""
    return ('l' if stub else '') + ''.join(
        '>{\\hsize=%.2f\\hsize\\centering\\arraybackslash}X' % w for w in widths)


def header(first, groups, sub):
    """Two header rows: each group over its pair of columns, then every
    column named, the single columns on the second row as the alarm-period
    table writes them."""
    n = len(first)
    top = [''] * n + ['\\multicolumn{2}{c}{\\textbf{%s}}' % g for g in groups]
    rules = ''.join('\\cmidrule(lr){%d-%d}' % (n + 1 + 2 * i, n + 2 + 2 * i) for i in range(len(groups)))
    low = ['\\textbf{%s}' % h for h in first] + ['\\textbf{%s}' % s for _ in groups for s in sub]
    return [' & '.join(top) + ' \\\\', rules, ' & '.join(low) + ' \\\\', '\\midrule']


def save(out, lang, name):
    out.append('\\end{tabularx}')
    path = os.path.join(OUT, '%s_%s.tex' % (lang, name))
    with open(path, 'w') as f:
        f.write('\n'.join(out) + '\n')
    return path


def write_volume(lang, rows):
    """The equal-rate series: the GOP, T and the mean rate, then each
    measure for the rules and the hybrid stacks."""
    k = 1 if lang == 'fr' else 2
    out = ['% generated by tools/make_tables_volume.py from src/data/revision.tsv',
           table_cells.setup(lang), '\\setlength{\\tabcolsep}{4pt}',
           '\\begin{tabularx}{\\linewidth}{%s}' % colspec(W_VOLUME, False), '\\toprule']
    out += header(('GOP', '$T$ (s)', RATE[lang]), MEASURE[lang], [p[k] for p in PAIR])
    splits = [s for s, _ in ROWS]
    body = []
    for split, period in ROWS:
        T = PERIOD[period]
        cells = [num(T, 2 if T != 9.0 else 0, lang), num(PACKETS[split] / (12 * T), 1, lang)]
        cells += [num(mean(rows, arm, split, period, m), nd, lang) for m, nd in METRICS for arm, _, _ in PAIR]
        body.append(cells)
    # T (9 under 9,32), the rate and every measure aligned on the decimal marker
    body = table_cells.align_rows(body, lang)
    # no rule between the rows of a GOP, a \\midrule between two GOPs
    # (table_cells.rules)
    after = table_cells.rules(splits)
    for i, (split, period) in enumerate(ROWS):
        # The GOP named once over its rows, vertically centered.
        span = splits.count(split)
        stub = GOP[split] if span == 1 else ('\\multirow[m]{%d}{*}{%s}' % (span, GOP[split]) if splits.index(split) == i else '')
        out.append(' & '.join([stub] + body[i]) + ' \\\\')
        if after[i]:
            out.append(after[i])
    return save(out, lang, 'revision-volume')


def write_coef(lang, rows):
    """The coefficient 0.1 series at GOP 9-3 and T = 9 s: the four stacks,
    the mean rate and each measure at the coefficients 0.01 and 0.1."""
    k = 1 if lang == 'fr' else 2
    out = ['% generated by tools/make_tables_volume.py from src/data/revision.tsv',
           table_cells.setup(lang),
           '\\begin{tabularx}{\\linewidth}{%s}' % colspec(W_COEF, True), '\\toprule']
    out += header((STACK[lang],), (RATE[lang],) + MEASURE[lang], [c[k] for c in COEF])
    body = []
    for arm, fr, en in STACKS:
        cells = [fr if lang == 'fr' else en]
        cells += [num((PACKETS_Q01 if suf else PACKETS['9M3D']) / (12 * PERIOD['p9']), 1, lang) for suf, _, _ in COEF]
        cells += [num(mean(rows, arm + suf, '9M3D', 'p9', m), nd, lang) for m, nd in METRICS for suf, _, _ in COEF]
        body.append(cells)
    # the numbers of each column aligned on the decimal marker
    body = table_cells.align_rows(body, lang, range(1, 1 + 2 * (1 + len(METRICS))))
    for i, cells in enumerate(body):
        out.append(' & '.join(cells) + ' \\\\')
        out.append('\\bottomrule' if i == len(body) - 1 else '\\midrule')
    return save(out, lang, 'revision-qualite-coef')


def main():
    rows = load()
    for lang in ('fr', 'en'):
        print(write_volume(lang, rows))
        print(write_coef(lang, rows))


if __name__ == '__main__':
    main()
