/* AST IMPLEMENTATION
 * Functions to create and manipulate Abstract Syntax Tree nodes
 * The AST is built during parsing and used for all subsequent phases
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* Create a number literal node */
ASTNode* createNum(int value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_NUM;
    node->data.num = value;  /* Store the integer value */
    return node;
}

/* Create a float literal node */
ASTNode* createFloat(double value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FLOAT;
    node->data.fnum = value;
    return node;
}

/* Create a string literal node */
ASTNode* createStringLit(char* value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_STRING_LIT;
    node->data.strlit = strdup(value);
    return node;
}

/* Create a variable reference node */
ASTNode* createVar(char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_VAR;
    node->data.name = strdup(name);  /* Copy the variable name */
    return node;
}

/* Create a binary operation node (for addition) */
ASTNode* createBinOp(char op, ASTNode* left, ASTNode* right) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_BINOP;
    node->data.binop.op = op;        /* Store operator (+) */
    node->data.binop.left = left;    /* Left subtree */
    node->data.binop.right = right;  /* Right subtree */
    return node;
}

/* Create a comparison operation node */
ASTNode* createCompare(CompareOp op, ASTNode* left, ASTNode* right) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_COMPARE;
    node->data.compare.op = op;
    node->data.compare.left = left;
    node->data.compare.right = right;
    return node;
}

/* Create a variable declaration node */
ASTNode* createDecl(char* name) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_DECL;
    node->data.name = strdup(name);  /* Store variable name */
    return node;
}

/*
ASTNode* createDeclWithAssgn(char* name, int value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_DECL;
    node->data.name = strdup(name); 
    node->data.value = value;
    return node;
}

*/

/* Create an assignment statement node */
ASTNode* createAssign(char* var, ASTNode* value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ASSIGN;
    node->data.assign.var = strdup(var);  /* Variable name */
    node->data.assign.value = value;      /* Expression tree */
    return node;
}

/* Create an array element assignment node */
ASTNode* createArrayAssign(char* name, ASTNode* index, ASTNode* value) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARRAY_ASSIGN;
    node->data.arrayAssign.name = strdup(name);
    node->data.arrayAssign.index = index;
    node->data.arrayAssign.value = value;
    return node;
}

/* Create an array access node */
ASTNode* createArrayAccess(char* name, ASTNode* index) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARRAY_ACCESS;
    node->data.arrayAccess.name = strdup(name);
    node->data.arrayAccess.index = index;
    return node;
}

/* Create a print statement node */
ASTNode* createPrint(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PRINT;
    node->data.expr = expr;  /* Expression to print */
    return node;
}

/* Create a print statement node that does NOT append a newline */
ASTNode* createPrintInline(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PRINTI;
    node->data.expr = expr;
    return node;
}

/* Create a print-character statement node (prints ASCII code as a char) */
ASTNode* createPrintChar(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_PRINTC;
    node->data.expr = expr;
    return node;
}

/* Create a while loop node */
ASTNode* createWhile(ASTNode* condition, ASTNode* body) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_WHILE;
    node->data.whileStmt.condition = condition;
    node->data.whileStmt.body = body;
    return node;
}

/* Create a for loop node */
ASTNode* createFor(ASTNode* init, ASTNode* condition, ASTNode* update, ASTNode* body) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FOR;
    node->data.forStmt.init = init;
    node->data.forStmt.condition = condition;
    node->data.forStmt.update = update;
    node->data.forStmt.body = body;
    return node;
}

/* Create an if / if-else statement node */
ASTNode* createIf(ASTNode* condition, ASTNode* thenBranch, ASTNode* elseBranch) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_IF;
    node->data.ifStmt.condition = condition;
    node->data.ifStmt.thenBranch = thenBranch;
    node->data.ifStmt.elseBranch = elseBranch;
    return node;
}

/* Create a switch statement node */
ASTNode* createSwitch(ASTNode* expr, ASTNode* cases) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_SWITCH;
    node->data.switch_stmt.expr = expr;
    node->data.switch_stmt.cases = cases;
    return node;
}

/* Create a case/default clause node */
ASTNode* createCase(int value, int isDefault, ASTNode* body) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_CASE;
    node->data.case_clause.value = value;
    node->data.case_clause.isDefault = isDefault;
    node->data.case_clause.body = body;
    node->data.case_clause.next = NULL;
    return node;
}

/* Create a break statement node */
ASTNode* createBreak() {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_BREAK;
    return node;
}

