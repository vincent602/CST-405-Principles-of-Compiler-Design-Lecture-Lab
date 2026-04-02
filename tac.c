#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "tac.h"

TACList tacList;
TACList optimizedList;

#define MAX_BREAK_DEPTH 64
static char* breakLabelStack[MAX_BREAK_DEPTH];
static int breakLabelTop = 0;

static void pushBreakLabel(char* label) {
    if (!label) return;
    if (breakLabelTop < MAX_BREAK_DEPTH) {
        breakLabelStack[breakLabelTop++] = label;
    } else {
        fprintf(stderr, "TAC Error: break stack overflow\n");
    }
}

static char* peekBreakLabel() {
    return (breakLabelTop > 0) ? breakLabelStack[breakLabelTop - 1] : NULL;
}

static void popBreakLabel() {
    if (breakLabelTop > 0) breakLabelTop--;
}

void initTAC() {
    tacList.head = NULL;
    tacList.tail = NULL;
    tacList.tempCount = 0;
    tacList.labelCount = 0;
    optimizedList.head = NULL;
    optimizedList.tail = NULL;
    breakLabelTop = 0;
}

char* newTemp() {
    char* temp = malloc(10);
    sprintf(temp, "t%d", tacList.tempCount++);
    return temp;
}

char* newLabel() {
    char* label = malloc(16);
    sprintf(label, "L%d", tacList.labelCount++);
    return label;
}

TACInstr* createTAC(TACOp op, char* arg1, char* arg2, char* result) {
    TACInstr* instr = malloc(sizeof(TACInstr));
    instr->op = op;
    instr->arg1 = arg1 ? strdup(arg1) : NULL;
    instr->arg2 = arg2 ? strdup(arg2) : NULL;
    instr->result = result ? strdup(result) : NULL;
    instr->next = NULL;
    return instr;
}

void appendTAC(TACInstr* instr) {
    if (!tacList.head) {
        tacList.head = tacList.tail = instr;
    } else {
        tacList.tail->next = instr;
        tacList.tail = instr;
    }
}

void appendOptimizedTAC(TACInstr* instr) {
    if (!optimizedList.head) {
        optimizedList.head = optimizedList.tail = instr;
    } else {
        optimizedList.tail->next = instr;
        optimizedList.tail = instr;
    }
}

char* generateTACExpr(ASTNode* node) {
    if (!node) return NULL;
    
    switch(node->type) {
        case NODE_NUM: {
            char* temp = malloc(20);
            sprintf(temp, "%d", node->data.num);
            return temp;
        }

        case NODE_FLOAT: {
            char* temp = malloc(40);
            sprintf(temp, "%f", node->data.fnum);
            return temp;
        }

        case NODE_STRING_LIT: {
            char* temp = newTemp();
            appendTAC(createTAC(TAC_STRING_ASSIGN, node->data.strlit, NULL, temp));
            return temp;
        }
        
        case NODE_VAR:
            return strdup(node->data.name);

        case NODE_ARRAY_ACCESS: {
            char* index = generateTACExpr(node->data.arrayAccess.index);
            char* temp = newTemp();
            appendTAC(createTAC(TAC_ARR_LOAD, node->data.arrayAccess.name, index, temp));
            return temp;
        }
        
        case NODE_BINOP: {
            char* left = generateTACExpr(node->data.binop.left);
            char* right = generateTACExpr(node->data.binop.right);
            char* temp = newTemp();
            
            switch(node->data.binop.op) {
                case '+':
                    appendTAC(createTAC(TAC_ADD, left, right, temp));
                    break;
                case '-':
                    appendTAC(createTAC(TAC_SUB, left, right, temp));
                    break;
                case '*':
                    appendTAC(createTAC(TAC_MUL, left, right, temp));
                    break;
                case '/':
                    appendTAC(createTAC(TAC_DIV, left, right, temp));
                    break;
                default:
                    break;
            }
            
            return temp;
        }

        case NODE_COMPARE: {
            char* left = generateTACExpr(node->data.compare.left);
            char* right = generateTACExpr(node->data.compare.right);
            char* temp = newTemp();

            TACOp op = TAC_LT;
            switch(node->data.compare.op) {
                case CMP_LT: op = TAC_LT; break;
                case CMP_LE: op = TAC_LE; break;
                case CMP_GT: op = TAC_GT; break;
                case CMP_GE: op = TAC_GE; break;
                case CMP_EQ: op = TAC_EQ; break;
                case CMP_NE: op = TAC_NE; break;
                default: op = TAC_LT; break;
            }

            appendTAC(createTAC(op, left, right, temp));
            return temp;
        }

        case NODE_FUNCTION_CALL: {
            ASTNode* arg = node->data.functioncall.args;
            while (arg) {
                generateTACExpr(arg->data.arglist.expr);
                arg = arg->data.arglist.next;
            }
            char* temp = newTemp();
            appendTAC(createTAC(TAC_CALL, node->data.functioncall.name, NULL, temp));
            return temp;
        }
        
        default:
            return NULL;
    }
}

