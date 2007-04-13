.class public org/apache/harmony/drlvm/tests/regression/h3225/MergeStackNegativeTest
.super java/lang/Object 

;
; Each instruction should have a fixed stack depth.
;
.method static test()V 
    .limit stack 2 
    .limit locals 1

    jsr LabelSub
    iconst_1
    jsr LabelSub
    return

LabelSub:
    astore 0 ; different depth
    ret 0
    
.end method

