#!/usr/bin/env python3
"""Shared layout of the article's generated tables (structure pass of
12 Nov 2026): the numbers of a column aligned on the decimal marker, the
local siunitx settings of a table file, and the order of the compared items.

The generators keep their own rounding: align() reads the cells they have
already formatted and rewrites each number inside siunitx's \\tablenum with
the same digits, so that no value changes. Inside a centered X column every
\\tablenum of the column gets the same box (its table-format), hence the
numbers line up on the decimal marker and the column stays centered under
its header. A comparator ("< 0,001") hangs before the box, and the other
cells of its column take a phantom of the same width. A cell that is not a
number (a dash, a level, a label) is left as it is.

The conventions themselves (order, decimals, headers, rules) are written in
the article's table conventions; this module only applies them.
"""
import re

# The compared items, in the order of every table and every list of the
# article: the stacks, the gaps between them, the GOPs and the fires.
STACKS = ('pq', 'rules', 'agents', 'hybrid')
GAPS = (('hybrid', 'rules'), ('agents', 'rules'), ('hybrid', 'agents'))
GOPS = ('3-9', '6-6', '9-3')
FIRES = ('F1', 'F2', 'F3')


def setup(lang):
    """Local siunitx settings, written at the head of a table file (the table
    float is a group, so they stop at its end): an explicit plus kept on the
    gaps; digit groups as each edition writes them, a thin space from four
    digits in French, a comma from five in English (MDPI)."""
    if lang == 'fr':
        return '\\sisetup{retain-explicit-plus, group-minimum-digits = 4}'
    return '\\sisetup{retain-explicit-plus, group-separator = {,}, group-minimum-digits = 5}'


def rules(groups):
    """The rule written after each row of a table body, from the group of
    each row (the label a \\multirow writes once, vertically centered, over
    the rows of its group; a table without groups gives each row a group of
    its own): no rule between the rows of a group, a \\midrule between two
    groups, the \\bottomrule after the last row. One rule for every table of
    the article (the choice of 13 Nov 2026): no partial rule
    (\\cmidrule) runs between two rows, hence none beside a group's label; a
    \\cmidrule(lr) is drawn only under a header that spans several columns.
    Returns one string per row, '' within a group."""
    groups = list(groups)
    out = []
    for i, g in enumerate(groups):
        if i == len(groups) - 1:
            out.append('\\bottomrule')
        else:
            out.append('\\midrule' if groups[i + 1] != g else '')
    return out


def parse(cell, lang):
    """(comparator, sign, integer digits, decimal digits) of a formatted
    number, or None when the cell is not a number."""
    s = cell.strip()
    cmp = ''
    m = re.fullmatch(r'\$<\s*(.*)\$', s)
    if m:
        cmp, s = '<', m.group(1).strip()
    sign = ''
    if s.startswith('$-$'):
        sign, s = '-', s[3:]
    elif s[:1] in '+-' and s[:1]:
        sign, s = s[0], s[1:]
    if lang == 'fr':
        s = s.replace('{,}', '.').replace('\\,', '')
    else:
        s = re.sub(r'(?<=\d),(?=\d{3}(?!\d))', '', s)
    m = re.fullmatch(r'(\d+)(?:\.(\d+))?', s)
    if not m:
        return None
    return cmp, sign, m.group(1), m.group(2) or ''


def align(column, lang, pvalues=False):
    """The cells of one column, numbers rewritten as \\tablenum of a common
    format; returns the column unchanged when it holds no number. A column of
    p (pvalues) keeps the comparator's room even when no cell needs it, so
    that every p column of the article puts its digits at the same place."""
    parsed = [parse(c, lang) for c in column]
    nums = [p for p in parsed if p]
    if not nums:
        return list(column)
    signed = any(p[1] for p in nums)
    comparator = pvalues or any(p[0] for p in nums)
    whole = max(len(p[2]) for p in nums)
    frac = max(len(p[3]) for p in nums)
    fmt = '%s%d.%d' % ('+' if signed else '', whole, frac)
    # siunitx's input is the decimal point in both editions: it prints the
    # edition's marker (a comma in French, set in the preamble), and the
    # source never reads "1,000" as a thousand
    mark = '.'
    out = []
    for c, p in zip(column, parsed):
        if not p:
            out.append(c)
            continue
        cmp, sign, i, d = p
        num = '\\tablenum[table-format=%s]{%s%s%s}' % (fmt, sign, i, mark + d if d else '')
        if comparator:
            # the comparator, its space and the number in one box: no line
            # break inside a cell (an overfull box shows a column too narrow)
            num = '\\mbox{%s%s}' % ('$<$\\;' if cmp else '\\phantom{$<$\\;}', num)
        out.append(num)
    return out


def align_rows(rows, lang, columns=None, pcols=()):
    """Align, column by column, a list of rows (lists of cells of equal
    length). columns: the indices to align, every column by default; pcols:
    the indices of the columns of p."""
    if not rows:
        return rows
    n = len(rows[0])
    cols = range(n) if columns is None else columns
    out = [list(r) for r in rows]
    for j in cols:
        col = align([r[j] for r in rows], lang, j in pcols)
        for r, c in zip(out, col):
            r[j] = c
    return out