void generateTAC(ASTNode* node) {
    if (!node) return;
    
    switch(node->type) {
        case NODE_DECL:
            appendTAC(createTAC(TAC_DECL, NULL, NULL, node->data.name));
            break;

        case NODE_TYPED_DECL:
            appendTAC(createTAC(TAC_DECL, NULL, NULL, node->data.typedDecl.varName));
            break;
            
        case NODE_ASSIGN: {
            char* expr = generateTACExpr(node->data.assign.value);
            appendTAC(createTAC(TAC_ASSIGN, expr, NULL, node->data.assign.var));
            break;
        }

        case NODE_ARRAY_ASSIGN: {
            char* index = generateTACExpr(node->data.arrayAssign.index);
            char* value = generateTACExpr(node->data.arrayAssign.value);
            appendTAC(createTAC(TAC_ARR_STORE, node->data.arrayAssign.name, index, value));
            break;
        }
        
        case NODE_PRINT: {
            char* expr = generateTACExpr(node->data.expr);
            appendTAC(createTAC(TAC_PRINT, expr, NULL, NULL));
            break;
        }

        case NODE_PRINTI: {
            char* expr = generateTACExpr(node->data.expr);
            appendTAC(createTAC(TAC_PRINTI, expr, NULL, NULL));
            break;
        }

        case NODE_PRINTC: {
            char* expr = generateTACExpr(node->data.expr);
            appendTAC(createTAC(TAC_PRINTC, expr, NULL, NULL));
            break;
        }

        case NODE_WHILE: {
            char* startLabel = newLabel();
            char* endLabel = newLabel();

            appendTAC(createTAC(TAC_LABEL, NULL, NULL, startLabel));

            char* cond = generateTACExpr(node->data.whileStmt.condition);
            appendTAC(createTAC(TAC_IFZ, cond, NULL, endLabel));

            pushBreakLabel(endLabel);
            generateTAC(node->data.whileStmt.body);
            popBreakLabel();

            appendTAC(createTAC(TAC_GOTO, NULL, NULL, startLabel));
            appendTAC(createTAC(TAC_LABEL, NULL, NULL, endLabel));
            break;
        }

        case NODE_FOR: {
            char* startLabel = newLabel();
            char* updateLabel = newLabel();
            char* endLabel = newLabel();

            /* Init executes once before the loop starts */
            if (node->data.forStmt.init) {
                generateTAC(node->data.forStmt.init);
            }

            appendTAC(createTAC(TAC_LABEL, NULL, NULL, startLabel));

            /* Condition check (optional): if false, jump to end */
            if (node->data.forStmt.condition) {
                char* cond = generateTACExpr(node->data.forStmt.condition);
                appendTAC(createTAC(TAC_IFZ, cond, NULL, endLabel));
            }

            /* Body */
            pushBreakLabel(endLabel);
            generateTAC(node->data.forStmt.body);
            popBreakLabel();

            /* Update label (useful target for 'continue' in the future) */
            appendTAC(createTAC(TAC_LABEL, NULL, NULL, updateLabel));

            /* Update executes after each iteration (optional) */
            if (node->data.forStmt.update) {
                generateTAC(node->data.forStmt.update);
            }

            appendTAC(createTAC(TAC_GOTO, NULL, NULL, startLabel));
            appendTAC(createTAC(TAC_LABEL, NULL, NULL, endLabel));
            break;
        }

        case NODE_SWITCH: {
            ASTNode* clause = node->data.switch_stmt.cases;
            int caseCount = 0;
            while (clause) {
                caseCount++;
                clause = clause->data.case_clause.next;
            }

            /* Evaluate control expression once and keep it in a named variable. */
            char switchVar[32];
            snprintf(switchVar, sizeof(switchVar), "__sw%d", tacList.labelCount);
            appendTAC(createTAC(TAC_DECL, NULL, NULL, switchVar));
            char* exprVal = generateTACExpr(node->data.switch_stmt.expr);
            appendTAC(createTAC(TAC_ASSIGN, exprVal, NULL, switchVar));

            if (caseCount == 0) {
                break;
            }

            char** bodyLabels = (char**)malloc(sizeof(char*) * (size_t)caseCount);
            ASTNode** caseArr = (ASTNode**)malloc(sizeof(ASTNode*) * (size_t)caseCount);
            int defaultIdx = -1;

            clause = node->data.switch_stmt.cases;
            for (int i = 0; i < caseCount; i++) {
                bodyLabels[i] = newLabel();
                caseArr[i] = clause;
                if (clause->data.case_clause.isDefault) defaultIdx = i;
                clause = clause->data.case_clause.next;
            }

            char* labelDispatchDone = newLabel();
            char* labelEnd = newLabel();

            /* Build a compact list of non-default cases for dispatch tests. */
            int* nonDef = (int*)malloc(sizeof(int) * (size_t)caseCount);
            int nonDefCnt = 0;
            for (int i = 0; i < caseCount; i++) {
                if (!caseArr[i]->data.case_clause.isDefault) {
                    nonDef[nonDefCnt++] = i;
                }
            }

            for (int ni = 0; ni < nonDefCnt; ni++) {
                int i = nonDef[ni];
                ASTNode* cas = caseArr[i];

                char caseConst[32];
                snprintf(caseConst, sizeof(caseConst), "%d", cas->data.case_clause.value);

                char* cmpTemp = newTemp();
                appendTAC(createTAC(TAC_EQ, switchVar, caseConst, cmpTemp));

                char* failLabel = (ni < nonDefCnt - 1) ? newLabel() : labelDispatchDone;
                appendTAC(createTAC(TAC_IFZ, cmpTemp, NULL, failLabel));
                appendTAC(createTAC(TAC_GOTO, NULL, NULL, bodyLabels[i]));

                if (ni < nonDefCnt - 1) {
                    appendTAC(createTAC(TAC_LABEL, NULL, NULL, failLabel));
                }
            }

            /* No explicit case matched: default or end. */
            appendTAC(createTAC(TAC_LABEL, NULL, NULL, labelDispatchDone));
            if (defaultIdx >= 0) {
                appendTAC(createTAC(TAC_GOTO, NULL, NULL, bodyLabels[defaultIdx]));
            } else {
                appendTAC(createTAC(TAC_GOTO, NULL, NULL, labelEnd));
            }

            pushBreakLabel(labelEnd);
            for (int i = 0; i < caseCount; i++) {
                appendTAC(createTAC(TAC_LABEL, NULL, NULL, bodyLabels[i]));
                generateTAC(caseArr[i]->data.case_clause.body);
            }
            popBreakLabel();

            appendTAC(createTAC(TAC_LABEL, NULL, NULL, labelEnd));

            free(bodyLabels);
            free(caseArr);
            free(nonDef);
            break;
        }

        case NODE_BREAK: {
            char* target = peekBreakLabel();
            if (target) {
                appendTAC(createTAC(TAC_GOTO, NULL, NULL, target));
            } else {
                fprintf(stderr, "TAC Error: break without enclosing context\n");
            }
            break;
        }

        case NODE_IF: {
            char* elseLabel = NULL;
            char* endLabel = newLabel();

            /* If-else needs two labels; plain if needs only end label. */
            if (node->data.ifStmt.elseBranch) {
                elseLabel = newLabel();
            }

            char* cond = generateTACExpr(node->data.ifStmt.condition);
            appendTAC(createTAC(TAC_IFZ, cond, NULL, elseLabel ? elseLabel : endLabel));

            /* Then */
            generateTAC(node->data.ifStmt.thenBranch);

            if (elseLabel) {
                appendTAC(createTAC(TAC_GOTO, NULL, NULL, endLabel));
                appendTAC(createTAC(TAC_LABEL, NULL, NULL, elseLabel));
                generateTAC(node->data.ifStmt.elseBranch);
            }

            appendTAC(createTAC(TAC_LABEL, NULL, NULL, endLabel));
            break;
        }

        case NODE_RETURN: {
            char* expr = NULL;
            if (node->data.returnExpr) {
                expr = generateTACExpr(node->data.returnExpr);
            }
            appendTAC(createTAC(TAC_RETURN, expr, NULL, NULL));
            break;
        }
        
        case NODE_STMT_LIST:
            generateTAC(node->data.stmtlist.stmt);
            generateTAC(node->data.stmtlist.next);
            break;

        case NODE_BLOCK:
            generateTAC(node->data.blockStmts);
            break;

        case NODE_FUNCTION_LIST:
            generateTAC(node->data.functionlist.function);
            generateTAC(node->data.functionlist.next);
            break;

        case NODE_FUNCTION:
            /* Emit a stable entry label so optimizations can treat each function as its own region. */
            appendTAC(createTAC(TAC_LABEL, NULL, NULL, node->data.function.name));
            generateTAC(node->data.function.body);
            break;

        case NODE_FUNCTION_CALL:
            generateTACExpr(node);
            break;
            
        default:
            break;
    }
}

