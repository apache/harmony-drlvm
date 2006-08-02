.class public TestInvokeSpecial
.super Super

.method public <init>()V
    aload_0
    invokespecial Super/<init>()V
    return
.end method

.method public static main([Ljava/lang/String;)V
   
    .limit stack 2
   
    new Super
    dup
    invokespecial Super/<init>()V

    ; object is instance of super class
    invokespecial Super/TestSuper()V
 
    return
.end method
