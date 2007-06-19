.class public org/apache/harmony/drlvm/tests/regression/h3225/RetOrderNegativeTest
.super java/lang/Object 

;
; Uses return addresses in incorrect order.
;
.method test()V 
   .limit stack 2
   .limit locals 2
    jsr LabelSub
    ret 0
    return
LabelSub:
    astore_1
    jsr LabelSubBranch
    return
LabelSubBranch:
    astore_0
    ret 1
    
.end method

