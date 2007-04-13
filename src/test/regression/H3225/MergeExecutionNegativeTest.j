.class public org/apache/harmony/drlvm/tests/regression/h3225/MergeExecutionNegativeTest
.super java/lang/Object 

;
; Subroutines shouldn't merge excution.
;
.method static test()V 
    .limit stack 1 
    .limit locals 1

    jsr LabelCommonPart
    jsr LabelSub
    return

LabelSub:
    nop

LabelCommonPart:
    astore 0
    ret 0
    
.end method

