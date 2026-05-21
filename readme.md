# RPAL Interpreter

A C++17 implementation of the **RPAL** (Right-reference Pedagogic
Algorithmic Language) interpreter. Built for **CS 3513 — Programming
Languages** (Programming Project). The program reads an RPAL source file,
lexes it, parses it into an Abstract Syntax Tree, standardizes the AST into
a Standardized Tree, flattens the ST into control structures, and executes
them on a Control-Stack-Environment machine. Output matches the reference
`rpal.exe` byte-for-byte on every sample program in `rpal/`.

## Build

```sh
make            # produces ./rpal20
make clean      # remove object files and the executable
```

The build uses `g++` with `-std=c++17 -O2 -Wall -Wextra`. No third-party
libraries; standard library only.

## Run

```sh
./rpal20 <source-file>
```

Optional flags (for debugging / inspection — not required by the spec):

| Flag      | Effect                                                    |
| --------- | --------------------------------------------------------- |
| *(none)*  | Execute the program; emit the result (matches `rpal.exe`) |
| `-ast`    | Print the Abstract Syntax Tree and exit                   |
| `-st`     | Print the Standardized Tree and exit                      |
| `-noout`  | Skip CSE evaluation                                       |

## Pipeline

```
source text
   │  Lexer  (lexer.cpp)
   ▼
tokens
   │  Parser  (parser.cpp)            // recursive descent, stack-based buildTree
   ▼
AST   (ast.h)
   │  Standardizer  (standardizer.cpp)
   ▼                  // rules: let / where / fcn_form / lambda / within /
                      //        and / rec / @
ST    (only gamma, lambda, ->, tau, aug, operators, leaves, <Y*>)
   │  CSE machine  (cse.cpp)
   ▼                  // flattens to control structures, then steps through
                      // them using a value stack and an env chain
output
```

## File Layout

```
PL_Assignment/
├── Makefile              # builds ./rpal20
├── readme.md             # this file
├── rpal20.cpp            # entry point (main)
├── token.h               # Token type + struct
├── lexer.h / lexer.cpp   # lexical analyzer (follows RPAL_Lex.pdf)
├── ast.h   / ast.cpp     # TreeNode + dot-indented printer
├── parser.h / parser.cpp # recursive-descent parser (follows RPAL_Grammar.pdf)
├── standardizer.h        # AST -> ST transformation
│   standardizer.cpp
└── cse.h / cse.cpp       # control structures, CSE machine, built-ins
```

## Standardization Rules Implemented

| Source                          | Standardized form                                |
| ------------------------------- | ------------------------------------------------ |
| `let(=(X,E), P)`                | `gamma(lambda(X, P), E)`                         |
| `where(P, =(X,E))`              | `gamma(lambda(X, P), E)`                         |
| `function_form(F, V1..Vn, E)`   | `=(F, lambda(V1, ... lambda(Vn, E)))`            |
| `lambda(V1..Vn, E)`             | `lambda(V1, ... lambda(Vn, E))`                  |
| `within(=(X1,E1), =(X2,E2))`    | `=(X2, gamma(lambda(X1, E2), E1))`               |
| `and(=1, ..., =N)`              | `=( ,(X1..XN) , tau(E1..EN) )`                   |
| `rec(=(X, E))`                  | `=(X, gamma(<Y*>, lambda(X', E)))`               |
| `@(E1, N, E2)`                  | `gamma(gamma(N, E1), E2)`                        |

## Built-in Functions

`Print`, `print`, `Conc` (curried), `Stem`, `Stern`, `Order`, `ItoS`, `Null`,
`Isstring`, `Isinteger`, `Istruthvalue`, `Istuple`, `Isfunction`, `Isdummy`,
and `nil`.

## CSE Machine Highlights

* **Environments** form a tree; each lambda application creates a new env
  whose parent is the closure's saved env (lexical scoping).
* **Closures** carry their definition-time env, so they remain valid after
  their defining env is popped from the stack.
* **`Y*` / η (eta)** handle recursion: `gamma(<Y*>, λ)` produces an η value;
  applying η rewrites a single `gamma(η, arg)` into two gammas with a stashed
  `LITERAL_VALUE` of `arg` between them, so the recursive lambda's body sees
  `X = η` while the outer application gets the actual argument.
* **Tuple indexing**: `gamma(tuple, INT)` returns the *n*-th element
  (1-based) — this is how `T N` works.
* **Control flow**: `BETA(then_idx, else_idx)` items are placed after the
  flattened condition; when fired they splice the chosen branch's control
  structure into the front of the control deque.

## Verification

All 15 reference programs in `rpal/` (add, conc.1, defns.1, fn1, fn2, fn3,
ftst, infix, infix2, pairs1, pairs2, pairs3, picture, towers, vectorsum)
produce output byte-identical to `rpal.exe` (after stripping the CRLF line
endings Wine inherits from the original Windows binary).

```sh
# Example
./rpal20 rpal/add        # prints  15
./rpal20 rpal/vectorsum  # prints  (5, 7, 9)
./rpal20 rpal/picture    # prints the multi-line tree diagram
```