void printTAC() {
    printf("Unoptimized TAC Instructions:\n");
    printf("─────────────────────────────\n");
    TACInstr* curr = tacList.head;
    int lineNum = 1;
    while (curr) {
        printf("%2d: ", lineNum++);
        switch(curr->op) {
            case TAC_DECL:
                printf("DECL %s", curr->result);
                printf("          // Declare variable '%s'\n", curr->result);
                break;
            case TAC_ADD:
                printf("%s = %s + %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Add: store result in %s\n", curr->result);
                break;
            case TAC_SUB:
                printf("%s = %s - %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Sub: store result in %s\n", curr->result);
                break;
            case TAC_MUL:
                printf("%s = %s * %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Mul: store result in %s\n", curr->result);
                break;
            case TAC_DIV:
                printf("%s = %s / %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Div: store result in %s\n", curr->result);
                break;
            case TAC_LT:
                printf("%s = %s < %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Compare: less-than\n");
                break;
            case TAC_LE:
                printf("%s = %s <= %s", curr->result, curr->arg1, curr->arg2);
                printf("    // Compare: less-or-equal\n");
                break;
            case TAC_GT:
                printf("%s = %s > %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Compare: greater-than\n");
                break;
            case TAC_GE:
                printf("%s = %s >= %s", curr->result, curr->arg1, curr->arg2);
                printf("    // Compare: greater-or-equal\n");
                break;
            case TAC_EQ:
                printf("%s = %s == %s", curr->result, curr->arg1, curr->arg2);
                printf("    // Compare: equal\n");
                break;
            case TAC_NE:
                printf("%s = %s != %s", curr->result, curr->arg1, curr->arg2);
                printf("    // Compare: not-equal\n");
                break;
            case TAC_LABEL:
                printf("LABEL %s", curr->result);
                printf("        // Label\n");
                break;
            case TAC_GOTO:
                printf("GOTO %s", curr->result);
                printf("         // Unconditional jump\n");
                break;
            case TAC_IFZ:
                printf("IFZ %s GOTO %s", curr->arg1, curr->result);
                printf("   // Jump if zero\n");
                break;
            case TAC_ARR_LOAD:
                printf("%s = %s[%s]", curr->result, curr->arg1, curr->arg2);
                printf("     // Load array element\n");
                break;
            case TAC_ARR_STORE:
                printf("%s[%s] = %s", curr->arg1, curr->arg2, curr->result);
                printf("     // Store array element\n");
                break;
            case TAC_CALL:
                printf("%s = CALL %s", curr->result, curr->arg1);
                printf("     // Function call\n");
                break;
            case TAC_RETURN:
                if (curr->arg1) {
                    printf("RETURN %s", curr->arg1);
                } else {
                    printf("RETURN");
                }
                printf("          // Return from function\n");
                break;
            case TAC_ASSIGN:
                printf("%s = %s", curr->result, curr->arg1);
                printf("           // Assign value to %s\n", curr->result);
                break;
            case TAC_STRING_ASSIGN:
                printf("%s = \"%s\"", curr->result, curr->arg1);
                printf("         // Assign string literal to %s\n", curr->result);
                break;
            case TAC_PRINT:
                printf("PRINT %s", curr->arg1);
                printf("          // Output value of %s\n", curr->arg1);
                break;
            case TAC_PRINTI:
                printf("PRINTI %s", curr->arg1);
                printf("         // Output value (inline) of %s\n", curr->arg1);
                break;
            case TAC_PRINTC:
                printf("PRINTC %s", curr->arg1);
                printf("         // Output character (ASCII) from %s\n", curr->arg1);
                break;
            default:
                break;
        }
        curr = curr->next;
    }
}

/* ================================================================
 * ENHANCED OPTIMIZATION ENGINE
 * ================================================================ */

/* Helper: Check if a string is a constant (all digits) */
int isConstant(const char* str) {
    if (!str || str[0] == '\0') return 0;
    for (int i = 0; str[i]; i++) {
        if (!isdigit(str[i]) && str[i] != '-') return 0;
    }
    return 1;
}

/* Helper: Check if a variable is a temporary (starts with 't') */
int isTemp(const char* str) {
    return str && str[0] == 't' && isdigit(str[1]);
}

/* LIVENESS ANALYSIS - Find which variables are "live" (used later)
 * A variable is live if its value is needed in a future instruction */
