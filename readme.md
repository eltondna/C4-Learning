### 1. Lexical Analysis  : Turn String into token
### 2. Expression Parser : Turn token into C - expression
### 3. Statement Parser  : Parse If, while, return statement
### 4. Driver + VM :
    - Read the source file
    - Initialize symbol table
    - Compile the whole program
    - Execute asm code on the VM

# C4 Instruction Set

This is the custom **virtual machine instruction set** used by the `c4` compiler (Robert Swierczek, *C in Four Functions*).  
The compiler translates C source code into these opcodes, which are then executed by the built-in VM.

---

## 1. Function & Control Flow

| Opcode | Meaning | Equivalent C-like Operation |
|--------|---------|-----------------------------|
| `LEA`  | Load Effective Address | `a = &bp[offset];` |
| `IMM`  | Load Immediate | `a = constant;` |
| `JMP`  | Jump | `pc = target;` |
| `JSR`  | Jump to Subroutine | `*--sp = pc; pc = target;` |
| `BZ`   | Branch if Zero | `if (a == 0) pc = target;` |
| `BNZ`  | Branch if Not Zero | `if (a != 0) pc = target;` |
| `ENT`  | Enter Subroutine | setup new stack frame |
| `ADJ`  | Adjust Stack | `sp += n;` (clear args) |
| `LEV`  | Leave Subroutine | restore stack frame and return |

---

## 2. Memory Access

| Opcode | Meaning | Equivalent C-like Operation |
|--------|---------|-----------------------------|
| `LI`   | Load Integer | `a = *(int*)a;` |
| `LC`   | Load Char | `a = *(char*)a;` |
| `SI`   | Store Integer | `*(int*)*sp++ = a;` |
| `SC`   | Store Char | `*(char*)*sp++ = a;` |
| `PSH`  | Push | `*--sp = a;` |

---

## 3. Arithmetic & Logic

| Opcode | Meaning | Equivalent C-like Operation |
|--------|---------|-----------------------------|
| `OR`   | Bitwise OR | `a = *sp++ \| a;` |
| `XOR`  | Bitwise XOR | `a = *sp++ ^ a;` |
| `AND`  | Bitwise AND | `a = *sp++ & a;` |
| `EQ`   | Equal | `a = (*sp++ == a);` |
| `NE`   | Not Equal | `a = (*sp++ != a);` |
| `LT`   | Less Than | `a = (*sp++ < a);` |
| `GT`   | Greater Than | `a = (*sp++ > a);` |
| `LE`   | Less or Equal | `a = (*sp++ <= a);` |
| `GE`   | Greater or Equal | `a = (*sp++ >= a);` |
| `SHL`  | Shift Left | `a = (*sp++ << a);` |
| `SHR`  | Shift Right | `a = (*sp++ >> a);` |
| `ADD`  | Addition | `a = *sp++ + a;` |
| `SUB`  | Subtraction | `a = *sp++ - a;` |
| `MUL`  | Multiply | `a = *sp++ * a;` |
| `DIV`  | Divide | `a = *sp++ / a;` |
| `MOD`  | Modulo | `a = *sp++ % a;` |

---

## 4. System Calls / Library Functions

These opcodes directly map to **C library or system calls**:

| Opcode | Equivalent Function | Description |
|--------|---------------------|-------------|
| `OPEN` | `open(path, flags)` | open a file |
| `READ` | `read(fd, buf, size)` | read from file |
| `CLOS` | `close(fd)` | close file |
| `PRTF` | `printf(fmt, ...)` | formatted output |
| `MALC` | `malloc(size)` | allocate memory |
| `FREE` | `free(ptr)` | free memory |
| `MSET` | `memset(ptr, val, size)` | set memory |
| `MCMP` | `memcmp(ptr1, ptr2, size)` | compare memory |
| `EXIT` | `exit(code)` | terminate program |

---

## Notes

- The VM is **stack-based**:
  - `a` is the accumulator (main register).
  - `sp` (stack pointer) and `bp` (base pointer) manage the stack.
  - Many operations use `a` and `*sp++` as operands.
- Function calls are implemented using `JSR`, `ENT`, `LEV`, and stack operations.
- Memory operations distinguish between `int` (`LI`, `SI`) and `char` (`LC`, `SC`).

---


