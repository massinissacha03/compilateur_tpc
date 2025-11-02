%{
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include "tree.h" 

void yyerror(char * s);
int yylex(void);

Node *root;
extern FILE *yyin;

%}



%union {
    int num;         
    char character[3];      
    char ident[64];   
    char twoChars[2]; 
    char oneChar[1] ;
    char type[5] ; 

    Node *node ; 
}

%token <type> TYPE
%token STATIC VOID IF ELSE WHILE RETURN
%token <character> CHARACTER 
%token <oneChar> ADDSUB
%token <oneChar> DIVSTAR
%token <num> NUM
%token <ident> IDENT
%token <twoChars> ORDER EQ
%token OR AND
%token '{' '}' '(' ')' '!' ';' ','


%type <node> Prog DeclVars DeclFoncts DeclFonct EnTeteFonct Parametres ListTypVar StaticDeclVars
%type <node> Corps SuiteInstr Instr Exp TB FB M E T F Arguments ListExp Declarateurs

%nonassoc THEN  
%nonassoc ELSE


%start Prog
%%
Prog:
    DeclVars DeclFoncts {
        $$ = makeNode(Prog);
        if ($1) addChild($$, $1); // DeclVars fils de Prog
        if ($2) addChild($$, $2); // DeclFoncts fils de Prog
        root = $$;

    }
    
    ;

DeclVars:
       DeclVars TYPE Declarateurs ';'{
        if ($1 == NULL) {
            $$ = makeNode(Type); 
            $$->value = strdup($2); 
            addChild($$, $3);    
        } else {
            Node *typeNode = makeNode(Type); 
            typeNode->value = strdup($2);
            addChild(typeNode, $3);        
            addSibling($1, typeNode);      
            $$ = $1;                       
        }

       }
    | {
        $$ = NULL;
    }
    ;
Declarateurs:
       Declarateurs ',' IDENT {
        $$ = $1; 
        Node *identNode = makeNode(Ident); 
        identNode->value = strdup($3);    
        addSibling($$, identNode);       


       }
    |  IDENT {
        $$ = makeNode(Ident); 
        $$->value = strdup($1); 


    }
    ;


DeclFoncts:
    DeclFoncts DeclFonct {
        if ($1 == NULL) {
            $$ = $2; 
        } else {
            $$ = $1; 
            addSibling($$, $2); 
        }
    }
    | DeclFonct {
        $$ = $1; 
    }
    ;



DeclFonct:
       EnTeteFonct Corps {
        $$ = makeNode(fonction);
           addChild($$, $1);
           addChild($$, $2);

       }
    ;
EnTeteFonct:
       TYPE IDENT '(' Parametres ')'
       {
            $$ = makeNode(Type);
            $$->value = strdup($1);
            Node * identNode = makeNode(Ident) ;
            identNode->value = strdup($2);
           addSibling($$, identNode);

           addSibling($$, $4); 
       }
    |  VOID IDENT '(' Parametres ')'
        {
            $$ = makeNode(Void);
            Node * identNode = makeNode(Ident); 
            identNode->value = strdup($2); 
           addSibling($$, identNode);

           addSibling($$, $4); 

        }
    ;

Parametres:
       VOID {
        $$ = makeNode(Parametres); 
        addChild($$ , makeNode(Void)); 

       }
    |  ListTypVar {
        $$ = makeNode(Parametres); 
        addChild($$, $1);

    }
    ;

ListTypVar:
    ListTypVar ',' TYPE IDENT {
        $$ = $1; 
        Node *typeNode = makeNode(Type); 
        typeNode->value = strdup($3);
        Node *identNode = makeNode(Ident); 
        identNode->value = strdup($4); 
        addChild(typeNode, identNode);
        addSibling($$, typeNode); 
    }
    | TYPE IDENT {
        $$ = makeNode(Type); 
        $$->value = strdup($1); 
        Node *identNode = makeNode(Ident);
        identNode->value = strdup($2);
        addChild($$, identNode); 
    }
    ;

Corps:
    '{' StaticDeclVars SuiteInstr '}' {
        $$ = makeNode(Corps); 
        if ($2) addChild($$, $2); 
        if ($3) addChild($$, $3); 
    }
    ;
StaticDeclVars:
    StaticDeclVars TYPE Declarateurs ';' {
        Node *typeNode = makeNode(Type);
        if (!typeNode) { fprintf(stderr, "Erreur : makeNode a échoué\n"); exit(3); }
        typeNode->value = strdup($2);
        if (!typeNode->value) { fprintf(stderr, "Erreur : strdup a échoué\n"); exit(3); }
        addChild(typeNode, $3);
        if ($1 == NULL) {
            $$ = typeNode;
        } else {
            addSibling($1, typeNode);
            $$ = $1;
        }
    }
    | StaticDeclVars STATIC TYPE Declarateurs ';' {
        Node *typeNode = makeNode(Type);
        if (!typeNode) { fprintf(stderr, "Erreur : makeNode a échoué\n"); exit(3); }
        typeNode->value = strdup($3);
        if (!typeNode->value) { fprintf(stderr, "Erreur : strdup a échoué\n"); exit(3); }
        Node *staticNode = makeNode(Static);
        if (!staticNode) { fprintf(stderr, "Erreur : makeNode a échoué\n"); exit(3); }
        addChild(typeNode, staticNode);
        addChild(typeNode, $4);
        if ($1 == NULL) {
            $$ = typeNode;
        } else {
            addSibling($1, typeNode);
            $$ = $1;
        }
    }
    | {
        $$ = NULL;
    }
    ;

    