typedef struct LivenessInfo {
    char* var;                    /* Variable name */
    int isLive;                   /* 1 if live, 0 if dead */
    struct LivenessInfo* next;
} LivenessInfo;

LivenessInfo* livenessTable = NULL;

void markAsLive(const char* var) {
    if (!var) return;
    
    /* Check if already in table */
    LivenessInfo* curr = livenessTable;
    while (curr) {
        if (strcmp(curr->var, var) == 0) {
            curr->isLive = 1;
            return;
        }
        curr = curr->next;
    }
    
    /* Add new entry */
    LivenessInfo* info = malloc(sizeof(LivenessInfo));
    info->var = strdup(var);
    info->isLive = 1;
    info->next = livenessTable;
    livenessTable = info;
}

int isLive(const char* var) {
    if (!var) return 0;
    
    LivenessInfo* curr = livenessTable;
    while (curr) {
        if (strcmp(curr->var, var) == 0) {
            return curr->isLive;
        }
        curr = curr->next;
    }
    return 0;  /* Not in table = dead */
}

/* Perform backward liveness analysis */
void analyzeLiveness() {
    /* Free old liveness info */
    while (livenessTable) {
        LivenessInfo* temp = livenessTable;
        livenessTable = livenessTable->next;
        free(temp->var);
        free(temp);
    }
    livenessTable = NULL;
    
    /* Pass 1: Scan backwards to find all used variables */
    TACInstr* curr = tacList.head;
    
    /* Count instructions first */
    int count = 0;
    while (curr) {
        count++;
        curr = curr->next;
    }
    
    /* Store instructions in array for backward scan */
    TACInstr** instructions = malloc(count * sizeof(TACInstr*));
    curr = tacList.head;
    for (int i = 0; i < count; i++) {
        instructions[i] = curr;
        curr = curr->next;
    }
    
    /* Scan backwards */
    for (int i = count - 1; i >= 0; i--) {
        TACInstr* instr = instructions[i];
        
        switch(instr->op) {
            case TAC_PRINT:
            case TAC_PRINTI:
            case TAC_PRINTC:
                /* Variable used in print is live */
                markAsLive(instr->arg1);
                break;

            case TAC_RETURN:
                /* Returned value is live */
                markAsLive(instr->arg1);
                break;
                
            case TAC_ASSIGN:
                /* If result is live, then arg1 must be live */
                if (isLive(instr->result)) {
                    markAsLive(instr->arg1);
                }
                break;

            case TAC_STRING_ASSIGN:
                /* String literal does not introduce operand dependencies. */
                break;
                
            case TAC_ADD:
            case TAC_SUB:
            case TAC_MUL:
            case TAC_DIV:
                /* If result is live, both operands must be live */
                if (isLive(instr->result)) {
                    markAsLive(instr->arg1);
                    markAsLive(instr->arg2);
                }
                break;

            case TAC_LT:
            case TAC_LE:
            case TAC_GT:
            case TAC_GE:
            case TAC_EQ:
            case TAC_NE:
                if (isLive(instr->result)) {
                    markAsLive(instr->arg1);
                    markAsLive(instr->arg2);
                }
                break;

            case TAC_ARR_LOAD:
                if (isLive(instr->result)) {
                    markAsLive(instr->arg1);
                    markAsLive(instr->arg2);
                }
                break;

            case TAC_ARR_STORE:
                markAsLive(instr->arg1);
                markAsLive(instr->arg2);
                markAsLive(instr->result);
                break;

            case TAC_IFZ:
                /* Condition variable is live */
                markAsLive(instr->arg1);
                break;
                
            default:
                break;
        }
    }
    
    free(instructions);
    
    printf("\n=== LIVENESS ANALYSIS ===\n");
    LivenessInfo* info = livenessTable;
    while (info) {
        printf("  %s: %s\n", info->var, info->isLive ? "LIVE" : "DEAD");
        info = info->next;
    }
    printf("=========================\n\n");
}

/* ================================================================
 * CONTROL-FLOW CLEANUPS (SAFE WITH LABELS/BRANCHES)
 * ================================================================ */

static void freeTACInstrNode(TACInstr* instr) {
    if (!instr) return;
    if (instr->arg1) free(instr->arg1);
    if (instr->arg2) free(instr->arg2);
    if (instr->result) free(instr->result);
    free(instr);
}

static void appendExistingNode(TACInstr** head, TACInstr** tail, TACInstr* node) {
    if (!node) return;
    node->next = NULL;
    if (!*head) {
        *head = *tail = node;
    } else {
        (*tail)->next = node;
        *tail = node;
    }
}

/* Pass: simplify IFZ with constant conditions.
 * - IFZ 0 GOTO L  =>  GOTO L
 * - IFZ nonzero   =>  removed (never taken)
 * Returns number of IFZ instructions simplified/removed. */
static int simplifyConstantBranches() {
    int simplified = 0;

    TACInstr* curr = optimizedList.head;
    TACInstr* newHead = NULL;
    TACInstr* newTail = NULL;

    while (curr) {
        TACInstr* next = curr->next;

        if (curr->op == TAC_IFZ && curr->arg1 && isConstant(curr->arg1)) {
            int v = atoi(curr->arg1);
            if (v == 0) {
                printf("  → BRANCH: IFZ %s GOTO %s  ⇒  GOTO %s (always taken)\n",
                       curr->arg1, curr->result, curr->result);
                simplified++;
                curr->op = TAC_GOTO;
                free(curr->arg1);
                curr->arg1 = NULL;
                if (curr->arg2) {
                    free(curr->arg2);
                    curr->arg2 = NULL;
                }
                appendExistingNode(&newHead, &newTail, curr);
            } else {
                printf("  → BRANCH: IFZ %s GOTO %s  ⇒  (removed, never taken)\n",
                       curr->arg1, curr->result);
                simplified++;
                freeTACInstrNode(curr);
            }
        } else {
            appendExistingNode(&newHead, &newTail, curr);
        }

        curr = next;
    }

    optimizedList.head = newHead;
    optimizedList.tail = newTail;
    return simplified;
}

typedef struct {
    const char* name;
    int index;
} LabelIndex;

