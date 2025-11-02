#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"

// tables des symboles globales et courantes pour gerer les portees
static SymbolTable *global_table = NULL;
static SymbolTable *current_table = NULL;

// verifier s'il y a un main ou pas
static int has_main = 0;

//// Entre dans une nouvelle portee
void enter_scope()
{
    SymbolTable *new_table = create_table();
    if (!new_table)
        exit(3);
    new_table->parent = current_table;
    current_table = new_table;
}

// Sort de la portee courante
void exit_scope(int print_tables)
{
    if (!current_table)
        return;
    if (print_tables)
        print_table(current_table);
    SymbolTable *temp = current_table;
    current_table = current_table->parent;
    free_table(temp);
}

// Convertit un type en SymType
SymType get_type_from_string(const char *type_str)
{
    if (!type_str)
        return SYMTYPE_VOID;
    if (strcmp(type_str, "int") == 0)
        return SYMTYPE_INT;
    if (strcmp(type_str, "char") == 0)
        return SYMTYPE_CHAR;
    return SYMTYPE_VOID;
}

// affi une erreur sémantique et arrête le programme
void semantic_error(const char *msg, int lineno, int column)
{
    fprintf(stderr, "[ERREUR SÉMANTIQUE] Ligne %d, colonne %d : %s\n", lineno, column, msg);
    exit(2);
}

// affiche une avertissement
void semantic_warning(const char *msg, int lineno, int column)
{
    fprintf(stderr, "[AVERTISSEMENT] Ligne %d, colonne %d : %s\n", lineno, column, msg);
}

// Vérifie le type d'une expression

