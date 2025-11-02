#ifndef TREE_H
#define TREE_H

#include "symbol_table.h" // Pour SymType

typedef enum {
    Prog, fonction, Corps, Parametres, Type, Void, Ident, Num, Character,
    AddSub, Divstar, Order, Eq, Or, And, NOT, If, While, Return, Else, Not,
    Static, funCall, Assign, Unary, ifelse
} label_t;

typedef struct Node {
    label_t label;
    struct Node *firstChild;
    struct Node *nextSibling;
    struct Node *parent;
    int lineno;
    int column;
    char *value;
    SymType expr_type;
} Node;

extern const char *StringFromLabel[];

Node *makeNode(label_t label);
void addSibling(Node *node, Node *sibling);
void addChild(Node *parent, Node *child);
void deleteTree(Node* node);
void printTree(Node *node);

#define FIRSTCHILD(node) node->firstChild
#define SECONDCHILD(node) node->firstChild->nextSibling
#define THIRDCHILD(node) node->firstChild->nextSibling->nextSibling

#endif