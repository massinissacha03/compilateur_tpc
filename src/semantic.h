#ifndef SEMANTIC_H
#define SEMANTIC_H
#include "tree.h"
#include "symbol_table.h"


// lance l’analyse sémantique à partir de la racine de l’arbre syntaxique

void semantic_analysis(Node* root, int print_tables);

#endif