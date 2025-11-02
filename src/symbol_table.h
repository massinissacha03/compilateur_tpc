#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H


// les tpes possible 
typedef enum { SYMTYPE_INT, SYMTYPE_CHAR, SYMTYPE_VOID, SYMTYPE_UNKNOWN } SymType;


//variable parametre fonction 

typedef enum { KIND_VAR, KIND_PARAM, KIND_FUNC } Kind;


// representation dans la table
typedef struct Symbol {
    char* name;
    SymType type; 
    Kind kind;
    int is_static; //static ou non 
    int offset; 
    int param_count; //pour fonction 
    SymType* param_types;
} Symbol;

typedef struct SymbolTable {
    Symbol* symbols;
    int count;
    int capacity;
    struct SymbolTable* parent;
} SymbolTable;




// Crée une nouvelle table de symboles vide
SymbolTable* create_table();


// Ajoute un symbole à la table
void add_symbol(SymbolTable* table, char* name, SymType type, Kind kind, int is_static);


Symbol* lookup(SymbolTable* table, char* name);


Symbol* lookup_local(SymbolTable* table, char* name);

void free_table(SymbolTable* table);

// Affiche le contenu de la table
void print_table(SymbolTable* table);

#endif