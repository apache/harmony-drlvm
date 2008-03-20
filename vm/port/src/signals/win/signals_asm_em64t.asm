
_TEXT   SEGMENT


PUBLIC  vectored_exception_handler
EXTRN   vectored_exception_handler_internal:PROC

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


; struct Registers {
; uint64 rsp;   ; 00h
; uint64 rbp;   ; 08h
; uint64 rip;   ; 10h
; // callee-saved
; uint64 rbx;   ; 18h
; uint64 r12;   ; 20h
; uint64 r13;   ; 28h
; uint64 r14;   ; 30h
; uint64 r15;   ; 38h
; // scratched
; uint64 rax;   ; 40h
; uint64 rcx;   ; 48h
; uint64 rdx;   ; 50h
; uint64 rsi;   ; 58h
; uint64 rdi;   ; 60h
; uint64 r8;    ; 68h
; uint64 r9;    ; 70h
; uint64 r10;   ; 78h
; uint64 r11;   ; 80h
;
; uint32 eflags;; 88h
; };
;
; void port_transfer_to_regs(Registers* regs)

PUBLIC  port_transfer_to_regs

port_transfer_to_regs PROC

    mov     rdx, rcx ; regs pointer (1st param - RCX) -> RDX

    mov     rbp, qword ptr [rdx+08h] ; RBP field
    mov     rbx, qword ptr [rdx+18h] ; RBX field
    mov     r12, qword ptr [rdx+20h] ; R12 field
    mov     r13, qword ptr [rdx+28h] ; R13 field
    mov     r14, qword ptr [rdx+30h] ; R14 field
    mov     r15, qword ptr [rdx+38h] ; R15 field
    mov     rsi, qword ptr [rdx+58h] ; RSI field
    mov     rdi, qword ptr [rdx+60h] ; RDI field
    mov     r8,  qword ptr [rdx+68h] ; R8 field
    mov     r9,  qword ptr [rdx+70h] ; R9 field
    mov     r10, qword ptr [rdx+78h] ; R10 field
    mov     r11, qword ptr [rdx+80h] ; R11 field

    mov     rax, qword ptr [rdx+00h] ; (new RSP) -> RAX
    mov     qword ptr [rsp], rax     ; (new RSP) -> [RSP] for future use
    mov     rcx, qword ptr [rdx+10h] ; (new RIP) -> RCX
    mov     qword ptr [rax-88h],rcx  ; (new RIP) -> [(new RSP) - 128 - 8]
    mov     rax, qword ptr [rdx+40h] ; RAX field

    movzx   rcx, word ptr [rdx+88h]  ; (word)EFLAGS -> RCX
    test    rcx, rcx
    je      __skipefl__
    pushfq
    and     dword ptr [rsp], 003F7202h ; Clear OF, DF, TF, SF, ZF, AF, PF, CF
    and     ecx, 00000CD5h           ; Clear all except OF, DF, SF, ZF, AF, PF, CF
    or      dword ptr [rsp], ecx
    popfq                            ; restore RFLAGS
__skipefl__:

    mov     rcx, qword ptr [rdx+48h] ; RCX field
    mov     rdx, qword ptr [rdx+50h] ; RDX field

    mov     rsp, qword ptr [rsp]     ; load new RSP
    jmp     qword ptr [rsp-88h]      ; JMP to new RIP

port_transfer_to_regs ENDP


; void port_longjump_stub(void)
;
; after returning from the called function, RSP points to the 2 argument
; slots in the stack. Saved Registers structure pointer is (RSP + 48)
;
; | interrupted |
; |  program    | <- RSP where the program was interrupted by exception
; |-------------|
; | 0x80 bytes  | <- preserved stack area - we will not change it
; |-------------|
; | return addr |
; | from stub   | <- for using in port_transfer_to_regs as [(new RSP) - 128 - 8]
; |-------------|
; |    saved    |
; |  Registers  | <- to restore register context
; |-------------|
; | [alignment] | <- align Regs pointer to 16-bytes boundary
; |-------------|
; |  pointer to |
; |  saved Regs | <- (RSP + 48)
; |-------------|
; |    arg 5    | <- present even if not used
; |-------------|
; |    arg 4    | <- present even if not used
; |-------------|
; |  32 bytes   | <- 'red zone' for argument registers flushing
; |-------------|
; | return addr |
; |  from 'fn'  | <- address to return to the port_longjump_stub
; |-------------|

PUBLIC  port_longjump_stub

port_longjump_stub PROC

    mov     rcx, qword ptr [rsp + 48] ; load RCX with the address of saved Registers
    call    port_transfer_to_regs   ; restore context
    ret                             ; dummy RET - unreachable
port_longjump_stub ENDP


_TEXT   ENDS

END
