#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tree.h"
#include "semantic.h"
#include "codegen.h"

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

extern char *optarg;
extern int optind, opterr, optopt;

extern int yylex(void);
extern int yyparse();
extern FILE *yyin;
extern Node *root;



// Affiche l'aide pour l'utilisation du programme
void manuel_utilisation(char *prog) {
    printf("Usage: %s [options] [fichier.tpc]\n", prog);
    printf("Options :\n");
    printf("  -t, --tree      Affiche l'arbre syntaxique.\n");
    printf("  -s, --symbols   Affiche les tables de symboles.\n");
    printf("  -e, --errors    Affiche une liste des erreurs courantes.\n");
    printf("  -h, --help      Affiche ce message d'aide.\n");
}


// Liste les erreurs fréquentes pour aider l'utilisateur
void afficher_erreurs_communes() {
    printf("=== Liste des erreurs courantes ===\n");
    printf("1. Oubli du point-virgule ';' à la fin des instructions.\n");
    printf("2. Parenthèses ou accolades non fermées.\n");
    printf("3. Déclaration incorrecte des fonctions.\n");
    printf("4. Utilisation de mots-clés réservés comme identifiants.\n");
    printf("5. Initialisation de variables statiques.\n");
    printf("6. Instructions incomplètes.\n");
    printf("7. Structures de contrôle mal formées.\n");
    printf("8. Fonction non void sans return.\n");
    printf("9. Fonction sans paramètres sans void.\n");
}

int main(int argc, char *argv[]) {
    int afficher_arbre = 0;
    int afficher_tables = 0;
    int afficher_erreurs = 0;
    char *filePath = NULL;
    int option;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tree") == 0) argv[i] = "-t";
        else if (strcmp(argv[i], "--help") == 0) argv[i] = "-h";
        else if (strcmp(argv[i], "--symbols") == 0) argv[i] = "-s";
        else if (strcmp(argv[i], "--errors") == 0) argv[i] = "-e";
        else if (strcmp(argv[i], "--check") == 0) {
            fprintf(stderr, "L’option --check est désactivée dans cette version.\n");
            exit(3);
        }
    }

    while ((option = getopt(argc, argv, "thse")) != -1) {
        switch (option) {
            case 't': afficher_arbre = 1; break;
            case 'h': manuel_utilisation(argv[0]); return 0;
            case 's': afficher_tables = 1; break;
            case 'e': afficher_erreurs = 1; break;
            default:
                fprintf(stderr, "Option inconnue : -%c\n", optopt);
                manuel_utilisation(argv[0]);
                exit(3);
        }
    }

    if (afficher_erreurs) {
        afficher_erreurs_communes();
        return 0;
    }

    if (optind < argc) {
        filePath = argv[optind];
    }

    if (filePath) {
        FILE *file = fopen(filePath, "r");
        if (!file) {
            fprintf(stderr, "Erreur : Impossible d'ouvrir '%s'.\n", filePath);
            exit(3);
        }
        yyin = file;
    } else {
        yyin = stdin;
    }

    if (yyparse() != 0) {
        fprintf(stderr, "Échec de l'analyse syntaxique\n");
        return 1;
    }

    if (!root) {
        fprintf(stderr, "Erreur : AST vide après analyse\n");
        return 2;
    }

    if (afficher_arbre) {
        printTree(root);
    }

    semantic_analysis(root, afficher_tables);

    // Préparation du nom du fichier de sortie
    char output_filename[256];
    if (filePath) {
        char *dot = strrchr(filePath, '.');
        if (dot && strcmp(dot, ".tpc") == 0) {
            snprintf(output_filename, sizeof(output_filename), "%.*s.asm", (int)(dot - filePath), filePath);
        } else {
            snprintf(output_filename, sizeof(output_filename), "%s.asm", filePath);
        }
    } else {
        strcpy(output_filename, "_anonymous.asm");
    }

    FILE* output = fopen(output_filename, "w");
    if (!output) {
        fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier de sortie '%s'\n", output_filename);
        exit(3);
    }

    generate_code(root, output);
    fclose(output);

    deleteTree(root);
    root = NULL;

    return 0;
}
