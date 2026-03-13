# CLAUDE.md

## Project Overview

This project implements a simple C compiler targeting the RISC-V architecture.

The compiler is written in C++23 and follows LLVM coding style.

The goal of this project is educational and experimental: the compiler does not aim
to fully support all C standards, but instead focuses on implementing a small,
clean, and understandable compilation pipeline.

The compiler generates RISC-V assembly.

---

## Target Architecture

Target: RISC-V RV64

Assumptions:

- LP64 ABI
- System V style calling convention
- Linux environment

Assembly syntax follows the GNU assembler format.

---

## Compiler Pipeline

The compiler follows a classic multi-stage design:

1. Lexer
2. Parser
3. AST construction
4. Semantic analysis
5. RISC-V code generation

The pipeline should remain modular.

Each stage must have clear data structures and responsibilities.

---

## Guidelines:

- headers in `include/`
- implementation in `lib/`
- tests in `test/`

## Coding Style

The project follows LLVM Coding Standards.

Key rules:

- Use `CamelCase` for types and classes
- Use `lowerCamelCase` for functions
- Avoid unnecessary dynamic allocation

Always favor clarity over cleverness.

---

## C++ Guidelines

Language: C++23

## AST Design

AST nodes should be simple and explicit.

Example node types:

- BinaryExpr
- UnaryExpr
- IntegerLiteral
- VariableRef
- FunctionDecl
- ReturnStmt

Avoid embedding semantic logic in AST nodes.

AST should represent syntax, not semantics.

---

## Code Generation

The backend generates RISC-V assembly.

Responsibilities:

- register allocation (simple strategy)
- stack frame management
- function call handling

