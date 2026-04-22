# Project 6 Performance Metrics

Generated: 2026-04-21 19:00:39 MST

## Input and Commands
- Input source: `test_str1.cm`
- Generated assembly: `output.s`
- Compilation command: `./minicompiler test_str1.cm output.s`
- Execution command: `spim -file output.s`

## Metrics
- Compilation time (seconds): **0.285336**
- Execution time (seconds): **0.006439**

## Notes
- Execution completed with SPIM.

## Compilation Phase Detection
- Phase 1: found
- Phase 2: found
- Phase 3: found
- Phase 4: found
- Phase 5: found
- Phase 6: found

## Full Compilation Output
```

╔════════════════════════════════════════════════════════════╗
║          MINIMAL C COMPILER - EDUCATIONAL VERSION          ║
╚════════════════════════════════════════════════════════════╝

┌──────────────────────────────────────────────────────────┐
│ PHASE 1: LEXICAL & SYNTAX ANALYSIS                       │
├──────────────────────────────────────────────────────────┤
│ • Reading source file: test_str1.cm
│ • Tokenizing input (scanner.l)
│ • Parsing grammar rules (parser.y)
│ • Building Abstract Syntax Tree
└──────────────────────────────────────────────────────────┘

Tokens recognized:
  TOKEN: STRING keyword
  TOKEN: ID 'greeting'
  TOKEN: ';'
  TOKEN: ID 'greeting'
  TOKEN: '='
  TOKEN: STRING_LITERAL "Hello"
  TOKEN: ';'
  TOKEN: ID 'output_string'
  TOKEN: '('
  TOKEN: ID 'greeting'
  TOKEN: ')'
  TOKEN: ';'

✓ Parse successful - program is syntactically correct!

┌──────────────────────────────────────────────────────────┐
│ PHASE 2: ABSTRACT SYNTAX TREE (AST)                      │
├──────────────────────────────────────────────────────────┤
│ Tree structure representing the program hierarchy:        │
└──────────────────────────────────────────────────────────┘
TYPED_DECL: string greeting
ASSIGN: greeting
  STRING_LITERAL: "Hello"
CALL: output_string
    VAR: greeting

┌──────────────────────────────────────────────────────────┐
│ PHASE 3: SEMANTIC ANALYSIS                               │
├──────────────────────────────────────────────────────────┤
│ Checking semantic correctness:                           │
│ • Variables declared before use                          │
│ • No duplicate declarations                              │
│ • Type consistency (for future extensions)               │
└──────────────────────────────────────────────────────────┘
SEMANTIC ANALYZER: Initialized

Starting semantic analysis...
───────────────────────────────
SYMBOL TABLE: Initialized

=== SYMBOL TABLE STATE ===
Count: 0, Next Offset: 0
(empty)
==========================

SYMBOL TABLE: Variable 'greeting' not found
SYMBOL TABLE: Variable 'greeting' not found
SYMBOL TABLE: Added variable 'greeting' (type: string) at offset -4

=== SYMBOL TABLE STATE ===
Count: 1, Next Offset: 4
Variables:
  [0] greeting (string) -> offset -4
==========================

  ✓ Variable 'greeting' (type: string) declared successfully
SYMBOL TABLE: Found variable 'greeting' at offset -4
  ✓ Assignment to declared variable 'greeting'
  → Checking type compatibility for assignment:
SYMBOL TABLE: Variable 'greeting' has type string
  → Expression is string literal (type: string)
  ✓ Type check passed: string = string
SYMBOL TABLE: Found variable 'greeting' at offset -4
  ✓ Variable 'greeting' is declared
SYMBOL TABLE: Found variable 'greeting' at offset -4
SYMBOL TABLE: Variable 'greeting' has type string
  → Variable 'greeting' has type: string
───────────────────────────────
✓ Semantic analysis passed - no errors found!

┌──────────────────────────────────────────────────────────┐
│ PHASE 4: INTERMEDIATE CODE GENERATION                    │
├──────────────────────────────────────────────────────────┤
│ Three-Address Code (TAC) - simplified instructions:       │
│ • Each instruction has at most 3 operands                │
│ • Temporary variables (t0, t1, ...) for expressions      │
└──────────────────────────────────────────────────────────┘
Unoptimized TAC Instructions:
─────────────────────────────
 1: DECL greeting          // Declare variable 'greeting'
 2: t0 = "Hello"         // Assign string literal to t0
 3: greeting = t0           // Assign value to greeting
 4: t1 = CALL output_string     // Function call
✓ TAC saved to: test_str1.cm.tac

┌──────────────────────────────────────────────────────────┐
│ PHASE 5: CODE OPTIMIZATION                               │
├──────────────────────────────────────────────────────────┤
│ Applying optimizations:                                  │
│ • Constant folding (evaluate compile-time expressions)   │
│ • Copy propagation (replace variables with values)       │
└──────────────────────────────────────────────────────────┘

=== OPTIMIZATION PASSES ===
Pass 1: Liveness Analysis

=== LIVENESS ANALYSIS ===
=========================

Pass 2: Optimization Transformations

=== OPTIMIZATION SUMMARY ===
  Dead code eliminated: 0 instruction(s)
  Constants folded: 0 operation(s)
  Copies propagated: 0 substitution(s)
============================

Pass 3: Dead-Branch Elimination
  Branches simplified: 0
  Unreachable removed: 0 instruction(s)

Optimized TAC Instructions:
─────────────────────────────
 1: DECL greeting
 2: t0 = "Hello"         // String literal
 3: greeting = t0           // Copy value
 4: t1 = CALL output_string     // Function call
✓ Optimized TAC saved to: test_str1.cm.opt.tac

┌──────────────────────────────────────────────────────────┐
│ PHASE 6: MIPS CODE GENERATION                            │
├──────────────────────────────────────────────────────────┤
│ Translating to MIPS assembly:                            │
│ • Variables stored on stack                              │
│ • Using $t0-$t7 for temporary values                     │
│ • System calls for print operations                      │
└──────────────────────────────────────────────────────────┘
SYMBOL TABLE: Initialized

=== SYMBOL TABLE STATE ===
Count: 0, Next Offset: 0
(empty)
==========================

SYMBOL TABLE: Variable 'greeting' not found
SYMBOL TABLE: Variable 'greeting' not found
SYMBOL TABLE: Added variable 'greeting' (type: string) at offset -4

=== SYMBOL TABLE STATE ===
Count: 1, Next Offset: 4
Variables:
  [0] greeting (string) -> offset -4
==========================

SYMBOL TABLE: Found variable 'greeting' at offset -4
SYMBOL TABLE: Found variable 'greeting' at offset -4
SYMBOL TABLE: Variable 'greeting' has type string
✓ MIPS assembly code generated to: output.s

╔════════════════════════════════════════════════════════════╗
║                  COMPILATION SUCCESSFUL!                   ║
║         Run the output file in a MIPS simulator           ║
╚════════════════════════════════════════════════════════════╝
```

