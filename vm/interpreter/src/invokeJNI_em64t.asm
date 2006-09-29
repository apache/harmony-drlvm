//    Licensed to the Apache Software Foundation (ASF) under one or more
//    contributor license agreements.  See the NOTICE file distributed with
//    this work for additional information regarding copyright ownership.
//    The ASF licenses this file to You under the Apache License, Version 2.0
//    (the "License"); you may not use this file except in compliance with
//    the License.  You may obtain a copy of the License at
// 
//      http://www.apache.org/licenses/LICENSE-2.0
// 
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//   Author: Ivan Volosyuk
//
	.text
	.align 2
.globl invokeJNI
	.type	invokeJNI, @function
invokeJNI:
//  rdi - memory
//  rsi - n fp args
//  rdx - n mem args
//  rcx - function ptr

	push	%rbp
	mov	%rsp, %rbp

// cycle to fill all fp args
    movq 8(%rdi), %xmm0
    movq 16(%rdi), %xmm1
    movq 24(%rdi), %xmm2
    movq 32(%rdi), %xmm3
    movq 40(%rdi), %xmm4
    movq 48(%rdi), %xmm5
    movq 56(%rdi), %xmm6
    movq 64(%rdi), %xmm7

// store memory args
	movq %rcx, %r10 // func ptr
	movq %rdx, %rcx // counter
	lea	8+64+48-8(%rdi,%rcx,8), %rdx
	sub	%rsp, %rdx
    cmpq $0, %rcx
    jz 2f
1:
	push	0(%rsp,%rdx)
	loop 1b
2:
    movq 80(%rdi), %rsi
    movq 88(%rdi), %rdx
    movq 96(%rdi), %rcx
    movq 104(%rdi), %r8
    movq 112(%rdi), %r9

    movq 72(%rdi), %rdi

	call	*%r10
	leave
	ret



