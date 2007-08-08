.class public org/apache/harmony/drlvm/tests/regression/h4579/neg
.super java.security.SecureClassLoader
.method public <init>()V
   aload_0
   invokespecial java.security.SecureClassLoader/<init>()V
   return
.end method

;
; super class (SecureClassLoader) is loaded by different (bootstrap) classloader than the current class
; super of the super class (ClassLoader) has protected method
; try to invoke it with invokevirtual
;
;
.method public static test()V
   ; obtain some classloader
   ldc	"org.apache.harmony.drlvm.tests.regression.h4579.neg"
   invokestatic	java/lang/Class/forName(Ljava/lang/String;)Ljava/lang/Class;

   invokevirtual java/lang/Class/getClassLoader()Ljava/lang/ClassLoader;

   ; try to invoke its protected method
   invokevirtual java/lang/ClassLoader/getPackages()[Ljava/lang/Package;
   return
.end method


.method public static main([Ljava/lang/String;)V
   invokestatic	org/apache/harmony/drlvm/tests/regression/h4579/neg/test()V
   return
.end method


