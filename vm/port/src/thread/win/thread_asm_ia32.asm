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
; void transfer_to_regs(Registers* regs)

PUBLIC  transfer_to_regs

transfer_to_regs PROC

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
    movzx   ecx,  byte ptr [edx+24h] ; (EFLAGS & 0xff) -> ECX
    test    ecx, ecx
    je      _label_
    push    ecx                      ; restore EFLAGS
    popfd
_label_:
    mov     ecx, dword ptr [edx+08h] ; ECX field
    mov     edx, dword ptr [edx+0Ch] ; EDX field
    mov     esp, dword ptr [esp+04h] ; ((new ESP - 4) -> ESP
    ret                              ; JMP by RET

transfer_to_regs ENDP


_TEXT   ENDS

END

