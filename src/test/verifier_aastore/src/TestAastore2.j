.class public TestAastore2
.super java/lang/Object
.field public testField [LTestInterface;

.method public <init>()V
   aload_0
   invokespecial java/lang/Object/<init>()V
   return
.end method

.method public test()V
   .limit stack 3
   .limit locals 2

   aload_0
   getfield TestAastore2/testField [LTestInterface;
   astore_1

   sipush 1
   sipush 1
   multianewarray [[LTestAastore; 2
   sipush 0

   ; target of a aastore instruction not assignment compatible
   ; with the class type specified in the instruction
   aload_1
   aastore

  return
.end method
