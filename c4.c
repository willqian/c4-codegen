// c4.c - C in four functions

// char, int, and pointer types
// if, while, return, and expression statements
// just enough features to allow self-compilation and a bit more

// Written by Robert Swierczek
// 福强进一步探索，尝试基于c4做代码生成等一系列工作

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#define int long long // 兼容64位系统

char *p, *lp, // current position in source code
     *data;   // data/bss pointer

int *e, *le,  // current position in emitted code
    *id,      // currently parsed identifier
    *sym,     // symbol table (simple list of identifiers)
    tk,       // current token
    ival,     // current token value
    ty,       // current expression type
    loc,      // local variable offset
    line,     // current line number
    src,      // print source and assembly flag
    debug,    // print executed instructions
    proto;    // proto code generate mode

struct stt_meta_id {
  int *id;          // 结构体成员定义，指向ident struct
  int id_offset;    // 结构体成员内存偏移
  int id_size;      // 结构体成员内存占用
  int id_type;      // 结构体成员type
  struct stt_meta_id *next;
};

// TODO: ./c4 c4.c c4.c struct.c 会报结构体重复定义，待处理
struct stt_meta {
  struct stt_meta_id *ids;
  int id_num;       // 结构体成员个数
  int size;         // 结构体总内存占用
  int defined;      // 结构体是否定义过
} *stt_metas; // 结构体元数据数组

struct proto_meta_param {
  int ty;
  int io;
  int uniq_id;
  struct proto_meta_param *next;
};

struct proto_gen_cmd {
  char *name;
  int number;
};

struct proto_gen_transaction {
  struct proto_meta_param *write;
  char *write_name;
  int write_name_size;
  struct proto_meta_param *read;
  char *read_name;
  int read_name_size;
};

struct proto_meta {
  int *id;
  char *name;
  int name_size;
  struct proto_meta_param *id_params;
  int defined;
  struct proto_gen_cmd cmd;
  struct proto_gen_transaction transaction;
} *proto_metas;

struct proto_context {
  int id;
  int number;
  int uniq_id;
} g_proto;

// tokens and classes (operators last and in precedence order)
// 结构体索引成员变量token . 和 -> 优先级最高
enum {
  Num = 128, Fun, Proto, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Struct, Ro, Wo, Rw, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak, Dot, Arrow,
};

// opcodes
enum { LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,MCPY,SLEN,EXIT };

// types
enum { CHAR, INT, STRUCT_BEGIN, PTR = 1024 }; // struct排列在INT后，最多定义到1023，后面是一级指针和二级指针

// identifier offsets (since we can't create an ident struct)
// 这个数据结构本身定义symbol，这里我们也复用为结构体定义的symbol
// 比如 struct member {...}; 又有 int member，那么这两个同名但类型不同，所以需要加一个STMetaType字段，表示这个symbol也是结构体meta定义
// 另外 struct 成员变量其实也会在这里复用，暂时先不处理
enum { Tk, Hash, Name, NameLen, Class, Type, Val, STMetaType, HClass, HType, HVal, Idsz };

