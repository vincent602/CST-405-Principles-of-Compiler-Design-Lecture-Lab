#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "symtab.h"

/* ============================================================
 * MIPS CODE GENERATION
 * ============================================================
 *
 * Now supports real function labels/calls/returns using a simple
 * stack-based calling convention:
 * - Caller pushes args right-to-left (so param i is at 8+4*i($fp))
 * - Callee prologue saves $fp/$ra, establishes $fp, allocates locals
 * - Return value in $v0 (int/char) or $f0 (float)
 *
 * Arrays:
 * - Local arrays live in the stack frame at a negative offset from $fp
 * - Array parameters are passed as pointers (stored at positive offsets)
 */

FILE* output;

/* Simple register pool to avoid using non-existent registers like $t10. */
static int intRegPool[10];
static int intRegPoolTop = 0;
/* Use $f2-$f9 as temporaries (avoid $f0 return reg and $f12 print arg reg). */
static int floatRegPool[8];
static int floatRegPoolTop = 0;

typedef struct {
    VarType type;
    int reg; /* int reg index ($tN) if int-ish, float reg index ($fN) if float */
} ExprResult;

typedef struct {
    int isFloat; /* 0: $tN, 1: $fN */
    int reg;
} SavedReg;

typedef struct FunctionInfo {
    char* name;
    VarType returnType;
    ParamList* params;
    struct FunctionInfo* next;
} FunctionInfo;

typedef struct EmittedFuncName {
    char* name;
    struct EmittedFuncName* next;
} EmittedFuncName;

typedef struct StringLiteralInfo {
    char* literal;
    char* label;
    struct StringLiteralInfo* next;
} StringLiteralInfo;

static FunctionInfo* functionTable = NULL;
static EmittedFuncName* emittedFunctions = NULL;
static StringLiteralInfo* stringLiterals = NULL;
static int nextStringLabel = 0;
static char currentFuncEndLabel[128];
static VarType currentFuncReturnType = TYPE_UNKNOWN;
static const char* currentFuncName = NULL;
static int localLabelCounter = 0;
#define CODEGEN_BREAK_STACK_MAX 128
static char breakTargetStack[CODEGEN_BREAK_STACK_MAX][128];
static int breakTargetTop = 0;

static void resetBreakTargets(void) {
    breakTargetTop = 0;
}

static void pushBreakTarget(const char* label) {
    if (!label) return;
    if (breakTargetTop >= CODEGEN_BREAK_STACK_MAX) {
        fprintf(stderr, "Error: codegen break-target stack overflow\n");
        exit(1);
    }
    snprintf(breakTargetStack[breakTargetTop], sizeof(breakTargetStack[breakTargetTop]), "%s", label);
    breakTargetTop++;
}

static void popBreakTarget(void) {
    if (breakTargetTop > 0) {
        breakTargetTop--;
    }
}

static const char* currentBreakTarget(void) {
    if (breakTargetTop <= 0) return NULL;
    return breakTargetStack[breakTargetTop - 1];
}

static void formatLocalLabel(char* buf, size_t bufsize, const char* stem, int id) {
    const char* fname = currentFuncName ? currentFuncName : "func";
    snprintf(buf, bufsize, "%s__%s_%d", fname, stem, id);
}

static void initRegPools() {
    intRegPoolTop = 0;
    for (int i = 9; i >= 0; i--) intRegPool[intRegPoolTop++] = i; /* $t0-$t9 */
    floatRegPoolTop = 0;
    for (int i = 9; i >= 2; i--) floatRegPool[floatRegPoolTop++] = i; /* $f2-$f9 */
}

static int allocIntReg() {
    if (intRegPoolTop <= 0) {
        fprintf(stderr, "Error: ran out of integer registers\n");
        exit(1);
    }
    return intRegPool[--intRegPoolTop];
}

static void freeIntReg(int r) {
    if (r < 0 || r > 9) return;
    if (intRegPoolTop >= (int)(sizeof(intRegPool) / sizeof(intRegPool[0]))) return;
    intRegPool[intRegPoolTop++] = r;
}

static int allocFloatReg() {
    if (floatRegPoolTop <= 0) {
        fprintf(stderr, "Error: ran out of float registers\n");
        exit(1);
    }
    return floatRegPool[--floatRegPoolTop];
}

static void freeFloatReg(int r) {
    if (r < 2 || r > 9) return;
    if (floatRegPoolTop >= (int)(sizeof(floatRegPool) / sizeof(floatRegPool[0]))) return;
    floatRegPool[floatRegPoolTop++] = r;
}

static VarType stringToType(const char* typeStr) {
    if (!typeStr) return TYPE_UNKNOWN;
    if (strcmp(typeStr, "int") == 0) return TYPE_INT;
    if (strcmp(typeStr, "float") == 0) return TYPE_FLOAT;
    if (strcmp(typeStr, "char") == 0) return TYPE_CHAR;
    if (strcmp(typeStr, "string") == 0) return TYPE_STRING;
    return TYPE_UNKNOWN;
}

