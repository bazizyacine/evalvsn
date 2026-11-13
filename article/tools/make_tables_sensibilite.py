#!/usr/bin/env python3
"""LaTeX table of the scenario sensitivity of the revision (study 10, tranche T4,
experiment E5, fire series), French and English, from src/data/revision.tsv through
analyse_sensibilite.py.

One row for the reference scenario, then one per variant. The rule stack's
packet loss and the PSNR of the rule and hybrid stacks (means over the 60
seeds), as in the load and scenario tables, then the per-seed difference hybrid -
rules: PSNR with its p (Holm over the seven variants; the reference row's p is a
single test), M1 loss, usable images and declarations. A cell that the 60 seeds
do not all back prints as a dash.

Usage: python3 tools/make_tables_sensibilite.py [OUT_DIR]   (default: src/tables)
"""
import os
import sys
from decimal import ROUND_HALF_UP, Decimal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import analyse_sensibilite as an  # noqa: E402
import table_cells  # noqa: E402

# The reference row names its values, the caption no longer does (the choice of 13 Nov 2026): AZ = 1 dB/m and PF = 0.5, the
# firealpha and firefail of every set of study 09 (sets.conf). On one
# line, the label widens the first column, pushes the loss and PSNR
# headers onto two lines and overfills the number cells: the values go on
# a second line of the label, no wider than the longest variant (seen on
# the MDPI class, 13 Nov 2026).
REFERENCE = '\\begin{tabular}[c]{@{}l@{}}%s\\\\ ($\\mathrm{AZ} = \\SI{1}{\\deci\\bel\\per\\metre}$, $\\mathrm{PF} = %s$)\\end{tabular}'
ROWS = ((None, REFERENCE % ('Référence', '0{,}5'), REFERENCE % ('Reference', '0.5')),
        ('az015', '$\\mathrm{AZ} = \\SI{0,15}{\\deci\\bel\\per\\metre}$', '$\\mathrm{AZ} = \\SI{0.15}{\\deci\\bel\\per\\metre}$'),
        ('az05', '$\\mathrm{AZ} = \\SI{0,5}{\\deci\\bel\\per\\metre}$', '$\\mathrm{AZ} = \\SI{0.5}{\\deci\\bel\\per\\metre}$'),
        ('az165', '$\\mathrm{AZ} = \\SI{1,65}{\\deci\\bel\\per\\metre}$', '$\\mathrm{AZ} = \\SI{1.65}{\\deci\\bel\\per\\metre}$'),
        ('pf0', '$\\mathrm{PF} = 0$', '$\\mathrm{PF} = 0$'),
        ('pf025', '$\\mathrm{PF} = 0{,}25$', '$\\mathrm{PF} = 0.25$'),
        ('pf075', '$\\mathrm{PF} = 0{,}75$', '$\\mathrm{PF} = 0.75$'),
        ('fd1', 'Pannes près du front\\textsuperscript{1}', 'Failures near the front\\textsuperscript{1}'))
# The groups of the load and scenario tables: the PSNR of the two stacks, then
# every per-seed difference under Hybrid - Rules, the PSNR gap first.
GROUPS = {'fr': ('PSNR (dB)', 'Hybride $-$ Règles'),
          'en': ('PSNR (dB)', 'Hybrid $-$ Rules')}
# The rule stack's loss first, its stack heading the column, as in the load and
# scenario tables (the author's request of 12 Nov 2026). Images and Decl. are
# defined in the metrics table (tab:metriques) and the reference values in the
# reference row; the only note left defines the failure law near the front.
LOSS_STACK = {'fr': 'Règles', 'en': 'Rules'}
HEAD = {'fr': ('Variante', 'Perte (\\%)', 'Règles', 'Hybride', 'PSNR (dB)', '$p$', 'Perte M1 (points)',
               'Images', 'Décl.'),
        'en': ('Variant', 'Loss (\\%)', 'Rules', 'Hybrid', 'PSNR (dB)', '$p$', 'M1 loss (points)',
               'Images', 'Decl.')}
