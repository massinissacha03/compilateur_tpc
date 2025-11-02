#!/bin/bash

echo "=== Déploiement des tests ==="
echo

total_all=0
pass_all=0

for category in good warn sem_err syn-err; do
    echo "=== Tests dans $category/ ==="
    total=0
    pass=0

    for f in tests/$category/*.tpc; do
        [ -f "$f" ] || continue
        ((total++))
        ((total_all++))

        echo -n "  $f => "
        ./bin/tpcc < "$f" > /dev/null 2>tmp.err
        code=$?
        echo "Code $code"

        # Avertissements pour good et warn
        if [[ "$category" == "good" || "$category" == "warn" ]]; then
            if grep -iq "avertissement\|warning" tmp.err; then
                echo "    Avertissement détecté :"
                grep -i "avertissement\|warning" tmp.err
            fi
        fi

        # Succès = code 0 pour good/warn, 2 pour sem_err, 1 pour syn-err
        if [[ "$category" == "good" && $code -eq 0 ]] ||
           [[ "$category" == "warn" && $code -eq 0 ]] ||
           [[ "$category" == "sem_err" && $code -eq 2 ]] ||
           [[ "$category" == "syn-err" && $code -eq 1 ]]; then
            ((pass++))
            ((pass_all++))
        fi
    done

    if (( total > 0 )); then
        pct=$((100 * pass / total))
        echo "=> Score $category : $pass / $total => $pct%"
    else
        echo "=> Aucun test trouvé pour $category/"
    fi
    echo
done

echo "=============================="
if (( total_all > 0 )); then
    pct=$((100 * pass_all / total_all))
    echo "Score global : $pass_all / $total_all => $pct%"
else
    echo "Aucun test exécuté."
fi

rm -f tmp.err *asm
