.class public org/apache/harmony/drlvm/tests/regression/h2103/SupClass
.super java/lang/Object

.method public <init>()V
   aload_0
   invokespecial java/lang/Object/<init>()V
   return
.end method

.method public static test()I
  .limit locals 3
  .limit stack 3
   new org/apache/harmony/drlvm/tests/regression/h2103/SubClass
   dup
   invokespecial org/apache/harmony/drlvm/tests/regression/h2103/SubClass/<init>()V
   invokespecial org/apache/harmony/drlvm/tests/regression/h2103/SubClass/mth()I
   ireturn
.end method

