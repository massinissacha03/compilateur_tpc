#include <stdio.h>
#include <string.h>
#include "codegen.h"
#include "tree.h"
#include <stdlib.h>

typedef struct
{
    char name[64];
    int offset;
    char type[8];
} LocalVar;

LocalVar locals[128];
int local_count = 0;
int current_offset = -8;
int label_counter = 0;

static void generate_exit_with_value(FILE *output, const char *val);

static int new_label(void);
static int get_local_offset(const char *name);
static void generate_expression(Node *expr, FILE *output);
static void generate_data_section(FILE *output, Node *body);
static void generate_function_exit(Node *function, FILE *output);
static void generate_prog(Node *node, FILE *output);
static void generate_num(Node *expr, FILE *output);
static void generate_ident(Node *expr, FILE *output);
static void generate_addsub(Node *expr, FILE *output);
static void generate_divstar(Node *expr, FILE *output);
static void generate_and(Node *expr, FILE *output);
static void generate_or(Node *expr, FILE *output);
static void generate_eq(Node *expr, FILE *output);
static void generate_order(Node *expr, FILE *output);
static void generate_funcall(Node *expr, FILE *output);
static void generate_character(Node *expr, FILE *output);
static void generate_assign(Node *node, FILE *output);
static void generate_if(Node *node, FILE *output);
static void generate_ifelse(Node *node, FILE *output);
static void generate_while(Node *node, FILE *output);
static void generate_function(Node *node, FILE *output);
static void generate_not(Node *expr, FILE *output);

//  appel pour quitter avec une valeur
static void generate_exit_with_value(FILE *output, const char *val)
{
    fprintf(output, "    mov rax, 60\n");
    fprintf(output, "    mov rdi, %s\n", val);
    fprintf(output, "    syscall\n");
}

// Crée une nouvelle etiquette unique
static int new_label()
{
    return label_counter++;
}

// recupere le décalage d’une variable locale
static int get_local_offset(const char *name)
{
    for (int i = 0; i < local_count; ++i)
    {
        if (strcmp(locals[i].name, name) == 0)
            return locals[i].offset;
    }
    return 0;
}

// code assembleur pour un num
static void generate_num(Node *expr, FILE *output)
{
    if (!expr || !expr->value)
        return;
    fprintf(output, "    mov rax, %s\n", expr->value);
}

//  le code pour un identifiant
static void generate_ident(Node *expr, FILE *output)
{
    if (!expr || !expr->value)
        return;

    int offset = get_local_offset(expr->value);
    const char *var_type = NULL;

    for (int i = 0; i < local_count; ++i)
    {
        if (strcmp(locals[i].name, expr->value) == 0)
        {
            var_type = locals[i].type;
            break;
        }
    }

    if (offset != 0)
    {
        if (var_type && strcmp(var_type, "char") == 0)
            fprintf(output, "    movzx rax, byte [rbp%+d]\n", offset);
        else
            fprintf(output, "    mov rax, [rbp%+d]\n", offset);
    }
    else
    {
        // Variable globale, suppose déclarée dans .data
        fprintf(output, "    mov rax, [rel %s]\n", expr->value);
    }
}

//  le code pour les op + et -
static void generate_addsub(Node *expr, FILE *output)
{
    Node *left = FIRSTCHILD(expr);
    Node *right = SECONDCHILD(expr);
    generate_expression(left, output);
    fprintf(output, "    push rax\n"); // Sauver premier opérande
    generate_expression(right, output);
    fprintf(output, "    mov ebx, eax\n"); // Deuxième opérande
    fprintf(output, "    pop rax\n");      // Restaurer premier opérande
    fprintf(output, "    mov eax, eax\n"); // 32 bits
    if (strcmp(expr->value, "+") == 0)
    {
        fprintf(output, "    add eax, ebx\n");
        fprintf(output, "    movsxd rax, eax\n");
    }
    else if (strcmp(expr->value, "-") == 0)
    {
        fprintf(output, "    sub eax, ebx\n");
        fprintf(output, "    movsxd rax, eax\n");
    }
}

static void generate_divstar(Node *expr, FILE *output)
{
    if (!expr)
        return;
    Node *left = FIRSTCHILD(expr);
    Node *right = SECONDCHILD(expr);

    generate_expression(left, output);
    fprintf(output, "    push rax\n");
    generate_expression(right, output);
    fprintf(output, "    pop rcx\n");

    if (strcmp(expr->value, "*") == 0)
    {
        fprintf(output, "    mov ebx, eax\n");
        fprintf(output, "    mov eax, ecx\n");
        fprintf(output, "    imul eax, ebx\n"); // Multiplication 32 bits
        fprintf(output, "    movsxd rax, eax\n");
    }
    else if (strcmp(expr->value, "/") == 0 || strcmp(expr->value, "%") == 0)
    {
        int label = new_label();
        fprintf(output, "    cmp eax, 0\n"); // Vérifier division par zéro
        fprintf(output, "    je .Ldivzero%d\n", label);
        fprintf(output, "    mov ebx, eax\n");
        fprintf(output, "    mov eax, ecx\n");
        fprintf(output, "    cdq\n");
        fprintf(output, "    idiv ebx\n");
        if (strcmp(expr->value, "/") == 0)
            fprintf(output, "    movsxd rax, eax\n"); // Résultat
        else
            fprintf(output, "    mov eax, edx\n    movsxd rax, eax\n"); // Reste
        fprintf(output, "    jmp .Lenddiv%d\n", label);
        fprintf(output, ".Ldivzero%d:\n", label);
        fprintf(output, "    mov rax, 60\n");
        fprintf(output, "    mov rdi, 5\n");
        fprintf(output, "    syscall\n");
        fprintf(output, ".Lenddiv%d:\n", label);
    }
}

