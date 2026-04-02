/* SEMANTIC ANALYZER IMPLEMENTATION
 * Checks the AST for semantic errors before code generation
 * This ensures the program makes sense semantically, even if syntactically correct
 * NOW WITH TYPE CHECKING!
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "symtab.h"

/* Track number of semantic errors found */
int semanticErrors = 0;
int semanticWarnings = 0;

typedef struct FunctionInfo {
    char* name;
    VarType returnType;
    ParamList* params;
    struct FunctionInfo* next;
} FunctionInfo;

static FunctionInfo* functionTable = NULL;
static int breakDepth = 0;

/* Initialize semantic analyzer */
void initSemantic() {
    semanticErrors = 0;
    semanticWarnings = 0;
    functionTable = NULL;
    breakDepth = 0;
    printf("SEMANTIC ANALYZER: Initialized\n\n");
}

/* Report a semantic error */
void reportSemanticError(const char* msg) {
    printf("✗ SEMANTIC ERROR: %s\n", msg);
    semanticErrors++;
}

/* Report a semantic warning (does not fail compilation) */
void reportSemanticWarning(const char* msg) {
    printf("⚠ SEMANTIC WARNING: %s\n", msg);
    semanticWarnings++;
}

static VarType stringToType(const char* typeStr) {
    if (!typeStr) return TYPE_UNKNOWN;
    if (strcmp(typeStr, "int") == 0) return TYPE_INT;
    if (strcmp(typeStr, "float") == 0) return TYPE_FLOAT;
    if (strcmp(typeStr, "char") == 0) return TYPE_CHAR;
    if (strcmp(typeStr, "string") == 0) return TYPE_STRING;
    return TYPE_UNKNOWN;
}

static int isBuiltinOutputString(const char* name) {
    return name && strcmp(name, "output_string") == 0;
}

static int isBuiltinStringConcat(const char* name) {
    return name && strcmp(name, "string_concat") == 0;
}

static int isBuiltinStringEqual(const char* name) {
    return name && strcmp(name, "string_equal") == 0;
}

static VarType getBuiltinReturnType(const char* name) {
    if (isBuiltinStringConcat(name)) return TYPE_STRING;
    if (isBuiltinStringEqual(name)) return TYPE_INT;
    if (isBuiltinOutputString(name)) return TYPE_UNKNOWN; /* treated like void */
    return TYPE_UNKNOWN;
}