SuiteInstr:
    SuiteInstr Instr {
        if ($1 == NULL) {
            $$ = $2; 
        } else if ($2 != NULL) {
            addSibling($1, $2); 
            $$ = $1;
        }
    }
    | {
        $$ = NULL; 
    }
    ;


Instr:
    IDENT '=' Exp ';' {
        $$ = makeNode(Assign); 

        Node *identNode = makeNode(Ident); 
        identNode->value = strdup($1); 
        addChild($$, identNode); 

        if ($3 != NULL) {
            addChild($$, $3); 
        }
    }
    | IF '(' Exp ')' Instr %prec THEN {
        $$ = makeNode(If); 
        addChild($$, $3); 
        addChild($$, $5); 
    }
   | IF '(' Exp ')' Instr ELSE Instr {
        $$ = makeNode(ifelse); 
        addChild($$, $3);      
        addChild($$, $5);      

        // else comme enfant de if 
        Node *elseNode = makeNode(Else); 
        addChild(elseNode, $7);
        addChild($$, elseNode); //
    }

    | WHILE '(' Exp ')' Instr {
        $$ = makeNode(While); 
        addChild($$, $3); 
        addChild($$, $5); 
    }
    | IDENT '(' Arguments ')' ';' {
        $$ = makeNode(funCall); 
                Node *identNode = makeNode(Ident); 
        identNode->value = strdup($1); 
        addChild($$, identNode); 
        if ($3 != NULL) addChild($$, $3); 
    }
    | RETURN Exp ';' {
        $$ = makeNode(Return); 
        if ($2 != NULL) {
            addChild($$, $2); 
        }
    }
    | RETURN ';' {
        $$ = makeNode(Return); 
    }
    | '{' SuiteInstr '}' {
        $$ = $2; 
    }
    | ';' {
        $$ = NULL; 
    }
    ;

  
Exp:
    Exp OR TB {
        $$ = makeNode(Or); 
        addChild($$, $1); 
        addChild($$, $3); 
    }
    | TB {
        $$ = $1; 
    }
    ;

TB:
    TB AND FB {
        $$ = makeNode(And); 
        addChild($$, $1); 
        addChild($$, $3); 
    }
    | FB {
        $$ = $1; 
    }
    ;

FB:
    FB EQ M {
        $$ = makeNode(Eq); 
        $$->value = strdup($2);
        addChild($$, $1); 
        addChild($$, $3); 
    }
    | M {
        $$ = $1; 
    }
    ;

M:
    M ORDER E {
        $$ = makeNode(Order); 
        $$->value = strdup($2); 

        addChild($$, $1); 
        addChild($$, $3); 
    }
    | E {
        $$ = $1; 
    }
    ;
E:
    E ADDSUB T {
        $$ = makeNode(AddSub); 
        $$->value = strdup($2); 
        addChild($$, $1); 
        addChild($$, $3); 
    }
    | T {
        $$ = $1; 
    }
    ;
T:
    T DIVSTAR F {
        $$ = makeNode(Divstar); 
        $$->value = strdup($2); 
        addChild($$, $1); 
        addChild($$, $3); 
    }
    | F {
        $$ = $1; 
    }
    ;

F:
    ADDSUB F {
        $$ = makeNode(Unary); 
        $$->value = strdup($1); 
        addChild($$, $2); 
    }
    | '!' F {
        $$ = makeNode(Not); 
        addChild($$, $2); 
    }
    | '(' Exp ')' {
        $$ = $2; 
    }
    | NUM {
        $$ = makeNode(Num); 
        asprintf(&($$->value), "%d", $1);
    }
    | CHARACTER {
        $$ = makeNode(Character); 
        $$->value = strdup($1);
    }
    | IDENT {
        $$ = makeNode(Ident); 
        $$->value = strdup($1); 
    }
    | IDENT '(' Arguments ')' {
        $$ = makeNode(funCall); 
        Node *identNode = makeNode(Ident); 
        identNode->value = strdup($1); 
        addChild($$, identNode); 
        if ($3 != NULL) addChild($$, $3); 
        }
    ;




Arguments:
    ListExp {
        $$ = $1; 
    }
    | {
        $$ = NULL; 
    }
    ;

ListExp:
    ListExp ',' Exp {
        $$ = $1; 
        addSibling($$, $3); 
    }
    | Exp {
        $$ = $1; 
    }
    ;

%%