static void generate_and(Node *expr, FILE *output)
{
    if (!expr)
        return;
    Node *left = FIRSTCHILD(expr);
    Node *right = SECONDCHILD(expr);

    int label = new_label();
    generate_expression(left, output);
    fprintf(output, "    test rax, rax\n");
    fprintf(output, "    jz .Lfalse%d\n", label);

    generate_expression(right, output);
    fprintf(output, "    test rax, rax\n");
    fprintf(output, "    jz .Lfalse%d\n", label);

    fprintf(output, "    mov rax, 1\n");
    fprintf(output, "    jmp .Lend%d\n", label);
    fprintf(output, ".Lfalse%d:\n", label);
    fprintf(output, "    mov rax, 0\n");
    fprintf(output, ".Lend%d:\n", label);
}

static void generate_or(Node *expr, FILE *output)
{
    if (!expr)
        return;
    Node *left = FIRSTCHILD(expr);
    Node *right = SECONDCHILD(expr);

    int label = new_label();
    generate_expression(left, output);
    fprintf(output, "    test rax, rax\n");
    fprintf(output, "    jnz .Ltrue%d\n", label);

    generate_expression(right, output);
    fprintf(output, "    test rax, rax\n");
    fprintf(output, "    jnz .Ltrue%d\n", label);

    fprintf(output, "    mov rax, 0\n");
    fprintf(output, "    jmp .Lend%d\n", label);
    fprintf(output, ".Ltrue%d:\n", label);
    fprintf(output, "    mov rax, 1\n");
    fprintf(output, ".Lend%d:\n", label);
}

static void generate_eq(Node *expr, FILE *output)
{
    if (!expr)
        return;
    Node *left = FIRSTCHILD(expr);
    Node *right = SECONDCHILD(expr);

    generate_expression(left, output);  // résultat dans rax
    fprintf(output, "    push rax\n");  // sauvegarde du left
    generate_expression(right, output); // résultat dans rax
    fprintf(output, "    pop rcx\n");   // left rax
    fprintf(output, "    cmp rcx, rax\n");

    if (strcmp(expr->value, "==") == 0)
        fprintf(output, "    sete al\n");
    else
        fprintf(output, "    setne al\n");

    fprintf(output, "    movzx rax, al\n"); // res final dans rax
}

static void generate_order(Node *expr, FILE *output)
{
    if (!expr)
        return;
    Node *left = FIRSTCHILD(expr);
    Node *right = SECONDCHILD(expr);

    generate_expression(left, output);
    fprintf(output, "    push rax\n");
    generate_expression(right, output);
    fprintf(output, "    pop rcx\n");
    fprintf(output, "    cmp rcx, rax\n");

    if (strcmp(expr->value, "<") == 0)
        fprintf(output, "    setl al\n");
    else if (strcmp(expr->value, ">") == 0)
        fprintf(output, "    setg al\n");
    else if (strcmp(expr->value, "<=") == 0)
        fprintf(output, "    setle al\n");
    else if (strcmp(expr->value, ">=") == 0)
        fprintf(output, "    setge al\n");

    fprintf(output, "    movzx rax, al\n");
}

static void generate_not(Node *expr, FILE *output)
{
    if (!expr)
        return;
    Node *child = FIRSTCHILD(expr);

    generate_expression(child, output);
    fprintf(output, "    test rax, rax\n");
    fprintf(output, "    setz al\n");
    fprintf(output, "    movzx rax, al\n");
}

static void generate_character(Node *expr, FILE *output)
{
    if (!expr || !expr->value)
        return;

    // Match exacts pour échappements
    if (strcmp(expr->value, "'\\n'") == 0)
        fprintf(output, "    mov rax, 10\n");
    else if (strcmp(expr->value, "'\\t'") == 0)
        fprintf(output, "    mov rax, 9\n");
    else if (strcmp(expr->value, "'\\\\'") == 0)
        fprintf(output, "    mov rax, 92\n");
    else if (strcmp(expr->value, "'\\0'") == 0)
        fprintf(output, "    mov rax, 0\n");
    else if (strlen(expr->value) >= 3 && expr->value[0] == '\'' && expr->value[2] == '\'')
        fprintf(output, "    mov rax, %d\n", expr->value[1]); // ex 'A'
    else
        fprintf(output, "    mov rax, 0\n"); // fallback
}