static FunctionInfo* findFunction(const char* name) {
    FunctionInfo* curr = functionTable;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

static void registerFunction(ASTNode* func) {
    if (!func || func->type != NODE_FUNCTION) return;
    if (findFunction(func->data.function.name)) {
        char errorMsg[256];
        sprintf(errorMsg, "Function '%s' already declared", func->data.function.name);
        reportSemanticError(errorMsg);
        return;
    }
    FunctionInfo* info = malloc(sizeof(FunctionInfo));
    info->name = strdup(func->data.function.name);
    info->returnType = stringToType(func->data.function.returnType);
    info->params = func->data.function.params;
    info->next = functionTable;
    functionTable = info;
}

static void registerFunctions(ASTNode* root) {
    if (!root) return;
    switch(root->type) {
        case NODE_FUNCTION:
            registerFunction(root);
            break;
        case NODE_FUNCTION_LIST:
            registerFunctions(root->data.functionlist.function);
            registerFunctions(root->data.functionlist.next);
            break;
        default:
            break;
    }
}

static int countArgs(ASTNode* argList) {
    int count = 0;
    ASTNode* curr = argList;
    while (curr) {
        count++;
        curr = curr->data.arglist.next;
    }
    return count;
}

static int countParams(ParamList* params) {
    int count = 0;
    ParamList* curr = params;
    while (curr) {
        count++;
        curr = curr->next;
    }
    return count;
}

/* INFER THE TYPE OF AN EXPRESSION
 * Returns the type that an expression evaluates to.
 */
VarType inferExprType(ASTNode* node) {
    if (!node) return TYPE_UNKNOWN;

    switch(node->type) {
        case NODE_NUM:
            /* Integer literals have type INT */
            printf("  → Expression is integer literal (type: int)\n");
            return TYPE_INT;

        case NODE_FLOAT:
            printf("  → Expression is float literal (type: float)\n");
            return TYPE_FLOAT;

        case NODE_STRING_LIT:
            printf("  → Expression is string literal (type: string)\n");
            return TYPE_STRING;

        case NODE_VAR: {
            /* Variable's type comes from symbol table */
            if (!isVarDeclared(node->data.name)) {
                /* This error is caught elsewhere, but return UNKNOWN */
                return TYPE_UNKNOWN;
            }
            if (isVarArray(node->data.name)) {
                reportSemanticError("Array used without index");
                return TYPE_UNKNOWN;
            }
            VarType varType = getVarType(node->data.name);
            printf("  → Variable '%s' has type: %s\n", 
                   node->data.name, typeToString(varType));
            return varType;
        }

        case NODE_ARRAY_ACCESS: {
            if (!isVarDeclared(node->data.arrayAccess.name)) {
                return TYPE_UNKNOWN;
            }
            if (!isVarArray(node->data.arrayAccess.name)) {
                reportSemanticError("Indexing non-array variable");
                return TYPE_UNKNOWN;
            }

            /* Optional bounds check when size and index are compile-time constants */
            int size = getArraySize(node->data.arrayAccess.name);
            if (size > 0 &&
                node->data.arrayAccess.index &&
                node->data.arrayAccess.index->type == NODE_NUM) {
                int idx = node->data.arrayAccess.index->data.num;
                if (idx < 0 || idx >= size) {
                    char errorMsg[256];
                    sprintf(errorMsg, "Array index out of bounds: %s[%d] (size %d)",
                            node->data.arrayAccess.name, idx, size);
                    reportSemanticError(errorMsg);
                }
            }

            VarType indexType = inferExprType(node->data.arrayAccess.index);
            if (indexType != TYPE_INT && indexType != TYPE_UNKNOWN) {
                reportSemanticError("Array index must be int");
            }
            return getVarType(node->data.arrayAccess.name);
        }

        case NODE_FUNCTION_CALL: {
            VarType builtinRet = getBuiltinReturnType(node->data.functioncall.name);
            if (builtinRet != TYPE_UNKNOWN || isBuiltinOutputString(node->data.functioncall.name)) {
                return builtinRet;
            }
            FunctionInfo* func = findFunction(node->data.functioncall.name);
            if (!func) {
                char errorMsg[256];
                sprintf(errorMsg, "Call to undeclared function '%s'", node->data.functioncall.name);
                reportSemanticError(errorMsg);
                return TYPE_UNKNOWN;
            }
            return func->returnType;
        }

        case NODE_COMPARE: {
            printf("  → Analyzing comparison operands:\n");
            VarType leftType = inferExprType(node->data.compare.left);
            VarType rightType = inferExprType(node->data.compare.right);

            if (leftType != rightType &&
                leftType != TYPE_UNKNOWN &&
                rightType != TYPE_UNKNOWN) {
                char errorMsg[256];
                sprintf(errorMsg, "Type mismatch in comparison: %s vs %s",
                        typeToString(leftType), typeToString(rightType));
                reportSemanticError(errorMsg);
            }

            /* Comparisons currently supported only on integers */
            if (leftType != TYPE_INT && leftType != TYPE_UNKNOWN) {
                reportSemanticError("Comparison operands must be int");
            }

            return TYPE_INT;
        }

        case NODE_BINOP: {
            /* For binary operations, check both operands */
            printf("  → Analyzing binary operation operands:\n");
            VarType leftType = inferExprType(node->data.binop.left);
            VarType rightType = inferExprType(node->data.binop.right);

            if (leftType == TYPE_STRING || rightType == TYPE_STRING) {
                reportSemanticError("String values are not valid operands for arithmetic operators; use string_concat()");
                return TYPE_UNKNOWN;
            }
            
            /* Both operands should have the same type */
            if (leftType != rightType && 
                leftType != TYPE_UNKNOWN && 
                rightType != TYPE_UNKNOWN) {
                char errorMsg[256];
                sprintf(errorMsg, "Type mismatch in binary operation: %s %c %s",
                        typeToString(leftType),
                        node->data.binop.op,
                        typeToString(rightType));
                reportSemanticError(errorMsg);
                return TYPE_UNKNOWN;
            }
            
            /* Result type is the same as operands (for now) */
            printf("  → Binary operation result type: %s\n", 
                   typeToString(leftType));
            return leftType;
        }

        default:
            return TYPE_UNKNOWN;
    }
}

/* Analyze an expression for semantic correctness */
void analyzeExpr(ASTNode* node) {
    if (!node) return;

    switch(node->type) {
        case NODE_NUM:
            /* Numbers are always valid */
            break;

        case NODE_FLOAT:
        case NODE_STRING_LIT:
            break;

        case NODE_VAR: {
            /* Check if variable has been declared */
            if (!isVarDeclared(node->data.name)) {
                char errorMsg[256];
                sprintf(errorMsg, "Variable '%s' used before declaration", node->data.name);
                reportSemanticError(errorMsg);
            } else {
                if (isVarArray(node->data.name)) {
                    char errorMsg[256];
                    sprintf(errorMsg, "Array '%s' used without index", node->data.name);
                    reportSemanticError(errorMsg);
                }
                printf("  ✓ Variable '%s' is declared\n", node->data.name);
            }
            break;
        }

        case NODE_ARRAY_ACCESS:
            if (!isVarDeclared(node->data.arrayAccess.name)) {
                char errorMsg[256];
                sprintf(errorMsg, "Array '%s' used before declaration", node->data.arrayAccess.name);
                reportSemanticError(errorMsg);
            } else if (!isVarArray(node->data.arrayAccess.name)) {
                reportSemanticError("Indexing non-array variable");
            } else {
                printf("  ✓ Array '%s' is declared\n", node->data.arrayAccess.name);

                int size = getArraySize(node->data.arrayAccess.name);
                if (size > 0 &&
                    node->data.arrayAccess.index &&
                    node->data.arrayAccess.index->type == NODE_NUM) {
                    int idx = node->data.arrayAccess.index->data.num;
                    if (idx < 0 || idx >= size) {
                        char errorMsg[256];
                        sprintf(errorMsg, "Array index out of bounds: %s[%d] (size %d)",
                                node->data.arrayAccess.name, idx, size);
                        reportSemanticError(errorMsg);
                    }
                }
            }
            analyzeExpr(node->data.arrayAccess.index);
            break;

        case NODE_FUNCTION_CALL: {
            ASTNode* argWalk = node->data.functioncall.args;
            while (argWalk) {
                analyzeExpr(argWalk->data.arglist.expr);
                argWalk = argWalk->data.arglist.next;
            }

            if (isBuiltinOutputString(node->data.functioncall.name)) {
                int argCount = countArgs(node->data.functioncall.args);
                if (argCount != 1) {
                    reportSemanticError("output_string expects exactly 1 argument");
                } else {
                    VarType t = inferExprType(node->data.functioncall.args->data.arglist.expr);
                    if (t != TYPE_STRING && t != TYPE_UNKNOWN) {
                        reportSemanticError("output_string expects a string argument");
                    }
                }
                break;
            }

            if (isBuiltinStringConcat(node->data.functioncall.name) ||
                isBuiltinStringEqual(node->data.functioncall.name)) {
                int argCount = countArgs(node->data.functioncall.args);
                if (argCount != 2) {
                    reportSemanticError("string_concat/string_equal expect exactly 2 arguments");
                } else {
                    ASTNode* leftArg = node->data.functioncall.args->data.arglist.expr;
                    ASTNode* rightArg = node->data.functioncall.args->data.arglist.next->data.arglist.expr;
                    VarType leftType = inferExprType(leftArg);
                    VarType rightType = inferExprType(rightArg);
                    if (leftType != TYPE_STRING && leftType != TYPE_UNKNOWN) {
                        reportSemanticError("First argument must be string");
                    }
                    if (rightType != TYPE_STRING && rightType != TYPE_UNKNOWN) {
                        reportSemanticError("Second argument must be string");
                    }
                }
                break;
            }

            FunctionInfo* func = findFunction(node->data.functioncall.name);
            if (!func) {
                char errorMsg[256];
                sprintf(errorMsg, "Call to undeclared function '%s'", node->data.functioncall.name);
                reportSemanticError(errorMsg);
                break;
            }
            int paramCount = countParams(func->params);
            int argCount = countArgs(node->data.functioncall.args);
            if (paramCount != argCount) {
                char errorMsg[256];
                sprintf(errorMsg, "Function '%s' expects %d args, got %d",
                        func->name, paramCount, argCount);
                reportSemanticError(errorMsg);
            }
            ParamList* p = func->params;
            ASTNode* a = node->data.functioncall.args;
            while (p && a) {
                VarType expected = stringToType(p->type);
                if (p->isArray) {
                    if (a->data.arglist.expr->type != NODE_VAR) {
                        reportSemanticError("Array parameter expects array variable");
                    } else {
                        char* argName = a->data.arglist.expr->data.name;
                        if (!isVarDeclared(argName) || !isVarArray(argName)) {
                            reportSemanticError("Array parameter expects array variable");
                        } else if (getVarType(argName) != expected) {
                            reportSemanticError("Array parameter type mismatch");
                        }
                    }
                } else {
                    VarType argType = inferExprType(a->data.arglist.expr);
                    if (argType != expected && argType != TYPE_UNKNOWN) {
                        reportSemanticError("Function argument type mismatch");
                    }
                }
                p = p->next;
                a = a->data.arglist.next;
            }
            break;
        }

        case NODE_COMPARE:
            analyzeExpr(node->data.compare.left);
            analyzeExpr(node->data.compare.right);
            break;

        case NODE_BINOP:
            /* Division-by-zero check (compile-time only for literal zeros) */
            if (node->data.binop.op == '/' && node->data.binop.right) {
                if (node->data.binop.right->type == NODE_NUM &&
                    node->data.binop.right->data.num == 0) {
                    reportSemanticError("Division by zero (integer literal 0)");
                } else if (node->data.binop.right->type == NODE_FLOAT &&
                           node->data.binop.right->data.fnum == 0.0) {
                    reportSemanticError("Division by zero (float literal 0.0)");
                }
            }

            /* Recursively analyze both operands */
            analyzeExpr(node->data.binop.left);
            analyzeExpr(node->data.binop.right);
            break;

        default:
            break;
    }
}

/* Analyze a statement for semantic correctness */
void analyzeStmt(ASTNode* node) {
    if (!node) return;

    switch(node->type) {
        case NODE_DECL: {
            /* Check if variable is already declared */
            if (isVarDeclared(node->data.name)) {
                char errorMsg[256];
                sprintf(errorMsg, "Variable '%s' already declared", node->data.name);
                reportSemanticError(errorMsg);
            } else {
                /* SINCE YOUR PARSER ONLY HANDLES "int" FOR NOW,
                 * We always declare variables as TYPE_INT
                 * When you add float/char support to your parser,
                 * you'll need to modify your AST to store the type
                 * and pass it here instead of hardcoding TYPE_INT */
                
                VarType declaredType = TYPE_INT;  /* All declarations are int for now */
                
                /* Add variable to symbol table WITH TYPE */
                int offset = addVar(node->data.name, declaredType, 0, 0);
                if (offset != -1) {
                    printf("  ✓ Variable '%s' (type: %s) declared successfully\n", 
                           node->data.name, typeToString(declaredType));
                }
            }
            break;
        }

        case NODE_TYPED_DECL: {
            if (isVarDeclared(node->data.typedDecl.varName)) {
                char errorMsg[256];
                sprintf(errorMsg, "Variable '%s' already declared", node->data.typedDecl.varName);
                reportSemanticError(errorMsg);
            } else {
                if (node->data.typedDecl.isArray && node->data.typedDecl.arraySize <= 0) {
                    char errorMsg[256];
                    sprintf(errorMsg, "Invalid array size for '%s': %d",
                            node->data.typedDecl.varName,
                            node->data.typedDecl.arraySize);
                    reportSemanticError(errorMsg);
                }
                VarType declaredType = stringToType(node->data.typedDecl.varType);
                int offset = addVar(node->data.typedDecl.varName,
                                    declaredType,
                                    node->data.typedDecl.isArray,
                                    node->data.typedDecl.arraySize);
                if (offset != -1) {
                    printf("  ✓ Variable '%s' (type: %s%s) declared successfully\n",
                           node->data.typedDecl.varName,
                           typeToString(declaredType),
                           node->data.typedDecl.isArray ? "[]" : "");
                }
            }
            break;
        }

        case NODE_ASSIGN: {
            /* STEP 1: Check if variable being assigned to has been declared */
            if (!isVarDeclared(node->data.assign.var)) {
                char errorMsg[256];
                sprintf(errorMsg, "Cannot assign to undeclared variable '%s'", 
                        node->data.assign.var);
                reportSemanticError(errorMsg);
            } else {
                printf("  ✓ Assignment to declared variable '%s'\n", 
                       node->data.assign.var);
                
                /* STEP 2: TYPE CHECKING - This is the new part! */
                printf("  → Checking type compatibility for assignment:\n");
                
                /* Get the type of the variable being assigned to */
                if (isVarArray(node->data.assign.var)) {
                    reportSemanticError("Cannot assign to array without index");
                    break;
                }
                VarType varType = getVarType(node->data.assign.var);
                
                /* Infer the type of the expression being assigned */
                VarType exprType = inferExprType(node->data.assign.value);
                
                /* Check if types match */
                if (varType != exprType && exprType != TYPE_UNKNOWN) {
                    char errorMsg[256];
                    sprintf(errorMsg, 
                            "Type mismatch: Cannot assign %s to variable '%s' of type %s",
                            typeToString(exprType),
                            node->data.assign.var,
                            typeToString(varType));
                    reportSemanticError(errorMsg);
                } else if (exprType != TYPE_UNKNOWN) {
                    printf("  ✓ Type check passed: %s = %s\n",
                           typeToString(varType), typeToString(exprType));
                }
            }
            
            /* Also check the expression for other semantic errors */
            analyzeExpr(node->data.assign.value);
            break;
        }

        case NODE_ARRAY_ASSIGN: {
            if (!isVarDeclared(node->data.arrayAssign.name)) {
                char errorMsg[256];
                sprintf(errorMsg, "Cannot assign to undeclared array '%s'", node->data.arrayAssign.name);
                reportSemanticError(errorMsg);
                break;
            }
            if (!isVarArray(node->data.arrayAssign.name)) {
                reportSemanticError("Index assignment on non-array variable");
                break;
            }

            int size = getArraySize(node->data.arrayAssign.name);
            if (size > 0 &&
                node->data.arrayAssign.index &&
                node->data.arrayAssign.index->type == NODE_NUM) {
                int idx = node->data.arrayAssign.index->data.num;
                if (idx < 0 || idx >= size) {
                    char errorMsg[256];
                    sprintf(errorMsg, "Array index out of bounds: %s[%d] (size %d)",
                            node->data.arrayAssign.name, idx, size);
                    reportSemanticError(errorMsg);
                }
            }

            VarType indexType = inferExprType(node->data.arrayAssign.index);
            if (indexType != TYPE_INT && indexType != TYPE_UNKNOWN) {
                reportSemanticError("Array index must be int");
            }
            VarType elemType = getVarType(node->data.arrayAssign.name);
            VarType valueType = inferExprType(node->data.arrayAssign.value);
            if (elemType != valueType && valueType != TYPE_UNKNOWN) {
                reportSemanticError("Type mismatch in array assignment");
            }
            analyzeExpr(node->data.arrayAssign.index);
            analyzeExpr(node->data.arrayAssign.value);
            break;
        }

        case NODE_PRINT:
            /* Check the expression being printed */
            printf("  → Checking print statement expression:\n");
            analyzeExpr(node->data.expr);
            /* Infer and report the type being printed */
            VarType printType = inferExprType(node->data.expr);
            if (printType == TYPE_STRING) {
                reportSemanticError("print() does not support string values; use output_string()");
            }
            printf("  → Printing expression of type: %s\n", typeToString(printType));
            break;

        case NODE_PRINTI:
            printf("  → Checking inline print statement expression:\n");
            analyzeExpr(node->data.expr);
            {
                VarType t = inferExprType(node->data.expr);
                if (t == TYPE_STRING) {
                    reportSemanticError("printi() does not support string values; use output_string()");
                }
                printf("  → Printing (inline) expression of type: %s\n", typeToString(t));
            }
            break;

        case NODE_PRINTC:
            printf("  → Checking printc statement expression:\n");
            analyzeExpr(node->data.expr);
            {
                VarType t = inferExprType(node->data.expr);
                if (t == TYPE_FLOAT) {
                    reportSemanticError("printc expects int/char (ASCII code), not float");
                }
                printf("  → Printing character from expression of type: %s\n", typeToString(t));
            }
            break;

        case NODE_WHILE: {
            printf("  → Checking while loop condition:\n");
            analyzeExpr(node->data.whileStmt.condition);
            VarType condType = inferExprType(node->data.whileStmt.condition);
            if (condType != TYPE_INT && condType != TYPE_UNKNOWN) {
                reportSemanticError("While condition must be int");
            }
            breakDepth++;
            analyzeStmt(node->data.whileStmt.body);
            breakDepth--;
            break;
        }

        case NODE_FOR: {
            printf("  → Checking for loop components:\n");

            /* Init */
            if (node->data.forStmt.init) {
                printf("    • for-init\n");
                analyzeStmt(node->data.forStmt.init);
            }

            /* Condition */
            if (!node->data.forStmt.condition) {
                reportSemanticWarning("For-loop has no condition (potentially infinite loop)");
            } else {
                printf("    • for-condition\n");
                analyzeExpr(node->data.forStmt.condition);

                if (node->data.forStmt.condition->type == NODE_NUM) {
                    if (node->data.forStmt.condition->data.num == 0) {
                        reportSemanticWarning("For-loop condition is constant 0 (dead loop)");
                    } else {
                        reportSemanticWarning("For-loop condition is constant non-zero (always true)");
                    }
                }

                VarType condType = inferExprType(node->data.forStmt.condition);
                if (condType != TYPE_INT && condType != TYPE_UNKNOWN) {
                    reportSemanticError("For-loop condition must be int");
                }
            }

            /* Body */
            if (!node->data.forStmt.body) {
                reportSemanticWarning("For-loop has empty body");
            } else {
                breakDepth++;
                analyzeStmt(node->data.forStmt.body);
                breakDepth--;
                if (node->data.forStmt.body->type == NODE_BLOCK &&
                    node->data.forStmt.body->data.blockStmts == NULL) {
                    reportSemanticWarning("For-loop body is an empty block");
                }
            }

            /* Update */
            if (node->data.forStmt.update) {
                printf("    • for-update\n");
                analyzeStmt(node->data.forStmt.update);
            }
            break;
        }

        case NODE_IF: {
            printf("  → Checking if statement condition:\n");
            analyzeExpr(node->data.ifStmt.condition);

            if (node->data.ifStmt.condition &&
                node->data.ifStmt.condition->type == NODE_NUM) {
                if (node->data.ifStmt.condition->data.num == 0) {
                    reportSemanticWarning("If condition is constant 0 (then-branch is dead)");
                } else {
                    reportSemanticWarning("If condition is constant non-zero (then-branch always taken)");
                }
            }

            VarType condType = inferExprType(node->data.ifStmt.condition);
            if (condType != TYPE_INT && condType != TYPE_UNKNOWN) {
                reportSemanticError("If condition must be int");
            }

            /* Then / else */
            analyzeStmt(node->data.ifStmt.thenBranch);
            if (node->data.ifStmt.elseBranch) {
                analyzeStmt(node->data.ifStmt.elseBranch);
            }
            break;
        }

        case NODE_SWITCH: {
            printf("  → Checking switch statement expression:\n");
            analyzeExpr(node->data.switch_stmt.expr);
            VarType switchType = inferExprType(node->data.switch_stmt.expr);
            if (switchType == TYPE_FLOAT) {
                reportSemanticError("Switch expression must be int/char (not float)");
            }

            /* Detect duplicate case values/default and validate case bodies. */
            int seenValues[512];
            int seenCount = 0;
            int hasDefault = 0;

            breakDepth++;
            ASTNode* clause = node->data.switch_stmt.cases;
            while (clause) {
                if (clause->data.case_clause.isDefault) {
                    if (hasDefault) {
                        reportSemanticError("Duplicate default clause in switch");
                    } else {
                        hasDefault = 1;
                    }
                } else {
                    int value = clause->data.case_clause.value;
                    int isDup = 0;
                    for (int i = 0; i < seenCount; i++) {
                        if (seenValues[i] == value) {
                            isDup = 1;
                            break;
                        }
                    }
                    if (isDup) {
                        char errorMsg[256];
                        sprintf(errorMsg, "Duplicate case value in switch: %d", value);
                        reportSemanticError(errorMsg);
                    } else if (seenCount < (int)(sizeof(seenValues) / sizeof(seenValues[0]))) {
                        seenValues[seenCount++] = value;
                    } else {
                        reportSemanticError("Too many case labels in switch");
                    }
                }

                analyzeStmt(clause->data.case_clause.body);
                clause = clause->data.case_clause.next;
            }
            breakDepth--;

            if (!hasDefault) {
                reportSemanticWarning("Switch statement has no default clause");
            }
            break;
        }

        case NODE_BREAK:
            if (breakDepth == 0) {
                reportSemanticError("break used outside loop/switch");
            }
            break;

        case NODE_RETURN:
            if (node->data.returnExpr) {
                analyzeExpr(node->data.returnExpr);
            }
            break;

        case NODE_BLOCK:
            analyzeStmt(node->data.blockStmts);
            break;

        case NODE_FUNCTION_CALL:
            analyzeExpr(node);
            break;

        case NODE_FUNCTION: {
            /* New scope for each function */
            initSymTab();
            ParamList* param = node->data.function.params;
            while (param) {
                VarType paramType = stringToType(param->type);
                /* Array parameters are passed by reference; we don't track a concrete size here. */
                addVar(param->name, paramType, param->isArray, 0);
                param = param->next;
            }
            analyzeStmt(node->data.function.body);
            break;
        }

        case NODE_FUNCTION_LIST:
            analyzeStmt(node->data.functionlist.function);
            analyzeStmt(node->data.functionlist.next);
            break;

        case NODE_STMT_LIST:
            /* Recursively analyze all statements */
            analyzeStmt(node->data.stmtlist.stmt);
            analyzeStmt(node->data.stmtlist.next);
            break;

        default:
            break;
    }
}

/* Analyze the entire program */
int analyzeProgram(ASTNode* root) {
    printf("Starting semantic analysis...\n");
    printf("───────────────────────────────\n");

    /* Initialize symbol table for semantic checking */
    initSymTab();
    registerFunctions(root);

    /* Analyze all statements */
    analyzeStmt(root);

    printf("───────────────────────────────\n");

    /* Report results */
    if (semanticErrors == 0) {
        printf("✓ Semantic analysis passed - no errors found!\n");
        if (semanticWarnings > 0) {
            printf("⚠ Semantic analysis produced %d warning(s)\n", semanticWarnings);
        }
        return 1;  /* Success */
    }

    printf("✗ Semantic analysis failed with %d error(s)\n", semanticErrors);
    if (semanticWarnings > 0) {
        printf("⚠ Semantic analysis also produced %d warning(s)\n", semanticWarnings);
    }
    return 0;  /* Failure */
}
