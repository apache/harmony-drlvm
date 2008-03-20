;
; Licensed to the Apache Software Foundation (ASF) under one or more
; contributor license agreements.  See the NOTICE file distributed with
; this work for additional information regarding copyright ownership.
; The ASF licenses this file to You under the Apache License, Version 2.0
; (the "License"); you may not use this file except in compliance with
; the License.  You may obtain a copy of the License at
;
;    http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

        .386P

_TEXT   SEGMENT PARA USE32 PUBLIC 'CODE'

; struct Registers {
; uint32 eax;    +00
; uint32 ebx;    +04
; uint32 ecx;    +08
; uint32 edx;    +0C
; uint32 edi;    +10
; uint32 esi;    +14
; uint32 ebp;    +18
; uint32 esp;    +1C
; uint32 eip;    +20
; uint32 eflags; +24
; };
;
; void port_transfer_to_regs(Registers* regs)

PUBLIC  port_transfer_to_regs

port_transfer_to_regs PROC

    mov     edx, dword ptr [esp+04h] ; store regs pointer to EDX
    mov     ebx, dword ptr [edx+20h] ; EIP field -> EBX
    mov     ecx, dword ptr [edx+1Ch] ; ESP field
    sub     ecx,4
    mov     dword ptr [esp+04h], ecx ; (new ESP - 4) -> [ESP + 4] (safe storage)
    mov     esi, dword ptr [edx+14h] ; ESI field
    mov     edi, dword ptr [edx+10h] ; EDI field
    mov     ebp, dword ptr [edx+18h] ; EBP field
    mov     dword ptr [ecx], ebx     ; new EIP -> (new ESP - 4) (as return address)
    mov     eax, dword ptr [edx+00h] ; EAX field
    mov     ebx, dword ptr [edx+04h] ; EBX field
    movzx   ecx, word ptr [edx+24h]  ; (word)EFLAGS -> ECX
    test    ecx, ecx
    je      _label_
    pushfd
    and     dword ptr [esp], 003F7202h ; Clear OF, DF, TF, SF, ZF, AF, PF, CF
    and     ecx, 00000CD5h           ; Clear all except OF, DF, SF, ZF, AF, PF, CF
    or      dword ptr [esp], ecx
    popfd                            ; restore EFLAGS
_label_:
    mov     ecx, dword ptr [edx+08h] ; ECX field
    mov     edx, dword ptr [edx+0Ch] ; EDX field
    mov     esp, dword ptr [esp+04h] ; ((new ESP - 4) -> ESP
    ret                              ; JMP by RET

port_transfer_to_regs ENDP


; void port_longjump_stub(void)
;
; after returning from the called function, EBP points to the pointer
; to saved Registers structure
;
; | interrupted |
; |  program    | <- ESP where the program was interrupted by exception
; |-------------|
; | return addr |
; | from stub   | <- for using in port_transfer_to_regs
; |-------------|
; |    saved    |
; |  Registers  | <- to restore register context
; |-------------|
; |  pointer to |
; |  saved Regs | <- EBP
; |-------------|
; |  arg 5      | <-
; |-------------|   |
; ...............    - arguments for 'fn'
; |-------------|   |
; |  arg 0      | <-
; |-------------|
; | return addr |
; |  from 'fn'  | <- address to return to the port_longjump_stub
; |-------------|

PUBLIC  port_longjump_stub

port_longjump_stub PROC

    mov     esp, ebp    ; ESP now points to the address of saved Registers
    call    port_transfer_to_regs   ; restore context
    ret                             ; dummy RET - unreachable
port_longjump_stub ENDP


_TEXT   ENDS

END