static int findLabelIndex(LabelIndex* labels, int labelCount, const char* name) {
    if (!name) return -1;
    for (int i = 0; i < labelCount; i++) {
        if (strcmp(labels[i].name, name) == 0) return labels[i].index;
    }
    return -1;
}

static int isInternalLabelName(const char* name) {
    if (!name || name[0] != 'L') return 0;
    if (!isdigit((unsigned char)name[1])) return 0;
    for (int i = 2; name[i]; i++) {
        if (!isdigit((unsigned char)name[i])) return 0;
    }
    return 1;
}

/* Pass: remove unreachable instructions based on control-flow reachability.
 * This is CFG-based (successor traversal) and is safe with branches/loops.
 * Returns number of instructions removed. */
static int eliminateUnreachableCode() {
    /* Collect instructions into an indexable array */
    int n = 0;
    for (TACInstr* scan = optimizedList.head; scan; scan = scan->next) n++;
    if (n == 0) return 0;

    TACInstr** insns = malloc(sizeof(TACInstr*) * (size_t)n);
    int idx = 0;
    for (TACInstr* scan = optimizedList.head; scan; scan = scan->next) {
        insns[idx++] = scan;
    }

    /* Build label -> index map */
    LabelIndex* labels = malloc(sizeof(LabelIndex) * (size_t)n);
    int labelCount = 0;
    for (int i = 0; i < n; i++) {
        if (insns[i]->op == TAC_LABEL && insns[i]->result) {
            labels[labelCount].name = insns[i]->result;
            labels[labelCount].index = i;
            labelCount++;
        }
    }

    /* Reachability (mark on push so the work stack never exceeds n).
     *
     * The TAC stream may contain multiple functions concatenated together.
     * We treat any non-internal label (i.e., not L<digits>) as a potential
     * function entry and seed reachability from it. */
    unsigned char* reachable = calloc((size_t)n, 1);
    int* work = malloc(sizeof(int) * (size_t)n);
    int workTop = 0;
    for (int i = 0; i < n; i++) {
        int isEntry = (i == 0);
        if (insns[i]->op == TAC_LABEL && insns[i]->result && !isInternalLabelName(insns[i]->result)) {
            isEntry = 1;
        }
        if (isEntry && !reachable[i]) {
            reachable[i] = 1;
            work[workTop++] = i;
        }
    }

    while (workTop > 0) {
        int i = work[--workTop];
        if (i < 0 || i >= n) continue;

        TACInstr* instr = insns[i];
        switch (instr->op) {
            case TAC_GOTO: {
                int t = findLabelIndex(labels, labelCount, instr->result);
                if (t >= 0 && !reachable[t]) {
                    reachable[t] = 1;
                    work[workTop++] = t;
                }
                break;
            }
            case TAC_IFZ: {
                /* Not constant anymore after simplifyConstantBranches(), but handle defensively. */
                if (instr->arg1 && isConstant(instr->arg1)) {
                    int v = atoi(instr->arg1);
                    if (v == 0) {
                        int t = findLabelIndex(labels, labelCount, instr->result);
                        if (t >= 0 && !reachable[t]) {
                            reachable[t] = 1;
                            work[workTop++] = t;
                        }
                        break;
                    }
                    /* nonzero: fall through only */
                } else {
                    int t = findLabelIndex(labels, labelCount, instr->result);
                    if (t >= 0 && !reachable[t]) {
                        reachable[t] = 1;
                        work[workTop++] = t;
                    }
                }
                if (i + 1 < n && !reachable[i + 1]) {
                    reachable[i + 1] = 1;
                    work[workTop++] = i + 1;
                }
                break;
            }
            case TAC_RETURN:
                /* Terminator */
                break;
            default:
                if (i + 1 < n && !reachable[i + 1]) {
                    reachable[i + 1] = 1;
                    work[workTop++] = i + 1;
                }
                break;
        }
    }

    /* Rebuild list with only reachable instructions */
    int removed = 0;
    TACInstr* newHead = NULL;
    TACInstr* newTail = NULL;
    for (int i = 0; i < n; i++) {
        TACInstr* node = insns[i];
        if (reachable[i]) {
            appendExistingNode(&newHead, &newTail, node);
        } else {
            removed++;
            freeTACInstrNode(node);
        }
    }
    optimizedList.head = newHead;
    optimizedList.tail = newTail;

    free(insns);
    free(labels);
    free(reachable);
    free(work);

    return removed;
}