void generate_expression(Node *expr, FILE *output)
{
    if (!expr || !output)
        return;

    switch (expr->label)
    {
    case Num:
        generate_num(expr, output);
        break;
    case Ident:
        generate_ident(expr, output);
        break;
    case AddSub:
        generate_addsub(expr, output);
        break;
    case Divstar:
        generate_divstar(expr, output);
        break;
    case And:
        generate_and(expr, output);
        break;
    case Or:
        generate_or(expr, output);
        break;
    case Eq:
        generate_eq(expr, output);
        break;
    case Order:
        generate_order(expr, output);
        break;
    case Not:
        generate_not(expr, output);
        break;
    case funCall:
        generate_funcall(expr, output);
        break;
    case Character:

        generate_character(expr, output);
        break;
    case Unary:
        if (expr->value && strcmp(expr->value, "-") == 0)
        {
            generate_expression(FIRSTCHILD(expr), output);
            fprintf(output, "    neg rax\n");
        }
    default:
        break;
    }
}
void generate_data_section(FILE *output, Node *body)
{
    if (!output)
        return;
    fprintf(output, "section .data\n");

    if (body && body->label == Corps)
    {
        for (Node *instr = body->firstChild; instr; instr = instr->nextSibling)
        {
            if (instr->label == Type)
            {
                int is_static = 0;
                for (Node *child = instr->firstChild; child; child = child->nextSibling)
                {
                    if (child->label == Static)
                    {
                        is_static = 1;
                        break;
                    }
                }
                if (is_static)
                {
                    for (Node *ident = instr->firstChild; ident; ident = ident->nextSibling)
                    {
                        if (ident->label == Ident && ident->value)
                        {

                            fprintf(output, "    %s dq 0\n", ident->value);
                        }
                    }
                }
            }
        }
    }

    fprintf(output, "\n");
}
void generate_function_exit(Node *function, FILE *output)
{
    if (!function || !output)
        return;

    Node *type = function->firstChild;
    Node *ident = type ? type->nextSibling : NULL;
    Node *params = ident ? ident->nextSibling : NULL;
    Node *body = params ? params->nextSibling : NULL;

    if (!ident || !ident->value)
        return;

    local_count = 0;
    current_offset = -8;
    label_counter = 0;

    if (strcmp(ident->value, "main") == 0)
        fprintf(output, "    global %s\n", ident->value);

    fprintf(output, "%s:\n", ident->value);
    fprintf(output, "    push rbp\n");
    fprintf(output, "    mov rbp, rsp\n");

    int param_count = 0;
    if (params && params->label == Parametres && params->firstChild && params->firstChild->label != Void)
    {
        const char *paramRegs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        for (Node *param = params->firstChild; param; param = param->nextSibling)
        {
            if (param->label == Type && param->firstChild)
            {
                for (Node *ident = param->firstChild; ident && ident->value; ident = ident->nextSibling)
                {
                    if (ident->label == Ident)
                    {
                        strcpy(locals[local_count].name, ident->value);
                        strcpy(locals[local_count].type, param->value ? param->value : "int");
                        locals[local_count].offset = current_offset;
                        current_offset -= 8;
                        if (param_count < 6)
                        {
                            fprintf(output, "    mov [rbp%+d], %s\n", locals[local_count].offset, paramRegs[param_count]);
                        }
                        else
                        {
                            int stack_offset = 16 + (param_count - 6) * 8;
                            fprintf(output, "    mov rax, [rbp+%d]\n", stack_offset);
                            fprintf(output, "    mov [rbp%+d], rax\n", locals[local_count].offset);
                        }
                        param_count++;
                        local_count++;
                    }
                }
            }
        }
    }

    int local_size = 0;
    if (body && body->label == Corps && body->firstChild)
    {
        for (Node *instr = body->firstChild; instr; instr = instr->nextSibling)
        {
            if (instr->label == Type && instr->firstChild)
            {
                for (Node *ident = instr->firstChild; ident && ident->value; ident = ident->nextSibling)
                {
                    if (ident->label == Ident)
                    {
                        strcpy(locals[local_count].name, ident->value);
                        strcpy(locals[local_count].type, instr->value ? instr->value : "int");
                        locals[local_count].offset = current_offset;
                        current_offset -= 8;
                        local_size += 8;
                        local_count++;
                    }
                }
            }
        }
    }

    if (local_size > 0)
    {
        fprintf(output, "    sub rsp, %d\n", local_size);
    }
    if (body && body->label == Corps)
    {
        for (Node *instr = body->firstChild; instr; instr = instr->nextSibling)
        {
            if (instr)
                generate_code(instr, output);
        }
    }

    // Ajouter un retour par défaut pour les fonctions int
    if (type && type->value && strcmp(type->value, "int") == 0)
    {
        fprintf(output, "    mov rax, 0\n"); // Retour par défaut 0
    }
    fprintf(output, "    mov rsp, rbp\n");
    fprintf(output, "    pop rbp\n");
    fprintf(output, "    ret\n");
}
static int uses_function(Node *node, const char *func_name)
{
    if (!node)
        return 0;
    if (node->label == funCall && node->firstChild && node->firstChild->value &&
        strcmp(node->firstChild->value, func_name) == 0)
    {
        return 1;
    }
    for (Node *child = node->firstChild; child; child = child->nextSibling)
    {
        if (uses_function(child, func_name))
            return 1;
    }
    return 0;
}

