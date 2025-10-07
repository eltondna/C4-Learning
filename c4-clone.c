#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <stdio.h>
#include <fcntl.h>

#define long long int

char * position, *last_position,            // Current Position in source code
     * data;                                // data/bss pointer

int * emit_position, *last_emit_position,   // Current position in the emitted code
    * identifier,                           // Currently parsed identifier
    * symbol,                               // Symbol Table (Simple list of identifier)
      token,                                // Current Token
      token_value,                          // Current Token Value 
      expression_type,                      // Current Expression Type
      local_var_off,                        // Local variable offset
      line,                                 // Current line number
      src,                                  // rint source and assembly flag
      debug;                                // Print executed instruction

// Token and Classes (operators last and in precedence order)
enum {
    Num = 128, Fun, Sys, Glo, Loc, Id, 
    Char, Else, Enum, If, Int, Return, Sizeof, While,
    Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr,
    Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// Opcode
enum {LEA, IMM, JMP, JSR, BZ, BNZ, ADJ, LEV, LI, LC, SI, SC, PSH, 
      OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, 
      MOD, OPEN, READ, CLOS, MALC, FREE, MSET, MCMP, EXIT
};


// Types
enum {CHAR, INT, PTR};



// Identifier Offsets {Since we cant create an indent struct}
// This is used in the Symbol Table -> 2D array
// i.e. Symbol[Id][Type]....

// Assume you are handling a variable "int a = S", the analyser identifiy a ,
// this will create a record 
/* 
    Symbol[i][Tk]       = Id;
    Symbol[i][Name]     = "a";
    Symbol[i][Class]    = Loc;
    Symbol[i][Type]     = INT;
    Symbol[i][Val]      = 5;
*/


// 这 10 个枚举常量，定义了“结构体字段在数组中的偏移量”。
// 1. Tk    : Token 类型 (i.e. id, int , return)
// 2. Hash  : Hash Value, used to compare symbol name (i.e. hash value of main)
// 3. Name  : Name of the symbol
// 4. Class : The type of the Symbol : (i.e. 局部变量(Loc), 全局变量 (Glo), 函数 (Fun), 系统调用 (Sys))
// 5. Type  : 数据类型：CHAR、INT、PTR 等
// 6. Val   : 1. 变量是内存偏移量 , 2. 函数是地址
// 7. HClass, HType, HVal: 保存“旧的”Class/Type/Val，用于在函数定义结束时恢复符号表（作用域回退）: 保存临时值
// 8. Idsz  : 表示每个 symbol entry 的大小（9）: 用于跳表

enum {TK, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz};



// 1. next() -> Lexical Analysis
void 
next(){
    char * pp;

    while (token = *position){
        ++position;
        if (token == '\n'){
            // ! Print in Debug mode
            if (src){
                printf("%d: %.*s", line, position-last_position, last_position);
                last_position = position;

                // This is an interesting C I/O 
                while (last_emit_position < emit_position){
                    printf("%8.4s", 
                        &
                        "LEA ,IMM ,JMP ,JSR , BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC ,PSH ,"
                        "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL , DIV ,MOD ,"
                        "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[*++last_emit_position * 5]);
                    if (*last_emit_position <= ADJ)
                            printf(" %d", *++last_emit_position);
                    else printf("\n");
                }
                ++line;
            // ! 1. Parse Comment
            }else if (token == "#"){
                while (*position != 0 && *position != '\n') ++position;
            }
            // ! 2. Parse Character (Variable Name || Function Name)
            else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || token == '_'){
                pp = position - 1;

                // ! Scan the whole word
                while ((*position >= 'a' && *position <= 'z') || 
                       (*position >= 'A' && *position <= 'Z') || 
                       (*position >= '0' && *position <= '9') || *position == '_'){
                        // ! Create Hash value for the token 
                        token = token * 147 + *position++;
                }

                // ! Add the length info into the hash value
                token = token << 6 + (position - pp);
                identifier = symbol;
                while (identifier[token]){  
                    if (token == identifier[Hash] && !memcmp((char*) identifier[Name], pp, position-pp)){
                        token = identifier[token];
                        return;
                    }
                    identifier = identifier + Idsz;
                }
                // ! If the identifier of this Symbol is not found, then insert it
                identifier[Name] = (int)pp;
                identifier[Hash] = token;
                token = identifier[token] = Id;
                return;
            }
            // ! 3. Parse number (base 10, 16, 8)
            else if (token >= '0' && token <= '9'){
                if (token_value = token - '0'){
                    // ! Capture the entire value
                    while (*position >= '0' && *position <= '9'){
                        token_value = token_value * 10 + *position++ - '0';
                    }
                }
                else if (*position == 'x' || *position == 'X'){
                    
                    while ((token = *++position) && 
                          ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F'))){
                            // ! To turn the whole hex value into base 10
                            token_value = token_value * 16 + (token & 15) + (token >= 'A' ? 9 : 0);
                    }
                }else{
                    while (*position >= '0' && *position <= '7'){
                        token_value = token_value * 8 + *position++ - '0';
                    }
                    token = Num;
                    return;
                }
            }
            // ! 4. Parse Comment or Divider
            else if (token == '/'){
                if (*position == '/'){
                    ++position;
                    while (*position != 0 && *position != '\n')
                        ++position;
                }else {
                    token = Div;
                    return;
                }
            }

            // ! 5. Parse Data -> i.e. "Hello\n"
            else if (token == '\'' || token == '"'){
                pp = data;
                while (*position != 0 && *position){
                    if ((token_value = *position++) == '\\'){
                        if ((token_value = *position++) == 'n')
                            token_value = '\n';
                    }
                    if (token == '"') 
                        *data++ = token_value;
                }
                ++position;
                if (token == '"')
                    token_value = (int) pp;
                else token = Num;
                return;
            }
            else if (token == '='){
                if (*position == '='){
                    ++position;
                    token = Eq;
                }else token = Assign; 
                return;
            }
            else if (token == '+'){
                if (*position == '+'){
                    token = Inc;
                    ++position;
                }
                else token = Add;
                return ;
            }
            else if (token == '-'){
                if (*position == '-') {
                    token = Dec;
                    ++position;
                }
                else token = Sub;
                return;
            }
            else if (token == '!'){
                if (*position == '='){
                    ++position;
                    token = Ne;
                }
                return;
            }
            else if (token == '<'){
                if (*position == '='){
                    ++position;
                    token = Le;
                }else if (*position == '<'){
                    ++position;
                    token = Shl;
                }else token = Lt;
                return;
            }
            else if (token == '>'){
                if (*position == '='){
                    ++position;
                    token = Ge;
                }else if (*position == '>'){
                    ++position;
                    token = Shr;
                }else token = Gt;
                return ;
            }
            else if (token == '|'){
                if (*position == '|'){
                    ++position;
                    token = Lor;
                }else token = Or;
                return ;
            }
            else if (token == '&'){
                if (*position == '&'){
                    ++position;
                    token = Lan;
                }else token = And;
                return;
            }
            else if (token == '^'){
                token = Xor;
                return;
            }
            else if (token == '%'){
                token = Mod;
                return;   
            }
            else if (token == '*'){
                token = Mul;
                return;
            }
            else if (token == '['){
                token = Brak;
                return;
            }
            else if (token == '?'){
                token = Cond;
                return;
            }
            else if (token == '-' || token == ';' || token == '{' || 
                     token == '}' || token == '(' || token == ')' || 
                     token == ']' || token == ',' || token == ':'){

                return;
            }
        }
    }
}


// 2. Expression Parser 
void expr(int lev){
    int t, *d;
    if (!token){
        printf("%d: Unexpeected eof in expression", line);
        exit(-1);
    }
    else if (token == Num){
        *++emit_position = IMM;
        *++emit_position = token_value;
        next();
        expression_type = INT;
    }
    else if (token == '"'){
        /* 
        * printf("hi"); -------> IMM  0x1000   ; 字符串 "hi" 的地址
        */
        *++emit_position = IMM;
        *++emit_position = token_value;
        next();
        // ! Why here seems strange : Since in C , this is allowed --> "Hello""World""Elton"
        while (token == '"'){
            next();
            // ! Align data to 4 字节边界
            data = (char *) ((int) data + sizeof(int) & - ~(sizeof(int)));
            expression_type = PTR;
        }
    }

    // ! 关键字是统一靠符号表实现的 -> inserted in main() 
    else if (token == Sizeof){
        next();
        if (token == '('){
            next();
        }else{
            printf("%d : Open paraen expected in sizeof\n", line);
            exit(-1);
        }
        expression_type = INT;
        if (token == Int) next();
        else if (token == Char){
            next();
            token = Char;
        }
        // 处理“类型声明中的指针层级”
        // sizeof (int*) sizeof(char *)
        // 用来记录当前类型的指针层级
        while (token == Mul){
            next();
            expression_type = expression_type + PTR;
        }
        if (token == ')')
            next();
        else {
            printf("%d: Close paren expected in sizeof\n", line);
            exit(-1);
        }
        *++emit_position = IMM;
        *++emit_position = (expression_type == CHAR) ? sizeof(char) : sizeof(int);
        expression_type = INT;
    }
    else if (token == Id){
        d = identifier;
        next();
        if (token == '('){
            next();
            t = 0;                      // 参数计数器
            while (token != ')'){       
                expr(Assign);           // 解析一个表达式（即一个参数）
                *++emit_position = PSH;             // 把表达式结果压栈
                ++t;                    // 参数计数+1
                if (tk == ',')          // 如果有逗号，说明后面还有参数
                    next();             // 继续解析下一个参数
            }
            next();                     // 跳过右括号 ')'
            if (d[Class] == Sys)
                *++emit_position = d[Val];
            else if (d[Class] == Fun)
                *++emit_position = JSR;
            else {
                printf("%d: Bad Function Call\n", line);
                exit(-1);
            }

            if (t){
                *++emit_position = ADJ; // 调整堆栈，因为参数已被压入
                *++emit_position = t;
            }
            expression_type = d[Type];
        }

        else if (d[Class] == Num){
            *++emit_position = IMM;
            *++emit_position = d[Val];
            expression_type = INT;
        }else {
            if (d[Class] == Loc){
                *++emit_position = LEA;
                *++emit_position = loc- d[Val];
            }else if (d[Class] == Glo){
                *++emit_position = IMM;
                *++emit_position = d[Val];
            } else {
                printf("%d: Undefined variable\n", line);
                exit(-1);
            }
        }
    }
    else if (token == '('){
        next();
        // ! The type of the value inside the parenthesis
        if (token == Int || token == Char){
            t = (token == Int) ? INT : CHAR;
            next();
        }
        // ! Inside the parenthesis is a Pointer
        while (token == Mul){
            next();
            t = t + PTR;
        }
        if (token == ')'){
            next();
        }else {
            printf("%d: Close paren expected\n", line);
            exit(-1);
        }
    }
    // ! Deferencing Logic
    else if (token == Mul){
        next();
        expr(Inc);
        if (expression_type > INT){
            expression_type = expression_type - PTR;
        }else {
            printf("%d: Bad dereference\n", line);
            exit(-1);
        }
        *++emit_position = (expression_type == CHAR) ? LC : LI;
    }
    // ! Retrieve address 
    else if (token == And){
        next();
        expr(Inc);
        // Cancel the LC and LI instruction
        if (*emit_position == LC || *emit_position == LI) --emit_position;
        else{
            printf("%d: Bad Address-of\n", line);
            exit(-1);
        }
        expression_type = expression_type + PTR;
    }
    else if (token == '!'){
        next();                      // 跳过 `!`
        expr(Inc);                   // 解析 `x`
        *++emit_position = PSH;      // 压入 x 的值
        *++emit_position = IMM;      // 加载 0
        *++emit_position = 0;
        *++emit_position = EQ;       // 比较 x == 0？ 是就返回 1，否则返回 0
        expression_type  = INT;
    }
    else if (token == '~'){
        next();
        expr(Inc);
        *++emit_position = PSH;
        *++emit_position = IMM;
        *++emit_position = -1;
        *++emit_position = XOR;      // x ^ -1 就是 ~x
        expression_type  = INT;
    }
    else if (token == Add){
        next();
        expr(Inc);
        expression_type = INT;
    }
    else if (token == Sub){
        next();
        *++emit_position = IMM;

        // 1. ! Handle "-10"
        if (token == Num){
            *++emit_position = -token_value;
            next();
        }
        // 2. ! Handle "-a"
        else{
            *++emit_position = -1;
            *++emit_position = PSH;
            expr(Inc);
            *++emit_position = MUL; 
        }
        expression_type = INT;
    }
    // ! Handle x++ ++x x-- --x
    // ☑️ 栈上的计算指令
    // ☑️ 写回内存的指令
    // ❌ 不决定语义（前缀还是后缀）
    else if (token == Inc || token == Dec){
        t = token;
        next();
        expr(Inc);
        // ! Here handle the variable
        if (*emit_position == LC){
            *emit_position   = PSH;
            *++emit_position = LC;
        }else if (*emit_position == LI){
            *emit_position   = PSH;
            *++emit_position = LI;
        }else {
            printf("%d: Bad lvalue in pre-increment\n", line);
            exit(-1);
        }
        *++emit_position = PSH;
        *++emit_position = IMM;
        // ! 如果变量是普通类型（int or char），就加/减 1
        // ! 如果变量是指针类型（如 int*），加/减一个指针跨度（比如 sizeof(int)）
        *++emit_position = (expression_type > PTR) ? sizeof(int) : sizeof(char);
        *++emit_position = (t == Inc) ? ADD : SUB;
        // ! 这是把新值写回变量的指令 store Char or store Integer
        *++emit_position = (expression_type == CHAR) ? SC : SI;
    } 
    else {
        printf("%d: Bad Expression\n", line);
        exit(-1);
    }

    while (token >= lev){
        t = expression_type;

        if (token == Assign){
            next();
            // When we parse LHS, it would be either an int or an char 
            // Therefore LI or LC will be there, but we dont want to load that int or char actually
            // We want the address of the variable
            // SO : If the last instruction is a load
            // REplace it with a Push address so the variable address is on the stack

            if (*expression_type == LC || *expression_type == LI){
                *expression_type = PSH;
            }
            else {
                printf("%d: Bad lvalue in assignment\n", line);
                exit(-1);
            }
            // Recursively parse the RHS
            expr(Assign);
            *++expression_type = ((ty=token) == CHAR) ? SC : SI;
        }else if (token == Cond){
            next();
        }

    }

}