/* Enhanced optimization with multiple passes */
void optimizeTAC() {
    printf("\n=== OPTIMIZATION PASSES ===\n");

    int hasControlFlow = 0;
    for (TACInstr* scan = tacList.head; scan; scan = scan->next) {
        if (scan->op == TAC_LABEL || scan->op == TAC_GOTO || scan->op == TAC_IFZ) {
            hasControlFlow = 1;
            break;
        }
    }
    
    /* PASS 1: Liveness Analysis */
    printf("Pass 1: Liveness Analysis\n");
    int enableDCE = 1;
    if (hasControlFlow) {
        /* NOTE: The current liveness analysis is linear and does not build a CFG,
         * so it is not safe in the presence of labels/branches/loops. */
        printf("  (skipped: control flow present; using conservative optimization)\n");
        enableDCE = 0;
    } else {
        analyzeLiveness();
    }
    
    /* Value propagation table */
    typedef struct {
        char* var;
        char* value;
    } VarValue;
    
    VarValue values[100];
    int valueCount = 0;
    
    int deadCodeEliminated = 0;
    int constantsFolded = 0;
    int copiesPropagated = 0;
    
    /* PASS 2: Dead Code Elimination, Constant Folding, Copy Propagation */
    printf("Pass 2: Optimization Transformations\n");

    /* Reset optimized list (defensive in case optimizeTAC is called more than once). */
    optimizedList.head = NULL;
    optimizedList.tail = NULL;
    
    TACInstr* curr = tacList.head;
    
    while (curr) {
        TACInstr* newInstr = NULL;
        int shouldEmit = 1;  /* Should we emit this instruction? */
        
        switch(curr->op) {
            case TAC_DECL:
                /* Always emit declarations */
                newInstr = createTAC(TAC_DECL, NULL, NULL, curr->result);
                break;
                
            case TAC_ADD:
            case TAC_SUB:
            case TAC_MUL:
            case TAC_DIV: {
                /* DEAD CODE ELIMINATION: Skip if result is never used */
                if (enableDCE && !isLive(curr->result)) {
                    printf("  → ELIMINATED: %s = %s op %s (dead code)\n", 
                           curr->result, curr->arg1, curr->arg2);
                    shouldEmit = 0;
                    deadCodeEliminated++;
                    break;
                }
                
                /* COPY PROPAGATION: Replace variables with their known values */
                char* left = curr->arg1;
                char* right = curr->arg2;
                
                /* Look up values in propagation table */
                for (int i = valueCount - 1; i >= 0; i--) {
                    if (strcmp(values[i].var, left) == 0) {
                        left = values[i].value;
                        copiesPropagated++;
                        break;
                    }
                }
                for (int i = valueCount - 1; i >= 0; i--) {
                    if (strcmp(values[i].var, right) == 0) {
                        right = values[i].value;
                        copiesPropagated++;
                        break;
                    }
                }
                
                /* CONSTANT FOLDING: Evaluate at compile time if both are constants */
                if (isConstant(left) && isConstant(right)) {
                    int result = 0;
                    int leftVal = atoi(left);
                    int rightVal = atoi(right);
                    switch(curr->op) {
                        case TAC_ADD:
                            result = leftVal + rightVal;
                            break;
                        case TAC_SUB:
                            result = leftVal - rightVal;
                            break;
                        case TAC_MUL:
                            result = leftVal * rightVal;
                            break;
                        case TAC_DIV:
                            if (rightVal == 0) {
                                /* Don't fold division by zero; keep runtime behavior and let semantic checks catch literals. */
                                newInstr = createTAC(curr->op, left, right, curr->result);
                                shouldEmit = 1;
                                goto done_binop;
                            }
                            result = leftVal / rightVal;
                            break;
                        default:
                            break;
                    }
                    char* resultStr = malloc(20);
                    sprintf(resultStr, "%d", result);
                    
                    printf("  → FOLDED: %s op %s = %d\n", left, right, result);
                    constantsFolded++;
                    
                    /* Store for propagation */
                    values[valueCount].var = strdup(curr->result);
                    values[valueCount].value = resultStr;
                    valueCount++;
                    
                    newInstr = createTAC(TAC_ASSIGN, resultStr, NULL, curr->result);
                } else {
                    newInstr = createTAC(curr->op, left, right, curr->result);
                }
done_binop:
                break;
            }

            case TAC_LT:
            case TAC_LE:
            case TAC_GT:
            case TAC_GE:
            case TAC_EQ:
            case TAC_NE: {
                if (enableDCE && !isLive(curr->result)) {
                    printf("  → ELIMINATED: %s = %s cmp %s (dead code)\n",
                           curr->result, curr->arg1, curr->arg2);
                    shouldEmit = 0;
                    deadCodeEliminated++;
                    break;
                }

                char* left = curr->arg1;
                char* right = curr->arg2;
                for (int i = valueCount - 1; i >= 0; i--) {
                    if (strcmp(values[i].var, left) == 0) {
                        left = values[i].value;
                        copiesPropagated++;
                        break;
                    }
                }
                for (int i = valueCount - 1; i >= 0; i--) {
                    if (strcmp(values[i].var, right) == 0) {
                        right = values[i].value;
                        copiesPropagated++;
                        break;
                    }
                }

                if (isConstant(left) && isConstant(right)) {
                    int leftVal = atoi(left);
                    int rightVal = atoi(right);
                    int resultVal = 0;
                    switch (curr->op) {
                        case TAC_LT: resultVal = (leftVal < rightVal); break;
                        case TAC_LE: resultVal = (leftVal <= rightVal); break;
                        case TAC_GT: resultVal = (leftVal > rightVal); break;
                        case TAC_GE: resultVal = (leftVal >= rightVal); break;
                        case TAC_EQ: resultVal = (leftVal == rightVal); break;
                        case TAC_NE: resultVal = (leftVal != rightVal); break;
                        default: resultVal = 0; break;
                    }
                    char* resultStr = malloc(20);
                    sprintf(resultStr, "%d", resultVal);
                    printf("  → FOLDED: %s cmp %s = %d\n", left, right, resultVal);
                    constantsFolded++;

                    values[valueCount].var = strdup(curr->result);
                    values[valueCount].value = resultStr;
                    valueCount++;

                    newInstr = createTAC(TAC_ASSIGN, resultStr, NULL, curr->result);
                } else {
                    newInstr = createTAC(curr->op, left, right, curr->result);
                }
                break;
            }
            
            case TAC_ASSIGN: {
                /* DEAD CODE ELIMINATION: Skip if result is never used */
                if (enableDCE && !isLive(curr->result) && isTemp(curr->result)) {
                    printf("  → ELIMINATED: %s = %s (dead code)\n", 
                           curr->result, curr->arg1);
                    shouldEmit = 0;
                    deadCodeEliminated++;
                    break;
                }
                
                /* COPY PROPAGATION: Replace variable with its known value */
                char* value = curr->arg1;
                
                for (int i = valueCount - 1; i >= 0; i--) {
                    if (strcmp(values[i].var, value) == 0) {
                        value = values[i].value;
                        copiesPropagated++;
                        break;
                    }
                }
                
                /* Store for future propagation */
                values[valueCount].var = strdup(curr->result);
                values[valueCount].value = strdup(value);
                valueCount++;
                
                newInstr = createTAC(TAC_ASSIGN, value, NULL, curr->result);
                break;
            }

            case TAC_STRING_ASSIGN: {
                /* Keep string literal materialization to avoid dangling temp uses. */
                newInstr = createTAC(TAC_STRING_ASSIGN, curr->arg1, NULL, curr->result);
                break;
            }
            
            case TAC_PRINT:
            case TAC_PRINTI:
            case TAC_PRINTC: {
                /* COPY PROPAGATION: Replace variable with its known value */
                char* value = curr->arg1;
                
                for (int i = valueCount - 1; i >= 0; i--) {
                    if (strcmp(values[i].var, value) == 0) {
                        value = values[i].value;
                        copiesPropagated++;
                        break;
                    }
                }
                
                newInstr = createTAC(curr->op, value, NULL, NULL);
                break;
            }

            case TAC_RETURN: {
                /* COPY PROPAGATION: Replace variable with its known value */
                char* value = curr->arg1;
                if (value) {
                    for (int i = valueCount - 1; i >= 0; i--) {
                        if (strcmp(values[i].var, value) == 0) {
                            value = values[i].value;
                            copiesPropagated++;
                            break;
                        }
                    }
                }

                newInstr = createTAC(TAC_RETURN, value, NULL, NULL);
                break;
            }

            case TAC_LABEL:
                /* Control-flow boundaries: flush propagation table */
                valueCount = 0;
                newInstr = createTAC(TAC_LABEL, NULL, NULL, curr->result);
                break;

            case TAC_GOTO:
                valueCount = 0;
                newInstr = createTAC(TAC_GOTO, NULL, NULL, curr->result);
                break;

            case TAC_IFZ: {
                char* value = curr->arg1;
                for (int i = valueCount - 1; i >= 0; i--) {
                    if (strcmp(values[i].var, value) == 0) {
                        value = values[i].value;
                        copiesPropagated++;
                        break;
                    }
                }
                valueCount = 0;
                newInstr = createTAC(TAC_IFZ, value, NULL, curr->result);
                break;
            }

            case TAC_ARR_LOAD:
            case TAC_ARR_STORE:
                newInstr = createTAC(curr->op, curr->arg1, curr->arg2, curr->result);
                break;
            case TAC_CALL:
                /* Call boundaries are conservatively treated as propagation barriers. */
                valueCount = 0;
                newInstr = createTAC(curr->op, curr->arg1, curr->arg2, curr->result);
                break;
        }
        
        if (shouldEmit && newInstr) {
            appendOptimizedTAC(newInstr);
        }
        
        curr = curr->next;
    }
    
    printf("\n=== OPTIMIZATION SUMMARY ===\n");
    printf("  Dead code eliminated: %d instruction(s)\n", deadCodeEliminated);
    printf("  Constants folded: %d operation(s)\n", constantsFolded);
    printf("  Copies propagated: %d substitution(s)\n", copiesPropagated);
    printf("============================\n\n");

    /* PASS 3: Dead-branch elimination + unreachable code removal */
    printf("Pass 3: Dead-Branch Elimination\n");
    int branchesSimplified = simplifyConstantBranches();
    int unreachableRemoved = eliminateUnreachableCode();
    printf("  Branches simplified: %d\n", branchesSimplified);
    printf("  Unreachable removed: %d instruction(s)\n", unreachableRemoved);
    printf("\n");
}

