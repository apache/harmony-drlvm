PUBLIC port_atomic_cas8

_TEXT	SEGMENT

port_atomic_cas8 PROC

;uint8 port_atomic_cas8(volatile uint8 * data , uint8 value, uint8 comp)
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

