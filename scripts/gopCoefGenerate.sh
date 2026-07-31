#!/bin/bash

# Usage: ./gopCoefGenerate.sh ../ipFiles/encoder.dat ../videos/mobile_qcif.avi ../sets/set1

ipPath="$1"
videoFile="$2"
outputDirectory="$3"

if [[ -z "$ipPath" || -z "$videoFile" || -z "$outputDirectory" ]]; then
    echo "Usage: $0 <encoder.dat> <videoFile> <outputDirectory>"
    exit 1
fi

gop_start=0.001
gop_end=1
gop_step=0.001

cp "$ipPath" "${ipPath}.bak"

# Extraire imagesNb (4e colonne) et FPS (6e colonne) depuis encoder.dat (première ligne non commentaire)
read imagesNb FPS < <(awk '($0 !~ /^#/ && NF>0){print $4, $6; exit}' "$ipPath")
if [[ -z "$imagesNb" || -z "$FPS" ]]; then
    echo "Impossible d'extraire imagesNb ou FPS depuis $ipPath"
    mv "${ipPath}.bak" "$ipPath"
    exit 1
fi

total_frames=$(( imagesNb * FPS / 25 ))

declare -A found_combinations
results_file="$(mktemp)"

gopCoef=$gop_start
while (( $(echo "$gopCoef <= $gop_end" | bc -l) )); do
    # Change the gopCoef column (8th column)
    awk -v gop="$gopCoef" '{
        if($0 ~ /^#/ || NF==0) { print $0; next }
        $8=gop; print $0
    }' "${ipPath}.bak" > "$ipPath"

    # Exécuter evalvsn
    ../bin/evalvsn -ip "$ipPath" -vf "$videoFile" -od "$outputDirectory"

    # Locate st-frame.dat (simId=1, videoName without extension)
    videoName=$(basename "$videoFile" | cut -d. -f1)
    stPath="$outputDirectory/${videoName}-1"
    frameFile="$stPath/st-frame.dat"

    if [[ -f "$frameFile" ]]; then
        M_count=$(awk '$2=="M"{c++}END{print c+0}' "$frameFile")
        D_count=$(awk '$2=="D"{c++}END{print c+0}' "$frameFile")
        combo="${M_count}M_${D_count}D"
        if [[ -z "${found_combinations[$combo]}" ]]; then
            found_combinations[$combo]="$gopCoef"
            echo "$combo gopCoef=$gopCoef" >> "$results_file"
            echo "gopCoef=$gopCoef : $M_count M, $D_count D"
        fi
    else
        echo "gopCoef=$gopCoef : st-frame.dat not found"
    fi

    # Check whether every combination was found
    all_found=true
    for ((m=1; m<=total_frames; m++)); do
        d=$((total_frames - m))
        combo="${m}M_${d}D"
        if [[ -z "${found_combinations[$combo]}" ]]; then
            all_found=false
            break
        fi
    done
    if $all_found; then
        echo "Toutes les combinaisons ont été trouvées. Arrêt."
        break
    fi

    gopCoef=$(echo "$gopCoef + $gop_step" | bc)
done

mv "${ipPath}.bak" "$ipPath"

echo -e "\nRésumé des combinaisons trouvées (combo gopCoef):"
sort "$results_file"
rm "$results_file"
