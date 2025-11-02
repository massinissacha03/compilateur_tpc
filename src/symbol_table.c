#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol_table.h"



// creer une nouvelle table de symboles
SymbolTable* create_table() {
    SymbolTable* table = malloc(sizeof(SymbolTable));
    if (!table) { fprintf(stderr, "Mémoire insuffisante\n"); exit(3); }
    table->symbols = malloc(16 * sizeof(Symbol));
    table->count = 0;
    table->capacity = 16;
    table->parent = NULL;
    return table;
}


//Ajoute un symbole dans la table
void add_symbol(SymbolTable* table, char* name, SymType type, Kind kind, int is_static) {
    if (table->count >= table->capacity) {
        table->capacity *= 2;
        table->symbols = realloc(table->symbols, table->capacity * sizeof(Symbol));
        if (!table->symbols) { fprintf(stderr, "Mémoire insuffisante\n"); exit(3); }
    }
    Symbol* sym = &table->symbols[table->count++];
    sym->name = strdup(name);
    sym->type = type;
    sym->kind = kind;
    sym->is_static = is_static;
    sym->offset = 0;
    sym->param_count = 0;
    sym->param_types = NULL;
}

// Recherche d’un symbole
Symbol* lookup(SymbolTable* table, char* name) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i].name, name) == 0) {
            return &table->symbols[i];
        }
    }
    if (table->parent) return lookup(table->parent, name);
    return NULL;

}


// Recherche d’un symbole seulement dans la table courante
Symbol* lookup_local(SymbolTable* table, char* name) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->symbols[i].name, name) == 0) {
            return &table->symbols[i];
        }
    }
    return NULL;
}

void free_table(SymbolTable* table) {
    for (int i = 0; i < table->count; i++) {
        free(table->symbols[i].name);
        free(table->symbols[i].param_types);
    }
    free(table->symbols);
    free(table);
}

void print_table(SymbolTable* table) {
    printf("Tableau des symboles:\n");
    for (int i = 0; i < table->count; i++) {
        Symbol* sym = &table->symbols[i];
        printf("  %s: %s, %s, %s, offset=%d",
               sym->name,
               sym->type == SYMTYPE_INT ? "int" : sym->type == SYMTYPE_CHAR ? "char" : "void",
               sym->kind == KIND_VAR ? "variable" : sym->kind == KIND_PARAM ? "paramètre" : "fonction",
               sym->is_static ? "static" : "non-static",
               sym->offset);
        if (sym->kind == KIND_FUNC && sym->param_count > 0) {
            printf(", params=[");
            for (int j = 0; j < sym->param_count; j++) {
                printf("%s%s", sym->param_types[j] == SYMTYPE_INT ? "int" : "char",
                       j < sym->param_count - 1 ? ", " : "");
            }
            printf("]");
        }
        printf("\n");
    }
}