void generate_prog(Node *node, FILE *output)
{
    if (!node || !output)
        return;

    // Émettre section .data une seule fois
    fprintf(output, "section .data\n");

    // Variables globales non statiques
    if (node->label == Prog && node->firstChild)
    {
        for (Node *child = node->firstChild; child; child = child->nextSibling)
        {
            if (child && child->label == Type && child->firstChild)
            {
                int is_static = 0;
                for (Node *mod = child->firstChild; mod; mod = mod->nextSibling)
                {
                    if (mod->label == Static)
                    {
                        is_static = 1;
                        break;
                    }
                }
                if (!is_static)
                {
                    for (Node *ident = child->firstChild; ident && ident->value; ident = ident->nextSibling)
                    {
                        if (ident->label == Ident)
                        {
                            fprintf(output, "    %s dq 0\n", ident->value);
                        }
                    }
                }
            }
        }
    }

    // Variables statiques locales
    if (node->label == Prog && node->firstChild)
    {
        for (Node *child = node->firstChild; child; child = child->nextSibling)
        {
            if (child && child->label == fonction)
            {
                Node *body = child->firstChild->nextSibling->nextSibling->nextSibling; // Corps
                generate_data_section(output, body);
            }
        }
    }

    fprintf(output, "\nsection .text\n");

    // Générer putchar seulement si utilisé
    int uses_putchar = uses_function(node, "putchar") || uses_function(node, "putint");
    int uses_getchar = uses_function(node, "getchar");
    int uses_putint = uses_function(node, "putint");
    int uses_getint = uses_function(node, "getint");

    if (uses_putchar)
    {
        fprintf(output, "putchar:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rbp, rsp\n");
        fprintf(output, "    sub rsp, 1\n");
        fprintf(output, "    mov byte [rsp], dil\n");
        fprintf(output, "    mov rax, 1\n");
        fprintf(output, "    mov rdi, 1\n");
        fprintf(output, "    lea rsi, [rsp]\n");
        fprintf(output, "    mov rdx, 1\n");
        fprintf(output, "    syscall\n");
        fprintf(output, "    mov rsp, rbp\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n");
    }

    if (uses_getchar)
    {
        fprintf(output, "getchar:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rbp, rsp\n");
        fprintf(output, "    sub rsp, 2\n");

        // Lire premier caractère
        fprintf(output, "    mov rax, 0\n");
        fprintf(output, "    mov rdi, 0\n");
        fprintf(output, "    lea rsi, [rsp+1]\n"); // on lit à rsp+1
        fprintf(output, "    mov rdx, 1\n");
        fprintf(output, "    syscall\n");
        fprintf(output, "    cmp rax, 1\n");
        fprintf(output, "    jne .Lgetchar_exit3\n");

        // stocker le caractère lu dans bl
        fprintf(output, "    movzx ebx, byte [rsp+1]\n");

        // Si c’est un saut de ligne tout de suite -> juste entrée
        fprintf(output, "    cmp bl, 10\n");
        fprintf(output, "    je .Lgetchar_empty\n");

        // Lire 2e caractère  '\n'
        fprintf(output, "    mov rax, 0\n");
        fprintf(output, "    mov rdi, 0\n");
        fprintf(output, "    lea rsi, [rsp]\n");
        fprintf(output, "    mov rdx, 1\n");
        fprintf(output, "    syscall\n");
        fprintf(output, "    cmp rax, 1\n");
        fprintf(output, "    jne .Lgetchar_exit3\n");

        fprintf(output, "    cmp byte [rsp], 10\n");
        fprintf(output, "    jne .Lgetchar_exit3\n");

        // OK → retourner le caractère lu
        fprintf(output, "    mov eax, ebx\n");
        fprintf(output, "    jmp .Lgetchar_end\n");

        // Entrée vide > retourner 0
        fprintf(output, ".Lgetchar_empty:\n");
        fprintf(output, "    mov eax, 0\n");
        fprintf(output, "    jmp .Lgetchar_end\n");

        // Trop de caractères > exit(3)
        fprintf(output, ".Lgetchar_exit3:\n");
        fprintf(output, "    mov rdi, 3\n");
        fprintf(output, "    mov rax, 60\n");
        fprintf(output, "    syscall\n");

        // Fin
        fprintf(output, ".Lgetchar_end:\n");
        fprintf(output, "    mov rsp, rbp\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n");
    }

    if (uses_putint)
    {
        fprintf(output, "putint:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rbp, rsp\n");
        fprintf(output, "    sub rsp, 32\n");

        fprintf(output, "    mov rax, rdi\n"); // valeur à afficher
        fprintf(output, "    mov rbx, 0\n");   // flag négatif = 0
        fprintf(output, "    cmp rax, 0\n");
        fprintf(output, "    jge .Lputint_abs\n");
        fprintf(output, "    mov rbx, 1\n"); // flag négatif
        fprintf(output, "    neg rax\n");    // rax = -rax

        fprintf(output, ".Lputint_abs:\n");
        fprintf(output, "    mov rcx, 10\n"); // base 10
        fprintf(output, "    mov rsi, rsp\n");
        fprintf(output, "    add rsi, 31\n");
        fprintf(output, "    mov byte [rsi], 0\n");
        fprintf(output, "    dec rsi\n");

        // Cas spécial 0
        fprintf(output, "    test rax, rax\n");
        fprintf(output, "    jnz .Lputint_loop\n");
        fprintf(output, "    mov byte [rsi], '0'\n");
        fprintf(output, "    dec rsi\n");
        fprintf(output, "    jmp .Lputint_sign\n");

        // Conversion
        fprintf(output, ".Lputint_loop:\n");
        fprintf(output, "    xor rdx, rdx\n");
        fprintf(output, "    div rcx\n");
        fprintf(output, "    add dl, '0'\n");
        fprintf(output, "    mov [rsi], dl\n");
        fprintf(output, "    dec rsi\n");
        fprintf(output, "    test rax, rax\n");
        fprintf(output, "    jnz .Lputint_loop\n");

        // Ajout du signe - si nécessaire
        fprintf(output, ".Lputint_sign:\n");
        fprintf(output, "    cmp rbx, 1\n");
        fprintf(output, "    jne .Lputint_print\n");
        fprintf(output, "    mov byte [rsi], '-'\n");
        fprintf(output, "    dec rsi\n");

        // Impression
        fprintf(output, ".Lputint_print:\n");
        fprintf(output, "    inc rsi\n");
        fprintf(output, "    mov rdx, rsp\n");
        fprintf(output, "    add rdx, 32\n");
        fprintf(output, "    sub rdx, rsi\n");
        fprintf(output, "    mov rax, 1\n");
        fprintf(output, "    mov rdi, 1\n");
        fprintf(output, "    mov rsi, rsi\n");
        fprintf(output, "    syscall\n");

        // Fin
        fprintf(output, "    mov rsp, rbp\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n");
    }

    if (uses_getint)
    {
        fprintf(output, "getint:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rbp, rsp\n");
        fprintf(output, "    sub rsp, 32\n");  // buffer temporaire
        fprintf(output, "    mov rax, 0\n");   // syscall read
        fprintf(output, "    mov rdi, 0\n");   // stdin
        fprintf(output, "    mov rsi, rsp\n"); // buffer
        fprintf(output, "    mov rdx, 32\n");
        fprintf(output, "    syscall\n");      // lire au max 32 octets
        fprintf(output, "    mov rcx, rax\n"); // rcx = nb d’octets lus
        fprintf(output, "    cmp rcx, 0\n");   // rien lu ?
        fprintf(output, "    je .Lgetint_error\n");

        fprintf(output, "    xor rax, rax\n"); // rax = résultat
        fprintf(output, "    mov rbx, 1\n");   // signe = +1
        fprintf(output, "    mov rsi, rsp\n"); // pointeur courant

        // Lire 1er caractère
        fprintf(output, "    movzx rdx, byte [rsi]\n");
        fprintf(output, "    cmp dl, '+'\n");
        fprintf(output, "    je .Lgetint_skip_sign\n");
        fprintf(output, "    cmp dl, '-'\n");
        fprintf(output, "    jne .Lgetint_check_digit\n");
        fprintf(output, "    mov rbx, -1\n"); // signe = -1
        fprintf(output, ".Lgetint_skip_sign:\n");
        fprintf(output, "    inc rsi\n"); // skip signe
        fprintf(output, "    dec rcx\n"); // chars restants
        fprintf(output, "    jmp .Lgetint_parse\n");

        fprintf(output, ".Lgetint_check_digit:\n");
        fprintf(output, "    cmp dl, '0'\n");
        fprintf(output, "    jl .Lgetint_error\n");
        fprintf(output, "    cmp dl, '9'\n");
        fprintf(output, "    jg .Lgetint_error\n");
        fprintf(output, "    jmp .Lgetint_parse\n");

        fprintf(output, ".Lgetint_parse:\n");
        fprintf(output, "    test rcx, rcx\n");
        fprintf(output, "    jz .Lgetint_done\n");

        fprintf(output, "    movzx rdx, byte [rsi]\n");
        fprintf(output, "    cmp dl, 10\n"); // '\n' ?
        fprintf(output, "    je .Lgetint_done\n");
        fprintf(output, "    cmp dl, '0'\n");
        fprintf(output, "    jl .Lgetint_error\n");
        fprintf(output, "    cmp dl, '9'\n");
        fprintf(output, "    jg .Lgetint_error\n");
        fprintf(output, "    sub dl, '0'\n"); // convertir
        fprintf(output, "    imul rax, 10\n");
        fprintf(output, "    add rax, rdx\n");
        fprintf(output, "    inc rsi\n");
        fprintf(output, "    dec rcx\n");
        fprintf(output, "    jmp .Lgetint_parse\n");

        fprintf(output, ".Lgetint_done:\n");
        fprintf(output, "    imul rax, rbx\n"); // appliquer le signe
        fprintf(output, "    mov rsp, rbp\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n");

        fprintf(output, ".Lgetint_error:\n");
        fprintf(output, "    mov rax, 60\n");
        fprintf(output, "    mov rdi, 5\n"); // code d’erreur 5
        fprintf(output, "    syscall\n");
    }
    // _start
    fprintf(output, "    global _start\n\n");
    fprintf(output, "_start:\n");
    fprintf(output, "    push rbp\n");
    fprintf(output, "    mov rbp, rsp\n");
    fprintf(output, "    call main\n");
    fprintf(output, "    mov rdi, rax\n");
    fprintf(output, "    mov rax, 60\n");
    fprintf(output, "    syscall\n");

    // Générer fonctions
    if (node->firstChild)
    {
        for (Node *function = node->firstChild; function; function = function->nextSibling)
        {
            if (function && function->label == fonction)
            {
                generate_function_exit(function, output);
            }
            else
            {
            }
        }
    }

    fprintf(output, "section .note.GNU-stack noalloc noexec nowrite progbits\n");
}
static void generate_funcall(Node *expr, FILE *output)
{
    if (!expr || !output)
        return;

    Node *ident = FIRSTCHILD(expr);
    Node *arg = ident ? ident->nextSibling : NULL;

    if (!ident || !ident->value)
        return;

    // putchar
    if (strcmp(ident->value, "putchar") == 0)
    {
        if (arg)
        {
            generate_expression(arg, output);  // Résultat dans rax
            fprintf(output, "    push rax\n"); // Sauvegarder rax
            fprintf(output, "    movzx rdi, al\n");
        }
        fprintf(output, "    call putchar\n");
        if (arg)
            fprintf(output, "    pop rax\n"); // Restaurer rax
    }
    // putint
    else if (strcmp(ident->value, "putint") == 0)
    {
        if (arg)
        {
            generate_expression(arg, output);  // Résultat dans rax
            fprintf(output, "    push rax\n"); // Sauvegarder rax
            fprintf(output, "    mov rdi, rax\n");
        }
        fprintf(output, "    call putint\n");
        if (arg)
            fprintf(output, "    pop rax\n"); // Restaurer rax
    }
    // getchar
    else if (strcmp(ident->value, "getchar") == 0)
    {
        fprintf(output, "    call getchar\n");
        fprintf(output, "    movsx rax, al\n");
    }
    // getint
    else if (strcmp(ident->value, "getint") == 0)
    {
        fprintf(output, "    call getint\n");
    }

    // Appel fonction utilisateur
    else
    {
        int argCount = 0;
        for (Node *a = arg; a; a = a->nextSibling)
            argCount++;

        const char *argRegs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        Node *args[16];
        int i = 0;
        for (Node *a = arg; a; a = a->nextSibling)
            args[i++] = a;

        for (i = argCount - 1; i >= 6; i--)
        {
            generate_expression(args[i], output);
            fprintf(output, "    push rax\n");
        }

        for (i = 0; i < argCount && i < 6; i++)
        {
            generate_expression(args[i], output);
            fprintf(output, "    mov %s, rax\n", argRegs[i]);
        }

        fprintf(output, "    xor rax, rax\n"); // Pour variadics (convention SysV)
        fprintf(output, "    call %s\n", ident->value);

        if (argCount > 6)
            fprintf(output, "    add rsp, %d\n", 8 * (argCount - 6));
    }
}
static void generate_assign(Node *node, FILE *output)
{
    if (!node || !output)
        return;
    Node *left = FIRSTCHILD(node);
    Node *right = SECONDCHILD(node);
    if (!left || !right)
        return;

    generate_expression(right, output);

    int offset = get_local_offset(left->value);
    const char *var_type = NULL;

    for (int i = 0; i < local_count; ++i)
    {
        if (strcmp(locals[i].name, left->value) == 0)
        {
            var_type = locals[i].type;
            break;
        }
    }

    if (offset != 0)
    {
        if (var_type && strcmp(var_type, "char") == 0)
            fprintf(output, "    mov [rbp%+d], al\n", offset);
        else
            fprintf(output, "    mov [rbp%+d], rax\n", offset);
    }
    else
    {
        fprintf(output, "    mov [rel %s], rax\n", left->value);
    }
}

