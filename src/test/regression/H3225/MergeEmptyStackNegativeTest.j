.class public org/apache/harmony/drlvm/tests/regression/h3225/MergeEmptyStackNegativeTest
.super java/lang/Object 

;
; Subroutines shouldn't merge excution.
;
.method static test()V 
    .limit stack 1 
    .limit locals 1

    jsr LabelSub1
    jsr LabelSub2
    return

LabelSub1:
    astore 0
    goto LabelCommonPart

LabelSub2:
    astore 0

LabelCommonPart:
    ret 0
    
.end method