static FunctionInfo* findFunctionInfo(const char* name) {
    FunctionInfo* curr = functionTable;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

static int hasEmittedFunction(const char* name) {
    EmittedFuncName* curr = emittedFunctions;
    while (curr) {
        if (strcmp(curr->name, name) == 0) return 1;
        curr = curr->next;
    }
    return 0;
}

static void markFunctionEmitted(const char* name) {
    EmittedFuncName* entry = malloc(sizeof(EmittedFuncName));
    entry->name = strdup(name);
    entry->next = emittedFunctions;
    emittedFunctions = entry;
}

static void registerFunction(ASTNode* func) {
    if (!func || func->type != NODE_FUNCTION) return;
    if (findFunctionInfo(func->data.function.name)) return;

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

static int isBuiltinOutputString(const char* name) {
    return name && strcmp(name, "output_string") == 0;
}

static int isBuiltinStringConcat(const char* name) {
    return name && strcmp(name, "string_concat") == 0;
}

static int isBuiltinStringEqual(const char* name) {
    return name && strcmp(name, "string_equal") == 0;
}

static const char* getOrCreateStringLabel(const char* literal) {
    StringLiteralInfo* curr = stringLiterals;
    while (curr) {
        if (strcmp(curr->literal, literal) == 0) return curr->label;
        curr = curr->next;
    }

    StringLiteralInfo* entry = malloc(sizeof(StringLiteralInfo));
    char labelBuf[32];
    snprintf(labelBuf, sizeof(labelBuf), "_str%d", nextStringLabel++);
    entry->literal = strdup(literal);
    entry->label = strdup(labelBuf);
    entry->next = stringLiterals;
    stringLiterals = entry;
    return entry->label;
}

static void collectStringLiterals(ASTNode* node) {
    if (!node) return;

    switch (node->type) {
        case NODE_STRING_LIT:
            getOrCreateStringLabel(node->data.strlit);
            break;

        case NODE_BINOP:
            collectStringLiterals(node->data.binop.left);
            collectStringLiterals(node->data.binop.right);
            break;

        case NODE_COMPARE:
            collectStringLiterals(node->data.compare.left);
            collectStringLiterals(node->data.compare.right);
            break;

        case NODE_ASSIGN:
            collectStringLiterals(node->data.assign.value);
            break;

        case NODE_ARRAY_ASSIGN:
            collectStringLiterals(node->data.arrayAssign.index);
            collectStringLiterals(node->data.arrayAssign.value);
            break;

        case NODE_ARRAY_ACCESS:
            collectStringLiterals(node->data.arrayAccess.index);
            break;

        case NODE_PRINT:
        case NODE_PRINTI:
        case NODE_PRINTC:
            collectStringLiterals(node->data.expr);
            break;

        case NODE_WHILE:
            collectStringLiterals(node->data.whileStmt.condition);
            collectStringLiterals(node->data.whileStmt.body);
            break;

        case NODE_FOR:
            collectStringLiterals(node->data.forStmt.init);
            collectStringLiterals(node->data.forStmt.condition);
            collectStringLiterals(node->data.forStmt.update);
            collectStringLiterals(node->data.forStmt.body);
            break;

        case NODE_IF:
            collectStringLiterals(node->data.ifStmt.condition);
            collectStringLiterals(node->data.ifStmt.thenBranch);
            collectStringLiterals(node->data.ifStmt.elseBranch);
            break;

        case NODE_SWITCH: {
            collectStringLiterals(node->data.switch_stmt.expr);
            ASTNode* clause = node->data.switch_stmt.cases;
            while (clause) {
                collectStringLiterals(clause->data.case_clause.body);
                clause = clause->data.case_clause.next;
            }
            break;
        }

        case NODE_STMT_LIST:
            collectStringLiterals(node->data.stmtlist.stmt);
            collectStringLiterals(node->data.stmtlist.next);
            break;

        case NODE_BLOCK:
            collectStringLiterals(node->data.blockStmts);
            break;

        case NODE_FUNCTION:
            collectStringLiterals(node->data.function.body);
            break;

        case NODE_FUNCTION_LIST:
            collectStringLiterals(node->data.functionlist.function);
            collectStringLiterals(node->data.functionlist.next);
            break;

        case NODE_FUNCTION_CALL:
            collectStringLiterals(node->data.functioncall.args);
            break;

        case NODE_ARG_LIST:
            collectStringLiterals(node->data.arglist.expr);
            collectStringLiterals(node->data.arglist.next);
            break;

        case NODE_RETURN:
            collectStringLiterals(node->data.returnExpr);
            break;

        default:
            break;
    }
}

static void emitStringDataSection(void) {
    for (StringLiteralInfo* curr = stringLiterals; curr; curr = curr->next) {
        fprintf(output, "%s: .asciiz \"%s\"\n", curr->label, curr->literal);
    }
}

static void emitRuntimeStringHelpers(void) {
    fprintf(output, "_string_concat:\n");
    fprintf(output, "    lw $t0, _heap_ptr\n");
    fprintf(output, "    move $v0, $t0\n");
    fprintf(output, "    move $t1, $a0\n");
    fprintf(output, "_string_concat_copy_a:\n");
    fprintf(output, "    lb $t2, 0($t1)\n");
    fprintf(output, "    beqz $t2, _string_concat_copy_b_start\n");
    fprintf(output, "    sb $t2, 0($t0)\n");
    fprintf(output, "    addi $t0, $t0, 1\n");
    fprintf(output, "    addi $t1, $t1, 1\n");
    fprintf(output, "    j _string_concat_copy_a\n");
    fprintf(output, "_string_concat_copy_b_start:\n");
    fprintf(output, "    move $t1, $a1\n");
    fprintf(output, "_string_concat_copy_b:\n");
    fprintf(output, "    lb $t2, 0($t1)\n");
    fprintf(output, "    beqz $t2, _string_concat_done\n");
    fprintf(output, "    sb $t2, 0($t0)\n");
    fprintf(output, "    addi $t0, $t0, 1\n");
    fprintf(output, "    addi $t1, $t1, 1\n");
    fprintf(output, "    j _string_concat_copy_b\n");
    fprintf(output, "_string_concat_done:\n");
    fprintf(output, "    sb $zero, 0($t0)\n");
    fprintf(output, "    addi $t0, $t0, 1\n");
    fprintf(output, "    sw $t0, _heap_ptr\n");
    fprintf(output, "    jr $ra\n\n");

    fprintf(output, "_string_equal:\n");
    fprintf(output, "    move $t0, $a0\n");
    fprintf(output, "    move $t1, $a1\n");
    fprintf(output, "_string_equal_loop:\n");
    fprintf(output, "    lb $t2, 0($t0)\n");
    fprintf(output, "    lb $t3, 0($t1)\n");
    fprintf(output, "    bne $t2, $t3, _string_equal_false\n");
    fprintf(output, "    beqz $t2, _string_equal_true\n");
    fprintf(output, "    addi $t0, $t0, 1\n");
    fprintf(output, "    addi $t1, $t1, 1\n");
    fprintf(output, "    j _string_equal_loop\n");
    fprintf(output, "_string_equal_false:\n");
    fprintf(output, "    li $v0, 0\n");
    fprintf(output, "    jr $ra\n");
    fprintf(output, "_string_equal_true:\n");
    fprintf(output, "    li $v0, 1\n");
    fprintf(output, "    jr $ra\n\n");
}

static int countArgs(ASTNode* args) {
    int n = 0;
    ASTNode* curr = args;
    while (curr) {
        n++;
        curr = curr->data.arglist.next;
    }
    return n;
}

static int countParams(ParamList* params) {
    int n = 0;
    ParamList* curr = params;
    while (curr) {
        n++;
        curr = curr->next;
    }
    return n;
}

static void collectDecls(ASTNode* node) {
    if (!node) return;
    switch(node->type) {
        case NODE_STMT_LIST:
            collectDecls(node->data.stmtlist.stmt);
            collectDecls(node->data.stmtlist.next);
            break;
        case NODE_BLOCK:
            collectDecls(node->data.blockStmts);
            break;
        case NODE_WHILE:
            collectDecls(node->data.whileStmt.body);
            break;
        case NODE_FOR:
            collectDecls(node->data.forStmt.init);
            collectDecls(node->data.forStmt.update);
            collectDecls(node->data.forStmt.body);
            break;
        case NODE_IF:
            collectDecls(node->data.ifStmt.thenBranch);
            collectDecls(node->data.ifStmt.elseBranch);
            break;
        case NODE_SWITCH: {
            ASTNode* c = node->data.switch_stmt.cases;
            while (c) {
                collectDecls(c->data.case_clause.body);
                c = c->data.case_clause.next;
            }
            break;
        }
        case NODE_TYPED_DECL: {
            VarType t = stringToType(node->data.typedDecl.varType);
            if (!isVarDeclared(node->data.typedDecl.varName)) {
                addVar(node->data.typedDecl.varName,
                       t,
                       node->data.typedDecl.isArray,
                       node->data.typedDecl.arraySize);
            }
            break;
        }
        case NODE_DECL:
            if (!isVarDeclared(node->data.name)) {
                addVar(node->data.name, TYPE_INT, 0, 0);
            }
            break;
        default:
            break;
    }
}

static ExprResult genExpr(ASTNode* node);
static void genStmt(ASTNode* node, int isMain);

static void emitPushIntReg(int treg) {
    fprintf(output, "    addi $sp, $sp, -4\n");
    fprintf(output, "    sw $t%d, 0($sp)\n", treg);
}

static void emitPushFloatReg(int freg) {
    fprintf(output, "    addi $sp, $sp, -4\n");
    fprintf(output, "    s.s $f%d, 0($sp)\n", freg);
}

static void emitPopIntReg(int treg) {
    fprintf(output, "    lw $t%d, 0($sp)\n", treg);
    fprintf(output, "    addi $sp, $sp, 4\n");
}

static void emitPopFloatReg(int freg) {
    fprintf(output, "    l.s $f%d, 0($sp)\n", freg);
    fprintf(output, "    addi $sp, $sp, 4\n");
}

static int saveCallerTemps(SavedReg* saved, int cap) {
    /* Our generated code uses $t0-$t9 and $f2-$f9 as temporaries in all functions.
     * Those are caller-saved in our calling convention, so preserve any currently
     * allocated temporaries across jal to keep expression evaluation correct. */
    int freeInt[10] = {0};
    for (int i = 0; i < intRegPoolTop; i++) {
        int r = intRegPool[i];
        if (r >= 0 && r <= 9) freeInt[r] = 1;
    }

    int freeFloat[10] = {0};
    for (int i = 0; i < floatRegPoolTop; i++) {
        int r = floatRegPool[i];
        if (r >= 2 && r <= 9) freeFloat[r] = 1;
    }

    int count = 0;
    for (int r = 0; r <= 9; r++) {
        if (!freeInt[r]) {
            if (count < cap) {
                saved[count].isFloat = 0;
                saved[count].reg = r;
            }
            emitPushIntReg(r);
            count++;
        }
    }
    for (int r = 2; r <= 9; r++) {
        if (!freeFloat[r]) {
            if (count < cap) {
                saved[count].isFloat = 1;
                saved[count].reg = r;
            }
            emitPushFloatReg(r);
            count++;
        }
    }

    if (count > cap) {
        fprintf(stderr, "Error: saveCallerTemps overflow (count=%d cap=%d)\n", count, cap);
        exit(1);
    }
    return count;
}

static void restoreCallerTemps(const SavedReg* saved, int count) {
    for (int i = count - 1; i >= 0; i--) {
        if (saved[i].isFloat) emitPopFloatReg(saved[i].reg);
        else emitPopIntReg(saved[i].reg);
    }
}

static int emitComputeArrayBaseAddress(const char* name) {
    /* Returns a $t register containing base address of the array. */
    int baseReg = allocIntReg();
    int offset = getVarOffset((char*)name);
    int size = getArraySize((char*)name);
    if (size == 0 && offset > 0) {
        /* Array parameter: slot contains pointer */
        fprintf(output, "    lw $t%d, %d($fp)\n", baseReg, offset);
    } else {
        /* Local array: base address is $fp + offset (offset is negative) */
        fprintf(output, "    addi $t%d, $fp, %d\n", baseReg, offset);
    }
    return baseReg;
}

static ExprResult genExpr(ASTNode* node) {
    ExprResult result = { .type = TYPE_UNKNOWN, .reg = -1 };
    if (!node) return result;

    switch(node->type) {
        case NODE_NUM: {
            int reg = allocIntReg();
            fprintf(output, "    li $t%d, %d\n", reg, node->data.num);
            result.type = TYPE_INT;
            result.reg = reg;
            break;
        }
        case NODE_FLOAT: {
            int reg = allocFloatReg();
            /* NOTE: 'li.s' is a pseudo-instruction supported by common MIPS assemblers/simulators. */
            fprintf(output, "    li.s $f%d, %f\n", reg, node->data.fnum);
            result.type = TYPE_FLOAT;
            result.reg = reg;
            break;
        }
        case NODE_STRING_LIT: {
            int reg = allocIntReg();
            const char* label = getOrCreateStringLabel(node->data.strlit);
            fprintf(output, "    la $t%d, %s\n", reg, label);
            result.type = TYPE_STRING;
            result.reg = reg;
            break;
        }
        case NODE_VAR: {
            int offset = getVarOffset(node->data.name);
            if (offset == -1) {
                fprintf(stderr, "Error: Variable %s not declared\n", node->data.name);
                exit(1);
            }
            VarType varType = getVarType(node->data.name);
            if (varType == TYPE_FLOAT) {
                int reg = allocFloatReg();
                fprintf(output, "    l.s $f%d, %d($fp)\n", reg, offset);
                result.type = TYPE_FLOAT;
                result.reg = reg;
            } else {
                int reg = allocIntReg();
                fprintf(output, "    lw $t%d, %d($fp)\n", reg, offset);
                result.type = varType;
                result.reg = reg;
            }
            break;
        }
        case NODE_ARRAY_ACCESS: {
            VarType elemType = getVarType(node->data.arrayAccess.name);

            int baseReg = emitComputeArrayBaseAddress(node->data.arrayAccess.name);
            ExprResult index = genExpr(node->data.arrayAccess.index);
            int idxReg = index.reg;

            /* idxReg = idxReg * 4 */
            fprintf(output, "    sll $t%d, $t%d, 2\n", idxReg, idxReg);
            fprintf(output, "    add $t%d, $t%d, $t%d\n", baseReg, baseReg, idxReg);

            if (elemType == TYPE_FLOAT) {
                int reg = allocFloatReg();
                fprintf(output, "    l.s $f%d, 0($t%d)\n", reg, baseReg);
                result.type = TYPE_FLOAT;
                result.reg = reg;
            } else {
                int reg = allocIntReg();
                fprintf(output, "    lw $t%d, 0($t%d)\n", reg, baseReg);
                result.type = elemType;
                result.reg = reg;
            }

            /* baseReg/idxReg are temporaries used only for addressing */
            freeIntReg(baseReg);
            freeIntReg(idxReg);
            break;
        }
        case NODE_BINOP: {
            ExprResult left = genExpr(node->data.binop.left);
            ExprResult right = genExpr(node->data.binop.right);

            if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                int reg = left.reg;
                switch(node->data.binop.op) {
                    case '+':
                        fprintf(output, "    add.s $f%d, $f%d, $f%d\n", reg, left.reg, right.reg);
                        break;
                    case '-':
                        fprintf(output, "    sub.s $f%d, $f%d, $f%d\n", reg, left.reg, right.reg);
                        break;
                    case '*':
                        fprintf(output, "    mul.s $f%d, $f%d, $f%d\n", reg, left.reg, right.reg);
                        break;
                    case '/':
                        fprintf(output, "    div.s $f%d, $f%d, $f%d\n", reg, left.reg, right.reg);
                        break;
                    default:
                        break;
                }
                result.type = TYPE_FLOAT;
                result.reg = reg;
                freeFloatReg(right.reg);
            } else {
                int reg = left.reg;
                switch(node->data.binop.op) {
                    case '+':
                        fprintf(output, "    add $t%d, $t%d, $t%d\n", reg, left.reg, right.reg);
                        break;
                    case '-':
                        fprintf(output, "    sub $t%d, $t%d, $t%d\n", reg, left.reg, right.reg);
                        break;
                    case '*':
                        fprintf(output, "    mul $t%d, $t%d, $t%d\n", reg, left.reg, right.reg);
                        break;
                    case '/':
                        fprintf(output, "    div $t%d, $t%d\n", left.reg, right.reg);
                        fprintf(output, "    mflo $t%d\n", reg);
                        break;
                    default:
                        break;
                }
                result.type = TYPE_INT;
                result.reg = reg;
                freeIntReg(right.reg);
            }
            break;
        }
        case NODE_COMPARE: {
            ExprResult left = genExpr(node->data.compare.left);
            ExprResult right = genExpr(node->data.compare.right);

            if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                fprintf(stderr, "Error: float comparisons are not supported\n");
                exit(1);
            }

            int reg = left.reg;
            switch (node->data.compare.op) {
                case CMP_LT:
                    fprintf(output, "    slt $t%d, $t%d, $t%d\n", reg, left.reg, right.reg);
                    break;
                case CMP_GT:
                    fprintf(output, "    slt $t%d, $t%d, $t%d\n", reg, right.reg, left.reg);
                    break;
                case CMP_LE:
                    /* left <= right  <=>  !(right < left) */
                    fprintf(output, "    slt $t%d, $t%d, $t%d\n", reg, right.reg, left.reg);
                    fprintf(output, "    xori $t%d, $t%d, 1\n", reg, reg);
                    break;
                case CMP_GE:
                    /* left >= right  <=>  !(left < right) */
                    fprintf(output, "    slt $t%d, $t%d, $t%d\n", reg, left.reg, right.reg);
                    fprintf(output, "    xori $t%d, $t%d, 1\n", reg, reg);
                    break;
                case CMP_EQ:
                    /* reg = (left == right) */
                    fprintf(output, "    xor $t%d, $t%d, $t%d\n", reg, left.reg, right.reg);
                    fprintf(output, "    sltiu $t%d, $t%d, 1\n", reg, reg);
                    break;
                case CMP_NE:
                    /* reg = (left != right) */
                    fprintf(output, "    xor $t%d, $t%d, $t%d\n", reg, left.reg, right.reg);
                    fprintf(output, "    sltu $t%d, $zero, $t%d\n", reg, reg);
                    break;
                default:
                    fprintf(stderr, "Error: unknown comparison op\n");
                    exit(1);
            }

            result.type = TYPE_INT;
            result.reg = reg;
            freeIntReg(right.reg);
            break;
        }
        case NODE_FUNCTION_CALL: {
            if (isBuiltinOutputString(node->data.functioncall.name)) {
                if (!node->data.functioncall.args) {
                    fprintf(stderr, "Error: output_string expects one argument\n");
                    exit(1);
                }
                ExprResult value = genExpr(node->data.functioncall.args->data.arglist.expr);
                if (value.type == TYPE_FLOAT) {
                    fprintf(stderr, "Error: output_string expects string/int address\n");
                    exit(1);
                }
                fprintf(output, "    move $a0, $t%d\n", value.reg);
                fprintf(output, "    li $v0, 4\n");
                fprintf(output, "    syscall\n");
                freeIntReg(value.reg);

                /* output_string behaves like void; return 0 if used in expression context. */
                result.type = TYPE_INT;
                result.reg = allocIntReg();
                fprintf(output, "    li $t%d, 0\n", result.reg);
                break;
            }

            if (isBuiltinStringConcat(node->data.functioncall.name) ||
                isBuiltinStringEqual(node->data.functioncall.name)) {
                if (!node->data.functioncall.args ||
                    !node->data.functioncall.args->data.arglist.next) {
                    fprintf(stderr, "Error: %s expects two arguments\n", node->data.functioncall.name);
                    exit(1);
                }

                SavedReg saved[18];
                int savedCount = saveCallerTemps(saved, 18);

                ExprResult left = genExpr(node->data.functioncall.args->data.arglist.expr);
                ExprResult right = genExpr(node->data.functioncall.args->data.arglist.next->data.arglist.expr);
                if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                    fprintf(stderr, "Error: %s expects string arguments\n", node->data.functioncall.name);
                    exit(1);
                }

                fprintf(output, "    move $a0, $t%d\n", left.reg);
                fprintf(output, "    move $a1, $t%d\n", right.reg);
                if (isBuiltinStringConcat(node->data.functioncall.name)) {
                    fprintf(output, "    jal _string_concat\n");
                } else {
                    fprintf(output, "    jal _string_equal\n");
                }
                freeIntReg(left.reg);
                freeIntReg(right.reg);

                restoreCallerTemps(saved, savedCount);

                result.reg = allocIntReg();
                fprintf(output, "    move $t%d, $v0\n", result.reg);
                result.type = isBuiltinStringConcat(node->data.functioncall.name) ? TYPE_STRING : TYPE_INT;
                break;
            }

            FunctionInfo* f = findFunctionInfo(node->data.functioncall.name);
            int argCount = countArgs(node->data.functioncall.args);

            SavedReg saved[18];
            int savedCount = saveCallerTemps(saved, 18);

            /* Push args right-to-left so param i is at 8+4*i($fp) in callee. */
            ASTNode** argVec = NULL;
            if (argCount > 0) {
                argVec = malloc(sizeof(ASTNode*) * argCount);
                ASTNode* curr = node->data.functioncall.args;
                for (int i = 0; i < argCount; i++) {
                    argVec[i] = curr;
                    curr = curr->data.arglist.next;
                }
            }

            ParamList* p = f ? f->params : NULL;
            int paramCount = f ? countParams(f->params) : 0;

            for (int i = argCount - 1; i >= 0; i--) {
                ASTNode* argExpr = argVec[i]->data.arglist.expr;

                /* If we know this parameter is an array, push its base address. */
                int isArrayParam = 0;
                ParamList* walk = p;
                for (int k = 0; k < i && walk; k++) walk = walk->next;
                if (walk && walk->isArray) isArrayParam = 1;

                if (isArrayParam) {
                    if (argExpr->type != NODE_VAR) {
                        fprintf(stderr, "Error: array parameter expects array variable\n");
                        exit(1);
                    }
                    int baseReg = emitComputeArrayBaseAddress(argExpr->data.name);
                    emitPushIntReg(baseReg);
                    freeIntReg(baseReg);
                } else {
                    ExprResult value = genExpr(argExpr);
                    if (value.type == TYPE_FLOAT) {
                        emitPushFloatReg(value.reg);
                        freeFloatReg(value.reg);
                    } else {
                        emitPushIntReg(value.reg);
                        freeIntReg(value.reg);
                    }
                }
            }

            fprintf(output, "    jal %s\n", node->data.functioncall.name);
            if (argCount > 0) {
                fprintf(output, "    addi $sp, $sp, %d\n", 4 * argCount);
            }

            restoreCallerTemps(saved, savedCount);

            if (argVec) free(argVec);

            if (f && f->returnType == TYPE_FLOAT) {
                int reg = allocFloatReg();
                fprintf(output, "    mov.s $f%d, $f0\n", reg);
                result.type = TYPE_FLOAT;
                result.reg = reg;
            } else {
                int reg = allocIntReg();
                fprintf(output, "    move $t%d, $v0\n", reg);
                result.type = f ? f->returnType : TYPE_INT;
                result.reg = reg;
            }

            (void)paramCount;
            break;
        }
        default:
            break;
    }

    return result;
}