static void generate_if(Node *node, FILE *output)
{
    if (!node || !output)
        return;

    int label = new_label();
    Node *cond = FIRSTCHILD(node);
    Node *current = cond ? cond->nextSibling : NULL;

    if (!cond)
    {
        return;
    }

    Node *thenInstrs[16];
    int thenCount = 0;
    while (current && thenCount < 16)
    {
        thenInstrs[thenCount++] = current;
        current = current->nextSibling;
    }

    if (cond->label == Order && (strcmp(cond->value, "<") == 0 || strcmp(cond->value, ">") == 0 ||
                                 strcmp(cond->value, "<=") == 0 || strcmp(cond->value, ">=") == 0))
    {
        Node *left = FIRSTCHILD(cond);
        Node *right = SECONDCHILD(cond);

        if (!left || !right)
        {
            return;
        }

        generate_expression(left, output);
        fprintf(output, "    mov ebx, eax\n");
        generate_expression(right, output);
        fprintf(output, "    cmp ebx, eax\n");

        if (strcmp(cond->value, "<") == 0)
            fprintf(output, "    jge .Lendif%d\n", label);
        else if (strcmp(cond->value, ">") == 0)
            fprintf(output, "    jle .Lendif%d\n", label);
        else if (strcmp(cond->value, "<=") == 0)
            fprintf(output, "    jg .Lendif%d\n", label);
        else if (strcmp(cond->value, ">=") == 0)
            fprintf(output, "    jl .Lendif%d\n", label);
    }
    else if (cond->label == Eq && (strcmp(cond->value, "==") == 0 || strcmp(cond->value, "!=") == 0))
    {
        Node *left = FIRSTCHILD(cond);
        Node *right = SECONDCHILD(cond);

        if (!left || !right)
        {
            return;
        }

        generate_expression(left, output);
        fprintf(output, "    mov ebx, eax\n");
        generate_expression(right, output);
        fprintf(output, "    cmp ebx, eax\n");
        if (strcmp(cond->value, "==") == 0)
            fprintf(output, "    jne .Lendif%d\n", label);
        else
            fprintf(output, "    je .Lendif%d\n", label);
    }
    else
    {
        generate_expression(cond, output);
        fprintf(output, "    cmp eax, 0\n");
        fprintf(output, "    je .Lendif%d\n", label);
    }

    fprintf(output, ".Lthen%d:\n", label);
    for (int i = 0; i < thenCount; ++i)
        generate_code(thenInstrs[i], output);

    fprintf(output, ".Lendif%d:\n", label);
}

