//    Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
//  
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
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

	push	%ebp
	movl	%esp, %ebp
	push	%ecx
	movl	8(%ebp), %eax
	movl	12(%ebp), %ecx
	leal	-4(%eax,%ecx,4), %eax
	subl	%esp, %eax
1:
	push	0(%esp,%eax)
	loop 1b
	movl	-4(%ebp), %ecx
	movl	16(%ebp), %eax
	call	*%eax
	leave
	ret



