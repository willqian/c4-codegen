c4 - C in four functions 中文注释 + 代码生成
========================

RPC代码生成模式:
    
    ./c4 -p proto.h > proto_gen.c

An exercise in minimalism.

Try the following:

    gcc -o c4 c4.c
    ./c4 hello.c
    ./c4 -s hello.c
    
    ./c4 c4.c hello.c
    ./c4 c4.c c4.c hello.c