/* Create a statement list node (links statements together) */
ASTNode* createStmtList(ASTNode* stmt1, ASTNode* stmt2) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_STMT_LIST;
    node->data.stmtlist.stmt = stmt1;  /* First statement */
    node->data.stmtlist.next = stmt2;  /* Rest of list */
    return node;
}

/* Create function definition node */
ASTNode* createFunction(char* returnType, char* name, ParamList* params, ASTNode* body) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FUNCTION;
    node->data.function.returnType = strdup(returnType);
    node->data.function.name = strdup(name);
    node->data.function.params = params;
    node->data.function.body = body;
    return node;
}

/* Create function list node */
ASTNode* createFunctionList(ASTNode* func1, ASTNode* func2) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FUNCTION_LIST;
    node->data.functionlist.function = func1;
    node->data.functionlist.next = func2;
    return node;
}

/* Create function call node */
ASTNode* createFunctionCall(char* name, ASTNode* args) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_FUNCTION_CALL;
    node->data.functioncall.name = strdup(name);
    node->data.functioncall.args = args;
    return node;
}

/* Create argument list node */
ASTNode* createArgList(ASTNode* expr, ASTNode* next) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_ARG_LIST;
    node->data.arglist.expr = expr;
    node->data.arglist.next = next;
    return node;
}

/* Create return statement node */
ASTNode* createReturn(ASTNode* expr) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_RETURN;
    node->data.returnExpr = expr;
    return node;
}

/* Create block node */
ASTNode* createBlock(ASTNode* stmts) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_BLOCK;
    node->data.blockStmts = stmts;
    return node;
}

/* Create parameter */
ParamList* createParam(char* type, char* name, int isArray) {
    ParamList* param = malloc(sizeof(ParamList));
    param->type = strdup(type);
    param->name = strdup(name);
    param->isArray = isArray;
    param->next = NULL;
    return param;
}

/* Add parameter to list (append) */
ParamList* addParam(ParamList* list, char* type, char* name, int isArray) {
    ParamList* param = createParam(type, name, isArray);
    if (!list) return param;
    ParamList* curr = list;
    while (curr->next) curr = curr->next;
    curr->next = param;
    return list;
}

/* Create typed declaration */
ASTNode* createTypedDecl(char* type, char* name, int isArray, int arraySize) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = NODE_TYPED_DECL;
    node->data.typedDecl.varType = strdup(type);
    node->data.typedDecl.varName = strdup(name);
    node->data.typedDecl.isArray = isArray;
    node->data.typedDecl.arraySize = isArray ? arraySize : 0;
    return node;
}

void printParams(ParamList* params) {
    ParamList* curr = params;
    while (curr) {
        printf("%s %s%s", curr->type, curr->name, curr->isArray ? "[]" : "");
        if (curr->next) printf(", ");
        curr = curr->next;
    }
    printf("\n");
}

