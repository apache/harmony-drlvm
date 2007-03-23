.class public org/apache/harmony/drlvm/tests/regression/h3446/SubSubTest
.super junit/framework/TestCase
.method public <init>()V
    aload_0
    invokespecial junit/framework/TestCase/<init>()V
    return
.end method

; 
; Subroutine is called from the other subroutine and 
; from the top level code. 
; 
.method public test()V 
   .limit stack 1 
   .limit locals 2 

   jsr LabelSub 
   jsr LabelSubSub 
   return 
LabelSub: 
   astore 1 
   jsr LabelSubSub 
   ret 1 
LabelSubSub: 
   astore 0 
   ret 0 
.end method 

