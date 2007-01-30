.class public org/apache/harmony/drlvm/tests/regression/h2808/finTest1
.super java/lang/Object
;
; standard initializer
.method public <init>()V
   aload_0
   invokespecial java/lang/Object/<init>()V
   return
.end method 
; final method
.method public final testMethod()I
  .limit locals 2
  .limit stack 2
  sipush 1
  ireturn
.end method
;
.method public test()V
  .limit stack 2
  .limit locals 2
  new org/apache/harmony/drlvm/tests/regression/h2808/finTest2
  dup
  invokespecial org/apache/harmony/drlvm/tests/regression/h2808/finTest2/<init>()V
  aload 0
  invokevirtual org/apache/harmony/drlvm/tests/regression/h2808/finTest2/testMethod()V
  return
.end method
