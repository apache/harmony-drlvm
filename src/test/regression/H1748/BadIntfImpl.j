.class public org/apache/harmony/drlvm/tests/regression/h1748/BadIntfImpl
.super java/lang/Object
.implements org/apache/harmony/drlvm/tests/regression/h1748/Intf

;
; standard initializer
.method public <init>()V
   aload_0
   invokespecial java/lang/Object/<init>()V
   return
.end method

.method protected test()V ; implement interface method as protected
  return
.end method
