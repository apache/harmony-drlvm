.class public org/apache/harmony/drlvm/tests/regression/h3098/JsrNoRet
.super java/lang/Object 
.method public <init>()V 
    aload_0 
    invokespecial java/lang/Object/<init>()V 
    return 
.end method 
.method public static testcase()V 
    .limit stack 2 
    .limit locals 2

    jsr LabelSub1
    jsr LabelSub2
    return

LabelSub1:
    astore 1
    jsr LabelSub3

LabelSub2:
    astore 1
    ret 1

LabelSub3:
;    return
    ret 1
    
.end method