static void genStmt(ASTNode* node, int isMain) {
    if (!node) return;

    switch(node->type) {
        case NODE_DECL:
        case NODE_TYPED_DECL:
            /* Declarations are pre-collected to size the frame; no code needed. */
            break;

        case NODE_ASSIGN: {
            int offset = getVarOffset(node->data.assign.var);
            if (offset == -1) {
                fprintf(stderr, "Error: Variable %s not declared\n", node->data.assign.var);
                exit(1);
            }
            ExprResult value = genExpr(node->data.assign.value);
            if (value.type == TYPE_FLOAT) {
                fprintf(output, "    s.s $f%d, %d($fp)\n", value.reg, offset);
                freeFloatReg(value.reg);
            } else {
                fprintf(output, "    sw $t%d, %d($fp)\n", value.reg, offset);
                freeIntReg(value.reg);
            }
            break;
        }

        case NODE_ARRAY_ASSIGN: {
            VarType elemType = getVarType(node->data.arrayAssign.name);
            int baseReg = emitComputeArrayBaseAddress(node->data.arrayAssign.name);
            ExprResult index = genExpr(node->data.arrayAssign.index);
            int idxReg = index.reg;

            fprintf(output, "    sll $t%d, $t%d, 2\n", idxReg, idxReg);
            fprintf(output, "    add $t%d, $t%d, $t%d\n", baseReg, baseReg, idxReg);

            ExprResult value = genExpr(node->data.arrayAssign.value);
            if (elemType == TYPE_FLOAT) {
                fprintf(output, "    s.s $f%d, 0($t%d)\n", value.reg, baseReg);
                freeFloatReg(value.reg);
            } else {
                fprintf(output, "    sw $t%d, 0($t%d)\n", value.reg, baseReg);
                freeIntReg(value.reg);
            }
            freeIntReg(baseReg);
            freeIntReg(idxReg);
            break;
        }

        case NODE_PRINT: {
            ExprResult value = genExpr(node->data.expr);
            if (value.type == TYPE_FLOAT) {
                fprintf(output, "    # Print float\n");
                fprintf(output, "    mov.s $f12, $f%d\n", value.reg);
                fprintf(output, "    li $v0, 2\n");
                fprintf(output, "    syscall\n");
                freeFloatReg(value.reg);
            } else {
                fprintf(output, "    # Print integer\n");
                fprintf(output, "    move $a0, $t%d\n", value.reg);
                fprintf(output, "    li $v0, 1\n");
                fprintf(output, "    syscall\n");
                freeIntReg(value.reg);
            }
            fprintf(output, "    # Print newline\n");
            fprintf(output, "    li $v0, 11\n");
            fprintf(output, "    li $a0, 10\n");
            fprintf(output, "    syscall\n");
            break;
        }

        case NODE_PRINTI: {
            ExprResult value = genExpr(node->data.expr);
            if (value.type == TYPE_FLOAT) {
                fprintf(output, "    # Print float (inline)\n");
                fprintf(output, "    mov.s $f12, $f%d\n", value.reg);
                fprintf(output, "    li $v0, 2\n");
                fprintf(output, "    syscall\n");
                freeFloatReg(value.reg);
            } else {
                fprintf(output, "    # Print integer (inline)\n");
                fprintf(output, "    move $a0, $t%d\n", value.reg);
                fprintf(output, "    li $v0, 1\n");
                fprintf(output, "    syscall\n");
                freeIntReg(value.reg);
            }
            break;
        }

        case NODE_PRINTC: {
            ExprResult value = genExpr(node->data.expr);
            if (value.type == TYPE_FLOAT) {
                fprintf(stderr, "Error: printc expects int/char ASCII code\n");
                exit(1);
            }
            fprintf(output, "    # Print character (ASCII)\n");
            fprintf(output, "    move $a0, $t%d\n", value.reg);
            fprintf(output, "    li $v0, 11\n");
            fprintf(output, "    syscall\n");
            freeIntReg(value.reg);
            break;
        }

        case NODE_WHILE: {
            int id = localLabelCounter++;
            char startLabel[128];
            char endLabel[128];
            formatLocalLabel(startLabel, sizeof(startLabel), "while_start", id);
            formatLocalLabel(endLabel, sizeof(endLabel), "while_end", id);

            fprintf(output, "%s:\n", startLabel);
            ExprResult cond = genExpr(node->data.whileStmt.condition);
            if (cond.type == TYPE_FLOAT) {
                fprintf(stderr, "Error: while condition must be int\n");
                exit(1);
            }
            fprintf(output, "    beqz $t%d, %s\n", cond.reg, endLabel);
            freeIntReg(cond.reg);

            pushBreakTarget(endLabel);
            genStmt(node->data.whileStmt.body, isMain);
            popBreakTarget();

            fprintf(output, "    j %s\n", startLabel);
            fprintf(output, "%s:\n", endLabel);
            break;
        }

        case NODE_FOR: {
            int id = localLabelCounter++;
            char startLabel[128];
            char endLabel[128];
            formatLocalLabel(startLabel, sizeof(startLabel), "for_start", id);
            formatLocalLabel(endLabel, sizeof(endLabel), "for_end", id);

            if (node->data.forStmt.init) {
                genStmt(node->data.forStmt.init, isMain);
            }

            fprintf(output, "%s:\n", startLabel);
            if (node->data.forStmt.condition) {
                ExprResult cond = genExpr(node->data.forStmt.condition);
                if (cond.type == TYPE_FLOAT) {
                    fprintf(stderr, "Error: for-loop condition must be int\n");
                    exit(1);
                }
                fprintf(output, "    beqz $t%d, %s\n", cond.reg, endLabel);
                freeIntReg(cond.reg);
            }

            pushBreakTarget(endLabel);
            genStmt(node->data.forStmt.body, isMain);
            popBreakTarget();

            if (node->data.forStmt.update) {
                genStmt(node->data.forStmt.update, isMain);
            }

            fprintf(output, "    j %s\n", startLabel);
            fprintf(output, "%s:\n", endLabel);
            break;
        }

        case NODE_IF: {
            int id = localLabelCounter++;
            char elseLabel[128];
            char endLabel[128];
            formatLocalLabel(elseLabel, sizeof(elseLabel), "if_else", id);
            formatLocalLabel(endLabel, sizeof(endLabel), "if_end", id);

            ExprResult cond = genExpr(node->data.ifStmt.condition);
            if (cond.type == TYPE_FLOAT) {
                fprintf(stderr, "Error: if condition must be int\n");
                exit(1);
            }

            if (node->data.ifStmt.elseBranch) {
                fprintf(output, "    beqz $t%d, %s\n", cond.reg, elseLabel);
                freeIntReg(cond.reg);

                genStmt(node->data.ifStmt.thenBranch, isMain);

                fprintf(output, "    j %s\n", endLabel);
                fprintf(output, "%s:\n", elseLabel);

                genStmt(node->data.ifStmt.elseBranch, isMain);

                fprintf(output, "%s:\n", endLabel);
            } else {
                fprintf(output, "    beqz $t%d, %s\n", cond.reg, endLabel);
                freeIntReg(cond.reg);

                genStmt(node->data.ifStmt.thenBranch, isMain);

                fprintf(output, "%s:\n", endLabel);
            }
            break;
        }

        case NODE_SWITCH: {
            ExprResult control = genExpr(node->data.switch_stmt.expr);
            if (control.type == TYPE_FLOAT) {
                fprintf(stderr, "Error: switch controlling expression must be int/char\n");
                exit(1);
            }

            int switchId = localLabelCounter++;
            char endLabel[128];
            formatLocalLabel(endLabel, sizeof(endLabel), "switch_end", switchId);

            ASTNode* clause = node->data.switch_stmt.cases;
            int clauseCount = 0;
            while (clause) {
                clauseCount++;
                clause = clause->data.case_clause.next;
            }

            if (clauseCount == 0) {
                freeIntReg(control.reg);
                fprintf(output, "%s:\n", endLabel);
                break;
            }

            ASTNode** clauses = malloc(sizeof(ASTNode*) * (size_t)clauseCount);
            char** labels = malloc(sizeof(char*) * (size_t)clauseCount);
            int defaultIdx = -1;

            clause = node->data.switch_stmt.cases;
            for (int i = 0; i < clauseCount; i++) {
                clauses[i] = clause;
                labels[i] = malloc(128);
                snprintf(labels[i], 128, "%s__switch_%d_case_%d",
                         currentFuncName ? currentFuncName : "func",
                         switchId,
                         i);
                if (clause->data.case_clause.isDefault) {
                    defaultIdx = i;
                }
                clause = clause->data.case_clause.next;
            }

            for (int i = 0; i < clauseCount; i++) {
                if (clauses[i]->data.case_clause.isDefault) continue;
                int cmpReg = allocIntReg();
                fprintf(output, "    li $t%d, %d\n", cmpReg, clauses[i]->data.case_clause.value);
                fprintf(output, "    beq $t%d, $t%d, %s\n", control.reg, cmpReg, labels[i]);
                freeIntReg(cmpReg);
            }

            if (defaultIdx >= 0) {
                fprintf(output, "    j %s\n", labels[defaultIdx]);
            } else {
                fprintf(output, "    j %s\n", endLabel);
            }
            freeIntReg(control.reg);

            pushBreakTarget(endLabel);
            for (int i = 0; i < clauseCount; i++) {
                fprintf(output, "%s:\n", labels[i]);
                genStmt(clauses[i]->data.case_clause.body, isMain);
            }
            popBreakTarget();

            fprintf(output, "%s:\n", endLabel);

            for (int i = 0; i < clauseCount; i++) {
                free(labels[i]);
            }
            free(labels);
            free(clauses);
            break;
        }

        case NODE_BREAK: {
            const char* breakTarget = currentBreakTarget();
            if (!breakTarget) {
                fprintf(stderr, "Error: 'break' used outside loop/switch\n");
                exit(1);
            }
            fprintf(output, "    j %s\n", breakTarget);
            break;
        }

        case NODE_RETURN: {
            if (node->data.returnExpr) {
                ExprResult value = genExpr(node->data.returnExpr);
                if (currentFuncReturnType == TYPE_FLOAT || value.type == TYPE_FLOAT) {
                    fprintf(output, "    mov.s $f0, $f%d\n", value.reg);
                    freeFloatReg(value.reg);
                } else {
                    fprintf(output, "    move $v0, $t%d\n", value.reg);
                    freeIntReg(value.reg);
                }
            }
            fprintf(output, "    j %s\n", currentFuncEndLabel);
            break;
        }

        case NODE_FUNCTION_CALL:
            /* Call used as a statement: evaluate and discard return value. */
            {
                ExprResult tmp = genExpr(node);
                if (tmp.type == TYPE_FLOAT) freeFloatReg(tmp.reg);
                else freeIntReg(tmp.reg);
            }
            break;

        case NODE_STMT_LIST:
            genStmt(node->data.stmtlist.stmt, isMain);
            genStmt(node->data.stmtlist.next, isMain);
            break;

        case NODE_BLOCK:
            genStmt(node->data.blockStmts, isMain);
            break;

        default:
            break;
    }
}

