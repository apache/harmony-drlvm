.class public org/apache/harmony/drlvm/tests/regression/h3225/MergeIntFloatNegativeTest
.super java/lang/Object 

;
; Merging different stack types result in a stack mismatch error.
;
.method static test()V 
    .limit stack 1 
    .limit locals 1

    iconst_0
    aconst_null
    ifnull LabelReturn
    pop
    fconst_0
LabelReturn:
    return

.end method

