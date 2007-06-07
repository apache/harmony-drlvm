.class public org/apache/harmony/drlvm/tests/regression/H2113/ExcInFinallyTest
.super java/lang/Object

.method public <init>()V
   aload_0
   invokespecial java/lang/Object/<init>()V
   return
.end method

.method public static test()V
.limit stack 3
.limit locals 6

	bipush 1
	newarray int
	astore_1
	iconst_0
	istore_2
	aload_1
TryStart:
	iload_2
	iconst_0
	iastore        ; <-- use var2
	goto EndSubroutine
TryEnd1:
    pop
	jsr BeginSubroutine
    goto ExitLabel
BeginSubroutine:   ; begin sub
	astore 4
    iconst_1
    iconst_0
    idiv           ; <-- guaranteed exception here
    istore_3
	ret 4
EndSubroutine:     ; end sub
	jsr BeginSubroutine
	iinc 2 1       ; <-- use var2
	goto ExitLabel
TryEnd2:
	pop
	getstatic java.lang.System.out Ljava/io/PrintStream;
	ldc "PASS"
	invokevirtual java/io/PrintStream/println(Ljava/lang/String;)V
ExitLabel:
	return

.catch all from TryStart to TryEnd1 using TryEnd1
.catch all from TryStart to TryEnd2 using TryEnd2
.end method
