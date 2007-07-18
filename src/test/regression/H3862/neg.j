.class public org/apache/harmony/drlvm/tests/regression/h3862/neg
.super java/lang/Object
.method public <init>()V
   aload_0
   invokespecial java/lang/Object/<init>()V
   return
.end method

;
; Subroutine call order differs from ret order
;
.method public static test()V
   .limit stack 1
   .limit locals 2

   jsr L1
   ret 0

L1:
   astore 1
   jsr L2
   return

L2:
   astore 0
   ret 1
.end method