/* Display the AST structure (for debugging and education) */
void printAST(ASTNode* node, int level) {
    if (!node) return;
    
    /* Indent based on tree depth */
    for (int i = 0; i < level; i++) printf("  ");
    
    /* Print node based on its type */
    switch(node->type) {
        case NODE_NUM:
            printf("NUM: %d\n", node->data.num);
            break;
        case NODE_FLOAT:
            printf("FLOAT: %f\n", node->data.fnum);
            break;
        case NODE_STRING_LIT:
            printf("STRING_LITERAL: \"%s\"\n", node->data.strlit);
            break;
        case NODE_VAR:
            printf("VAR: %s\n", node->data.name);
            break;
        case NODE_ARRAY_ACCESS:
            printf("ARRAY_ACCESS: %s\n", node->data.arrayAccess.name);
            printAST(node->data.arrayAccess.index, level + 1);
            break;
        case NODE_BINOP:
            printf("BINOP: %c\n", node->data.binop.op);
            printAST(node->data.binop.left, level + 1);
            printAST(node->data.binop.right, level + 1);
            break;
        case NODE_COMPARE: {
            const char* opStr = "?";
            switch(node->data.compare.op) {
                case CMP_LT: opStr = "<"; break;
                case CMP_LE: opStr = "<="; break;
                case CMP_GT: opStr = ">"; break;
                case CMP_GE: opStr = ">="; break;
                case CMP_EQ: opStr = "=="; break;
                case CMP_NE: opStr = "!="; break;
            }
            printf("COMPARE: %s\n", opStr);
            printAST(node->data.compare.left, level + 1);
            printAST(node->data.compare.right, level + 1);
            break;
        }
        case NODE_DECL:
            printf("DECL: %s\n", node->data.name);
            break;
        case NODE_ARRAY_ASSIGN:
            printf("ARRAY_ASSIGN: %s\n", node->data.arrayAssign.name);
            printAST(node->data.arrayAssign.index, level + 1);
            printAST(node->data.arrayAssign.value, level + 1);
            break;
        case NODE_ASSIGN:
            printf("ASSIGN: %s\n", node->data.assign.var);
            printAST(node->data.assign.value, level + 1);
            break;
        case NODE_PRINT:
            printf("PRINT\n");
            printAST(node->data.expr, level + 1);
            break;
        case NODE_PRINTI:
            printf("PRINTI\n");
            printAST(node->data.expr, level + 1);
            break;
        case NODE_PRINTC:
            printf("PRINTC\n");
            printAST(node->data.expr, level + 1);
            break;
        case NODE_WHILE:
            printf("WHILE\n");
            printAST(node->data.whileStmt.condition, level + 1);
            printAST(node->data.whileStmt.body, level + 1);
            break;
        case NODE_FOR:
            printf("FOR\n");
            if (node->data.forStmt.init) {
                printAST(node->data.forStmt.init, level + 1);
            }
            printAST(node->data.forStmt.condition, level + 1);
            if (node->data.forStmt.update) {
                printAST(node->data.forStmt.update, level + 1);
            }
            printAST(node->data.forStmt.body, level + 1);
            break;
        case NODE_IF:
            printf("IF\n");
            printAST(node->data.ifStmt.condition, level + 1);
            printAST(node->data.ifStmt.thenBranch, level + 1);
            if (node->data.ifStmt.elseBranch) {
                for (int i = 0; i < level + 1; i++) printf("  ");
                printf("ELSE\n");
                printAST(node->data.ifStmt.elseBranch, level + 2);
            }
            break;
        case NODE_SWITCH:
            printf("SWITCH\n");
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("EXPR:\n");
            printAST(node->data.switch_stmt.expr, level + 2);
            for (int i = 0; i < level + 1; i++) printf("  ");
            printf("CASES:\n");
            printAST(node->data.switch_stmt.cases, level + 2);
            break;
        case NODE_CASE: {
            ASTNode* c = node;
            while (c) {
                for (int i = 0; i < level; i++) printf("  ");
                if (c->data.case_clause.isDefault) {
                    printf("DEFAULT:\n");
                } else {
                    printf("CASE %d:\n", c->data.case_clause.value);
                }

                if (c->data.case_clause.body) {
                    printAST(c->data.case_clause.body, level + 1);
                } else {
                    for (int i = 0; i < level + 1; i++) printf("  ");
                    printf("(empty - fall-through)\n");
                }
                c = c->data.case_clause.next;
            }
            break;
        }
        case NODE_BREAK:
            printf("BREAK\n");
            break;
        case NODE_STMT_LIST:
            /* Print statements in sequence at same level */
            printAST(node->data.stmtlist.stmt, level);
            printAST(node->data.stmtlist.next, level);
            break;
        case NODE_FUNCTION_LIST:
            printAST(node->data.functionlist.function, level);
            printAST(node->data.functionlist.next, level);
            break;
        case NODE_FUNCTION:
            printf("FUNCTION: %s %s(", node->data.function.returnType, node->data.function.name);
            printParams(node->data.function.params);
            printAST(node->data.function.body, level + 1);
            break;
        case NODE_BLOCK:
            printf("BLOCK\n");
            printAST(node->data.blockStmts, level + 1);
            break;
        case NODE_RETURN:
            printf("RETURN\n");
            printAST(node->data.returnExpr, level + 1);
            break;
        case NODE_FUNCTION_CALL:
            printf("CALL: %s\n", node->data.functioncall.name);
            printAST(node->data.functioncall.args, level + 1);
            break;
        case NODE_ARG_LIST:
            printAST(node->data.arglist.expr, level);
            printAST(node->data.arglist.next, level);
            break;
        case NODE_TYPED_DECL:
            printf("TYPED_DECL: %s %s%s", node->data.typedDecl.varType, node->data.typedDecl.varName,
                   node->data.typedDecl.isArray ? "[]" : "");
            if (node->data.typedDecl.isArray) {
                printf(" size %d", node->data.typedDecl.arraySize);
            }
            printf("\n");
            break;
    }
}