void next()
{
  char *pp;

  while (tk = *p) {
    ++p;
    if (tk == '\n') {
      if (src) {
        // 先把源码打印出来，包含被注释的部分，例如
        // 16: int main(int argc, char **argv)
        // 17: {
        // 18:   //int a, b;
        // 19:   return 0;
        //     ENT  0
        //     IMM  0
        //     LEV
        printf("%d: %.*s", line, p - lp, lp);
        lp = p;
        while (le < e) {
          // 取如下字符串初始内存地址作为数组，数组长度位5个char，le地址存的是opcode地址，取值乘以5得到的是对应的字符串
          // 例如 ENT 0
          printf("%8.4s", &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
                           "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                           "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,MCPY,SLEN,EXIT,"[*++le * 5]);
          // opcode小于等于ADJ的都是带一个参数，例如IMM 0  
          if (*le <= ADJ) printf(" %d\n", *++le); else printf("\n");
        }
      }
      ++line;
    }
    else if (tk == '#') { // 并不支持宏和头文件引用
      while (*p != 0 && *p != '\n') ++p;
    }
    else if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_') {
      pp = p - 1;
      while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')
        tk = tk * 147 + *p++; // 计算哈希
      tk = (tk << 6) + (p - pp); // 得到最终的hash，低位存的是p - pp也就是字符串的长度   
      id = sym;
      while (id[Tk]) { // 老派程序设计思想，一切基于内存取值，Idsz为id数据结构的长度，以此进行遍历
        if (tk == id[Hash] && !memcmp((char *)id[Name], pp, p - pp)) { tk = id[Tk]; return; }
        id = id + Idsz;
      }
      id[Name] = (int)pp; // pp存的是字符串基地址
      id[NameLen] = p - pp;
      id[Hash] = tk;
      tk = id[Tk] = Id;
      return;
    }
    else if (tk >= '0' && tk <= '9') {
      if (ival = tk - '0') { while (*p >= '0' && *p <= '9') ival = ival * 10 + *p++ - '0'; }
      else if (*p == 'x' || *p == 'X') { // 处理16进制
        while ((tk = *++p) && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F')))
          ival = ival * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
      }
      else { while (*p >= '0' && *p <= '7') ival = ival * 8 + *p++ - '0'; } // 处理8进制
      tk = Num;
      return;
    }
    else if (tk == '/') { // 不支持 /**/ 格式的多行注释
      if (*p == '/') {
        ++p;
        while (*p != 0 && *p != '\n') ++p;
      }
      else {
        tk = Div;
        return;
      }
    }
    else if (tk == '\'' || tk == '"') {
      pp = data;
      while (*p != 0 && *p != tk) {
        if ((ival = *p++) == '\\') {
          if ((ival = *p++) == 'n') ival = '\n'; // 只处理\n转义，其他转义直接过滤掉，比如\t直接变为t
        }
        if (tk == '"') *data++ = ival; // 把每个ival字符传入到data中
      }
      ++p;
      if (tk == '"') ival = (int)pp; else tk = Num; // 如果是字符串，设置data字符串首地址给ival
      return;
    }
    else if (tk == '=') { if (*p == '=') { ++p; tk = Eq; } else tk = Assign; return; }
    else if (tk == '+') { if (*p == '+') { ++p; tk = Inc; } else tk = Add; return; }
    else if (tk == '-') {
      if (*p == '-') { ++p; tk = Dec; } else if (*p == '>') { ++p; tk = Arrow; } else tk = Sub;
      return;
    }
    else if (tk == '!') { if (*p == '=') { ++p; tk = Ne; } return; }
    else if (tk == '<') { if (*p == '=') { ++p; tk = Le; } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return; }
    else if (tk == '>') { if (*p == '=') { ++p; tk = Ge; } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return; }
    else if (tk == '|') { if (*p == '|') { ++p; tk = Lor; } else tk = Or; return; }
    else if (tk == '&') { if (*p == '&') { ++p; tk = Lan; } else tk = And; return; }
    else if (tk == '^') { tk = Xor; return; }
    else if (tk == '%') { tk = Mod; return; }
    else if (tk == '*') { tk = Mul; return; }
    else if (tk == '[') { tk = Brak; return; }
    else if (tk == '?') { tk = Cond; return; }
    else if (tk == '.') { tk = Dot; return; }
    else if (tk == '~' || tk == ';' || tk == '{' || tk == '}' || tk == '(' || tk == ')' || tk == ']' || tk == ',' || tk == ':') return;
  }
}

void expr(int lev)
{
  int t, *d, i, k;
  struct stt_meta_id *ptr, *mem;
  // e指向的是代码段，当然这里代码段都是存在内存里
  // expr先处理的是表达式的左部分，右部分放在了后面的while(tk >= lev)循环里
  if (!tk) { printf("%d: unexpected eof in expression\n", line); exit(-1); }
  else if (tk == Num) { *++e = IMM; *++e = ival; next(); ty = INT; } // IMM 123 type:INT
  else if (tk == '"') { // 处理字符串 IMM ival type:PTR
    *++e = IMM; *++e = ival; next();
    while (tk == '"') next(); // 如果是连续的多个字符串，继续往后处理并进行拼接
    data = (char *)((int)data + sizeof(int) & -sizeof(int)); ty = PTR; // 由于data是一个个char传入的，这里做指针对齐
  }
  // TODO:
  // 1. 这个sizeof的实现不符合C语言标准，在sizeof里面只做类型推导，而不会真的做运算，所以要处理好 = ++ --
  // 2. sizeof嵌套问题也要处理，sizeof返回的是unsigned long类型
  // 3. sizeof(1)也要处理，返回的是unsigned long长度
  // 4. ./c4 c4.c struct.c 的sizeof空结构体(struct empty)会有问题，待处理
  else if (tk == Sizeof) { // 支持int char (int *) (char *) (int **) (char **) ... int的长度是64位 long long
    next(); if (tk == '(') next(); else { printf("%d: open paren expected in sizeof\n", line); exit(-1); }
    if (tk == Int || tk == Char || tk == Struct) {
      ty = INT; if (tk == Int) next(); else if (tk == Char) { next(); ty = CHAR; }
      else if (tk == Struct) { 
        next(); ty = id[STMetaType]; 
        if (stt_metas[ty].defined == 0) { printf("%d: struct not defined in sizeof\n", line); exit(-1); }
        next();
      }
      while (tk == Mul) { next(); ty = ty + PTR; } // 老派程序设计思想，这里可以很好处理指针递归，例如(int *) (int **)
                                                  // type本身是INT或CHAR，加上PTR分别指代(INT *)和(CHAR *)
                                                  // 如果是(int **)，则type加上两次PTR
    }
    else if (tk == '(') { // 处理括号表达式，类型cast
      expr(Assign);
    }
    else if (tk == Id || tk == Mul || tk == And) { // 处理sizeof(var)
      expr(Assign);
    }
    if (tk == ')') next(); else { printf("%d: close paren expected in sizeof\n", line); exit(-1); }
    // int (int *) (char *) ... 作为sizeof(int)处理
    // 增加结构体的支持
    if (ty == CHAR) { t = sizeof(char); }
    else if (ty >= STRUCT_BEGIN && ty < PTR) { t = stt_metas[ty].size; }
    else { t = sizeof(int); }
    *++e = IMM; *++e = t;
    ty = INT;
  }
  else if (tk == Id) { // 处理定义的变量和函数
    d = id; next();
    if (tk == '(') { // 函数调用
      next();
      t = 0;
      while (tk != ')') {
        expr(Assign); // 获取函数参数，Assign的含义是，后续如果符号优先级大于等于Assign，则可以进行表达式运算
                      // 优先级爬坡，下面while (tk >= lev)会提到，这是一种递归的算法，最终递归终止于最高优先级运算
                      // 比如第一个参数如果是: sizeof(int) + 4，会递归调用先处理sizeof(int)，然后next token是 + 号，
                      // 优先级高于assign，递归获得下一个num 4，4后面是,逗号或者)右括号，退出递归调用
        if (ty >= STRUCT_BEGIN && ty < PTR) { // 结构体要多次PSH
          if (*e == LC || *e == LI) { *--e = 0; } // 上一条load指令先去掉
          k = (stt_metas[id[Type]].size + sizeof(int) - 1) / sizeof(int);
          if (k == 0) k = 1; // 没有成员的结构体也分配空间
          i = k - 1;
          while (i >= 0) { // 需要注意，push的方向，先push高地址，再push低地址
            if (id[Class] == Loc) { 
              *++e = LEA; *++e = loc - id[Val] + i;
            }
            else if (id[Class] == Glo) { *++e = IMM; *++e = id[Val] + i * sizeof(int); }
            *++e = ((ty = d[Type]) == CHAR) ? LC : LI;
            *++e = PSH; ++t; // 把结构体值PUSH入栈
            i--;
          }
        } else {
          *++e = PSH; ++t; // 把参数PUSH入栈
        } 
        //if (tk == ',') next(); // 这里有bug，如果参数之间没有逗号，也不会报错，如下两行fix了这个问题
        if (tk == ',') { next(); if(tk == ')') { printf("%d: error unexpected comma in function call\n", line); exit(-1); }}
        else if(tk != ')') { printf("%d: error missing comma in function call\n", line); exit(-1); }
      }
      next();
      if (d[Class] == Sys) *++e = d[Val]; // 如果是系统调用，d[Val]指代的是opcode，例如printf是PRTF
      else if (d[Class] == Fun) { *++e = JSR; *++e = d[Val]; } // 如果是函数调用，JSR d[Val]，d[Val]是函数入口地址
      else { printf("%d: bad function call\n", line); exit(-1); }
      if (t) { *++e = ADJ; *++e = t; } // 函数调用完毕后，需要出栈 ADJ t，t是参数个数
      ty = d[Type]; 
    }
    else if (d[Class] == Num) { *++e = IMM; *++e = d[Val]; ty = INT; } // Enum类型，IMM d[Val] type: INT
                                                                       // 参考main函数的Enum处理部分
    else {
      if (d[Class] == Loc) { *++e = LEA; *++e = loc - d[Val]; } // 本地变量 loc - d[Val]，得到是变量的相对位置，
                                                                // 函数参数是从2开始递增，局部变量从-1开始递减
                                                                // 具体栈帧结构参考main函数中的说明，loc的处理也在
                                                                // main函数中
      else if (d[Class] == Glo) { *++e = IMM; *++e = d[Val]; } // 全局变量
      else { printf("%d: undefined variable\n", line); exit(-1); }
      *++e = ((ty = d[Type]) == CHAR) ? LC : LI; // LC LI load指令，从寄存器取指针指向的值，reg = *(reg)
    }
  }
  else if (tk == '(') {
    next();
    if (tk == Int || tk == Char) { // 类型转换，包含 int char (int *) (char *) (int **) (char **) ...
      t = (tk == Int) ? INT : CHAR; next();
      while (tk == Mul) { next(); t = t + PTR; } // 指针，如果有多个* Mul，例如(int **)，t对应的是指针的指针
      if (tk == ')') next(); else { printf("%d: bad cast\n", line); exit(-1); }
      expr(Inc); // 递归expr，优先级需要大于等于Inc，也就是处理i++ i--
      if (ty == CHAR && t >= PTR) {
        printf("%d: cannot convert char type into pointer\n", line); exit(-1);
      }
      ty = t;
    }
    else if (tk == Struct) { // 处理struct类型转换
      next();
      if (stt_metas[id[STMetaType]].defined == 0) { printf("%d: struct not defined\n", line); exit(-1); }
      t = id[STMetaType]; next();
      if (tk != Mul) { printf("%d: cast type struct is not allowed\n", line); exit(-1); }
      while (tk == Mul) { next(); t = t + PTR; }
      if (tk == ')') next(); else { printf("%d: bad cast\n", line); exit(-1); }
      expr(Inc); // 递归expr，优先级需要大于等于Inc，也就是处理i++ i--
      if ((ty >= STRUCT_BEGIN && ty < PTR) && t >= PTR) {
        printf("%d: cannot convert struct type into pointer\n", line); exit(-1);
      }
      ty = t;
    }
    else { // 直接作为普通的表达式处理，这个优先级最高，直接运算括号内部expr
      expr(Assign);
      if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    }
  }
  else if (tk == Mul) { // 指针 *ptr
    next(); expr(Inc); // 递归expr，优先级大于等于INC，只需要判断 ++ --
    if (ty >= PTR) ty = ty - PTR; else { printf("%d: bad dereference\n", line); exit(-1); } // 减一层PTR
    *++e = (ty == CHAR) ? LC : LI; // load，reg = *(reg)
  }
  else if (tk == And) { // &取地址
    next(); expr(Inc); // 递归expr，优先级大于等于INC，只需要判断 ++ --
    // 由于expr取变量时，默认是加上了LC/LI指令，这里需要取地址，则去掉这条load指令即可，对应的是
    if (*e == LC || *e == LI) --e; else { printf("%d: bad address-of\n", line); exit(-1); }
    ty = ty + PTR; // 再加一层PTR
  }
  else if (tk == '!') { next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = 0; *++e = EQ; ty = INT; }
  else if (tk == '~') { next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = -1; *++e = XOR; ty = INT; }
  else if (tk == Add) { next(); expr(Inc); ty = INT; }
  else if (tk == Sub) {
    next(); *++e = IMM;
    // 对于数字，直接加个符号，对于变量，使用MUL乘法运算乘上-1
    if (tk == Num) { *++e = -ival; next(); } else { *++e = -1; *++e = PSH; expr(Inc); *++e = MUL; }
    ty = INT;
  }
  else if (tk == Inc || tk == Dec) { // 自增和自减 ++i --i
    t = tk; next(); expr(Inc);
    // 可以参考下面 i++ i-- 的注释，这部分逻辑更简单，因为是先进行加减操作，再传给左值
    if (*e == LC) { *e = PSH; *++e = LC; }
    else if (*e == LI) { *e = PSH; *++e = LI; }
    else { printf("%d: bad lvalue in pre-increment\n", line); exit(-1); }
    *++e = PSH;
    *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
    *++e = (t == Inc) ? ADD : SUB;
    *++e = (ty == CHAR) ? SC : SI;
  }
  else { printf("%d: bad expression\n", line); exit(-1); }

  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method
                      // 参考逆波兰式，例如 1+1 先把两个1入栈，运算符在栈顶，也就是 | 1 | 1 | + |
                      // 1+1*2则是 | 1 | 1 | 2 | * | + |，先运算乘法，然后运算加法
                      // 这种expr入栈的操作是递归的，使用优先级爬坡的方式设定终点，也就是终止于最高优先级
                      // 这里有一个while循环，是指在每一层expr递归时也是可以往前遍历的
                      // 例如1+1*2/2，除号和乘号优先级平行，不需要递归，是顺序遍历的，对应的栈如下
                      // | 1 | 1 | 2 | * | 2 | / | + |
    t = ty;
    // TODO: 处理结构体赋值
    if (tk == Assign) {
      next();
      // 修改LC/LI为PSH，把变量栈地址入栈，后面需要SC/SI store 变量
      if (*e == LC || *e == LI) *e = PSH; else { printf("%d: bad lvalue in assignment\n", line); exit(-1); }
      expr(Assign); *++e = ((ty = t) == CHAR) ? SC : SI;
    }
    else if (tk == Cond) { // 用jump来处理 a == 1 ? 0 : 1; 这样的运算，参考下图
      // |   BZ  |
      // |   b1  |
      // |   01  |
      // |  ...  |
      // |   0n  | 
      // |  JMP  |   
      // |   b2  |
      // |   11  | <-- b1
      // |  ...  |
      // |   1n  |
      // |       | <-- b2
      //
      // if else 和这个类似
      // 如果BZ判定成功，则顺序执行01-0n代码，到了JMP跳出else部分，否则，跳转到b1执行11-1n的else逻辑代码 
      next();
      *++e = BZ; d = ++e; // BZ 然后下一条指令直接跳过，先暂存在d中
      expr(Assign); // 冒号前的语句
      if (tk == ':') next(); else { printf("%d: conditional missing colon\n", line); exit(-1); }
      *d = (int)(e + 3); *++e = JMP; d = ++e; // BZ后面那个指令，存 e + 3 这个地址，也就是跳过了JMP xxx，到了Cond中
      expr(Cond); // 冒号后的语句
      *d = (int)(e + 1); // BZ/JMP后面那个指令，存 e + 1这个地址，也就是直接到了下一条指令
    }
    else if (tk == Lor) { next(); *++e = BNZ; d = ++e; expr(Lan); *d = (int)(e + 1); ty = INT; }
    else if (tk == Lan) { next(); *++e = BZ;  d = ++e; expr(Or);  *d = (int)(e + 1); ty = INT; }
    else if (tk == Or)  { next(); *++e = PSH; expr(Xor); *++e = OR;  ty = INT; }
    else if (tk == Xor) { next(); *++e = PSH; expr(And); *++e = XOR; ty = INT; }
    else if (tk == And) { next(); *++e = PSH; expr(Eq);  *++e = AND; ty = INT; }
    else if (tk == Eq)  { next(); *++e = PSH; expr(Lt);  *++e = EQ;  ty = INT; }
    else if (tk == Ne)  { next(); *++e = PSH; expr(Lt);  *++e = NE;  ty = INT; }
    else if (tk == Lt)  { next(); *++e = PSH; expr(Shl); *++e = LT;  ty = INT; }
    else if (tk == Gt)  { next(); *++e = PSH; expr(Shl); *++e = GT;  ty = INT; }
    else if (tk == Le)  { next(); *++e = PSH; expr(Shl); *++e = LE;  ty = INT; }
    else if (tk == Ge)  { next(); *++e = PSH; expr(Shl); *++e = GE;  ty = INT; }
    else if (tk == Shl) { next(); *++e = PSH; expr(Add); *++e = SHL; ty = INT; }
    else if (tk == Shr) { next(); *++e = PSH; expr(Add); *++e = SHR; ty = INT; }
    else if (tk == Add) {
      next(); *++e = PSH; expr(Mul); // 把加号后面的参数PUSH，然后expr递归，优先级要大于等于mul乘法
      if ((ty = t) > PTR) { 
        // 如果是指针，把前面计算的值PUSH进去，然后 IMM sizeof(int)，也就是指针长度，然后MUL，指针值单个长度乘以偏移
        // 这里type指向的是递归前数的值，所以就是把后面加上来的值乘以指针长度
        *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;
      }
      *++e = ADD; // 递归调用后再ADD，相当于把ADD指令放在了栈顶
                  // 如果对应于AST生成树，也就是先把ADD当做节点，先计算右子树，优先级越高离根越远
    }
    else if (tk == Sub) {
      next(); *++e = PSH; expr(Mul);
      // 指针减法分为两种，两个指针相减，得到的是索引的差，类似于数组的下标，如果是指针与数相减，得到的是指针的地址
      if (t > PTR && t == ty) {
        *++e = SUB; *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = DIV; ty = INT; // 两个指针地址相减
      }
      else if ((ty = t) > PTR) { 
        *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL; *++e = SUB; // 这里和加法就是一样的了
      }
      else *++e = SUB;
    }
    else if (tk == Mul) { next(); *++e = PSH; expr(Inc); *++e = MUL; ty = INT; }
    else if (tk == Div) { next(); *++e = PSH; expr(Inc); *++e = DIV; ty = INT; }
    else if (tk == Mod) { next(); *++e = PSH; expr(Inc); *++e = MOD; ty = INT; }
    else if (tk == Inc || tk == Dec) {
      // i++ i--，处理和--i ++i 类似
      // 示例如下:
      // char *c;
      // char i;
      // c = malloc(1);
      // *c = 'a';
      // i = *c++;
      // 跟c相关的有三个地址，&c c的栈地址，c 分配的内存指针地址，*c 指针指向的值
      // PSH LC    ==> | &c  | reg = c 
      // PSH       ==> | &c  | c | reg = c    
      // IMM 1 ADD ==> | &c  | reg = c + 1 
      // SC        ==> |     | *(&c) = c + 1 reg = c + 1
      // PSH       ==> | c+1 | reg = c + 1
      // IMM 1 SUB ==> |     | reg = c
      // 最后的结果就是赋值给i的是c，但c的地址已经+1了
      // i = *c++ 的*星号取值部分在对应的expr进行处理
      if (*e == LC) { *e = PSH; *++e = LC; } // 由于需要对地址做偏移操作，所以改为PSH LC/LI
      else if (*e == LI) { *e = PSH; *++e = LI; }
      else { printf("%d: bad lvalue in post-increment\n", line); exit(-1); }
      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
      *++e = (tk == Inc) ? ADD : SUB; // ++ --
      *++e = (ty == CHAR) ? SC : SI;
      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
      *++e = (tk == Inc) ? SUB : ADD; // 赋给左值时，反过来操作一下
      next();
    }
    else if (tk == Brak) { //处理数组索引 例如 a[2] = 5;
      next(); *++e = PSH; expr(Assign); // 把第一个值PSH，然后expr递归计算
      if (tk == ']') next(); else { printf("%d: close bracket expected\n", line); exit(-1); }
      if (t > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  } // 对于除开char，都要做指针计算
      else if (t < PTR) { printf("%d: pointer type expected\n", line); exit(-1); }
      *++e = ADD; // 得到偏移地址
      *++e = ((ty = t - PTR) == CHAR) ? LC : LI; // load 指针指向的值 
    }
    // 处理struct -> arrow符号
    else if (tk == Arrow) {
      // 先判断是不是struct *
      if (t < (STRUCT_BEGIN + PTR) || t >= (PTR * 2)) {
        printf("%d: -> op not struct pointer\n", line); exit(-1);
      }
      ty = t - PTR;
      // 然后判断->指向的id合不合法
      next();
      if (stt_metas[ty].defined == 0) { printf("%d: struct not defined\n", line); exit(-1); }
      ptr = stt_metas[ty].ids;
      mem = 0;
      while (ptr) { 
        if (ptr->id[Hash] == id[Hash]) { mem = ptr; ptr = 0; }
        else ptr = ptr->next;
      }
      if (mem == 0) { printf("%d: struct has no member\n", line); exit(-1); }
      next();
      // PSH入栈
      *++e = PSH;
      // 进行offset运算，相加，得到成员变量的地址
      *++e = IMM; *++e = mem->id_offset; *++e = ADD;
      // 最后获取成员变量地址的值
      *++e = ((ty = mem->id_type) == CHAR) ? LC : LI;
    }
    // 处理struct . 符号
    else if (tk == Dot) {
      // 先判断是不是struct
      if (t < STRUCT_BEGIN || t >= PTR) {
        printf("%d: dot op not struct\n", line); exit(-1);
      }
      // 然后判断 . 指向的id合不合法
      next();
      if (stt_metas[ty].defined == 0) { printf("%d: struct not defined\n", line); exit(-1); }
      ptr = stt_metas[ty].ids;
      mem = 0;
      while (ptr) {
        if (ptr->id[Hash] == id[Hash]) { mem = ptr; ptr = 0; }
        else ptr = ptr->next;
      }
      if (mem == 0) { printf("%d: struct has no member\n", line); exit(-1); }
      next();
      // 由于上一条指令是LI已经load到了结构体的内存地址，去掉这一条指令，直接操作地址
      if (*e == LI) { *e = PSH; }
      else { printf("%d: struct var error\n", line); exit(-1); }
      // 进行offset运算，相加，得到成员变量的地址
      *++e = IMM; *++e = mem->id_offset; *++e = ADD;
      // 最后获取成员变量地址的值
      *++e = ((ty = mem->id_type) == CHAR) ? LC : LI;
    }
    else { printf("%d: compiler error tk=%d\n", line, tk); exit(-1); }
  }
}

