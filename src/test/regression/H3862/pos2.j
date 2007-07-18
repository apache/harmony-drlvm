.class public org/apache/harmony/drlvm/tests/regression/h3862/pos2
.super java/lang/Object
.method public <init>()V
   aload_0
   invokespecial java/lang/Object/<init>()V
   return
.end method

;
; Return store ret addr in sub-sub
;
.method public static test()V
   .limit stack 5
   .limit locals 5

   jsr L1
   return

L1:
   jsr L2
   ret 2

L2:
   astore 1
   astore 2
   ret 1

.end method
