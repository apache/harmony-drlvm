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
// Author: Salikh Zakirov
// 
// Assembly code needed to interface JIT-ed code with C code.
//

// my_memset:	function to set a number of bytes to a char value
	
	.section .text
// -- Begin  memset
	.proc  memset#
	.align 32
// Replicate the value into all bytes using mmx broadcast
// Fall through to aligned short (<16 bytes) code
// live out:	r21 (alignment), r31(replicated c),
//		r32(s), r33(c), r34(n)
	.global memset#
	.prologue
memset:
	mov	r8=r32			// Return value
	cmp.le	p14=16,r34
	and	r22=0xF,r32		// Spec test for 16-byte boundary
	and	r21=7,r32		// Spec test for 8-byte boundary
	mux1	r31=r33,@brcst		// Replicate byte value
 (p14)	br.cond.dpnt	Not_short
	;;
// Handle short values quickly
	cmp.ne	p15=0,r21		// If zero, skip alignment
	cmp.le	p11,p10=8,r34		// Spec test for st8 safety
	tbit.nz	p13,p12=r32,0		// Spec test for st1 alignment
	cmp.ge	p14=0,r34		// Spec test for early exit
 (p14)	br.ret.dpnt	b0
 (p15)	br.cond.dpnt	Align_short
	;;
// We're aligned and p11/p10 is set/clear if we need to do the st8
// Use complementary predicates to allow length tests in parallel with store
Short:
{ .mmi
	.pred.rel "mutex",p10,p11
 (p11)	st8	[r32]=r31,8
 (p11)	cmp.le	p13,p12=12,r34
 (p10)	cmp.le	p13,p12=4,r34
} { .mmi
	.pred.rel "mutex",p12,p13
 (p11)	add	r34=-8,r34
	;;
 (p13)	st4	[r32]=r31,4
 (p13)	cmp.le	p11,p10=6,r34
} { .mii
 (p12)	cmp.le	p11,p10=2,r34
	.pred.rel "mutex",p10,p11
 (p13)	add	r34=-4,r34
	;;
 (p11)	cmp.le	p13=3,r34
} { .mii
 (p11)	st2	[r32]=r31,2
 (p10)	cmp.le	p13=1,r34
	;;
} { .mib
 (p13)	st1	[r32]=r31
	br.ret.sptk	b0
	;;
}
// Align, while taking care not to exceed length
// Similar to aligned code above, but adds an alignment test to length test
Align_short:
{ .mmi
	.pred.rel "mutex",p12,p13
 (p13)	st1	[r32]=r33,1
 (p13)	cmp.le	p11,p10=3,r34
 (p12)	cmp.le	p11,p10=2,r34
} { .mii
 (p13)	add	r34=-1,r34
	;;
 (p11)	tbit.nz	p11,p10=r32,1		// length is OK, are we on 2-byte boundary?
	;;
} { .mmi
	.pred.rel "mutex",p10,p11
 (p11)	st2	[r32]=r31,2
 (p10)	cmp.le	p13,p12=4,r34
 (p11)	cmp.le	p13,p12=6,r34
} { .mmi
 (p11)	add	r34=-2,r34
	;;
 (p13)	tbit.nz	p13,p12=r32,2
	;;
} { .mmi
	.pred.rel "mutex",p12,p13
 (p13)	st4	[r32]=r31,4
 (p12)	cmp.le	p11,p10=8,r34
 (p13)	cmp.le	p11,p10=12,r34
} { .mib
 (p13)	add	r34=-4,r34
 	br.cond.sptk	Short
	;;
}	
// Code for lengths >= 16
// If we're not on a 16-byte boundary, move to one
// live out: r31 (replicated c), r33(unsigned c), r32(s), r34(unsigned n)
Not_short:
	cmp.ne	p15=0,r22		//0: Low 4 bits zero?
	cmp.ne	p11,p10=0,r33
	tbit.nz	p13,p12=r32,0		// Spec test for st1 alignment
  (p15)	br.cond.dpnt	Align_long
	;;
// OK, it's long, it's aligned to a 16-byte boundary.
// If r33 is not zero, skip to st8 code, otherwise fall into spill f0 version
Is_aligned:
	cmp.ne	p14=0,r33		// Check value of fill character
	add	r16=128,r32	// prefetch pointer
	.save	ar.lc,r11,t01
