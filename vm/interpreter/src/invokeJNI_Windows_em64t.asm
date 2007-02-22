
PUBLIC	invokeJNI

_TEXT	SEGMENT

invokeJNI PROC

;    int64 invokeJNI(uint64 *args, uword n_fps, uword n_stacks, GenericFunctionPointer f);
;   rcx - memory
;   rdx - n fp args
;   r8  - n mem args
;   r9  - function ptr

	push	rbp
	mov	rbp, rsp
    mov rax, rcx ; mem
    mov rcx, r8 ; n mem args

; cycle to fill all fp args
    movsd xmm0, qword ptr [rax+8]
    movsd xmm1, qword ptr [rax+16]
    movsd xmm2, qword ptr [rax+24]
    movsd xmm3, qword ptr [rax+32]

; store memory args
	mov r10, r9 ; func ptr
	lea	r9, qword ptr [rax+rcx*8+64]
	sub	r9, rsp ; offset
    cmp rcx, 0
    jz cycle_end
cycle:
	push qword ptr [rsp+r9]
	loop cycle
cycle_end:
    mov rcx, [rax + 40]
    mov rdx, [rax + 48]
    mov r8,  [rax + 56]
    mov r9,  [rax + 64]

    sub rsp, 32 ; shadow space

	call	r10
	leave
	ret

invokeJNI ENDP


_TEXT	ENDS

END

