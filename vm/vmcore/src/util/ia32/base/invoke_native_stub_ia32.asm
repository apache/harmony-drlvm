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
//   Author: Intel, Evgueni Brevnov
//
	.text
	.align 2
.globl vm_invoke_native_array_stub
	.type	vm_invoke_native_array_stub, @function
vm_invoke_native_array_stub:

	push	%ebp
	movl	%esp, %ebp
    push    %ebx
    push    %esi
    push    %edi
	movl	8(%ebp), %eax
	movl	12(%ebp), %ecx
	leal	-4(%eax,%ecx,4), %eax
	subl	%esp, %eax
    or      %ecx, %ecx
    je 2f
1:
    push 0(%esp,%eax)
    loop 1b
2:
	movl	16(%ebp), %eax
	call	*%eax
    leal    -12(%ebp), %esp
    pop     %edi
    pop     %esi
    pop     %ebx
	leave
	ret