[t01:]	mov	r11=ar.lc
	mov	r24=r34
  (p14)	br.cond.dpnt	Nonzero
	;;
//
// Version when memset is clearing memory
//
	.body
	add	r17=16,r32	// second spill pointer
	cmp.le	p13=32,r34	// Spec for first set of spills
	cmp.ge	p14=127,r34
	and	r24=127,r34
	mov	r21=144		// = 128+16, length needed for second prefetch
 (p14)	br.cond.dpnt		Zero_medium
//
/// Enter loop code when length is at least 128
/// Prefetch each line with a spill
///
	stf.spill	[r32]=f0,32
	cmp.le		p9=r21,r34
	shr.u		r22=r34,7	// line size is 128
	add		r21=128,r21	// next prefetch safe length
	;;
 (p9)	stf.spill	[r16]=f0,128
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	add		r22=-1,r22	// Loop count
	;;
	mov		ar.lc=r22
 (p9)	stf.spill	[r16]=f0,128
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	;;
 (p9)	stf.spill	[r16]=f0,128
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	;;
 (p9)	stf.spill	[r16]=f0,128
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	;;
 (p9)	stf.spill	[r16]=f0,128
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	;;
// Counted loop storing 128 bytes/iteration,
/// with out-of-order spills causing line prefetch
// live out:	r11(ar.lc), r17(s+16), r23(128-n&15), r24(n&15), r32(s)
//              r33(replicated c), r34(n), p13(n&15>32)
Zero_loop:
 (p9)	stf.spill	[r16]=f0,128
	stf.spill	[r17]=f0,32
	cmp.le		p9=r21,r34
	;;
	stf.spill	[r32]=f0,32
	stf.spill	[r17]=f0,32
	add		r21=128,r21	// next prefetch safe length
	;;
	stf.spill	[r32]=f0,32
	stf.spill	[r17]=f0,32
	cmp.le		p13=32,r24
	;;
	stf.spill	[r32]=f0,64
	stf.spill	[r17]=f0,32
	br.cloop.sptk	Zero_loop
	;;
	add		r32=-32,r32
	;;
Zero_medium:
 (p13)	stf.spill	[r32]=f0,32	// Redundant if entered from loop path
 (p13)	stf.spill	[r17]=f0,32
	cmp.le		p12=64,r24
	;;
 (p12)	stf.spill	[r32]=f0,32
 (p12)	stf.spill	[r17]=f0,32
	cmp.le		p11=96,r24
	;;
 (p11)	stf.spill	[r32]=f0,32
 (p11)	stf.spill	[r17]=f0,32
	tbit.nz		p10=r24,4
	;;
 (p10)	stf.spill	[r32]=f0,16
	tbit.nz		p9=r24,3
	;;
 (p9)	st8		[r32]=r0,8
	tbit.nz		p13=r24,2
	;;
// 
// Clean up any partial word stores.
//	
	tbit.nz		p12=r24,1
 (p13)	st4		[r32]=r0,4
	;;
 (p12)	st2		[r32]=r0,2
	tbit.nz		p11=r24,0
	;;
 (p11)	st1		[r32]=r0,1
	mov		ar.lc=r11
	br.ret.sptk.many	b0
	;;
//
// Fill character is not zero
// Now that p is aligned to a 16-byte boundary
//     use straight-line code for n<=64, a loop otherwise
// live out:	r8 (return value, original value of r32)
//		p14 (n>=MINIMUM_LONG)
//
Nonzero:
	MINIMUM_LONG=0x40
	add	r17=8,r32		//0: second pointer
	mov	r21=136		// = 128+8, length needed for second prefetch
	add	r22=64,r34	// May need extra 1/2 iteration
	cmp.le	p13=16,r34	// Spec for use when loop is skipped
	cmp.gt	p14=MINIMUM_LONG,r34
 (p14)	br.cond.dpnt	Nonzero_medium
	;;
