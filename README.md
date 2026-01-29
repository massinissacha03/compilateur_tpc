# Compilateur **TPC**

**Massinissa CHABANE - Boualem CHIBANE**

**Cible :** x86_64 Linux (ELF64, convention d’appel System V AMD64)
**Langage d’implémentation :** C (+ Flex/Bison)

> TPC est un sous‑ensemble pédagogique et strictement typé du langage C. Ce dépôt contient un compilateur « tpcc » qui traduit un programme TPC en assembleur NASM, puis en exécutable ELF64.

---

## Sommaire

* [Aperçu rapide](#aperçu-rapide)
* [Objectifs et fonctionnalités](#objectifs-et-fonctionnalités)
* [Architecture du projet](#architecture-du-projet)
* [Prérequis](#prérequis)
* [Installation & Build](#installation--build)
* [Utilisation](#utilisation)
* [Jeux d’essais (tests)](#jeux-dessais-tests)
* [Sémantique : règles et vérifications](#sémantique--règles-et-vérifications)
* [Génération de code](#génération-de-code)
* [Erreurs & avertissements](#erreurs--avertissements)
* [Exemples TPC](#exemples-tpc)
* [Limitations connues / cas manquants](#limitations-connues--cas-manquants)
* [Corrections (héritées du projet S1)](#corrections-héritées-du-projet-s1)
* [Arborescence](#arborescence)
* [Contribuer](#contribuer)

---

## Aperçu rapide

```bash
make
./bin/tpcc "tests/good/good1.tpc" > "tests/good/good1.asm"
nasm -f elf64 tests/good/good1.asm -o good1.o
gcc -no-pie -nostartfiles good1.o -o good1
./good1
```

---

## Démo & Portfolio

Vous pouvez **tester / découvrir le projet sur le portfolio de Massinissa** :

➡️  [https://massinissa-portfolio.netlify.app/](https://massinissa-portfolio.netlify.app/)

> N'hésitez pas à ajouter un bouton « Tester TPC » sur le portfolio qui renvoie vers cette page ou une démo en ligne.

---

## Objectifs et fonctionnalités

* **Pipeline complet** : analyse lexicale, syntaxique, sémantique, génération d’assembleur NASM.
* **Système sémantique robuste** : gestion des types (`int`, `char`, `void`), des portées (globale, locale, paramètres), des fonctions et de leurs signatures.
* **Fonctions prédéfinies** : `getchar`, `getint`, `putchar`, `putint` (non redéclarables).
* **Code exécutable** : production d’un `.asm` NASM 64 bits, puis d’un ELF64 exécutable via `nasm` + `gcc`.
* **Variables** : locales (pile), paramètres, et **static locales** (section `.data`) comme en C.
* **Offets de pile** : allocation stricte via `rbp` (offsets négatifs), recalculés par fonction.
* **Gestion de void en expression** : erreurs sur usages interdits (ex. `x = putint(5)`).

---

## Architecture du projet

* `src/` : sources C et entêtes (`main.c`, `parser.y`, `lexer.l`, `semantic.c`, `codegen.c/.h`, `tree.h`, …)
* `bin/` : exécutable `tpcc`
* `obj/` : objets intermédiaires
* `rep/` : rapport
* `tests/` : jeux d’essais

  * `good/` : programmes corrects
  * `syn-err/` : erreurs syntaxiques
  * `sem-err/` : erreurs sémantiques
  * `warn/` : programmes valides avec avertissements

---

## Prérequis

* **Linux x86_64**
* **Outils** : `make`, `gcc`, `nasm`, `flex`, `bison`

Installation (Debian/Ubuntu) :

```bash
sudo apt-get update
sudo apt-get install -y build-essential nasm flex bison
```

---

## Installation & Build

Compilation complète :

```bash
make
```

Le binaire est généré dans `./bin/tpcc`.

Nettoyage :

```bash
make clean   # objets
make mrproper # objets + binaires/ASM si prévu par le Makefile
```

---

## Utilisation

1. **Compiler un source TPC en NASM** :

```bash
./bin/tpcc input.tpc > output.asm
```

2. **Assembler & lier** :

```bash
nasm -f elf64 output.asm -o output.o
gcc -no-pie -nostartfiles output.o -o a.out
./a.out
```

> L’option `-nostartfiles` suppose que le prologue/épilogue générés suffisent pour le point d’entrée.

### Astuce : enchaîner en une commande

```bash
SRC=tests/good/good1.tpc; ASM=${SRC%.tpc}.asm; OUT=${SRC##*/}; OUT=${OUT%.tpc}; \
./bin/tpcc "$SRC" > "$ASM" && \
  nasm -f elf64 "$ASM" -o "$OUT.o" && \
  gcc -no-pie -nostartfiles "$OUT.o" -o "$OUT" && \
  ./"$OUT"
```

---

## Jeux d’essais (tests)

* **Exécuter un test “good”** : voir [Utilisation](#utilisation).
* **Valider la batterie** : utilisez vos scripts existants (bons / mauvais). À défaut, exemples :

Vérifier que les bons compilent et s’exécutent :

```bash
for t in tests/good/*.tpc; do
  asm="${t%.tpc}.asm"; out="${t##*/}"; out="${out%.tpc}";
  echo "== $t ==" || exit 1
  ./bin/tpcc "$t" > "$asm" && nasm -f elf64 "$asm" -o "$out.o" && \
  gcc -no-pie -nostartfiles "$out.o" -o "$out" && "./$out";
  echo;
done
```

Vérifier que les **syn-err** et **sem-err** sont bien rejetés :

```bash
for t in tests/syn-err/*.tpc tests/sem-err/*.tpc; do
  echo "== $t ==";
  if ./bin/tpcc "$t" >/dev/null 2>&1; then
    echo "ERREUR: $t aurait dû échouer"; exit 1;
  else
    echo "OK: rejet attendu";
  fi;
  echo;
done
```

---

## Sémantique : règles et vérifications

* **Portées imbriquées** via tables de symboles chaînées :

  * `enter_scope()` à l’entrée d’une fonction/bloc, `exit_scope()` à la sortie.
  * Détection de **redéclarations** et **résolution** des identifiants par portée.
* **Typage strict** (`SYMTYPE_INT`, `SYMTYPE_CHAR`, `SYMTYPE_VOID`) assuré par `check_expression()` :

  * Variables non déclarées → erreur.
  * Appels de fonction : vérification **nombre** et **type** des arguments.
  * Affectation à un identifiant de fonction → erreur.
  * **Interdiction** d’utiliser une fonction `void` dans une expression (ex. `x = putint(5)`).
* **Conversions** :

  * `int → char` implicite **autorisée** mais **avertie**.
* **Fonctions prédéfinies** :

  * `getchar`, `getint`, `putchar`, `putint` injectées en **table globale** au démarrage (non redéclarables).
* **Entrée obligatoire** : présence d’une fonction `int main(void)`.
* **`return`** : types vérifiés vis‑à‑vis du type de retour déclaré.

---

## Génération de code

* **NASM 64 bits** cible **ELF64**, convention d’appel **System V AMD64**.
* **Prologue/épilogue standard** par fonction :

  * Prologue : `push rbp; mov rbp, rsp; ...`
  * Épilogue : `mov rsp, rbp; pop rbp; ret`
* **Paramètres** : via registres **rdi, rsi, rdx, rcx, r8, r9** (puis pile au‑delà).
* **Locaux** : sur la **pile** (offsets **négatifs** depuis `rbp`), `current_offset` réinitialisé par fonction.
* **Globaux & `static` locaux** : section `.data`, **zéro‑initialisés** par défaut.
* **Expressions** :

  * `generate_addsub()` : addition/soustraction via `rax`/`rbx`.
  * `generate_divstar()` : `*`, `/`, `%` avec **test de division par zéro** (fin du programme avec **code retour 5**).
  * `generate_eq()` / `generate_order()` : comparaisons (`sete`, `setne`, `setl`, `setg`, …).
* **Contrôle** : `if`, `ifelse`, `while` sur **étiquettes uniques** (`new_label()`).
* **Fonctions internes conditionnelles** : inclusion de `putint`, `putchar`, etc. **uniquement si utilisées** (`uses_function()`).

---

## Erreurs & avertissements

* **Erreurs sémantiques** : message clair + arrêt de compilation (ex. variable non déclarée, mauvais types, usage invalide de `void`).
* **Erreurs d’exécution générées** : division par zéro → sortie **5**.
* **Avertissements** : conversions implicites `int → char`, potentiels masquages, etc. (selon implémentation).

---

## Exemples TPC

Hello + I/O :

```c
int main(void) {
  putint(42);
  putchar('\n');
  return 0;
}
```

Fonction + paramètres :

```c
int sqr(int x) { return x*x; }
int main(void) {
  return sqr(5);
}
```

`static` local :

```c
int main(void) {
  static int counter; // stockage persistant en .data, init à 0
  counter = counter + 1;
  return counter;
}
```

---

## Limitations connues / cas manquants

Les cas suivants **ne sont pas gérés** (ou partiellement) dans l’état actuel :

1. **Appels imbriqués / expressions complexes** avec multiplications successives non réduites :

   ```c
   int fun(int a){ return a*a*a; }
   int main(void){ return fun(5); }
   ```
2. **Masquage de variable globale par `static` local** :

   ```c
   int a;
   int main(void){ static int a; }
   ```
3. **Récursion** (ex. Fibonacci) – limitations actuelles de la gestion d’appels/niveaux :

   ```c
   int fibonacci(int n){
     if (n == 0 || n == 1) { return 0; }
     return fibonacci(n-1) + fibonacci(n-2);
   }
   ```
4. **Puissance via produits successifs** dans `main` :

   ```c
   int main(void){ int a; a = 3; return a*a*a; }
   ```

> Voir `tests/sem-err/` et `tests/warn/` pour des cas limitrophes supplémentaires.

---

## Corrections (héritées du projet S1)

* **Organisation des tests** : déplacés dans `tests/` (plus à la racine), classés par type.
* **Build** : résolution du warning `implicit declaration of function 'asprintf'`.
* **Diagnostics** : ajout/fiabilisation du **numéro de ligne** (la colonne reste absente).
* **Visualisation AST** : correction de la hiérarchie (`static` devient **fils** et non **père**) + labels explicites.
* **Batterie de tests** : scripts séparés *bons*/*mauvais* (pas encore de rapport synthétique).
* **Rapport** : densifié/synthétisé (objectif : plus court que 20 pages).

---

## Arborescence

```
.
├── bin/                 # tpcc
├── obj/                 # .o intermédiaires
├── rep/                 # rapport
├── src/
│   ├── main.c
│   ├── parser.y         # Bison (syntaxe)
│   ├── lexer.l          # Flex (lexical)
│   ├── semantic.c       # vérifs de types/portées
│   ├── codegen.c/.h     # génération NASM
│   └── tree.h           # AST
└── tests/
    ├── good/
    ├── syn-err/
    ├── sem-err/
    └── warn/
```

---

## Contribuer

Idées d’amélioration :

* Rapport de tests **synthétique** (résumé vert/rouge, temps, etc.).
* Meilleure prise en charge de la **récursion** et des **appels imbriqués**.
* Numéro de **colonne** dans les diagnostics.
* Optimisations simples (propagation de constantes, évitement de `mov` inutiles).

---
