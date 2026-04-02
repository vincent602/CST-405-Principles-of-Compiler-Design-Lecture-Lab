%{
/* LEXICAL ANALYZER (SCANNER)
 * This is the first phase of compilation - breaking input into tokens
 * Flex generates a scanner from these rules that reads character by character
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"  /* Token definitions from Bison */

/* Global flag to enable token display */
extern int displayTokens;
%}

/* Flex options to suppress warnings for unused functions */
%option nounput
%option noinput

%%

"int"           {
    if (displayTokens) printf("  TOKEN: INT keyword\n");
    return INT;
}

"void"          {
    if (displayTokens) printf("  TOKEN: VOID keyword\n");
    return VOID;
}

"float"         {
    if (displayTokens) printf("  TOKEN: FLOAT keyword\n");
    return FLOAT;
}

"char"          {
    if (displayTokens) printf("  TOKEN: CHAR keyword\n");
    return CHAR;
}

"string"        {
    if (displayTokens) printf("  TOKEN: STRING keyword\n");
    return STRING_KW;
}

"print"         {
    if (displayTokens) printf("  TOKEN: PRINT keyword\n");
    return PRINT;
}

"printi"        {
    if (displayTokens) printf("  TOKEN: PRINTI keyword\n");
    return PRINTI;
}

"printc"        {
    if (displayTokens) printf("  TOKEN: PRINTC keyword\n");
    return PRINTC;
}

"return"        {
    if (displayTokens) printf("  TOKEN: RETURN keyword\n");
    return RETURN;
}

"while"         {
    if (displayTokens) printf("  TOKEN: WHILE keyword\n");
    return WHILE;
}

"for"           {
    if (displayTokens) printf("  TOKEN: FOR keyword\n");
    return FOR;
}

"if"            {
    if (displayTokens) printf("  TOKEN: IF keyword\n");
    return IF;
}

"else"          {
    if (displayTokens) printf("  TOKEN: ELSE keyword\n");
    return ELSE;
}

"switch"        {
    if (displayTokens) printf("  TOKEN: SWITCH keyword\n");
    return SWITCH;
}

"case"          {
    if (displayTokens) printf("  TOKEN: CASE keyword\n");
    return CASE;
}

"default"       {
    if (displayTokens) printf("  TOKEN: DEFAULT keyword\n");
    return DEFAULT;
}

"break"         {
    if (displayTokens) printf("  TOKEN: BREAK keyword\n");
    return BREAK;
}

[a-zA-Z_][a-zA-Z0-9_]* {
    if (displayTokens) printf("  TOKEN: ID '%s'\n", yytext);
    yylval.str = strdup(yytext);
    return ID;
}

\"([^\\\"\n]|\\.)*\" {
    size_t len = strlen(yytext);
    char* literal = malloc(len - 1);
    if (!literal) {
        fprintf(stderr, "Lexical Error: out of memory while scanning string literal\n");
        exit(1);
    }
    memcpy(literal, yytext + 1, len - 2);
    literal[len - 2] = '\0';
    if (displayTokens) printf("  TOKEN: STRING_LITERAL \"%s\"\n", literal);
    yylval.str = literal;
    return STRING_LITERAL;
}

[0-9]+"."[0-9]+ {
    if (displayTokens) printf("  TOKEN: FNUM %s\n", yytext);
    yylval.fnum = atof(yytext);
    return FNUM;
}

[0-9]+          {
    if (displayTokens) printf("  TOKEN: NUM %s\n", yytext);
    yylval.num = atoi(yytext);
    return NUM;
}

"+"             {
    if (displayTokens) printf("  TOKEN: '+'\n");
    return '+';
}

"-"             {
    if (displayTokens) printf("  TOKEN: '-'\n");
    return '-';
}

"*"             {
    if (displayTokens) printf("  TOKEN: '*'\n");
    return '*';
}

"/"             {
    if (displayTokens) printf("  TOKEN: '/'\n");
    return '/';
}

"=="            {
    if (displayTokens) printf("  TOKEN: '=='\n");
    return EQ;
}

"!="            {
    if (displayTokens) printf("  TOKEN: '!='\n");
    return NE;
}

"&&"            {
    if (displayTokens) printf("  TOKEN: '&&'\n");
    return AND;
}

"||"            {
    if (displayTokens) printf("  TOKEN: '||'\n");
    return OR;
}

"<="            {
    if (displayTokens) printf("  TOKEN: '<='\n");
    return LE;
}

">="            {
    if (displayTokens) printf("  TOKEN: '>='\n");
    return GE;
}

"<"             {
    if (displayTokens) printf("  TOKEN: '<'\n");
    return '<';
}

">"             {
    if (displayTokens) printf("  TOKEN: '>'\n");
    return '>';
}

"!"             {
    if (displayTokens) printf("  TOKEN: '!'\n");
    return NOT;
}

"="             {
    if (displayTokens) printf("  TOKEN: '='\n");
    return '=';
}

";"             {
    if (displayTokens) printf("  TOKEN: ';'\n");
    return ';';
}

","             {
    if (displayTokens) printf("  TOKEN: ','\n");
    return ',';
}

":"             {
    if (displayTokens) printf("  TOKEN: ':'\n");
    return ':';
}

"("             {
    if (displayTokens) printf("  TOKEN: '('\n");
    return '(';
}

")"             {
    if (displayTokens) printf("  TOKEN: ')'\n");
    return ')';
}

"["             {
    if (displayTokens) printf("  TOKEN: '['\n");
    return '[';
}

"]"             {
    if (displayTokens) printf("  TOKEN: ']'\n");
    return ']';
}

"{"             {
    if (displayTokens) printf("  TOKEN: '{'\n");
    return '{';
}

"}"             {
    if (displayTokens) printf("  TOKEN: '}'\n");
    return '}';
}

[ \t\n]         { }

.               { printf("Lexical Error: Unknown character '%c'\n", *yytext); }

%%

/* Required by Flex - signals end of input */
int yywrap() {
    return 1;  /* 1 means no more input files */
}
