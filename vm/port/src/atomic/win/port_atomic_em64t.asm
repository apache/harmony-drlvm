PUBLIC port_atomic_cas8

_TEXT	SEGMENT

port_atomic_cas8 PROC

;U_8 port_atomic_cas8(volatile U_8 * data , U_8 value, U_8 comp)
;
;    rcx - *data - pointer to the byte which shoud be exchanged
;    rdx - value - new value
;    r8  - comp  - previous conditional value
;
;   It's a leaf function so no prolog and epilog are used.

    mov rax, r8
    lock cmpxchg [rcx], dl
    ret

port_atomic_cas8 ENDP

_TEXT	ENDS

END

