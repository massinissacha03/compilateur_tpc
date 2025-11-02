%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"
#include "tpc.tab.h"

int yycolumn = 1; // Initialiser la colonne à 1

void yyerror(char *s);

#define YY_USER_ACTION \
    yycolumn += yyleng; // Incrémenter la colonne pour chaque token

%}

%option yylineno

%x COMMENT COMMENT_slash

%%

[ \t]+         { yycolumn += yyleng; /* Compter les espaces/tabulations */ }
\n             { yycolumn = 1; /* Réinitialiser la colonne à la nouvelle ligne */ }

"static"       { return STATIC; }
"int"          { strcpy(yylval.type, yytext); return TYPE; }
"char"         { strcpy(yylval.type, yytext); return TYPE; }
"void"         { return VOID; }
"if"           { return IF; }
"else"         { return ELSE; }
"while"        { return WHILE; }
"return"       { return RETURN; }

[a-zA-Z_][a-zA-Z_0-9]* { strcpy(yylval.ident, yytext); return IDENT; }

[0-9]+         { yylval.num = atoi(yytext); return NUM; }
'\\.'|'[^\\]'  { strcpy(yylval.character, yytext); return CHARACTER; }

"=="|"!="      { strcpy(yylval.twoChars, yytext); return EQ; }
"<="|">="|"<"|">" { strcpy(yylval.twoChars, yytext); return ORDER; }

"+"|"-"        { strcpy(yylval.oneChar, yytext); return ADDSUB; }
"*"|"/"|"%"    { strcpy(yylval.oneChar, yytext); return DIVSTAR; }

"&&"           { return AND; }
"||"           { return OR; }

"="            { return '='; }
";"            { return ';'; }
","            { return ','; }
"("            { return '('; }
")"            { return ')'; }
"{"            { return '{'; }
"}"            { return '}'; }
"!"            { return '!'; }

"//"           { BEGIN(COMMENT_slash); }
<COMMENT_slash>. { /* Ignorer */ }
<COMMENT_slash>\n { BEGIN(INITIAL); yycolumn = 1; }

"/*"           { BEGIN(COMMENT); }
<COMMENT>.     { yycolumn += 1; /* Compter les caractères dans les commentaires */ }
<COMMENT>\n    { yycolumn = 1; }
<COMMENT>"*/"  { BEGIN(INITIAL); }

.              { 
    fprintf(stderr, "Erreur lexicale à la ligne %d, colonne %d : caractère inconnu '%s'\n", 
            yylineno, yycolumn - yyleng, yytext); 
    exit(1); 
}

%%

void yyerror(char *s) {
    fprintf(stderr, "Erreur syntaxique à la ligne %d, colonne %d : %s près de '%s'\n", 
            yylineno, yycolumn, s, yytext);
    exit(1);
}