# Each X column holds its widest unbreakable header word or cell plus an
# equal share of the free width; with the loss column, the loss, the PSNR gap
# and the M1 loss headers take two lines (measured on the MDPI class,
# 12 Nov 2026: "Hybride" 38.3 pt bold, "Images" 37.1 pt, "Perte M1" 40.8 pt,
# "< 0,001" 33.6 pt, one unit about 40 pt). The two-line reference label
# widens the first column (13 Nov 2026): the loss column takes 0.05 of the
# images column, so that its bold "Règles" still holds on its width.
WIDTHS = ('0.82', '0.95', '1.15', '0.88', '1.03', '1.21', '1.07', '0.89')


def rounded(x, nd):
    # Half up, as the text rounds: 0.125 prints 0.13.
    return Decimal(repr(x)).quantize(Decimal(1).scaleb(-nd), rounding=ROUND_HALF_UP)


def plain(x, nd, lang):
    if x is None:
        return '--'
    s = str(rounded(x, nd))
    return s.replace('.', '{,}') if lang == 'fr' else s


def signed(x, nd, lang):
    if x is None:
        return '--'
    d = rounded(x, nd)
    s = str(abs(d))
    s = s.replace('.', '{,}') if lang == 'fr' else s
    if d == 0:
        return s
    return ('+' if d > 0 else '$-$') + s


def pval(p, lang):
    if p is None:
        return '--'
    if p < 0.001:
        return '$< 0{,}001$' if lang == 'fr' else '$< 0.001$'
    s = str(rounded(p, 3))
    return s.replace('.', '{,}') if lang == 'fr' else s


def gain(v, metric, fams, ref):
    """(mean, Holm p) of hybrid - rules under variant v; the reference's p is a single test."""
    if v is None:
        t = ref[metric]
        return (t['mean'], t['p']) if t['n'] == an.SEEDS else (None, None)
    t = fams[metric][v]
    return (t['mean'], t['holm']) if t else (None, None)


def write(lang, means, fams, ref, out_dir):
    g1, g2 = GROUPS[lang]
    cols = 'l' + ''.join('>{\\hsize=%s\\hsize\\centering\\arraybackslash}X' % f for f in WIDTHS)
    out = ['% generated by tools/make_tables_sensibilite.py from src/data/revision.tsv',
           table_cells.setup(lang),
           '\\begin{tabularx}{\\linewidth}{%s}' % cols, '\\toprule',
           ' & \\textbf{%s} & \\multicolumn{2}{c}{\\textbf{%s}} & \\multicolumn{5}{c}{\\textbf{%s}} \\\\' % (LOSS_STACK[lang], g1, g2),
           '\\cmidrule(lr){2-2}\\cmidrule(lr){3-4}\\cmidrule(lr){5-9}',
           ' & '.join('\\textbf{%s}' % h for h in HEAD[lang]) + ' \\\\', '\\midrule']
    body = []
    for i, (v, fr, en) in enumerate(ROWS):
        a = lambda s: s if v is None else '%s-%s' % (s, v)  # noqa: E731
        psnr, p = gain(v, 'psnr', fams, ref)
        cells = [plain(means[a('rules')]['loss'], 1, lang),
                 plain(means[a('rules')]['psnr'], 2, lang), plain(means[a('hybrid')]['psnr'], 2, lang),
                 signed(psnr, 2, lang), pval(p, lang),
                 signed(gain(v, 'm1loss', fams, ref)[0], 1, lang), signed(gain(v, 'usable', fams, ref)[0], 1, lang),
                 signed(gain(v, 'declared', fams, ref)[0], 1, lang)]
        body.append([fr if lang == 'fr' else en] + cells)
    # the numbers of each column aligned on the decimal marker
    body = table_cells.align_rows(body, lang, range(1, 9), pcols=(5,))
    for i, r in enumerate(body):
        out.append(' & '.join(r) + ' \\\\')
        out.append('\\bottomrule' if i == len(body) - 1 else '\\midrule')
    out.append('\\end{tabularx}')
    path = os.path.join(out_dir, '%s_revision-sensibilite.tex' % lang)
    with open(path, 'w') as f:
        f.write('\n'.join(out) + '\n')
    return path


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'src', 'tables')
    means = an.means()
    fams = {m: an.family('gain', m) for m in ('psnr', 'm1loss', 'usable', 'declared')}
    ref = {m: an.summary(list(an.contrast('gain', None, m).values())) for m in ('psnr', 'm1loss', 'usable', 'declared')}
    for lang in ('fr', 'en'):
        print(write(lang, means, fams, ref, out_dir))


if __name__ == '__main__':
    main()