## Compilation Output by Phase

### Phase 1
```
│ PHASE 1: LEXICAL & SYNTAX ANALYSIS                       │
├──────────────────────────────────────────────────────────┤
│ • Reading source file: test_str1.cm
│ • Tokenizing input (scanner.l)
│ • Parsing grammar rules (parser.y)
│ • Building Abstract Syntax Tree
└──────────────────────────────────────────────────────────┘

Tokens recognized:
  TOKEN: STRING keyword
  TOKEN: ID 'greeting'
  TOKEN: ';'
  TOKEN: ID 'greeting'
  TOKEN: '='
  TOKEN: STRING_LITERAL "Hello"
  TOKEN: ';'
  TOKEN: ID 'output_string'
  TOKEN: '('
  TOKEN: ID 'greeting'
  TOKEN: ')'
  TOKEN: ';'

✓ Parse successful - program is syntactically correct!

┌──────────────────────────────────────────────────────────┐
```

### Phase 2
```
│ PHASE 2: ABSTRACT SYNTAX TREE (AST)                      │
├──────────────────────────────────────────────────────────┤
│ Tree structure representing the program hierarchy:        │
└──────────────────────────────────────────────────────────┘
TYPED_DECL: string greeting
ASSIGN: greeting
  STRING_LITERAL: "Hello"
CALL: output_string
    VAR: greeting

┌──────────────────────────────────────────────────────────┐
```