static void generate_ifelse(Node *node, FILE *output)
{
    if (!node || !output)
        return;

    int label = new_label();
    Node *cond = FIRSTCHILD(node);
    Node *current = cond ? cond->nextSibling : NULL;

    if (!cond)
        return;

    // Récupérer les instructions du bloc then
    Node *thenInstrs[16];
    int thenCount = 0;
    while (current && current->label != Else && thenCount < 16)
    {
        thenInstrs[thenCount++] = current;
        current = current->nextSibling;
    }

    // Récupérer les instructions du bloc else
    Node *elseInstrs[16];
    int elseCount = 0;
    if (current && current->label == Else)
    {
        Node *child = current->firstChild;
        while (child && elseCount < 16)
        {
            elseInstrs[elseCount++] = child;
            child = child->nextSibling;
        }
    }

    // Générer la condition
    if (cond->label == Order && (strcmp(cond->value, "<") == 0 || strcmp(cond->value, ">") == 0 ||
                                 strcmp(cond->value, "<=") == 0 || strcmp(cond->value, ">=") == 0))
    {
        Node *left = FIRSTCHILD(cond);
        Node *right = SECONDCHILD(cond);

        if (!left || !right)
            return;

        generate_expression(left, output);
        fprintf(output, "    mov ebx, eax\n");
        generate_expression(right, output);
        fprintf(output, "    cmp ebx, eax\n");

        if (strcmp(cond->value, "<") == 0)
            fprintf(output, "    jge .Lelse%d\n", label);
        else if (strcmp(cond->value, ">") == 0)
            fprintf(output, "    jle .Lelse%d\n", label);
        else if (strcmp(cond->value, "<=") == 0)
            fprintf(output, "    jg .Lelse%d\n", label);
        else if (strcmp(cond->value, ">=") == 0)
            fprintf(output, "    jl .Lelse%d\n", label);
    }
    else if (cond->label == Eq && (strcmp(cond->value, "==") == 0 || strcmp(cond->value, "!=") == 0))
    {
        Node *left = FIRSTCHILD(cond);
        Node *right = SECONDCHILD(cond);

        if (!left || !right)
            return;

        generate_expression(left, output);
        fprintf(output, "    mov ebx, eax\n");
        generate_expression(right, output);
        fprintf(output, "    cmp ebx, eax\n");
        if (strcmp(cond->value, "==") == 0)
            fprintf(output, "    jne .Lelse%d\n", label);
        else
            fprintf(output, "    je .Lelse%d\n", label);
    }
    else
    {
        generate_expression(cond, output);
        fprintf(output, "    cmp eax, 0\n");
        fprintf(output, "    je .Lelse%d\n", label);
    }

    // Bloc then
    fprintf(output, ".Lthen%d:\n", label);
    for (int i = 0; i < thenCount; ++i)
        generate_code(thenInstrs[i], output);

    // Sauter à la fin si le bloc else existe
    if (elseCount > 0)
        fprintf(output, "    jmp .Lendif%d\n", label);

    // Bloc else
    fprintf(output, ".Lelse%d:\n", label);
    for (int i = 0; i < elseCount; ++i)
        generate_code(elseInstrs[i], output);

    // Fin
    fprintf(output, ".Lendif%d:\n", label);
}

