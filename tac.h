#ifndef TAC_H
#define TAC_H

#include "ast.h"

/* THREE-ADDRESS CODE (TAC)
 * Intermediate representation between AST and machine code
 * Each instruction has at most 3 operands (result = arg1 op arg2)
 * Makes optimization and code generation easier
 */

/* TAC INSTRUCTION TYPES */
typedef enum {
    TAC_ADD,     /* Addition: result = arg1 + arg2 */
    TAC_SUB,     /* Subtraction */
    TAC_MUL,     /* Multiplication */
    TAC_DIV,     /* Division */
    /* Comparison: result = (arg1 op arg2) where result is 0/1 */
    TAC_LT,      /* < */
    TAC_LE,      /* <= */
    TAC_GT,      /* > */
    TAC_GE,      /* >= */
    TAC_EQ,      /* == */
    TAC_NE,      /* != */
    /* Control flow */
    TAC_LABEL,   /* Label: result is label name */
    TAC_GOTO,    /* Unconditional jump: result is label name */
    TAC_IFZ,     /* Conditional jump if arg1 == 0: goto result */
    TAC_ARR_LOAD,  /* Array load: result = array[index] */
    TAC_ARR_STORE, /* Array store: array[index] = value */
    TAC_CALL,    /* Function call */
    TAC_RETURN,  /* Return from function */
    TAC_ASSIGN,  /* Assignment: result = arg1 */
    TAC_STRING_ASSIGN, /* String literal assignment: result = "arg1" */
    TAC_PRINT,   /* Print: print(arg1) */
    TAC_PRINTI,  /* Inline print (no newline): printi(arg1) */
    TAC_PRINTC,  /* Print character (ASCII): printc(arg1) */
    TAC_DECL     /* Declaration: declare result */
} TACOp;

/* TAC INSTRUCTION STRUCTURE */
typedef struct TACInstr {
    TACOp op;               /* Operation type */
    char* arg1;             /* First operand (if needed) */
    char* arg2;             /* Second operand (for binary ops) */
    char* result;           /* Result/destination */
    struct TACInstr* next;  /* Linked list pointer */
} TACInstr;

/* TAC LIST MANAGEMENT */
typedef struct {
    TACInstr* head;    /* First instruction */
    TACInstr* tail;    /* Last instruction (for efficient append) */
    int tempCount;     /* Counter for temporary variables (t0, t1, ...) */
    int labelCount;    /* Counter for labels (L0, L1, ...) */
} TACList;

/* TAC GENERATION FUNCTIONS */
void initTAC();                                                    /* Initialize TAC lists */
char* newTemp();                                                   /* Generate new temp variable */
char* newLabel();                                                  /* Generate new label */
TACInstr* createTAC(TACOp op, char* arg1, char* arg2, char* result); /* Create TAC instruction */
void appendTAC(TACInstr* instr);                                  /* Add instruction to list */
void generateTAC(ASTNode* node);                                  /* Convert AST to TAC */
char* generateTACExpr(ASTNode* node);                             /* Generate TAC for expression */

/* TAC OPTIMIZATION AND OUTPUT */
void printTAC();                                                   /* Display unoptimized TAC */
void optimizeTAC();                                                /* Apply optimizations */
void printOptimizedTAC();                                          /* Display optimized TAC */
void saveTACToFile(const char* filename);                         /* Save unoptimized TAC to file */
void saveOptimizedTACToFile(const char* filename);                /* Save optimized TAC to file */

#endif
