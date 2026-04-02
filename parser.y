%{
/* SYNTAX ANALYZER (PARSER)
 * This is the second phase of compilation - checking grammar rules
 * Bison generates a parser that builds an Abstract Syntax Tree (AST)
 * The parser uses tokens from the scanner to verify syntax is correct
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* External declarations for lexer interface */
extern int yylex();      /* Get next token from scanner */
extern int yyparse();    /* Parse the entire input */
extern FILE* yyin;       /* Input file handle */

void yyerror(const char* s);  /* Error handling function */
ASTNode* root = NULL;          /* Root of the Abstract Syntax Tree */

/* Lower logical expressions into existing AST nodes.
 * bool(x) := (x != 0)
 * x && y  := bool(x) * bool(y)
 * x || y  := (bool(x) + bool(y)) != 0
 * !x      := (x == 0)
 */
static ASTNode* makeBoolExpr(ASTNode* e) {
    return createCompare(CMP_NE, e, createNum(0));
}

static ASTNode* makeLogicalAnd(ASTNode* left, ASTNode* right) {
    ASTNode* l = makeBoolExpr(left);
    ASTNode* r = makeBoolExpr(right);
    return createBinOp('*', l, r);
}

static ASTNode* makeLogicalOr(ASTNode* left, ASTNode* right) {
    ASTNode* l = makeBoolExpr(left);
    ASTNode* r = makeBoolExpr(right);
    ASTNode* sum = createBinOp('+', l, r);
    return createCompare(CMP_NE, sum, createNum(0));
}

static ASTNode* makeLogicalNot(ASTNode* e) {
    return createCompare(CMP_EQ, e, createNum(0));
}
%}

/* SEMANTIC VALUES UNION
 * Defines possible types for tokens and grammar symbols
 * This allows different grammar rules to return different data types
 */
%union {
    int num;                /* For integer literals */
    double fnum;            /* For float literals */
    char* str;              /* For identifiers and string literals */
    struct ASTNode* node;   /* For AST nodes */
    struct ParamList* plist;/* For parameter lists */
}

/* TOKEN DECLARATIONS with their semantic value types */
%token <num> NUM        /* Number token carries an integer value */
%token <fnum> FNUM      /* Float number token carries a double value */
%token <str> ID         /* Identifier token carries a string */
%token <str> STRING_LITERAL

/* KEYWORDS - ADD THESE FOUR LINES */
%token INT VOID FLOAT CHAR STRING_KW    /* Type keywords */
%token PRINT PRINTI PRINTC RETURN           /* Statement keywords */
%token WHILE                  /* Control flow keywords */
%token FOR                    /* Control flow keywords */
%token IF ELSE                /* Control flow keywords */
%token SWITCH CASE DEFAULT BREAK /* Switch control flow keywords */
%token AND OR NOT             /* Logical operators: && || ! */

/* COMPARISON TOKENS */
%token LE GE EQ NE            /* <= >= == != */

/* NON-TERMINAL TYPES - Define what type each grammar rule returns */
%type <node> program function_list function block stmt_list stmt decl assign assign_expr assign_expr_opt expr expr_opt print_stmt return_stmt arg_list arg_list_opt func_call while_stmt for_stmt if_stmt
%type <node> switch_stmt break_stmt case_list case_clause opt_stmt_list
%type <plist> params params_opt param
%type <str> type_spec

/* OPERATOR PRECEDENCE AND ASSOCIATIVITY */
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/'
%right NOT

%%

/* GRAMMAR RULES - Define the structure of our language */

/* PROGRAM RULE - Entry point of our grammar */
program:
    function_list {
        root = $1;
    }
    | stmt_list {
        root = $1;
    }
    ;

function_list:
    function {
        $$ = $1;
    }
    | function_list function {
        $$ = createFunctionList($1, $2);
    }
    ;

function:
    type_spec ID '(' params_opt ')' block {
        $$ = createFunction($1, $2, $4, $6);
        free($1);
        free($2);
    }
    ;