static void emitFunction(ASTNode* func) {
    if (!func || func->type != NODE_FUNCTION) return;

    if (hasEmittedFunction(func->data.function.name)) {
        /* Avoid duplicate labels in assembly output. Semantic analysis should report this. */
        fprintf(output, "    # Skipping duplicate function definition: %s\n\n",
                func->data.function.name);
        return;
    }
    markFunctionEmitted(func->data.function.name);

    int isMain = (strcmp(func->data.function.name, "main") == 0);
    currentFuncName = func->data.function.name;
    localLabelCounter = 0;
    resetBreakTargets();
    currentFuncReturnType = stringToType(func->data.function.returnType);
    snprintf(currentFuncEndLabel, sizeof(currentFuncEndLabel), "%s__end", func->data.function.name);

    if (isMain) {
        fprintf(output, ".globl main\n");
    }
    fprintf(output, "%s:\n", func->data.function.name);

    /* Build symbol table for params + locals to determine frame size. */
    initSymTab();

    int paramIndex = 0;
    ParamList* p = func->data.function.params;
    while (p) {
        VarType pt = stringToType(p->type);
        int paramOffset = 8 + 4 * paramIndex;
        /* Array params are pointers; keep arraySize = 0 to distinguish. */
        addVarAtOffset(p->name, pt, p->isArray, 0, paramOffset);
        paramIndex++;
        p = p->next;
    }

    collectDecls(func->data.function.body);
    int frameSize = getFrameSize();

    /* Prologue */
    fprintf(output, "    # Prologue\n");
    fprintf(output, "    addi $sp, $sp, -8\n");
    fprintf(output, "    sw $fp, 0($sp)\n");
    fprintf(output, "    sw $ra, 4($sp)\n");
    fprintf(output, "    move $fp, $sp\n");
    if (frameSize > 0) {
        fprintf(output, "    addi $sp, $sp, -%d\n", frameSize);
    }

    initRegPools();
    genStmt(func->data.function.body, isMain);

    /* Epilogue */
    fprintf(output, "%s:\n", currentFuncEndLabel);
    fprintf(output, "    # Epilogue\n");
    fprintf(output, "    move $sp, $fp\n");
    fprintf(output, "    lw $fp, 0($sp)\n");
    fprintf(output, "    lw $ra, 4($sp)\n");
    fprintf(output, "    addi $sp, $sp, 8\n");

    if (isMain) {
        fprintf(output, "    li $v0, 10\n");
        fprintf(output, "    syscall\n");
    } else {
        fprintf(output, "    jr $ra\n");
    }

    fprintf(output, "\n");
}

