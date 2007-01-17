.class public org.apache.harmony.drlvm.tests.regression.H1578.fcmp1
.super java/lang/Object


;
; standard initializer
.method public <init>()V
   aload_0
   invokenonvirtual java/lang/Object/<init>()V
   return
.end method

;
; test method

.method public static test()I
   .limit stack 2
   .limit locals 2
 
  ldc -0.0f
  ldc 0.0f 
  fdiv
  ldc -3.4028235E39f
  
  fcmpl
  ireturn
.end method

