#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "tree.h"
#include "symbol_table.h" // Ajouté pour SymType

extern int yylineno;
extern int yycolumn;

const char *StringFromLabel[] = {
    "Programme", "FONCTION", "Corps", "Parametres", "type", "void",
    "identificateur", "NUM", "CHARACTER", "ADDSUB", "DIVSTAR",
    "ORDER", "EQ", "OR", "AND", "NOT", "IF", "WHILE", "return",
    "ELSE", "NOT", "static", "AppelFonction", "=", "unary", "IF / ELSE"
};

Node *makeNode(label_t label) {
    Node *node = malloc(sizeof(Node));
    if (!node) {
        printf("Run out of memory\n");
        exit(1);
    }
    node->label = label;
    node->firstChild = node->nextSibling = NULL;
    node->parent = NULL;
    node->lineno = yylineno;
    node->column = yycolumn;
    node->value = NULL;
    node->expr_type = SYMTYPE_UNKNOWN;
    return node;
}

void addSibling(Node *node, Node *sibling) {
    if (!node || !sibling) return;
    Node *current = node;
    while (current->nextSibling) {
        current = current->nextSibling;
    }
    current->nextSibling = sibling;
    sibling->parent = current->parent;
}

void addChild(Node *parent, Node *child) {
    if (!parent || !child) return;
    if (!parent->firstChild) {
        parent->firstChild = child;
    } else {
        addSibling(parent->firstChild, child);
    }
    child->parent = parent;
}

void deleteTree(Node *node) {
    if (!node) return;
    deleteTree(node->firstChild);
    deleteTree(node->nextSibling);
    free(node->value);
    free(node);
}

void printTree(Node *node) {
    static bool rightmost[128];
    static int depth = 0;
    for (int i = 1; i < depth; i++) {
        printf(rightmost[i] ? "    " : "\u2502   ");
    }
    if (depth > 0) {
        printf(rightmost[depth] ? "\u2514\u2500\u2500 " : "\u251c\u2500\u2500 ");
    }
    
    printf("%s", StringFromLabel[node->label]);
    if (node->value != NULL) {
        printf("(%s)", node->value);
    }
    printf("\n");
    depth++;
    for (Node *child = node->firstChild; child != NULL; child = child->nextSibling) {
        rightmost[depth] = (child->nextSibling) ? false : true;
        if (child != NULL) printTree(child);
    }
    depth--;
}