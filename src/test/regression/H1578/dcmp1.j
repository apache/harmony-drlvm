.class public org.apache.harmony.drlvm.tests.regression.H1578.dcmp1
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
   .limit stack 6
   .limit locals 2
  
    ldc2_w -1.0d
    ldc2_w -0.0d
    ldc2_w 0.0d 
    ddiv
    dcmpg	
    ireturn  

.end method

