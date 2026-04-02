/* SYMBOL TABLE IMPLEMENTATION
 * Manages variable declarations, lookups, AND TYPE INFORMATION
 * Essential for semantic analysis (checking if variables are declared and type-correct)
 * Provides memory layout information for code generation
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

/* Global symbol table instance */
SymbolTable symtab;

/* Convert type enum to readable string */
const char* typeToString(VarType type) {
    switch(type) {
        case TYPE_INT:   return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_CHAR:  return "char";
        case TYPE_STRING:return "string";
        default:         return "unknown";
    }
}

int getTypeSize(VarType type) {
    switch(type) {
        case TYPE_CHAR:
            return 1;
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRING:
        default:
            return 4;
    }
}

/* Initialize an empty symbol table */
void initSymTab() {
    symtab.count = 0;       /* No variables yet */
    symtab.nextOffset = 0;  /* Start at stack offset 0 */
    printf("SYMBOL TABLE: Initialized\n");
    printSymTab();
}

int getFrameSize() {
    return symtab.nextOffset;
}

static int addSymbolInternal(char* name, VarType type, int isArray, int arraySize, int offset, int contributesToFrame) {
    if (isVarDeclared(name)) {
        printf("SYMBOL TABLE: Failed to add '%s' - already declared\n", name);
        return -1;
    }

    symtab.vars[symtab.count].name = strdup(name);
    symtab.vars[symtab.count].offset = offset;
    symtab.vars[symtab.count].type = type;
    symtab.vars[symtab.count].isArray = isArray;
    symtab.vars[symtab.count].arraySize = isArray ? arraySize : 0;
    symtab.count++;

    if (contributesToFrame) {
        if (isArray && arraySize > 0) {
            symtab.nextOffset += getTypeSize(type) * arraySize;
        } else {
            symtab.nextOffset += getTypeSize(type);
        }
    }

    printf("SYMBOL TABLE: Added variable '%s' (type: %s) at offset %d\n",
           name, typeToString(type), offset);
    printSymTab();
    return offset;
}

/* Add a new variable to the symbol table WITH TYPE */
int addVar(char* name, VarType type, int isArray, int arraySize) {
    /* Locals live at negative offsets from $fp. */
    int sizeBytes = (isArray && arraySize > 0) ? (getTypeSize(type) * arraySize) : getTypeSize(type);
    int offset = -(symtab.nextOffset + sizeBytes);
    return addSymbolInternal(name, type, isArray, arraySize, offset, 1);
}

int addVarAtOffset(char* name, VarType type, int isArray, int arraySize, int offset) {
    /* Parameters live at positive offsets from $fp and do not consume local frame space. */
    return addSymbolInternal(name, type, isArray, arraySize, offset, 0);
}

/* Look up a variable's stack offset */
int getVarOffset(char* name) {
    /* Linear search through symbol table */
    for (int i = 0; i < symtab.count; i++) {
        if (strcmp(symtab.vars[i].name, name) == 0) {
            printf("SYMBOL TABLE: Found variable '%s' at offset %d\n", name, symtab.vars[i].offset);
            return symtab.vars[i].offset;  /* Found it */
        }
    }
    printf("SYMBOL TABLE: Variable '%s' not found\n", name);
    return -1;  /* Variable not found - semantic error */
}

/* Get the type of a variable */
VarType getVarType(char* name) {
    /* Linear search through symbol table */
    for (int i = 0; i < symtab.count; i++) {
        if (strcmp(symtab.vars[i].name, name) == 0) {
            printf("SYMBOL TABLE: Variable '%s' has type %s\n", 
                   name, typeToString(symtab.vars[i].type));
            return symtab.vars[i].type;  /* Return the type */
        }
    }
    printf("SYMBOL TABLE: Variable '%s' not found for type lookup\n", name);
    return TYPE_UNKNOWN;  /* Variable not found */
}

/* Check if a variable is an array */
int isVarArray(char* name) {
    for (int i = 0; i < symtab.count; i++) {
        if (strcmp(symtab.vars[i].name, name) == 0) {
            return symtab.vars[i].isArray;
        }
    }
    return 0;
}

/* Get array size */
int getArraySize(char* name) {
    for (int i = 0; i < symtab.count; i++) {
        if (strcmp(symtab.vars[i].name, name) == 0) {
            return symtab.vars[i].arraySize;
        }
    }
    return 0;
}

/* Check if a variable has been declared */
int isVarDeclared(char* name) {
    return getVarOffset(name) != -1;  /* True if found, false otherwise */
}

/* Print current symbol table contents for debugging/tracing */
void printSymTab() {
    printf("\n=== SYMBOL TABLE STATE ===\n");
    printf("Count: %d, Next Offset: %d\n", symtab.count, symtab.nextOffset);
    if (symtab.count == 0) {
        printf("(empty)\n");
    } else {
        printf("Variables:\n");
        for (int i = 0; i < symtab.count; i++) {
            printf("  [%d] %s (%s)%s -> offset %d\n", 
                   i, 
                   symtab.vars[i].name, 
                   typeToString(symtab.vars[i].type),  /* SHOW THE TYPE */
                   symtab.vars[i].isArray ? "[]" : "",
                   symtab.vars[i].offset);
        }
    }
    printf("==========================\n\n");
}
