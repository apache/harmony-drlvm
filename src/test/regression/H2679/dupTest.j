.class public org/apache/harmony/drlvm/tests/regression/h2679/DupTest
.super junit/framework/TestCase

.method public <init>()V
    .limit stack 1
    .limit locals 1
    aload_0
    invokespecial junit/framework/TestCase/<init>()V
    return
.end method

.method public testFloat()V
    .limit stack 4
    .limit locals 1
    fconst_1
    ldc 1.0f
    dup
    getstatic java/lang/System/err Ljava/io/PrintStream;
    swap
    invokevirtual java/io/PrintStream/println(F)V
    fconst_0
    invokestatic junit/framework/Assert/assertEquals(FFF)V
    return
.end method

.method public testDouble()V
    .limit stack 8
    .limit locals 1
    dconst_1
    dconst_1
    dup2
    getstatic java/lang/System/err Ljava/io/PrintStream;
    dup_x2
    pop
    invokevirtual java/io/PrintStream/println(D)V
    dconst_0
    invokestatic junit/framework/Assert/assertEquals(DDD)V
    return
.end method
