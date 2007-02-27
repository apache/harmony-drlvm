PUBLIC  vectored_exception_handler
EXTRN   vectored_exception_handler_internal:PROC

PUBLIC  asm_c_exception_handler
EXTRN   c_exception_handler:PROC

PUBLIC  asm_exception_catch_callback
EXTRN   exception_catch_callback_wrapper:PROC

PUBLIC  asm_jvmti_exception_catch_callback
EXTRN   jvmti_exception_catch_callback_wrapper:PROC

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
    add     esp, 32
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
    sub     rsp, 32 ; allocate stack for 4 registers
    call    c_exception_handler
    add     esp, 32
    popfq
    ret

asm_c_exception_handler ENDP


asm_exception_catch_callback PROC

; void __cdecl exception_catch_callback_wrapper()
; Args:
;   rcx - none
;   rdx - none
;   r8  - none
;   r9  - none

    pushfq
    cld
    sub     rsp, 32 ; allocate stack for 4 registers
    call    exception_catch_callback_wrapper
    add     esp, 32
    popfq
    ret

asm_exception_catch_callback ENDP


asm_jvmti_exception_catch_callback PROC

; void __cdecl jvmti_exception_catch_callback_wrapper()
; Args:
;   rcx - none
;   rdx - none
;   r8  - none
;   r9  - none

    pushfq
    cld
    sub     rsp, 32 ; allocate stack for 4 registers
    call    jvmti_exception_catch_callback_wrapper
    add     esp, 32
    popfq
    ret

asm_jvmti_exception_catch_callback ENDP

_TEXT   ENDS

END