static void generate_while(Node *node, FILE *output)
{
    if (!node || !output)
        return;

    int label = new_label();
    Node *cond = FIRSTCHILD(node);
    Node *current = cond ? cond->nextSibling : NULL;

    if (!cond)
    {
        return;
    }

    Node *bodyInstrs[16];
    int count = 0;
    while (current && count < 16)
    {
        bodyInstrs[count++] = current;
        current = current->nextSibling;
    }

    fprintf(output, ".Lstart%d:\n", label);

    if (cond->label == Order && (strcmp(cond->value, "<") == 0 || strcmp(cond->value, ">") == 0 ||
                                 strcmp(cond->value, "<=") == 0 || strcmp(cond->value, ">=") == 0))
    {
        Node *left = FIRSTCHILD(cond);
        Node *right = SECONDCHILD(cond);

        if (!left || !right)
        {
            return;
        }

        generate_expression(left, output);

        fprintf(output, "    mov ebx, eax\n");
        generate_expression(right, output);
        fprintf(output, "    cmp ebx, eax\n");

        if (strcmp(cond->value, "<") == 0)
            fprintf(output, "    jge .Lend%d\n", label);
        else if (strcmp(cond->value, ">") == 0)
            fprintf(output, "    jle .Lend%d\n", label);
        else if (strcmp(cond->value, "<=") == 0)
            fprintf(output, "    jg .Lend%d\n", label);
        else if (strcmp(cond->value, ">=") == 0)
            fprintf(output, "    jl .Lend%d\n", label);
    }
    else if (cond->label == Eq && (strcmp(cond->value, "==") == 0 || strcmp(cond->value, "!=") == 0))
    {
        Node *left = FIRSTCHILD(cond);
        Node *right = SECONDCHILD(cond);

        if (!left || !right)
        {
            return;
        }

        generate_expression(left, output);
        fprintf(output, "    mov ebx, eax\n");
        generate_expression(right, output);
        fprintf(output, "    cmp ebx, eax\n");
        if (strcmp(cond->value, "==") == 0)
            fprintf(output, "    jne .Lend%d\n", label);
        else
            fprintf(output, "    je .Lend%d\n", label);
    }
    else
    {
        generate_expression(cond, output);
        fprintf(output, "    cmp eax, 0\n");
        fprintf(output, "    je .Lend%d\n", label);
    }

    for (int i = 0; i < count; ++i)
    {
        generate_code(bodyInstrs[i], output);
    }

    fprintf(output, "    jmp .Lstart%d\n", label);
    fprintf(output, ".Lend%d:\n", label);
}