### Phase 3
```
│ PHASE 3: SEMANTIC ANALYSIS                               │
├──────────────────────────────────────────────────────────┤
│ Checking semantic correctness:                           │
│ • Variables declared before use                          │
│ • No duplicate declarations                              │
│ • Type consistency (for future extensions)               │
└──────────────────────────────────────────────────────────┘
SEMANTIC ANALYZER: Initialized

Starting semantic analysis...
───────────────────────────────
SYMBOL TABLE: Initialized

=== SYMBOL TABLE STATE ===
Count: 0, Next Offset: 0
(empty)
==========================

SYMBOL TABLE: Variable 'greeting' not found
SYMBOL TABLE: Variable 'greeting' not found
SYMBOL TABLE: Added variable 'greeting' (type: string) at offset -4

=== SYMBOL TABLE STATE ===
Count: 1, Next Offset: 4
Variables:
  [0] greeting (string) -> offset -4
==========================

  ✓ Variable 'greeting' (type: string) declared successfully
SYMBOL TABLE: Found variable 'greeting' at offset -4
  ✓ Assignment to declared variable 'greeting'
  → Checking type compatibility for assignment:
SYMBOL TABLE: Variable 'greeting' has type string
  → Expression is string literal (type: string)
  ✓ Type check passed: string = string
SYMBOL TABLE: Found variable 'greeting' at offset -4
  ✓ Variable 'greeting' is declared
SYMBOL TABLE: Found variable 'greeting' at offset -4
SYMBOL TABLE: Variable 'greeting' has type string
  → Variable 'greeting' has type: string
───────────────────────────────
✓ Semantic analysis passed - no errors found!

┌──────────────────────────────────────────────────────────┐
```

### Phase 4
```
│ PHASE 4: INTERMEDIATE CODE GENERATION                    │
├──────────────────────────────────────────────────────────┤
│ Three-Address Code (TAC) - simplified instructions:       │
│ • Each instruction has at most 3 operands                │
│ • Temporary variables (t0, t1, ...) for expressions      │
└──────────────────────────────────────────────────────────┘
Unoptimized TAC Instructions:
─────────────────────────────
 1: DECL greeting          // Declare variable 'greeting'
 2: t0 = "Hello"         // Assign string literal to t0
 3: greeting = t0           // Assign value to greeting
 4: t1 = CALL output_string     // Function call
✓ TAC saved to: test_str1.cm.tac

┌──────────────────────────────────────────────────────────┐
```

### Phase 5
```
│ PHASE 5: CODE OPTIMIZATION                               │
├──────────────────────────────────────────────────────────┤
│ Applying optimizations:                                  │
│ • Constant folding (evaluate compile-time expressions)   │
│ • Copy propagation (replace variables with values)       │
└──────────────────────────────────────────────────────────┘

=== OPTIMIZATION PASSES ===
Pass 1: Liveness Analysis

=== LIVENESS ANALYSIS ===
=========================

Pass 2: Optimization Transformations

=== OPTIMIZATION SUMMARY ===
  Dead code eliminated: 0 instruction(s)
  Constants folded: 0 operation(s)
  Copies propagated: 0 substitution(s)
============================

Pass 3: Dead-Branch Elimination
  Branches simplified: 0
  Unreachable removed: 0 instruction(s)

Optimized TAC Instructions:
─────────────────────────────
 1: DECL greeting
 2: t0 = "Hello"         // String literal
 3: greeting = t0           // Copy value
 4: t1 = CALL output_string     // Function call
✓ Optimized TAC saved to: test_str1.cm.opt.tac

┌──────────────────────────────────────────────────────────┐
```

### Phase 6
```
│ PHASE 6: MIPS CODE GENERATION                            │
├──────────────────────────────────────────────────────────┤
│ Translating to MIPS assembly:                            │
│ • Variables stored on stack                              │
│ • Using $t0-$t7 for temporary values                     │
│ • System calls for print operations                      │
└──────────────────────────────────────────────────────────┘
SYMBOL TABLE: Initialized

=== SYMBOL TABLE STATE ===
Count: 0, Next Offset: 0
(empty)
==========================

SYMBOL TABLE: Variable 'greeting' not found
SYMBOL TABLE: Variable 'greeting' not found
SYMBOL TABLE: Added variable 'greeting' (type: string) at offset -4

=== SYMBOL TABLE STATE ===
Count: 1, Next Offset: 4
Variables:
  [0] greeting (string) -> offset -4
==========================

SYMBOL TABLE: Found variable 'greeting' at offset -4
SYMBOL TABLE: Found variable 'greeting' at offset -4
SYMBOL TABLE: Variable 'greeting' has type string
✓ MIPS assembly code generated to: output.s

╔════════════════════════════════════════════════════════════╗
║                  COMPILATION SUCCESSFUL!                   ║
║         Run the output file in a MIPS simulator           ║
╚════════════════════════════════════════════════════════════╝
```

## Execution Output
```
Loaded: /opt/homebrew/Cellar/spim/9.1.24/share/exceptions.s
Hello
```