void stmt() // 不支持for循环
{
  int *a, *b;

  if (tk == If) {
    next();
    if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); }
    expr(Assign);
    if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    *++e = BZ; b = ++e;
    stmt();
    if (tk == Else) {
      *b = (int)(e + 3); *++e = JMP; b = ++e;
      next();
      stmt();
    }
    *b = (int)(e + 1);
  }
  else if (tk == While) {
    next();
    a = e + 1;
    if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); }
    expr(Assign);
    if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    *++e = BZ; b = ++e;
    stmt();
    *++e = JMP; *++e = (int)a;
    *b = (int)(e + 1);
  }
  else if (tk == Return) {
    next();
    if (tk != ';') expr(Assign);
    *++e = LEV;
    if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); }
  }
  else if (tk == '{') {
    next();
    while (tk != '}') stmt();
    next();
  }
  else if (tk == ';') {
    next();
  }
  else {
    expr(Assign);
    if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); }
  }
}

int main(int argc, char **argv)
{
  int fd, bt, mem_bt, ty, poolsz, *idmain;
  int *pc, *sp, *bp, a, cycle; // vm registers
  int i, j, k, *t; // temps
  struct stt_meta *stm; // struct meta
  struct stt_meta_id *stm_id; // struct meta id
  struct stt_meta_id **cur;
  int sttotal;
  int stindex;
  struct proto_meta *pproto;
  struct proto_meta *proto_ptr;
  int protoindex;
  int *d;
  struct proto_meta_param **pmp_cur, **trans_write_cur, **trans_read_cur;
  int wo,ro;
  char *str;

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'p') { proto = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c4 [-s] [-d] [-p] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256*1024; // arbitrary size
  sttotal = PTR;
  stindex = STRUCT_BEGIN;
  protoindex = 0;
  g_proto.id = 1;
  g_proto.number = 0x1000;
  g_proto.uniq_id = 1;
  if (!(sym = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; }
  if (!(le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; }
  if (!(data = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; }
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%d) stack area\n", poolsz); return -1; }
  if (!(stt_metas = malloc(sttotal * sizeof(struct stt_meta)))) { printf("could not malloc struct meta area\n"); return -1; }
  if (!(proto_metas = malloc(sttotal * sizeof(struct proto_meta)))) { printf("could not malloc proto area\n"); return -1; }

  memset(sym,  0, poolsz);
  memset(e,    0, poolsz);
  memset(data, 0, poolsz);
  memset(stt_metas,  0, sttotal * sizeof(struct stt_meta));
  memset(proto_metas,  0, sttotal * sizeof(struct proto_meta));

  // 先把这些特殊符号加到id table上，最后一个是main，程序从main开始运行
  p = "char else enum if int struct IO_RO IO_WO IO_RW return sizeof while "
      "open read close printf malloc free memset memcmp memcpy exit void main";
  i = Char; while (i <= While) { next(); id[Tk] = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { next(); id[Class] = Sys; id[Type] = INT; id[Val] = i++; } // add library to symbol table
  next(); id[Tk] = Char; // handle void type
  next(); idmain = id; // keep track of main

  if (!(lp = p = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }
  if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }
  p[i] = 0;
  close(fd);

  // parse declarations
  line = 1;
  next();
  while (tk) {
    bt = INT; // basetype
    if (tk == Int) next(); // 处理 int 开头的定义
    else if (tk == Char) { next(); bt = CHAR; } // 处理 char 开头的定义 
    else if (tk == Enum) { // 处理 Enum 定义
      next();
      if (tk != '{') next();
      if (tk == '{') {
        next();
        i = 0;
        while (tk != '}') {
          if (tk != Id) { printf("%d: bad enum identifier %d\n", line, tk); return -1; }
          next();
          if (tk == Assign) {
            next();
            if (tk != Num) { printf("%d: bad enum initializer\n", line); return -1; }
            i = ival;
            next();
          }
          id[Class] = Num; id[Type] = INT; id[Val] = i++; // Enum，class作为Num处理
          if (tk == ',') next();
        }
        next();
      }
    }
    else if (tk == Struct) { // 处理 Struct 结构体
      next();
      if (tk != Id) { printf("%d: expected struct id, but token %d\n", line, tk); return -1; }
      next();
      if (tk == '{') { // 结构体定义
        // 先判断结构体有没有重复定义
        if (stt_metas[id[STMetaType]].defined != 0) { printf("%d: struct redefined\n", line); return -1; }
        id[STMetaType] = stindex;
        stm = &stt_metas[stindex]; // 获取当前结构体meta
        bt = stindex;
        stindex++;
        k = 0; // 成员变量offset
        cur = &stm->ids;
        next();
        while (tk != '}') {
          // 先判断成员变量类型
          if (tk == Int) { next(); mem_bt = INT;}
          else if (tk == Char) { next(); mem_bt = CHAR;}
          else if (tk == Struct) { // 结构体嵌套定义
            next();
            if (tk != Id) { printf("%d: expected struct id, but token %d\n", line, tk); return -1; }
            // 判断结构体是否定义过
            if (stt_metas[id[STMetaType]].defined == 0 && id[STMetaType] != (stindex - 1)) { printf("%d: struct not defined\n", line); return -1; }
            mem_bt = id[STMetaType];
            next();
          }
          // 然后处理对应的成员变量
          while (tk != ';') {
            ty = mem_bt;
            while (tk == Mul) { next(); ty = ty + PTR; } // 有*号，是指针，递归进行
            if (tk != Id) { printf("%d: bad struct member declaration\n", line); return -1; }
            stm_id = (struct stt_meta_id *)malloc(sizeof(struct stt_meta_id));
            memset(stm_id, 0, sizeof(struct stt_meta_id));
            stm_id->id = id;
            // 这里的int其实是long long，暂时只考虑8字节对齐
            if (ty == CHAR) { j = 1; }
            else if (ty >= STRUCT_BEGIN && ty < PTR) {
              j = stt_metas[ty].size;
              k = (k + sizeof(int) - 1) & -sizeof(int);
            }
            else {
              j = sizeof(int);
              k = (k + sizeof(int) - 1) & -sizeof(int);
            }
            stm_id->id_offset = k;
            k = k + j;
            stm_id->id_size = j;
            stm_id->id_type = ty;
            *cur = stm_id;
            cur = &((*cur)->next);
            stm->id_num++;
            next();
            if (tk == ',') next();
          }
          next();
        }
        stm->size = k;
        stm->defined = 1;
        next();
      } 
      else { // 已经定义过的 
        if (stt_metas[id[STMetaType]].defined == 0) { printf("%d: struct not defined\n", line); return -1; }
        bt = id[STMetaType];
      }
    }
    while (tk != ';' && tk != '}') {
      ty = bt; // 符号类型，或函数返回值类型
      while (tk == Mul) { next(); ty = ty + PTR; } // 有*号，是指针，递归进行
      if (tk != Id) { printf("%d: bad global declaration\n", line); return -1; }
      if (id[Class]) { printf("%d: duplicate global definition\n", line); return -1; }
      next();
      id[Type] = ty;
      // TODO: 待处理结构体函数返回值
      if (tk == '(') { // function 处理函数定义
        d = id;
        id[Class] = Fun;
        id[Val] = (int)(e + 1); // 函数地址
        if (proto) {
          pproto = &proto_metas[protoindex++];
          pproto->id = d;
          pproto->name = malloc(d[NameLen] + 1);
          j = 0;
          while (j < d[NameLen]) {
            pproto->name[j] = ((char *)d[Name])[j];
            j++;
          }
          pproto->name[j] = '\0';
          pproto->name_size = j + 1;
          // printf("proto [%s]\n", pproto->name);
          pmp_cur = &pproto->id_params;
        }
        next(); i = 0;
        while (tk != ')') { // 处理函数参数定义
          ty = INT;
          if (tk == Int) next();
          else if (tk == Char) { next(); ty = CHAR; }
          else if (tk == Struct) {
            next();
            if (stt_metas[id[STMetaType]].defined == 0) { printf("%d: struct not defined\n", line); return -1; }
            ty = id[STMetaType];
            next();
          }
          while (tk == Mul) { next(); ty = ty + PTR; }
          if (tk != Id) { printf("%d: bad parameter declaration\n", line); return -1; }
          if (id[Class] == Loc) { printf("%d: duplicate parameter definition\n", line); return -1; }
          // 如果之前定义过该变量（例如全局），暂存在HClass HType HVal中（如果没定义过则是空值），并设置新的Class Type Val
          id[HClass] = id[Class]; id[Class] = Loc;
          id[HType]  = id[Type];  id[Type] = ty;
          if (id[Type] >= STRUCT_BEGIN && id[Type] < PTR) { // 结构体变量
            k = (stt_metas[id[Type]].size + sizeof(int) - 1) / sizeof(int);
            if (k == 0) k = 1; // 没有成员的结构体也分配空间
            i = i + k;
            id[HVal] = id[Val];   id[Val] = i - 1;  // 局部变量是高地址往低地址走，所以先要索引到结构体的低地址
                                                    // 函数参数与局部变量的位置在不同的区域，看栈帧结构
          } else {
            id[HVal] = id[Val];   id[Val] = i++;
          }
          d = id;
          next();
          if (proto) {
            *pmp_cur = (struct proto_meta_param *)malloc(sizeof(struct proto_meta_param));
            (*pmp_cur)->ty = d[Type];
            (*pmp_cur)->uniq_id = g_proto.uniq_id++;
            if (tk == Ro || tk == Wo || tk == Rw) {
              (*pmp_cur)->io = tk;
              next();
            }
            // printf("proto param type %d io[%d]\n", (*pmp_cur)->ty, (*pmp_cur)->io);
            pmp_cur = &((*pmp_cur)->next);
          }    
          if (tk == ',') next();
        }
        next();
        if (proto) {
          if (tk != ';') { printf("%d: bad function proto\n", line); return -1; }
          pproto->defined = 1;
          id = sym; // unwind symbol table locals
          while (id[Tk]) {
            if (id[Class] == Loc) { // 恢复Loc变量Class Type Val
              id[Class] = id[HClass];
              id[Type] = id[HType];
              id[Val] = id[HVal];
            }
            id = id + Idsz;
          }
        }
        else if (tk != '{') { printf("%d: bad function\n", line); return -1; }
        else {
          loc = ++i; // loc等于++i，以上计算了函数的参数，下面是函数的局部变量
          next();
          while (tk == Int || tk == Char || tk == Struct) { // 处理函数局部变量定义
            if (tk == Int) { bt = INT; }
            else if (tk == Char) { bt = CHAR; }
            else if (tk == Struct) {
              next();
              if (stt_metas[id[STMetaType]].defined == 0) { printf("%d: struct not defined\n", line); return -1; }
              bt = id[STMetaType];
            }
            next();
            while (tk != ';') {
              ty = bt;
              while (tk == Mul) { next(); ty = ty + PTR; }
              if (tk != Id) { printf("%d: bad local declaration\n", line); return -1; }
              if (id[Class] == Loc) { printf("%d: duplicate local definition\n", line); return -1; }
              // 如果之前定义过该变量，暂存在HClass HType HVal中，并设置新的Class Type Val
              id[HClass] = id[Class]; id[Class] = Loc;
              id[HType]  = id[Type];  id[Type] = ty;
              if (id[Type] >= STRUCT_BEGIN && id[Type] < PTR) { // 结构体变量
                k = (stt_metas[id[Type]].size + sizeof(int) - 1) / sizeof(int);
                if (k == 0) k = 1; // 没有成员的结构体也分配空间
                id[HVal] = id[Val];   id[Val] = i + k; // 局部变量是高地址往低地址走，所以先要索引到结构体的低地址
                i = i + k;
              } else {
                id[HVal] = id[Val];   id[Val] = ++i; // 局部变量从2开始计数
              }
              next();
              if (tk == ',') next();
            }
            next();
          }
          *++e = ENT; *++e = i - loc; // enter 函数，ENT i - loc，也就是局部变量的个数
          while (tk != '}') stmt(); // 处理函数内部语句
          *++e = LEV; // 退出函数
          id = sym; // unwind symbol table locals
          while (id[Tk]) {
            if (id[Class] == Loc) { // 恢复Loc变量Class Type Val
              id[Class] = id[HClass];
              id[Type] = id[HType];
              id[Val] = id[HVal];
            }
            id = id + Idsz;
          }          
        }
      }
      else { // 全局变量
        id[Class] = Glo;
        id[Val] = (int)data;
        if (id[Type] >= STRUCT_BEGIN && id[Type] < PTR) { // 结构体变量
          k = stt_metas[id[Type]].size;
          if (k == 0) k = sizeof(int); // 没有成员的结构体也分配空间
          data = data + k; // 分配了结构体的长度
        } else {
          data = data + sizeof(int); // 分配了一个int长度的数据内存
        }
      }
      if (tk == ',') next();
    }
    next();
  }

  if (proto) {
    proto_ptr = proto_metas;
    while (proto_ptr->defined) {
      printf("=======================================\n");
      printf("[%s]\n", proto_ptr->name);
      pmp_cur = &proto_ptr->id_params;
      while ((*pmp_cur)) {
        printf("  [ty:%d IO:%s]\n", (*pmp_cur)->ty, (*pmp_cur)->io == Ro ? "RO" : (*pmp_cur)->io == Wo ? "WO" : (*pmp_cur)->io == Rw ? "RW" : "N/A");
        pmp_cur = &((*pmp_cur)->next);
      }
      printf("\n");
      // 1. 生成proto的cmd
      j = 0;
      proto_ptr->cmd.name = malloc(proto_ptr->name_size);
      while (j < proto_ptr->name_size) {
        if (proto_ptr->name[j] >= 'a' && proto_ptr->name[j] <= 'z') {
          proto_ptr->cmd.name[j] = proto_ptr->name[j] - 32;
        } else {
          proto_ptr->cmd.name[j] = proto_ptr->name[j];
        }
        j++;
      }
      proto_ptr->cmd.number = g_proto.number++;
      printf("#define %s 0x%x\n", proto_ptr->cmd.name, proto_ptr->cmd.number);
      printf("\n");
      // 2. 生成proto的RPC struct
      trans_write_cur = &proto_ptr->transaction.write;
      trans_read_cur = &proto_ptr->transaction.read;
      pmp_cur = &proto_ptr->id_params;
      while (*pmp_cur) {
        wo = 0; ro = 0;
        if ((*pmp_cur)->ty >= PTR) { // 指针类型才有读写方向
          if ((*pmp_cur)->io == Wo) {
            wo = 1;
          } else if ((*pmp_cur)->io == Rw) {
            wo = 1; ro = 1;
          } else {
            ro = 1;
          }
        } else {
          ro = 1;
        }
        if (wo == 1) {
          *trans_write_cur = (struct proto_meta_param *)malloc(sizeof(struct proto_meta_param));
          (*trans_write_cur)->ty = (*pmp_cur)->ty;
          (*trans_write_cur)->io = Wo;
          (*trans_write_cur)->uniq_id = (*pmp_cur)->uniq_id;
          trans_write_cur = &((*trans_write_cur)->next);
        }
        if (ro == 1) {
          *trans_read_cur = (struct proto_meta_param *)malloc(sizeof(struct proto_meta_param));
          (*trans_read_cur)->ty = (*pmp_cur)->ty;
          (*trans_read_cur)->io = Ro;
          (*trans_read_cur)->uniq_id = (*pmp_cur)->uniq_id;
          trans_read_cur = &((*trans_read_cur)->next);
        }
        pmp_cur = &((*pmp_cur)->next);
      }
      str = "_trans_write_t";
      proto_ptr->transaction.write_name_size = proto_ptr->name_size + strlen(str);
      proto_ptr->transaction.write_name = malloc(proto_ptr->transaction.write_name_size);
      memcpy(proto_ptr->transaction.write_name, proto_ptr->name, proto_ptr->name_size - 1);
      memcpy(proto_ptr->transaction.write_name + proto_ptr->name_size - 1, str, strlen(str));
      proto_ptr->transaction.write_name[proto_ptr->transaction.write_name_size - 1] = '\0';
      str = "_trans_read_t";
      proto_ptr->transaction.read_name_size = proto_ptr->name_size + strlen(str);
      proto_ptr->transaction.read_name = malloc(proto_ptr->transaction.read_name_size);
      memcpy(proto_ptr->transaction.read_name, proto_ptr->name, proto_ptr->name_size - 1);
      memcpy(proto_ptr->transaction.read_name + proto_ptr->name_size - 1, str, strlen(str));
      proto_ptr->transaction.read_name[proto_ptr->transaction.read_name_size - 1] = '\0';

      pmp_cur = &proto_ptr->transaction.write;
      if (*pmp_cur) {
        printf("struct %s {\n", proto_ptr->transaction.write_name);
        printf("};\n\n");
      }
      pmp_cur = &proto_ptr->transaction.read;
      if (*pmp_cur) {
        printf("struct %s {\n", proto_ptr->transaction.read_name);
        printf("};\n");
      }
      // 3. 生成proto的RPC interface
      printf("=======================================\n");
      printf("\n\n");
      proto_ptr++;
    }
    return 0;
  }

  if (!(pc = (int *)idmain[Val])) { printf("main() not defined\n"); return -1; } // pc指针指向idman[Val] 也就是main在汇编中的地址
  if (src) return 0;

  // setup stack
  bp = sp = (int *)((int)sp + poolsz); // bp是base pointer基地址
  *--sp = EXIT; // call exit if main returns
                // 栈底，自上往下，EXIT和PSH其实是opcode，相当于这两个指令直接存在栈底
                // 这里要注意一点，当pc程序指针走到PSH时，是继续往高地址走，而不是像栈往低地址走
                // 所以当退出main函数时，最后两条指令是PSH EXIT，PSH是把return的值入栈，然后EXIT取走这个值
  *--sp = PSH; t = sp; // PUSH
  *--sp = argc; // main第一个参数
  *--sp = (int)argv; // main第二个参数
  *--sp = (int)t; // 这里存的是t的地址，也就是指向栈底的PSH opcode位置 

  // 栈帧布局参考下图
  // 以进入 main 为例
  // LEA 3 指向的是 argc
  // LEA 2 指向的是 argv
  // LEA 1 上一层调用的 pc+1 地址
  // LEA 0 是指向上一层调用的栈帧基地址，然后当前BP指向这一层的栈帧基地址
  // SP 指向栈顶
  // LEA -1 是第一个局部变量
  // LEA 是基于 BP 做偏移，如果是正值，则是往上走，可以索引函数参数，如果是负值，往下走，可以索引局部变量
  // ENT 进入函数时，会带上这个函数的参数个数
  //
  // |  arg1  |  3
  // |  arg2  |  2
  // | pc + 1 |  1
  // | pre bp |  0
  // | local1 | -1
  // | local2 | -2
 
  // run...
  cycle = 0;
  while (1) {
    i = *pc++; ++cycle; // 每个指令就是一个PC，ENT 2是两个PC，分别是ENT和2
    if (debug) {
      printf("%d> %.4s", cycle,
        &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
         "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
         "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,MCPY,SLEN,EXIT,"[i * 5]);
      if (i <= ADJ) printf(" %d\n", *pc); else printf("\n");
    }
    if      (i == LEA) a = (int)(bp + *pc++);                             // load local address
    else if (i == IMM) a = *pc++;                                         // load global address or immediate
    else if (i == JMP) pc = (int *)*pc;                                   // jump
    // 把这一层的下一条pc指令存入sp中，pc更新为跳转的地址
    else if (i == JSR) { *--sp = (int)(pc + 1); pc = (int *)*pc; }        // jump to subroutine
    // 如果相等则顺序执行，如果不等则跳转到指向的地址
    else if (i == BZ)  pc = a ? pc + 1 : (int *)*pc;                      // branch if zero
    else if (i == BNZ) pc = a ? (int *)*pc : pc + 1;                      // branch if not zero
    // 存上一层bp栈帧基地址，然后bp更新为当前sp地址，sp再指向新的stack栈顶，偏移量直接跳过了局部变量个数
    else if (i == ENT) { *--sp = (int)bp; bp = sp; sp = sp - *pc++; }     // enter subroutine
    else if (i == ADJ) sp = sp + *pc++;                                   // stack adjust
    // 退出当前函数调用，bp取回上一层的栈帧基地址，pc取回之前存入的pc地址，sp往上移动两个位置
    else if (i == LEV) { sp = bp; bp = (int *)*sp++; pc = (int *)*sp++; } // leave subroutine
    else if (i == LI)  a = *(int *)a;                                     // load int
    else if (i == LC)  a = *(char *)a;                                    // load char
    // SI/SC从栈顶取变量赋值给地址指向的值
    else if (i == SI)  *(int *)*sp++ = a;                                 // store int
    else if (i == SC)  a = *(char *)*sp++ = a;                            // store char
    else if (i == PSH) *--sp = a;                                         // push

    // 以下运算，都是要从栈中取出一个值，然后跟 a 寄存器做相应的运算，再更新到 a
    else if (i == OR)  a = *sp++ |  a;
    else if (i == XOR) a = *sp++ ^  a;
    else if (i == AND) a = *sp++ &  a;
    else if (i == EQ)  a = *sp++ == a;
    else if (i == NE)  a = *sp++ != a;
    else if (i == LT)  a = *sp++ <  a;
    else if (i == GT)  a = *sp++ >  a;
    else if (i == LE)  a = *sp++ <= a;
    else if (i == GE)  a = *sp++ >= a;
    else if (i == SHL) a = *sp++ << a;
    else if (i == SHR) a = *sp++ >> a;
    else if (i == ADD) a = *sp++ +  a;
    else if (i == SUB) a = *sp++ -  a;
    else if (i == MUL) a = *sp++ *  a;
    else if (i == DIV) a = *sp++ /  a;
    else if (i == MOD) a = *sp++ %  a;

    // 以下都是系统调用，出栈在函数调用处做了处理
    else if (i == OPEN) a = open((char *)sp[1], *sp);
    else if (i == READ) a = read(sp[2], (char *)sp[1], *sp);
    else if (i == CLOS) a = close(*sp);
    // 这里printf最多处理6个参数，引用pc是方便取参数
    // 例如 printf("%d\n", a) 当前 pc 为 ADJ 2，pc[1] = 2，也就是参数个数
    // sp + 2 得到的是第一个参数往上的栈地址，然后以此为基准来按顺序取参数
    // 这里实际上引用了一些空参数，但由于printf自己做了处理所有没有关系
    else if (i == PRTF) { t = sp + pc[1]; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]); }
    else if (i == MALC) a = (int)malloc(*sp);
    else if (i == FREE) free((void *)*sp);
    else if (i == MSET) a = (int)memset((char *)sp[2], sp[1], *sp);
    else if (i == MCMP) a = memcmp((char *)sp[2], (char *)sp[1], *sp);
    else if (i == MCPY) a = memcpy((char *)sp[2], (char *)sp[1], *sp);
    else if (i == SLEN) a = strlen((char *)*sp);
    else if (i == EXIT) { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; }
    else { printf("unknown instruction = %d! cycle = %d\n", i, cycle); return -1; }
  }
}