static void generate_function(Node *node, FILE *output)
{
    if (!node || !output)
        return;
    Node *type = FIRSTCHILD(node);
    Node *ident = type ? type->nextSibling : NULL;
    Node *params = ident ? ident->nextSibling : NULL;
    Node *body = params ? params->nextSibling : NULL;

    if (!ident || !body || !ident->value)
        return;

    local_count = 0;
    current_offset = -8;
    label_counter = 0;

    fprintf(output, "%s:\n", ident->value);
    fprintf(output, "    push rbp\n");
    fprintf(output, "    mov rbp, rsp\n");

    generate_code(body, output);

    fprintf(output, "    mov rsp, rbp\n");
    fprintf(output, "    pop rbp\n");
    fprintf(output, "    ret\n");
}
void generate_expr_recursive(Node *expr, FILE *output)
{
    if (!expr)
        return;

    if (expr->label == funCall)
    {
        generate_funcall(expr, output);
    }
    else if (expr->label == Num || expr->label == Ident)
    {
        generate_expression(expr, output);
    }
    else if ((expr->label == AddSub || expr->label == Divstar) &&
             expr->value &&
             (strcmp(expr->value, "+") == 0 ||
              strcmp(expr->value, "-") == 0 ||
              strcmp(expr->value, "*") == 0 ||
              strcmp(expr->value, "/") == 0))
    {
        Node *left = FIRSTCHILD(expr);
        Node *right = SECONDCHILD(expr);
        const char *op = expr->value;

        generate_expr_recursive(left, output);
        fprintf(output, "    push rax\n");
        generate_expr_recursive(right, output);
        fprintf(output, "    mov rbx, rax\n");
        fprintf(output, "    pop rcx\n");

        if (strcmp(op, "+") == 0)
            fprintf(output, "    lea rax, [rcx + rbx]\n");
        else if (strcmp(op, "-") == 0)
            fprintf(output, "    mov rax, rcx\n    sub rax, rbx\n");
        else if (strcmp(op, "*") == 0)
            fprintf(output, "    mov rax, rcx\n    imul rax, rbx\n");
        else if (strcmp(op, "/") == 0)
        {
            int label = new_label();
            fprintf(output, "    cmp rbx, 0\n    je .Ldivzero%d\n", label);
            fprintf(output, "    mov rax, rcx\n    cqo\n    idiv rbx\n");
            fprintf(output, "    jmp .Lenddiv%d\n", label);
            fprintf(output, ".Ldivzero%d:\n    mov rax, 60\n    mov rdi, 5\n    syscall\n", label);
            fprintf(output, ".Lenddiv%d:\n", label);
        }
    }
    else
    {
        generate_expression(expr, output);
    }
}
void generate_code(Node *node, FILE *output)
{

    if (!node || !output)
        return;

    switch (node->label)
    {
    case Prog:
        generate_prog(node, output);
        break;
    case Assign:
        generate_assign(node, output);
        break;
    case If:
        generate_if(node, output);
        break;
    case ifelse:
        generate_ifelse(node, output);
        break;
    case While:
        generate_while(node, output);
        break;
    case fonction:
        generate_function_exit(node, output);
        break;

    case Return:
    {
        Node *returnExpr = FIRSTCHILD(node);
        if (returnExpr &&
            returnExpr->label == Divstar &&
            returnExpr->value && strcmp(returnExpr->value, "*") == 0)
        {
            Node *left = FIRSTCHILD(returnExpr);
            Node *right = SECONDCHILD(returnExpr);

            int leftFun = (left && left->label == funCall);
            int rightFun = (right && right->label == funCall);

            if (leftFun && right)
            {

                generate_expression(right, output);
                fprintf(output, "    push rax\n");
                generate_funcall(left, output);
                fprintf(output, "    pop rcx\n");
                fprintf(output, "    imul rax, rcx\n");
            }
            else if (rightFun && left)
            {
                generate_expression(left, output);
                fprintf(output, "    push rax\n");
                generate_funcall(right, output);
                fprintf(output, "    pop rcx\n");
                fprintf(output, "    imul rax, rcx\n");
            }

            else
            {
                // fallback pour autre cas
                generate_expression(returnExpr, output);
            }
        }
        else if (returnExpr)
        {
            generate_expression(returnExpr, output);
        }
        else
        {
            fprintf(output, "    mov rax, 0\n");
        }

        // Épilogue
        fprintf(output, "    mov rsp, rbp\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n");
        break;
    }

    case funCall:
        generate_funcall(node, output);
        break;
    default:

        break;
    }
}