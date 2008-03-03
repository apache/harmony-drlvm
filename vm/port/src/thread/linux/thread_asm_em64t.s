//
// Licensed to the Apache Software Foundation (ASF) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// The ASF licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

	.text
	.align 16

// struct Registers {
// uint64 rsp;   ; 00h
// uint64 rbp;   ; 08h
// uint64 rip;   ; 10h
// // callee-saved
// uint64 rbx;   ; 18h
// uint64 r12;   ; 20h
// uint64 r13;   ; 28h
// uint64 r14;   ; 30h
// uint64 r15;   ; 38h
// // scratched
// uint64 rax;   ; 40h
// uint64 rcx;   ; 48h
// uint64 rdx;   ; 50h
// uint64 rsi;   ; 58h
// uint64 rdi;   ; 60h
// uint64 r8;    ; 68h
// uint64 r9;    ; 70h
// uint64 r10;   ; 78h
// uint64 r11;   ; 80h
//
// uint32 eflags;; 88h
// };
//
// void transfer_to_regs(Registers* regs)

.globl transfer_to_regs
	.type	transfer_to_regs, @function
transfer_to_regs:
    movq    %rdi, %rdx // regs pointer (1st param - RDI) -> RDX

    movq    0x08(%rdx), %rbp // RBP field
    movq    0x18(%rdx), %rbx // RBX field
    movq    0x20(%rdx), %r12 // R12 field
    movq    0x28(%rdx), %r13 // R13 field
    movq    0x30(%rdx), %r14 // R14 field
    movq    0x38(%rdx), %r15 // R15 field
    movq    0x58(%rdx), %rsi // RSI field
    movq    0x60(%rdx), %rdi // RDI field
    movq    0x68(%rdx), %r8  // R8 field
    movq    0x70(%rdx), %r9  // R9 field
    movq    0x78(%rdx), %r10 // R10 field
    movq    0x80(%rdx), %r11 // R11 field

    movq    0x00(%rdx), %rax // (new RSP) -> RAX
    movq    %rax, (%rsp)     // (new RSP) -> [RSP] for future use
    movq    0x10(%rdx), %rcx // (new RIP) -> RCX
    movq    %rcx, -0x88(%rax)// (new RIP) -> [(new RSP) - 128 - 8]
    movq    0x40(%rdx), %rax // RAX field

    movzbq  0x88(%rdx), %rcx // (EFLAGS & 0xff) -> RCX
    test    %rcx, %rcx
    je      __skipefl__
    push    %rcx
    popfq
__skipefl__:

    movq    0x48(%rdx), %rcx // RCX field
    movq    0x50(%rdx), %rdx // RDX field

    movq    (%rsp), %rsp     // load new RSP
    jmpq    * -0x88(%rsp)    // JMP to new RIP

