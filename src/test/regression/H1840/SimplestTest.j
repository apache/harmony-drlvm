.class public org/apache/harmony/drlvm/tests/regression/h1840/SimplestTest
.super junit/framework/TestCase
.method public <init>()V
    aload_0
    invokespecial junit/framework/TestCase/<init>()V
    return
.end method

;
; Launches testcases which check subroutine verification.
;
.method public test()V
    .limit stack 1
    .limit locals 2

    jsr LabelSub
LabelSub:
    return
.end method

