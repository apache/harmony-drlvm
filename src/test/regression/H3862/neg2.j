.class public org/apache/harmony/drlvm/tests/regression/h3862/neg2
.super java/lang/Object
.method public <init>()V
   aload_0
   invokespecial java/lang/Object/<init>()V
   return
.end method

;
; Return ret address remains on the stack after returning from subroutine
;
.method public static test()V
   .limit stack 5
   .limit locals 5

   jsr L1
   astore 1
   ret 1

   return

L1:
   dup
   astore 1
   ret 1

.end method