//
/// Enter loop code when length is at least 128
/// Prefetch each line with a st8
///
	st8		[r32]=r31,16
	cmp.le		p9=r21,r34
	shr.u		r22=r22,7	// line size is 128
	add		r21=128,r21	// next prefetch safe length
	;;
 (p9)	st8		[r16]=r31,128
	add		r22=-1,r22	// Loop count
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	;;
	mov		ar.lc=r22
 (p9)	st8		[r16]=r31,128
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	;;
 (p9)	st8		[r16]=r31,128
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	;;
 (p9)	st8		[r16]=r31,128
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	;;
 (p9)	st8		[r16]=r31,128
	cmp.le		p9=r21,r34
	add		r21=128,r21	// next prefetch safe length
	;;
// Counted loop storing 128 bytes/iteration,
/// with out-of-order spills causing line prefetch
// live out:	r11(ar.lc), r17(s+16), r23(128-n&15), r24(n&15), r32(s)
//              r33(replicated c), r34(n), p13(n&15>32)
Nonzero_loop:
 (p9)	st8		[r16]=r31,128
	st8		[r17]=r31,16
	cmp.lt		p10,p11=127,r24	// should we store the last 64?
	;;
	st8		[r32]=r31,16
	st8		[r17]=r31,16
 (p10)	add		r24=-128,r24	// Update count of remaining bytes
	;;
	st8		[r32]=r31,16
	st8		[r17]=r31,16
 (p11)	add		r24=-64,r24	// Update count of remaining bytes
	;;
	st8		[r32]=r31,16
	st8		[r17]=r31,16
	cmp.le		p9=r21,r34	// Compare prefetch offset with length
	;;
 (p10)	st8		[r32]=r31,16
 (p10)	st8		[r17]=r31,16
	add		r21=128,r21	// next prefetch-safe length
	;;
 (p10)	st8		[r32]=r31,16
 (p10)	st8		[r17]=r31,16
	cmp.le		p13=16,r24	// Spec for epilog
	;;
 (p10)	st8		[r32]=r31,16
 (p10)	st8		[r17]=r31,16
// (p10)	cmp.lt.unc	p11,p12=64,r24	// p11 true if we need another iter
	;;
//  {.mmi
 (p10)	st8		[r32]=r31,32
 (p10)	st8		[r17]=r31,16
//} {.mib
//	.pred.rel "mutex",p11,p12
// (p11)	add		r32=32,r32	// skip the bytes stored out-of-order
// (p12)	add		r32=16,r32	// prepare for epilogue
	br.cloop.sptk	Nonzero_loop
	;;
//}
 (p10)	add	r32=-16,r32
	;;
// Short memsets are done with predicated straightline code
// live out:	r8 (return value, original value of r32)
Nonzero_medium:
 (p13)	st8	[r32]=r31,16
 (p13)	st8	[r17]=r31,16
	cmp.le	p12=0x20,r24		//0: 32 <= n?
	;;
 (p12)	st8	[r32]=r31,16
 (p12)	st8	[r17]=r31,16
	cmp.le	p11=0x30,r24		//0: 48 <= n?
	;;
 (p11)	st8	[r32]=r31,16
 (p11)	st8	[r17]=r31,16
	tbit.nz	p10=r24,3
	;;
 (p10)	st8	[r32]=r31,8
	tbit.nz	p9=r24,2
	;;
// 
// Clean up any partial word stores.
//	
	tbit.nz	p8=r24,1
 (p9)	st4	[r32]=r31,4
	;;
 (p8)	st2	[r32]=r31,2
	tbit.nz	p7=r24,0
	;;
 (p7)	st1	[r32]=r31,1
	mov	ar.lc=r11
	br.ret.sptk.many	b0
	;;
Align_long:
 (p13)	st1	[r32]=r33,1
 (p13)	add	r34=-1,r34
	;;
	tbit.nz	p13=r32,1
	;;
 (p13)	st2	[r32]=r31,2
 (p13)	add	r34=-2,r34
	;;
	tbit.nz	p13=r32,2
	;;
 (p13)	st4	[r32]=r31,4
 (p13)	add	r34=-4,r34
	;;
	tbit.nz	p13,p12=r32,3
	;;
 (p13)	st8	[r32]=r31,8
 (p13)	add	r34=-8,r34
	;;
 	cmp.le	p11,p10=8,r34		// Spec for entry to Short
 	cmp.le	p13,p12=16,r34	
 (p12)	br.cond.dpnt	Short
	br.cond.dptk	Is_aligned
	;;
//
// -- End  memset
	.endp  memset#
// End
