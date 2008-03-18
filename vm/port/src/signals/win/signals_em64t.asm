PUBLIC  vectored_exception_handler
EXTRN   vectored_exception_handler_internal:PROC

_TEXT   SEGMENT

vectored_exception_handler PROC

; LONG NTAPI vectored_exception_handler(LPEXCEPTION_POINTERS nt_exception)
; Args:
;   rcx - nt_exception
;   rdx - none
;   r8  - none
;   r9  - none

    pushfq
    cld
    sub     rsp, 32 ; allocate stack for 4 registers
    call    vectored_exception_handler_internal
    add     rsp, 32
    popfq
    ret

vectored_exception_handler ENDP

_TEXT   ENDS

END