static void emitSyntheticMain(ASTNode* stmts) {
    /* Legacy mode: a file with only statements (no explicit functions) becomes main. */
    ASTNode fake;
    memset(&fake, 0, sizeof(fake));
    fake.type = NODE_FUNCTION;
    fake.data.function.returnType = "int";
    fake.data.function.name = "main";
    fake.data.function.params = NULL;
    fake.data.function.body = createBlock(stmts);
    emitFunction(&fake);
}

static void emitFunctions(ASTNode* node) {
    if (!node) return;
    switch(node->type) {
        case NODE_FUNCTION:
            emitFunction(node);
            break;
        case NODE_FUNCTION_LIST:
            emitFunctions(node->data.functionlist.function);
            emitFunctions(node->data.functionlist.next);
            break;
        default:
            break;
    }
}

static void emitProgram(ASTNode* root) {
    if (!root) return;

    /* Register function signatures so calls can be lowered. */
    registerFunctions(root);

    switch(root->type) {
        case NODE_FUNCTION_LIST:
            emitFunctions(root);
            break;
        case NODE_FUNCTION:
            emitFunction(root);
            break;
        default:
            emitSyntheticMain(root);
            break;
    }
}

void generateMIPS(ASTNode* root, const char* filename) {
    output = fopen(filename, "w");
    if (!output) {
        fprintf(stderr, "Cannot open output file %s\n", filename);
        exit(1);
    }

    functionTable = NULL;
    emittedFunctions = NULL;
    stringLiterals = NULL;
    nextStringLabel = 0;
    collectStringLiterals(root);

    fprintf(output, ".data\n");
    fprintf(output, "_heap_ptr: .word _heap_mem\n");
    fprintf(output, "_heap_mem: .space 8192\n");
    emitStringDataSection();
    fprintf(output, "\n.text\n\n");
    emitRuntimeStringHelpers();

    emitProgram(root);

    fclose(output);
}
