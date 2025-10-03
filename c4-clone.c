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
            // ! 2. Parse Charcater (Variable Name || Function Name)
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


// 2. 