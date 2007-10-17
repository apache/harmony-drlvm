PUBLIC  vectored_exception_handler
EXTRN   vectored_exception_handler_internal:PROC

PUBLIC  asm_c_exception_handler
EXTRN   c_exception_handler:PROC

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


asm_c_exception_handler PROC

; void __cdecl asm_c_exception_handler(Class *exn_class, bool in_java)
; void __cdecl c_exception_handler(Class* exn_class, bool in_java)
; Args:
;   rcx - nt_exception
;   rdx - in_java (actually in dl)
;   r8  - none
;   r9  - none

    pushfq
    cld
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    r8
    push    r9
    push    r10
    push    r11
    sub     rsp, 32 ; allocate stack for 4 registers
    call    c_exception_handler
    add     rsp, 32
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax
    popfq
    ret

asm_c_exception_handler ENDP


_TEXT   ENDS

END
