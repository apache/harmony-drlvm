.class public org/apache/harmony/drlvm/tests/regression/h2196/LoopExcTest
.super java/lang/Object

.method public static main([Ljava/lang/String;)V
.limit stack 4
.limit locals 4

	iconst_0
	istore_2
    iconst_m1
    istore_3
    iload 2
    iconst_0
    isub
    ifeq FirstThrower
TryStart:
	aconst_null
    ldc "hi"
	invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V
    goto ExitLabel
FirstThrower:
	aconst_null
    ldc "hi"
	invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V
    goto ExitLabel
ExcTarget:
    pop
TryEnd1:
    iinc 3 1
    iload_3
    ifeq NextLabel
	iinc 2 1       ; <-- use var2
    iload 2
    ifne PassLabel
	aconst_null
    ldc "hi"
	invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V
NextLabel:
	aconst_null
    ldc "hi"
	invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V
NextLabel2:
	aconst_null
    ldc "hi"
	invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V
PassLabel:
	getstatic java.lang.System.out Ljava/io/PrintStream;
	ldc "PASS"
	invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V
ExitLabel:
	return

.catch all from TryStart to ExitLabel using ExcTarget
.end method