params_opt:
    /* empty */ {
        $$ = NULL;
    }
    | params {
        $$ = $1;
    }
    ;

params:
    param {
        $$ = $1;
    }
    | param ',' params {
        $1->next = $3;
        $$ = $1;
    }
    ;

param:
    type_spec ID {
        $$ = createParam($1, $2, 0);
        free($1);
        free($2);
    }
    | type_spec ID '[' ']' {
        $$ = createParam($1, $2, 1);
        free($1);
        free($2);
    }
    ;

type_spec:
    INT { $$ = strdup("int"); }
    | FLOAT { $$ = strdup("float"); }
    | CHAR { $$ = strdup("char"); }
    | STRING_KW { $$ = strdup("string"); }
    | VOID { $$ = strdup("void"); }
    ;

/* STATEMENT LIST - Handles multiple statements */
stmt_list:
    stmt { 
        /* Base case: single statement */
        $$ = $1;  /* Pass the statement up as-is */
    }
    | stmt_list stmt { 
        /* Recursive case: list followed by another statement */
        $$ = createStmtList($1, $2);  /* Build linked list of statements */
    }
    ;

/* STATEMENT TYPES - The three kinds of statements we support */
stmt:
    decl        /* Variable declaration */
    | assign    /* Assignment statement */
    | print_stmt /* Print statement */
    | return_stmt /* Return statement */
    | func_call ';' /* Function call statement */
    | block     /* Nested block */
    | while_stmt /* While loop */
    | for_stmt   /* For loop */
    | if_stmt    /* If / if-else */
    | switch_stmt /* Switch statement */
    | break_stmt  /* Break statement */
    ;

/* DECLARATION RULE - "int x;" */
decl:
    type_spec ID ';' {
        $$ = createTypedDecl($1, $2, 0, 0);
        free($1);
        free($2);
    }
    | type_spec ID '[' NUM ']' ';' {
        $$ = createTypedDecl($1, $2, 1, $4);
        free($1);
        free($2);
    }
    ;

/* ASSIGNMENT RULE - "x = expr;" */
assign:
    ID '=' expr ';' { 
        /* Create assignment node with variable name and expression */
        $$ = createAssign($1, $3);  /* $1 = ID, $3 = expr */
        free($1);                   /* Free the identifier string */
    }
    | ID '[' expr ']' '=' expr ';' {
        $$ = createArrayAssign($1, $3, $6);
        free($1);
    }
    ;

/* Assignment expression for for-loop headers (no trailing semicolon) */
assign_expr:
    ID '=' expr {
        $$ = createAssign($1, $3);
        free($1);
    }
    | ID '[' expr ']' '=' expr {
        $$ = createArrayAssign($1, $3, $6);
        free($1);
    }
    ;

assign_expr_opt:
    /* empty */ { $$ = NULL; }
    | assign_expr { $$ = $1; }
    ;

expr_opt:
    /* empty */ { $$ = NULL; }
    | expr { $$ = $1; }
    ;

