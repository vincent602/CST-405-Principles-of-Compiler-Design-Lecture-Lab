#ifndef SYMTAB_H
#define SYMTAB_H

/* SYMBOL TABLE
 * Tracks all declared variables during compilation
 * Maps variable names to their memory locations (stack offsets) AND TYPES
 * Used for semantic checking and code generation
 */

#define MAX_VARS 100  /* Maximum number of variables supported */

/* TYPE ENUMERATION - Supported data types */
typedef enum {
    TYPE_INT,      /* Integer type */
    TYPE_FLOAT,    /* Float type (for future use) */
    TYPE_CHAR,     /* Character type (for future use) */
    TYPE_STRING,   /* String type (pointer to null-terminated bytes) */
    TYPE_UNKNOWN   /* Unknown/error type */
} VarType;

/* SYMBOL ENTRY - Information about each variable */
typedef struct {
    char* name;     /* Variable identifier */
    int offset;     /* Stack offset in bytes (for MIPS stack frame) */
    VarType type;   /* Data type of the variable */
    int isArray;    /* 1 if array, 0 if scalar */
    int arraySize;  /* Number of elements if array, 0 otherwise */
} Symbol;

/* SYMBOL TABLE STRUCTURE */
typedef struct {
    Symbol vars[MAX_VARS];  /* Array of all variables */
    int count;              /* Number of variables declared */
    int nextOffset;         /* Next available stack offset */
} SymbolTable;

/* SYMBOL TABLE OPERATIONS */
void initSymTab();                          /* Initialize empty symbol table */
int addVar(char* name, VarType type, int isArray, int arraySize);       /* Add new variable with type, returns offset or -1 if duplicate */
int addVarAtOffset(char* name, VarType type, int isArray, int arraySize, int offset); /* Add symbol at a specific offset (used for params) */
int getVarOffset(char* name);               /* Get stack offset for variable, -1 if not found */
VarType getVarType(char* name);             /* Get type of variable, TYPE_UNKNOWN if not found */
int isVarDeclared(char* name);              /* Check if variable exists (1=yes, 0=no) */
int isVarArray(char* name);                 /* Check if variable is an array (1=yes, 0=no) */
int getArraySize(char* name);               /* Get array size, 0 if not array */
int getTypeSize(VarType type);              /* Get size of type in bytes */
int getFrameSize();                         /* Bytes needed for locals/arrays in current scope */
void printSymTab();                         /* Print current symbol table contents for tracing */
const char* typeToString(VarType type);     /* Convert type enum to string for printing */

#endif
