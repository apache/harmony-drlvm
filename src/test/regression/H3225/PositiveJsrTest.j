.class public org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest
.super junit/framework/TestCase
.method public <init>()V
    aload_0
    invokespecial junit/framework/TestCase/<init>()V
    return
.end method

;
; Launches testcases which check subroutine verification.
;
.method public static main([Ljava/lang/String;)V
    .limit stack 2
    .limit locals 1

    new org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest
    dup
    invokespecial org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest/<init>()V
    astore_0

    aload_0
    invokevirtual org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest/testMinimalLimits()V

    aload_0
    invokevirtual org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest/testLastJsr()V

    aload_0
    invokevirtual org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest/testCommonReturn()V

    aload_0
    invokevirtual org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest/testMultipleCalls()V

    aload_0
    invokevirtual org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest/testNestedSubs()V

    aload_0
    invokevirtual org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest/testCallFromHandler()V

    aload_0
    invokevirtual org/apache/harmony/drlvm/tests/regression/h3225/PositiveJsrTest/testBranches()V

    return
.end method

;
; Minimal number of locals is one since
; one passes a class reference.
;
.method public testMinimalLimits()V
    .limit stack 0
    .limit locals 1

    return
.end method

;
; A subroutine call can be the last instruction,
; when the subroutine doesn't return.
;
.method public testLastJsr()V
    .limit stack 1
    .limit locals 1

    goto LabelEndMethod
LabelReturn:
    return
LabelEndMethod:
    jsr LabelReturn
.end method

;
; Calls merge execution into common return instruction.
;
.method public testCommonReturn()V 
    .limit stack 1 
    .limit locals 1

    aconst_null
    ifnull LabelCodeBranch
    jsr LabelSub1

LabelCodeBranch:
    jsr LabelSub2

LabelSub1:
    astore 0
    goto LabelCommonPart

LabelSub2:
    astore 0
    goto LabelCommonPart

LabelCommonPart:
    return
    
.end method

;
; Multiple calls to one subroutine.
;
.method public testMultipleCalls()V
    .limit stack 1
    .limit locals 1

    jsr LabelSub
    jsr LabelSub
    jsr LabelSub
    jsr LabelSub
    jsr LabelSub
    jsr LabelSub
    jsr LabelSub
    jsr LabelSub
    return
LabelSub:
    astore 0
    ret 0
.end method

;
; A subroutine is called from another subroutine twice.
;
.method public testNestedSubs()V
    .limit stack 1
    .limit locals 2

    jsr LabelSub
    jsr LabelSub
    return

LabelSub:
    astore 0
    jsr LabelSubSub
    ret 0

LabelSubSub:
    astore 1
    ret 1

.end method

;
; A subroutine is called from the exception handler of
; the other subroutine.
;
.method public testCallFromHandler()V
    .limit stack 1
    .limit locals 2

    jsr LabelSub
    jsr LabelSub
    return

LabelSub:
    astore 0
    jsr LabelSubSub
LabelStartHandler:
    jsr LabelSubSub
LabelEndHandler:
    ret 0

LabelSubSub:
    astore 1
    ret 1

LabelHandler:
    pop
    jsr LabelSubSub
    return

.catch all from LabelStartHandler to LabelEndHandler using LabelHandler
.end method


;
; Subroutine contains different branches.
;
.method public testBranches()V
    .limit stack 1
    .limit locals 2

    aconst_null
    astore_0
    jsr LabelSub
    aload_0
    pop
    iconst_0
    istore_0
    jsr LabelSub
    iload_0
    return

LabelSub:
    astore 1
LabelBranch:
    aconst_null
    ifnonnull LabelBranch
    aconst_null
    ifnull LabelRet
    goto LabelBranch
LabelRet:
    ret 1

.end method