void printOptimizedTAC() {
    printf("Optimized TAC Instructions:\n");
    printf("─────────────────────────────\n");
    TACInstr* curr = optimizedList.head;
    int lineNum = 1;
    while (curr) {
        printf("%2d: ", lineNum++);
        switch(curr->op) {
            case TAC_DECL:
                printf("DECL %s\n", curr->result);
                break;
            case TAC_ADD:
                printf("%s = %s + %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Runtime addition needed\n");
                break;
            case TAC_SUB:
                printf("%s = %s - %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Runtime subtraction needed\n");
                break;
            case TAC_MUL:
                printf("%s = %s * %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Runtime multiplication needed\n");
                break;
            case TAC_DIV:
                printf("%s = %s / %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Runtime division needed\n");
                break;
            case TAC_LT:
                printf("%s = %s < %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Compare\n");
                break;
            case TAC_LE:
                printf("%s = %s <= %s", curr->result, curr->arg1, curr->arg2);
                printf("    // Compare\n");
                break;
            case TAC_GT:
                printf("%s = %s > %s", curr->result, curr->arg1, curr->arg2);
                printf("     // Compare\n");
                break;
            case TAC_GE:
                printf("%s = %s >= %s", curr->result, curr->arg1, curr->arg2);
                printf("    // Compare\n");
                break;
            case TAC_EQ:
                printf("%s = %s == %s", curr->result, curr->arg1, curr->arg2);
                printf("    // Compare\n");
                break;
            case TAC_NE:
                printf("%s = %s != %s", curr->result, curr->arg1, curr->arg2);
                printf("    // Compare\n");
                break;
            case TAC_LABEL:
                printf("LABEL %s\n", curr->result);
                break;
            case TAC_GOTO:
                printf("GOTO %s\n", curr->result);
                break;
            case TAC_IFZ:
                printf("IFZ %s GOTO %s\n", curr->arg1, curr->result);
                break;
            case TAC_ARR_LOAD:
                printf("%s = %s[%s]", curr->result, curr->arg1, curr->arg2);
                printf("     // Array load\n");
                break;
            case TAC_ARR_STORE:
                printf("%s[%s] = %s", curr->arg1, curr->arg2, curr->result);
                printf("     // Array store\n");
                break;
            case TAC_CALL:
                printf("%s = CALL %s", curr->result, curr->arg1);
                printf("     // Function call\n");
                break;
            case TAC_ASSIGN:
                printf("%s = %s", curr->result, curr->arg1);
                if (isConstant(curr->arg1)) {
                    printf("           // Constant value: %s\n", curr->arg1);
                } else {
                    printf("           // Copy value\n");
                }
                break;
            case TAC_STRING_ASSIGN:
                printf("%s = \"%s\"", curr->result, curr->arg1);
                printf("         // String literal\n");
                break;
            case TAC_PRINT:
                printf("PRINT %s", curr->arg1);
                if (isConstant(curr->arg1)) {
                    printf("          // Print constant: %s\n", curr->arg1);
                } else {
                    printf("          // Print variable\n");
                }
                break;
            case TAC_PRINTI:
                printf("PRINTI %s", curr->arg1);
                if (isConstant(curr->arg1)) {
                    printf("         // Print constant (inline): %s\n", curr->arg1);
                } else {
                    printf("         // Print variable (inline)\n");
                }
                break;
            case TAC_PRINTC:
                printf("PRINTC %s", curr->arg1);
                if (isConstant(curr->arg1)) {
                    printf("         // Print char constant (ASCII): %s\n", curr->arg1);
                } else {
                    printf("         // Print char from variable/expression\n");
                }
                break;
            case TAC_RETURN:
                if (curr->arg1) {
                    printf("RETURN %s", curr->arg1);
                } else {
                    printf("RETURN");
                }
                printf("          // Return\n");
                break;
            default:
                break;
        }
        curr = curr->next;
    }
}

void saveTACToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s' for writing\n", filename);
        return;
    }

    fprintf(file, "# Three-Address Code (TAC) - Unoptimized\n");
    fprintf(file, "# Generated by Minimal C Compiler\n");
    fprintf(file, "# ─────────────────────────────────────\n\n");

    TACInstr* curr = tacList.head;
    int lineNum = 1;
    while (curr) {
        fprintf(file, "%2d: ", lineNum++);
        switch(curr->op) {
            case TAC_DECL:
                fprintf(file, "DECL %s\n", curr->result);
                break;
            case TAC_ADD:
                fprintf(file, "%s = %s + %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_SUB:
                fprintf(file, "%s = %s - %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_MUL:
                fprintf(file, "%s = %s * %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_DIV:
                fprintf(file, "%s = %s / %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_LT:
                fprintf(file, "%s = %s < %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_LE:
                fprintf(file, "%s = %s <= %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_GT:
                fprintf(file, "%s = %s > %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_GE:
                fprintf(file, "%s = %s >= %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_EQ:
                fprintf(file, "%s = %s == %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_NE:
                fprintf(file, "%s = %s != %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_LABEL:
                fprintf(file, "LABEL %s\n", curr->result);
                break;
            case TAC_GOTO:
                fprintf(file, "GOTO %s\n", curr->result);
                break;
            case TAC_IFZ:
                fprintf(file, "IFZ %s GOTO %s\n", curr->arg1, curr->result);
                break;
            case TAC_ARR_LOAD:
                fprintf(file, "%s = %s[%s]\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_ARR_STORE:
                fprintf(file, "%s[%s] = %s\n", curr->arg1, curr->arg2, curr->result);
                break;
            case TAC_CALL:
                fprintf(file, "%s = CALL %s\n", curr->result, curr->arg1);
                break;
            case TAC_ASSIGN:
                fprintf(file, "%s = %s\n", curr->result, curr->arg1);
                break;
            case TAC_STRING_ASSIGN:
                fprintf(file, "%s = \"%s\"\n", curr->result, curr->arg1);
                break;
            case TAC_PRINT:
                fprintf(file, "PRINT %s\n", curr->arg1);
                break;
            case TAC_PRINTI:
                fprintf(file, "PRINTI %s\n", curr->arg1);
                break;
            case TAC_PRINTC:
                fprintf(file, "PRINTC %s\n", curr->arg1);
                break;
            case TAC_RETURN:
                if (curr->arg1) {
                    fprintf(file, "RETURN %s\n", curr->arg1);
                } else {
                    fprintf(file, "RETURN\n");
                }
                break;
            default:
                break;
        }
        curr = curr->next;
    }

    fclose(file);
}

void saveOptimizedTACToFile(const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file '%s' for writing\n", filename);
        return;
    }

    fprintf(file, "# Three-Address Code (TAC) - Optimized\n");
    fprintf(file, "# Generated by Minimal C Compiler\n");
    fprintf(file, "# Optimizations applied:\n");
    fprintf(file, "#   - Dead code elimination\n");
    fprintf(file, "#   - Constant folding\n");
    fprintf(file, "#   - Copy propagation\n");
    fprintf(file, "# ─────────────────────────────────────\n\n");

    TACInstr* curr = optimizedList.head;
    int lineNum = 1;
    while (curr) {
        fprintf(file, "%2d: ", lineNum++);
        switch(curr->op) {
            case TAC_DECL:
                fprintf(file, "DECL %s\n", curr->result);
                break;
            case TAC_ADD:
                fprintf(file, "%s = %s + %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_SUB:
                fprintf(file, "%s = %s - %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_MUL:
                fprintf(file, "%s = %s * %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_DIV:
                fprintf(file, "%s = %s / %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_LT:
                fprintf(file, "%s = %s < %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_LE:
                fprintf(file, "%s = %s <= %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_GT:
                fprintf(file, "%s = %s > %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_GE:
                fprintf(file, "%s = %s >= %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_EQ:
                fprintf(file, "%s = %s == %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_NE:
                fprintf(file, "%s = %s != %s\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_LABEL:
                fprintf(file, "LABEL %s\n", curr->result);
                break;
            case TAC_GOTO:
                fprintf(file, "GOTO %s\n", curr->result);
                break;
            case TAC_IFZ:
                fprintf(file, "IFZ %s GOTO %s\n", curr->arg1, curr->result);
                break;
            case TAC_ARR_LOAD:
                fprintf(file, "%s = %s[%s]\n", curr->result, curr->arg1, curr->arg2);
                break;
            case TAC_ARR_STORE:
                fprintf(file, "%s[%s] = %s\n", curr->arg1, curr->arg2, curr->result);
                break;
            case TAC_CALL:
                fprintf(file, "%s = CALL %s\n", curr->result, curr->arg1);
                break;
            case TAC_ASSIGN:
                fprintf(file, "%s = %s\n", curr->result, curr->arg1);
                break;
            case TAC_STRING_ASSIGN:
                fprintf(file, "%s = \"%s\"\n", curr->result, curr->arg1);
                break;
            case TAC_PRINT:
                fprintf(file, "PRINT %s\n", curr->arg1);
                break;
            case TAC_PRINTI:
                fprintf(file, "PRINTI %s\n", curr->arg1);
                break;
            case TAC_PRINTC:
                fprintf(file, "PRINTC %s\n", curr->arg1);
                break;
            case TAC_RETURN:
                if (curr->arg1) {
                    fprintf(file, "RETURN %s\n", curr->arg1);
                } else {
                    fprintf(file, "RETURN\n");
                }
                break;
            default:
                break;
        }
        curr = curr->next;
    }

    fclose(file);
}
