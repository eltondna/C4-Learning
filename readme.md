

## C4 Compiler structure
1. `Lexical Analysis`  : Turn String into token
2. `Expression Parser` : Turn token into C - expression
3. `Statement Parse`r`  : Parse If, while, return statement
4. `Driver + VM` :
    - Read the source file
    - Initialize symbol table
    - Compile the whole program
    - Execute asm code on the VM

## C4 Instruction Set

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


## 1. Lexical Analysis (Tokeniser)

#### `Variable`
1. `position`             : 当前正在扫描的源码位置指针
2. `token`                : 当前 token 的类型（比如 `Num`, `Id`, `If`，或者 `+`、`-` 这样的符号）。
3. `symbol`               : 符号表起始地址，用来保存标识符相关信息（名字、类型、作用域、值）。
4. `pp`                   : 指向当前正在解析的字符串的起始位置（比如标识符 `main` 的开头）
5. `token_value`          : 1. 如果是数字 → 存数值 2. 如果是字符串 → 存在 data 区的地址
6. `data`                 : 全局“数据区”的指针，用来存储字符串常量
7. `last_position`        
- 作用：记录上一行源码的起始地址。
- 用途：调试输出（当你用 `-s` 参数时，它会打印出当前行源码 + 对应生成的指令）。
- 不是“上一个指令”，而是“上一行源代码”的开头。
8. `emit_position`
- 作用：指向当前代码生成位置
- 用途：`expr()` 和 `stmt()` 在解析语法时，直接往 `emit_position` 数组里写虚拟机指令。
- 所以它可以理解成汇编代码缓冲区的写入指针。
9. `last_emit_position`
- 作用：记录“上次已经打印过”的指令位置。
- 用途：在 `-s` 模式下，编译器会打印出某一行源码和对应生成的指令：
- 所以它可以理解成上次输出的最后一条指令



### C4 Execution Flow 
main()
├── 初始化关键字、系统函数（插入符号表）
├── 设置源代码 src 指针
├── 调用 program()
│   └── 循环调用 global_declaration()
│       ├── 解析变量定义（插入符号表，Class = Glo）
│       ├── 解析函数定义（插入符号表，Class = Fun）
│       └── 调用 function_declaration()
│           ├── 设置函数参数（插入符号表，Class = Loc）
│           └── 编译函数体
│               ├── 调用 statement()
│               │   └── 可能会调用 expr()
│               │       └── expr() 用到 symbol 表中的符号
│               └── 生成代码
└── 最后执行虚拟机 run()