/* EXPRESSION RULES - Build expression trees */
expr:
    NUM { 
        /* Literal number */
        $$ = createNum($1);  /* Create leaf node with number value */
    }
    | FNUM {
        $$ = createFloat($1);
    }
    | STRING_LITERAL {
        $$ = createStringLit($1);
        free($1);
    }
    | ID { 
        /* Variable reference */
        $$ = createVar($1);  /* Create leaf node with variable name */
        free($1);            /* Free the identifier string */
    }
    | ID '[' expr ']' {
        $$ = createArrayAccess($1, $3);
        free($1);
    }
    | func_call {
        $$ = $1;
    }
    | '(' expr ')' {
        $$ = $2;
    }
    | expr '+' expr { 
        /* Addition operation - builds binary tree */
        $$ = createBinOp('+', $1, $3);  /* Left child, op, right child */
    }
    | expr '-' expr {
        $$ = createBinOp('-', $1, $3);
    }
    | expr '*' expr {
        $$ = createBinOp('*', $1, $3);
    }
    | expr '/' expr {
        $$ = createBinOp('/', $1, $3);
    }
    | expr '<' expr {
        $$ = createCompare(CMP_LT, $1, $3);
    }
    | expr '>' expr {
        $$ = createCompare(CMP_GT, $1, $3);
    }
    | expr LE expr {
        $$ = createCompare(CMP_LE, $1, $3);
    }
    | expr GE expr {
        $$ = createCompare(CMP_GE, $1, $3);
    }
    | expr EQ expr {
        $$ = createCompare(CMP_EQ, $1, $3);
    }
    | expr NE expr {
        $$ = createCompare(CMP_NE, $1, $3);
    }
    | expr AND expr {
        $$ = makeLogicalAnd($1, $3);
    }
    | expr OR expr {
        $$ = makeLogicalOr($1, $3);
    }
    | NOT expr %prec NOT {
        $$ = makeLogicalNot($2);
    }
    ;

while_stmt:
    WHILE '(' expr ')' block {
        $$ = createWhile($3, $5);
    }
    ;

for_stmt:
    FOR '(' assign_expr_opt ';' expr_opt ';' assign_expr_opt ')' block {
        $$ = createFor($3, $5, $7, $9);
    }
    ;

if_stmt:
    IF '(' expr ')' stmt %prec LOWER_THAN_ELSE {
        $$ = createIf($3, $5, NULL);
    }
    | IF '(' expr ')' stmt ELSE stmt {
        $$ = createIf($3, $5, $7);
    }
    ;

switch_stmt:
    SWITCH '(' expr ')' '{' case_list '}' {
        $$ = createSwitch($3, $6);
    }
    ;

case_list:
    /* empty */ {
        $$ = NULL;
    }
    | case_list case_clause {
        if ($1 == NULL) {
            $$ = $2;
        } else {
            ASTNode* tail = $1;
            while (tail->data.case_clause.next) {
                tail = tail->data.case_clause.next;
            }
            tail->data.case_clause.next = $2;
            $$ = $1;
        }
    }
    ;

case_clause:
    CASE NUM ':' opt_stmt_list {
        $$ = createCase($2, 0, $4);
    }
    | DEFAULT ':' opt_stmt_list {
        $$ = createCase(0, 1, $3);
    }
    ;

opt_stmt_list:
    /* empty */ { $$ = NULL; }
    | stmt_list { $$ = $1; }
    ;

break_stmt:
    BREAK ';' {
        $$ = createBreak();
    }
    ;

/* PRINT STATEMENT - "print(expr);" */
print_stmt:
    PRINT '(' expr ')' ';' { 
        /* Create print node with expression to print */
        $$ = createPrint($3);  /* $3 is the expression inside parens */
    }
    | PRINTI '(' expr ')' ';' {
        $$ = createPrintInline($3);
    }
    | PRINTC '(' expr ')' ';' {
        $$ = createPrintChar($3);
    }
    ;

return_stmt:
    RETURN ';' {
        $$ = createReturn(NULL);
    }
    | RETURN expr ';' {
        $$ = createReturn($2);
    }
    ;

func_call:
    ID '(' arg_list_opt ')' {
        $$ = createFunctionCall($1, $3);
        free($1);
    }
    ;

arg_list_opt:
    /* empty */ { $$ = NULL; }
    | arg_list { $$ = $1; }
    ;

arg_list:
    expr { $$ = createArgList($1, NULL); }
    | expr ',' arg_list { $$ = createArgList($1, $3); }
    ;

block:
    '{' '}' { $$ = createBlock(NULL); }
    | '{' stmt_list '}' { $$ = createBlock($2); }
    ;

%%

/* ERROR HANDLING - Called by Bison when syntax error detected */
void yyerror(const char* s) {
    fprintf(stderr, "Syntax Error: %s\n", s);
}
