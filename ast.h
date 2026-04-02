#ifndef AST_H
#define AST_H

/* ABSTRACT SYNTAX TREE (AST)
 * The AST is an intermediate representation of the program structure
 * It represents the hierarchical syntax of the source code
 * Each node represents a construct in the language
 */

/* PARAMETER LIST STRUCTURE for function parameters */
typedef struct ParamList {
    char* type;                 /* Parameter type (int, float, etc.) */
    char* name;                 /* Parameter name */
    int isArray;                /* 1 if parameter is an array */
    struct ParamList* next;     /* Next parameter in list */
} ParamList;

/* NODE TYPES - Different kinds of AST nodes in our language */
typedef enum {
    NODE_NUM,       /* Numeric literal (e.g., 42) */
    NODE_FLOAT,     /* Float literal (e.g., 3.14) */
    NODE_STRING_LIT,/* String literal (e.g., "hello") */
    NODE_VAR,       /* Variable reference (e.g., x) */
    NODE_ARRAY_ACCESS, /* Array element access (e.g., a[i]) */
    NODE_BINOP,     /* Binary operation (e.g., x + y) */
    NODE_COMPARE,   /* Comparison operation (e.g., x < y) */
    NODE_DECL,      /* Variable declaration (e.g., int x) - OLD STYLE */
    NODE_ARRAY_ASSIGN, /* Array element assignment (e.g., a[i] = x) */
    NODE_ASSIGN,    /* Assignment statement (e.g., x = 10) */
    NODE_PRINT,     /* Print statement (e.g., print(x)) */
    NODE_PRINTI,    /* Print without newline (e.g., printi(x)) */
    NODE_PRINTC,    /* Print a single character (e.g., printc(10)) */
    NODE_WHILE,     /* While loop */
    NODE_FOR,       /* For loop */
    NODE_IF,        /* If / if-else statement */
    NODE_SWITCH,    /* switch (expr) { ... } */
    NODE_CASE,      /* case/default clause (linked list) */
    NODE_BREAK,     /* break; */
    NODE_STMT_LIST, /* List of statements (program structure) */
    /* NEW: Function-related nodes */
    NODE_FUNCTION,      /* Function definition */
    NODE_FUNCTION_LIST, /* List of functions */
    NODE_FUNCTION_CALL, /* Function call */
    NODE_RETURN,        /* Return statement */
    NODE_BLOCK,         /* Code block { } */
    NODE_ARG_LIST,      /* Function call arguments */
    NODE_TYPED_DECL     /* Variable declaration with type */
} NodeType;

/* COMPARISON OPERATORS */
typedef enum {
    CMP_LT, /* < */
    CMP_LE, /* <= */
    CMP_GT, /* > */
    CMP_GE, /* >= */
    CMP_EQ, /* == */
    CMP_NE  /* != */
} CompareOp;

/* AST NODE STRUCTURE
 * Uses a union to efficiently store different node data
 * Only the relevant fields for each node type are used
 */
