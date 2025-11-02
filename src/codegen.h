#ifndef CODEGEN_H
#define CODEGEN_H
#include "tree.h"



// Déclaration de la fonction qui génère le code asm à partir de l'AST
void generate_code(Node* node, FILE* output);

#endif