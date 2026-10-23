#!/bin/bash
#
# encode_only.sh - stage E1 of the comparative study: compression alone.
#
# Runs EvalVSN's encoding pass and its lossless decoding on one video for
# one or more encoder files, without ns-2, and prints per point what the
# thesis's rubric etude-comparative reads at that stage: the reference PSNR
# and SSIM, the encoded bytes and the compression ratio, the M and D frame
# counts, and the trace statistics of paper 4 (mean frame size, coefficient
# of variation, peak-to-mean ratio, mean and peak bit rate). Nothing here
# touches sets/ or sets.conf: the outputs live under the directory given
# with -od (default e1/, ignored by git), one subdirectory per video and
# encoder file, and several instances may run side by side, since no lock
# and no shared file is involved - the cheap, parallel stage.
#
# Usage:
#   bash tools/encode_only.sh -vf ../evalvsn/videos/fire_qcif.avi ipFiles/GC-PN-0p03-13.dat [more.dat...]
#   bash tools/encode_only.sh -vf ../evalvsn/videos/ir_qcif.avi -od /tmp/e1 ipFiles/*.dat
#   bash tools/encode_only.sh -vf VIDEO -fps 1 FILE.dat      # the decoded frame rate the trace stats assume
#
# Output: one tab-separated line per simId on stdout (a header first), and
# the same lines appended to <od>/e1.tsv, so a sweep over many files
# accumulates one table.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STUDY="$HERE/.."                      # tools/ sits under experiments/
EVALVSN="$STUDY/../evalvsn/bin/evalvsn"
OUT="$STUDY/e1"
VIDEO=""
FPS=1
FILES=()

die() { echo "encode_only.sh: $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    -vf) shift; VIDEO="$1" ;;
    -od) shift; OUT="$1" ;;
    -fps) shift; FPS="$1" ;;
    -h|--help) sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) die "unknown option $1" ;;
    *) FILES+=("$1") ;;
  esac
  shift
done

[[ -n "$VIDEO" ]] || die "give the video with -vf"
[[ -f "$VIDEO" ]] || die "no such video: $VIDEO"
[[ ${#FILES[@]} -gt 0 ]] || die "give at least one encoder file"
[[ -x "$EVALVSN" ]] || die "$EVALVSN is not built"
command -v python3 >/dev/null || die "python3 is needed for the trace statistics"

videoName="$(basename "${VIDEO%.*}")"
mkdir -p "$OUT"
table="$OUT/e1.tsv"
header=$'video\tsimId\tframes\tM\tD\trefPSNR\trefSSIM\tbytes\tcompRatio\tmeanFrameBytes\tcov\tpeakMean\tmeanKbps\tpeakKbps'
[[ -f "$table" ]] || printf '%s\n' "$header" > "$table"
printf '%s\n' "$header"

for ip in "${FILES[@]}"; do
  [[ -f "$ip" ]] || die "no such encoder file: $ip"
  label="$(basename "${ip%.*}")"
  od="$OUT/$videoName/$label"
  rm -rf "$od"
  mkdir -p "$od"
  "$EVALVSN" -ip "$ip" -vf "$VIDEO" -od "$od" > "$od/encode.log" 2>&1 \
    || die "evalvsn failed on $ip (see $od/encode.log)"

  # One simId per non-comment line of the encoder file; the encoding pass
  # leaves <video>-<simId>/st-frame.dat with the reference PSNR and SSIM per
  # frame once the lossless decode has run, which the pass does.
  while read -r simId _rest || [[ -n "${simId:-}" ]]; do
    [[ -z "$simId" || "$simId" == \#* ]] && continue
    st="$od/$videoName-$simId/st-frame.dat"
    [[ -f "$st" ]] || die "$st missing after encoding"
    line="$(python3 - "$st" "$videoName" "$simId" "$FPS" <<'PY'
import sys, math
st, video, sim, fps = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
rows = []
for l in open(st, encoding='utf8'):
    if l.startswith('#') or not l.strip():
        continue
    f = l.split()
    rows.append((f[1], int(f[2]), float(f[3]), float(f[4])))
enc = [r for r in rows if r[0] in ('M', 'D')]
sizes = [r[1] for r in enc]
n = len(rows); m = sum(1 for r in enc if r[0] == 'M'); d = sum(1 for r in enc if r[0] == 'D')
psnr = sum(r[2] for r in rows) / n if n else 0.0
ssim = sum(r[3] for r in rows) / n if n else 0.0
total = sum(sizes)
mean = total / len(sizes) if sizes else 0.0
var = sum((x - mean) ** 2 for x in sizes) / (len(sizes) - 1) if len(sizes) > 1 else 0.0
cov = math.sqrt(var) / mean if mean else 0.0
peak = max(sizes) if sizes else 0
raw = 25344 * n            # bytes of the captured greyscale QCIF frames
ratio = raw / total if total else 0.0
print('\t'.join([video, sim, str(n), str(m), str(d), f'{psnr:.3f}', f'{ssim:.5f}', str(total),
                 f'{ratio:.3f}', f'{mean:.1f}', f'{cov:.3f}', f'{peak / mean if mean else 0:.3f}',
                 f'{mean * 8 * fps / 1000:.2f}', f'{peak * 8 * fps / 1000:.2f}']))
PY
)"
    printf '%s\n' "$line" | tee -a "$table"
  done < "$ip"
done
