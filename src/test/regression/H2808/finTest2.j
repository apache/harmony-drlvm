.class public org/apache/harmony/drlvm/tests/regression/h2808/finTest2
.super org/apache/harmony/drlvm/tests/regression/h2808/finTest1
;
; standard initializer
.method public <init>()V
   aload_0
   invokespecial finTest1/<init>()V
   return
.end method 
;
.method public final testMethod()I
  .limit locals 2
  .limit stack 2
  sipush 2
  ireturn
.end method

