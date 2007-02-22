.class public org/apache/harmony/drlvm/tests/regression/h3067/WideGoto
.super java/lang/Object 
.method public <init>()V 
    aload_0 
    invokespecial java/lang/Object/<init>()V 
    return 
.end method 
.method public test([Ljava/lang/String;)V 
    .limit stack 1 
    .limit locals 2

    nop           ; 20 nops
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop           ; replaced with wide on fly
    goto Label1
Label1:
    return        ; return from main
.end method