typedef struct ASTNode {
    NodeType type;  /* Identifies what kind of node this is */
    
    /* Union allows same memory to store different data types */
    union {
        /* Literal number value (NODE_NUM) */
        int num;

        /* Literal float value (NODE_FLOAT) */
        double fnum;

        /* String literal value (NODE_STRING_LIT) */
        char* strlit;
        
        /* Variable or declaration name (NODE_VAR, NODE_DECL) */
        char* name;
        /* int value; */ /* For potential future use in declarations with assignment */

        
        /* Binary operation structure (NODE_BINOP) */
        struct {
            char op;                    /* Operator character ('+') */
            struct ASTNode* left;       /* Left operand */
            struct ASTNode* right;      /* Right operand */
        } binop;

        /* Comparison operation (NODE_COMPARE) */
        struct {
            CompareOp op;               /* Comparison operator */
            struct ASTNode* left;       /* Left operand */
            struct ASTNode* right;      /* Right operand */
        } compare;

        /* Array access (NODE_ARRAY_ACCESS) */
        struct {
            char* name;                 /* Array name */
            struct ASTNode* index;      /* Index expression */
        } arrayAccess;
        
        /* Assignment structure (NODE_ASSIGN) */
        struct {
            char* var;                  /* Variable being assigned to */
            struct ASTNode* value;      /* Expression being assigned */
        } assign;

        /* Array assignment (NODE_ARRAY_ASSIGN) */
        struct {
            char* name;                 /* Array name */
            struct ASTNode* index;      /* Index expression */
            struct ASTNode* value;      /* Expression being assigned */
        } arrayAssign;
        
        /* Print expression (NODE_PRINT) */
        struct ASTNode* expr;

        /* While loop (NODE_WHILE) */
        struct {
            struct ASTNode* condition;  /* Loop condition */
            struct ASTNode* body;       /* Loop body (block) */
        } whileStmt;

        /* For loop (NODE_FOR) */
        struct {
            struct ASTNode* init;       /* Init statement (optional) */
            struct ASTNode* condition;  /* Loop condition */
            struct ASTNode* update;     /* Update statement (optional) */
            struct ASTNode* body;       /* Loop body (block) */
        } forStmt;

        /* If statement (NODE_IF) */
        struct {
            struct ASTNode* condition;  /* Condition expression */
            struct ASTNode* thenBranch; /* Then block */
            struct ASTNode* elseBranch; /* Else block (NULL if none) */
        } ifStmt;

        /* Switch statement (NODE_SWITCH) */
        struct {
            struct ASTNode* expr;       /* Controlling expression */
            struct ASTNode* cases;      /* Linked list of NODE_CASE nodes */
        } switch_stmt;

        /* Case/default clause (NODE_CASE) */
        struct {
            int value;                  /* Case constant (unused for default) */
            int isDefault;              /* 1 for default, 0 for case */
            struct ASTNode* body;       /* Statement list (or NULL) */
            struct ASTNode* next;       /* Next case/default clause */
        } case_clause;
        
        /* Statement list structure (NODE_STMT_LIST) */
        struct {
            struct ASTNode* stmt;       /* Current statement */
            struct ASTNode* next;       /* Rest of the list */
        } stmtlist;
        
        /* NEW: Function definition (NODE_FUNCTION) */
        struct {
            char* returnType;           /* Return type: int, void, etc. */
            char* name;                 /* Function name */
            ParamList* params;          /* Parameter list (NULL if no params) */
            struct ASTNode* body;       /* Function body (block) */
        } function;
        
        /* NEW: Function list (NODE_FUNCTION_LIST) */
        struct {
            struct ASTNode* function;
            struct ASTNode* next;
        } functionlist;
        
        /* NEW: Function call (NODE_FUNCTION_CALL) */
        struct {
            char* name;                 /* Function name */
            struct ASTNode* args;       /* Argument list (NULL if no args) */
        } functioncall;
        
        /* NEW: Return statement (NODE_RETURN) */
        struct ASTNode* returnExpr;     /* Expression to return (NULL for void) */
        
        /* NEW: Code block (NODE_BLOCK) */
        struct ASTNode* blockStmts;     /* Statements inside block */
        
        /* NEW: Argument list (NODE_ARG_LIST) */
        struct {
            struct ASTNode* expr;       /* Current argument expression */
            struct ASTNode* next;       /* Next argument */
        } arglist;
        
        /* NEW: Typed declaration (NODE_TYPED_DECL) */
        struct {
            char* varType;              /* Variable type */
            char* varName;              /* Variable name */
            int isArray;                /* 1 if array */
            int arraySize;              /* Array size (0 if not array) */
        } typedDecl;
    } data;
} ASTNode;

/* AST CONSTRUCTION FUNCTIONS
 * These functions are called by the parser to build the tree
 */
ASTNode* createNum(int value);                                   /* Create number node */
ASTNode* createFloat(double value);                              /* Create float literal node */
ASTNode* createStringLit(char* value);                           /* Create string literal node */
ASTNode* createVar(char* name);                                  /* Create variable node */
ASTNode* createBinOp(char op, ASTNode* left, ASTNode* right);   /* Create binary op node */
ASTNode* createCompare(CompareOp op, ASTNode* left, ASTNode* right); /* Create comparison node */
ASTNode* createDecl(char* name);                                /* Create declaration node */
ASTNode* createAssign(char* var, ASTNode* value);               /* Create assignment node */
ASTNode* createArrayAssign(char* name, ASTNode* index, ASTNode* value); /* Create array assignment */
ASTNode* createArrayAccess(char* name, ASTNode* index);          /* Create array access node */
ASTNode* createPrint(ASTNode* expr);                            /* Create print node */
ASTNode* createPrintInline(ASTNode* expr);                      /* Create print (no newline) node */
ASTNode* createPrintChar(ASTNode* expr);                        /* Create print character node */
ASTNode* createStmtList(ASTNode* stmt1, ASTNode* stmt2);        /* Create statement list */
ASTNode* createWhile(ASTNode* condition, ASTNode* body);        /* Create while loop node */
ASTNode* createFor(ASTNode* init, ASTNode* condition, ASTNode* update, ASTNode* body); /* Create for loop node */
ASTNode* createIf(ASTNode* condition, ASTNode* thenBranch, ASTNode* elseBranch); /* Create if/if-else node */
ASTNode* createSwitch(ASTNode* expr, ASTNode* cases);           /* Create switch statement node */
ASTNode* createCase(int value, int isDefault, ASTNode* body);   /* Create case/default clause node */
ASTNode* createBreak();                                          /* Create break statement node */

/* NEW: Function-related construction functions */
ASTNode* createFunction(char* returnType, char* name, ParamList* params, ASTNode* body);
ASTNode* createFunctionList(ASTNode* func1, ASTNode* func2);
ASTNode* createFunctionCall(char* name, ASTNode* args);
ASTNode* createArgList(ASTNode* expr, ASTNode* next);
ASTNode* createReturn(ASTNode* expr);
ASTNode* createBlock(ASTNode* stmts);
ParamList* createParam(char* type, char* name, int isArray);
ParamList* addParam(ParamList* list, char* type, char* name, int isArray);
ASTNode* createTypedDecl(char* type, char* name, int isArray, int arraySize);

/* AST DISPLAY FUNCTION */
void printAST(ASTNode* node, int level);                        /* Pretty-print the AST */
void printParams(ParamList* params);                            /* Print parameter list */

#endif