SymType check_expression(Node *node, SymbolTable *table)
{
    if (!node || !table)
        return SYMTYPE_UNKNOWN;

    switch (node->label)
    {
    case Num:
        node->expr_type = SYMTYPE_INT;
        return SYMTYPE_INT;

    case Character:
        node->expr_type = SYMTYPE_CHAR;
        return SYMTYPE_CHAR;

    case Ident:
    {
        Symbol *sym = lookup(table, node->value);
        if (!sym)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Variable '%s' non déclarée", node->value);
            semantic_error(msg, node->lineno, node->column);
        }
        node->expr_type = sym->type;
        return sym->type;
    }

    case funCall:
    {
        Node *ident = FIRSTCHILD(node);
        Node *args = SECONDCHILD(node);
        Symbol *func = lookup(table, ident->value);
        if (!func)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Fonction '%s' non définie", ident->value);
            semantic_error(msg, ident->lineno, ident->column);
        }
        if (func->kind != KIND_FUNC)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "'%s' n'est pas une fonction", ident->value);
            semantic_error(msg, ident->lineno, ident->column);
        }
        node->expr_type = func->type;

        // Vérifier les arguments
        int arg_count = 0;
        for (Node *arg = args; arg; arg = arg->nextSibling)
            arg_count++;
        if (arg_count != func->param_count)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Nombre d'arguments incorrect pour '%s' (attendu %d, reçu %d)",
                     ident->value, func->param_count, arg_count);
            semantic_error(msg, node->lineno, node->column);
        }
        int i = 0;
        for (Node *arg = args; arg; arg = arg->nextSibling)
        {
            SymType arg_type = check_expression(arg, table);
            if (arg_type == SYMTYPE_VOID)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Argument %d de '%s' ne peut pas être de type void", i + 1, ident->value);
                semantic_error(msg, arg->lineno, arg->column);
            }
            if (arg_type != func->param_types[i] && arg_type != SYMTYPE_UNKNOWN)
            {
                if (arg_type == SYMTYPE_INT && func->param_types[i] == SYMTYPE_CHAR)
                {
                    semantic_warning("Conversion int vers char", arg->lineno, arg->column);
                }
            }
            i++;
        }

        // fonction void utilisée dans une expression
        if (func->type == SYMTYPE_VOID && node->parent)
        {
            switch (node->parent->label)
            {
            case AddSub:
            case Divstar:
            case Order:
            case Eq:
            case Or:
            case And:
            case Not:
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Utilisation invalide de la fonction void '%s' dans une expression", ident->value);
                semantic_error(msg, node->lineno, node->column);
                break;
            }

            case Assign:

                break;
            default:
                break;
            }
        }
        // les affectations
        if (func->type == SYMTYPE_VOID && node->parent && node->parent->label == Assign)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Appel de fonction void '%s' utilisé dans une affectation", ident->value);
            semantic_error(msg, node->lineno, node->column);
        }
        return func->type;
    }

    case AddSub:
    {
        Node *left_node = FIRSTCHILD(node);
        Node *right_node = SECONDCHILD(node);
        SymType left = check_expression(left_node, table);
        SymType right = check_expression(right_node, table);
        if (left == SYMTYPE_VOID || right == SYMTYPE_VOID)
        {
            semantic_error("Opérandes de type void invalides", node->lineno, node->column);
        }
        node->expr_type = SYMTYPE_INT;
        return SYMTYPE_INT;
    }

    case Divstar:
    {
        Node *left_node = FIRSTCHILD(node);
        Node *right_node = SECONDCHILD(node);
        SymType left = check_expression(left_node, table);
        SymType right = check_expression(right_node, table);
        if (left == SYMTYPE_VOID || right == SYMTYPE_VOID)
        {
            semantic_error("Opérandes de type void invalides", node->lineno, node->column);
        }
        // Détecter division par zéro pour les constantes
        if (right_node->label == Num && right_node->value && atoi(right_node->value) == 0)
        {
            if (strcmp(node->value, "/") == 0 || strcmp(node->value, "%") == 0)
            {
                semantic_warning("Division par zéro", node->lineno, node->column);
            }
        }
        node->expr_type = SYMTYPE_INT;
        return SYMTYPE_INT;
    }

    case Order:
    case Eq:
    {
        Node *left_node = FIRSTCHILD(node);
        Node *right_node = SECONDCHILD(node);
        SymType left = check_expression(left_node, table);
        SymType right = check_expression(right_node, table);
        if (left == SYMTYPE_VOID || right == SYMTYPE_VOID)
        {
            semantic_error("Opérandes de type void invalides", node->lineno, node->column);
        }
        node->expr_type = SYMTYPE_INT;
        return SYMTYPE_INT;
    }

    case Or:
    case And:
        check_expression(FIRSTCHILD(node), table);
        check_expression(SECONDCHILD(node), table);
        node->expr_type = SYMTYPE_INT;
        return SYMTYPE_INT;

    case Not:
    case Unary:
        check_expression(FIRSTCHILD(node), table);
        node->expr_type = SYMTYPE_INT;
        return SYMTYPE_INT;

    case Assign:
    {
        Node *left_node = FIRSTCHILD(node);
        Node *right_node = SECONDCHILD(node);
        SymType left = check_expression(left_node, table);
        SymType right = check_expression(right_node, table);
        // Vérification pour Test 164 : interdire l'assignation d'un nom de fonction
        if (right_node->label == Ident && right_node->value)
        {
            Symbol *sym = lookup(table, right_node->value);
            if (sym && sym->kind == KIND_FUNC)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Affectation invalide : '%s' est une fonction", right_node->value);
                semantic_error(msg, right_node->lineno, right_node->column);
            }
        }
        if (left == SYMTYPE_VOID || right == SYMTYPE_VOID)
        {
            semantic_error("Affectation avec type void", node->lineno, node->column);
        }
        if (left == SYMTYPE_CHAR && right == SYMTYPE_INT)
        {
            semantic_warning("Affectation int vers char", node->lineno, node->column);
        }

        node->expr_type = left;
        return left;
    }

    case If:
    case While:
    {
        Node *condition = FIRSTCHILD(node);
        SymType cond_type = check_expression(condition, table);
        if (cond_type == SYMTYPE_VOID)
        {
            semantic_error("Condition de type void invalide dans une instruction de contrôle", node->lineno, node->column);
        }
        // Vérifier les autres enfants
        for (Node *child = condition->nextSibling; child; child = child->nextSibling)
        {
            check_expression(child, table);
        }
        node->expr_type = SYMTYPE_UNKNOWN;
        return SYMTYPE_UNKNOWN;
    }

    default:
        for (Node *child = node->firstChild; child; child = child->nextSibling)
        {
            if (child)
                check_expression(child, table);
        }
        return SYMTYPE_UNKNOWN;
    }
}

// Analyse sémantique globale
void semantic_analysis(Node *root, int print_tables)
{

    // Initialisation de la table globale
    global_table = create_table();
    if (!global_table)
        exit(3);
    current_table = global_table;

    add_symbol(global_table, "getchar", SYMTYPE_CHAR, KIND_FUNC, 0);
    Symbol *getchar = lookup(global_table, "getchar");
    if (getchar)
        getchar->param_count = 0;
    add_symbol(global_table, "getint", SYMTYPE_INT, KIND_FUNC, 0);
    Symbol *getint = lookup(global_table, "getint");
    if (getint)
        getint->param_count = 0;
    add_symbol(global_table, "putchar", SYMTYPE_VOID, KIND_FUNC, 0);
    Symbol *putchar = lookup(global_table, "putchar");
    if (putchar)
    {
        putchar->param_count = 1;
        putchar->param_types = malloc(sizeof(SymType));
        if (!putchar->param_types)
            exit(3);
        putchar->param_types[0] = SYMTYPE_CHAR;
    }
    add_symbol(global_table, "putint", SYMTYPE_VOID, KIND_FUNC, 0);
    Symbol *putint = lookup(global_table, "putint");
    if (putint)
    {
        putint->param_count = 1;
        putint->param_types = malloc(sizeof(SymType));
        if (!putint->param_types)
            exit(3);
        putint->param_types[0] = SYMTYPE_INT;
    }

    if (root->label == Prog)
    {
        Node *decl_vars = FIRSTCHILD(root);
        Node *decl_foncts = NULL;
        if (decl_vars && decl_vars->label == Type)
        {
            for (Node *var = decl_vars; var && var->label == Type; var = var->nextSibling)
            {
                SymType type = get_type_from_string(var->value);
                Node *first_ident = var->firstChild;
                for (Node *ident = first_ident; ident; ident = ident->nextSibling)
                {
                    if (strcmp(ident->value, "getchar") == 0 || strcmp(ident->value, "getint") == 0 ||
                        strcmp(ident->value, "putchar") == 0 || strcmp(ident->value, "putint") == 0)
                    {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Redéclaration interdite de '%s' (nom de fonction prédéfinie)", ident->value);
                        semantic_error(msg, ident->lineno, ident->column);
                    }
                    Symbol *existing = lookup_local(current_table, ident->value);
                    if (existing)
                    {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Redéclaration de '%s' (déjà déclaré comme %s)",
                                 ident->value, existing->kind == KIND_VAR ? "variable" : "fonction");
                        semantic_error(msg, ident->lineno, ident->column);
                    }
                    for (Node *other = first_ident; other != ident; other = other->nextSibling)
                    {
                        if (other->label == Ident && other->value && strcmp(ident->value, other->value) == 0)
                        {
                            char msg[256];
                            snprintf(msg, sizeof(msg), "Redéclaration de la variable globale '%s' dans la même déclaration", ident->value);
                            semantic_error(msg, ident->lineno, ident->column);
                        }
                    }
                    add_symbol(current_table, ident->value, type, KIND_VAR, 0);
                }
                if (!var->nextSibling)
                    break;
            }
            decl_foncts = decl_vars ? decl_vars->nextSibling : NULL;
        }
        else
        {
            decl_foncts = decl_vars;
        }

        if (decl_foncts)
        {
            for (Node *func = decl_foncts; func; func = func->nextSibling)
            {
                if (func->label != fonction)
                    continue;
                Node *type_node = FIRSTCHILD(func);
                Node *ident = SECONDCHILD(func);
                Node *params = type_node->nextSibling->nextSibling;
                Node *corps = params->nextSibling;

                SymType type = type_node->label == Void ? SYMTYPE_VOID : get_type_from_string(type_node->value);
                Symbol *existing = lookup_local(current_table, ident->value);
                if (existing)
                {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Redéclaration de '%s' (déjà déclaré comme %s)", ident->value,
                             existing->kind == KIND_VAR ? "variable" : "fonction");
                    semantic_error(msg, ident->lineno, ident->column);
                }
                if (strcmp(ident->value, "main") == 0)
                {
                    if (type != SYMTYPE_INT)
                    {
                        semantic_error("La fonction 'main' doit retourner un int", ident->lineno, ident->column);
                    }
                    has_main = 1;
                }

                add_symbol(current_table, ident->value, type, KIND_FUNC, 0);
                Symbol *func_sym = lookup(current_table, ident->value);
                if (!func_sym)
                {
                    semantic_error("Échec de l'ajout de la fonction", ident->lineno, ident->column);
                }

                enter_scope();
                if (params->firstChild && params->firstChild->label != Void)
                {
                    for (Node *param = params->firstChild; param; param = param->nextSibling)
                    {
                        SymType param_type = get_type_from_string(param->value);
                        Node *param_ident = param->firstChild;
                        if (lookup_local(current_table, param_ident->value))
                        {
                            char msg[256];
                            snprintf(msg, sizeof(msg), "Redéclaration du paramètre '%s'", param_ident->value);
                            semantic_error(msg, param_ident->lineno, param_ident->column);
                        }
                        add_symbol(current_table, param_ident->value, param_type, KIND_PARAM, 0);
                        func_sym->param_count++;
                        func_sym->param_types = realloc(func_sym->param_types, func_sym->param_count * sizeof(SymType));
                        if (!func_sym->param_types)
                            exit(3);
                        func_sym->param_types[func_sym->param_count - 1] = param_type;
                    }
                }

                for (Node *decl = corps->firstChild; decl && decl->label == Type; decl = decl->nextSibling)
                {
                    SymType decl_type = get_type_from_string(decl->value);
                    int is_static = (decl->firstChild && decl->firstChild->label == Static);
                    Node *first_ident = is_static ? decl->firstChild->nextSibling : decl->firstChild;
                    for (Node *ident = first_ident; ident; ident = ident->nextSibling)
                    {
                        if (lookup_local(current_table, ident->value))
                        {
                            char msg[256];
                            snprintf(msg, sizeof(msg), "Redéclaration de la variable locale '%s'", ident->value);
                            semantic_error(msg, ident->lineno, ident->column);
                        }
                        for (Node *other = first_ident; other != ident; other = other->nextSibling)
                        {
                            if (other->label == Ident && other->value && strcmp(ident->value, other->value) == 0)
                            {
                                char msg[256];
                                snprintf(msg, sizeof(msg), "Redéclaration de la variable locale '%s' dans la même déclaration", ident->value);
                                semantic_error(msg, ident->lineno, ident->column);
                            }
                        }
                        add_symbol(current_table, ident->value, decl_type, KIND_VAR, is_static);
                    }
                }
                for (Node *instr = corps->firstChild; instr; instr = instr->nextSibling)
                {
                    if (instr->label == Return)
                    {
                        if (!instr->firstChild)
                        {
                            if (type != SYMTYPE_VOID)
                            {
                                semantic_error("Instruction return vide interdite dans une fonction non void",
                                               instr->lineno, instr->column);
                            }
                        }
                        else
                        {
                            SymType return_type = check_expression(instr->firstChild, current_table);
                            if (type == SYMTYPE_VOID)
                            {
                                semantic_error("Instruction return avec une valeur interdite dans une fonction void",
                                               instr->lineno, instr->column);
                            }
                            else if (return_type != type)
                            {
                                if ((type == SYMTYPE_INT && return_type == SYMTYPE_CHAR) ||
                                    (type == SYMTYPE_CHAR && return_type == SYMTYPE_INT))
                                {
                                    char msg[256];
                                    snprintf(msg, sizeof(msg),
                                             "Avertissement : conversion implicite de '%s' vers '%s' dans return",
                                             return_type == SYMTYPE_INT ? "int" : "char",
                                             type == SYMTYPE_INT ? "int" : "char");
                                    semantic_warning(msg, instr->lineno, instr->column);
                                }
                                else
                                {
                                    char msg[256];
                                    snprintf(msg, sizeof(msg),
                                             "Type de retour incorrect pour '%s' (attendu %s, obtenu %s)",
                                             ident->value,
                                             type == SYMTYPE_INT ? "int" : type == SYMTYPE_CHAR ? "char"
                                                                                                : "void",
                                             return_type == SYMTYPE_INT ? "int" : return_type == SYMTYPE_CHAR ? "char"
                                                                                                              : "void");
                                    semantic_error(msg, instr->lineno, instr->column);
                                }
                            }
                        }
                    }
                    else
                    {
                        check_expression(instr, current_table);
                    }
                }

                exit_scope(print_tables);
                if (!func->nextSibling)
                    break;
            }
        }
    }

    if (!has_main)
    {
        semantic_error("Fonction 'main' absente", root->lineno, root->column);
    }

    if (print_tables)
        print_table(global_table);
    free_table(global_table);
    global_table = NULL;
    current_table = NULL;
}