/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/** 
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.4 $
 */  

//
// This file contains IA32-specific code that does not depend on any VM internals.
// For the most part, this means math helpers.
//

#include <assert.h>
#include <float.h>
#include <math.h>

#define LOG_DOMAIN "vm.helpers"
#include "cxxlog.h"

#include "jit_runtime_support.h"
#include "open/thread.h"
#include "nogc.h" // for malloc_fixed_code_for_jit()
#include "encoder.h"
#include "vm_stats.h"
#include "vm_arrays.h"

#ifdef PLATFORM_POSIX

#ifndef _isnan
#define _isnan isnan
#endif

#endif // PLATFORM_POSIX

extern bool dump_stubs;

//static uint64 vm_lshl(unsigned count, uint64 n)
//{
//    assert(!tmn_is_suspend_enabled());
//    return n << (count & 0x3f);
//} //vm_lshl


// The arguments are:
// edx:eax          - the value to be shifted
// ecx              - how many bits to shift by
// The result is returned in edx:eax.


void * getaddress__vm_lshl_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_lshl_naked


//static int64 vm_lshr(unsigned count, int64 n)
//{
//    assert(!tmn_is_suspend_enabled());
//    return n >> (count & 0x3f);
//} //vm_lshr


// The arguments are:
// edx:eax          - the value to be shifted
// ecx              - how many bits to shift by
// The result is returned in edx:eax.


void * getaddress__vm_lshr_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_lshr_naked


//static uint64 vm_lushr(unsigned count, uint64 n)
//{
//    assert(!tmn_is_suspend_enabled());
//    return n >> (count & 0x3f);
//} //vm_lushr


// The arguments are:
// edx:eax          - the value to be shifted
// ecx              - how many bits to shift by
// The result is returned in edx:eax.


void * getaddress__vm_lushr_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_lushr_naked


static int64 __stdcall vm_lmul(int64 m, int64 n) stdcall__;

static int64 __stdcall vm_lmul(int64 m, int64 n)
{
    assert(!tmn_is_suspend_enabled());

    return m * n;
} //vm_lmul

#ifdef VM_LONG_OPT
static int64 __stdcall vm_lmul_const_multiplier(int64 m, int64 n) stdcall__;

static int64 __stdcall vm_lmul_const_multiplier(int64 m, int64 n)
{
    assert(!tmn_is_suspend_enabled());
    __asm{
        mov  eax,dword ptr [ebp+0ch]
        mov  ecx,dword ptr [ebp+10h]
        mul   ecx 
        mov   ebx,eax
        mov   eax,dword ptr [ebp+08h]
        mul   ecx
        add   edx,ebx
    }
} //vm_lmul_const_multiplier
#endif


//static int64 __stdcall do_lrem(int64 m, int64 n) stdcall__;

//static int64 __stdcall do_lrem(int64 m, int64 n)
//{
//    assert(!tmn_is_suspend_enabled());
//
//    return m % n;
//} //do_lrem


void * getaddress__vm_const_lrem_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_const_lrem_naked

static void *getaddress__vm_const_ldiv_naked()
{
    static void *addr = 0;
    return addr;
} //getaddress__vm_ldiv_naked


static double vm_rt_ddiv(double a, double b)
{
    double result = a / b;
    return result;
} //vm_rt_ddiv

/*
static int32 d2i_infinite(double d)
{
#ifdef __INTEL_COMPILER
#pragma warning(disable: 4146)
#endif
    if(_isnan(d)) {
            return 0;
        } else if(d > (double)2147483647) {
            return 2147483647;      // maxint
        } else if(d < (double)-2147483648) {
            return -2147483648;     // minint
        } else {
            // The above should exhaust all possibilities
            return 0;
        }
#ifdef __INTEL_COMPILER
#pragma warning(default: 4146)
#endif
}
*/
//static short fpstatus = 0x0e7f;
void *getaddress__vm_d2i()
{
    static void *addr = 0;
    return addr;    
} //getaddress__vm_d2i

void *getaddress__vm_d2l()
{
    static void *addr = 0;
   return addr;       
} //getaddress__vm_d2l


static int64 __stdcall vm_d2l(double d) stdcall__;

static int64 __stdcall vm_d2l(double d)
{
    assert(!tmn_is_suspend_enabled());

#ifdef VM_STATS
    vm_stats_total.num_d2l++;
#endif

    int64 result;

    int64 (*gad2l)(int, int, int, double);
    gad2l = (int64 ( *)(int, int, int, double) )getaddress__vm_d2l();

    result = gad2l(0, 0, 0, d);

#ifdef __INTEL_COMPILER
#pragma warning(disable: 4146)
#endif
    // 0x80000000 is the integer indefinite value
    if(0x80000000 == *(uint32*)((char*)&result+4)) {

#ifdef PLATFORM_POSIX
        if (isnan(d))
            return 0;
#else
        if (_isnan(d))
            return 0;
#endif 

        if(d >= (double)(__INT64_C(0x7fffffffffffffff))) {
            return __INT64_C(0x7fffffffffffffff);      // maxint
        } else if(d < (double)-__INT64_C(0x8000000000000000)) {
            return -__INT64_C(0x8000000000000000);     // minint
        } else {
            // The above should exhaust all possibilities
            ASSERT(0, "All the possible cases are expected to be already covered");
            return result;
        }

    } else {
        return result;
    }

#ifdef __INTEL_COMPILER
#pragma warning(default: 4146)
#endif
} //vm_d2l

/*
static int32 f2i_infinite(float f)
{
#ifdef __INTEL_COMPILER
#pragma warning(disable: 4146)
#endif
    if(_isnan(f)) {
            return 0;
        } else if(f > (double)2147483647) {
            return 2147483647;      // maxint
        } else if(f < (double)-2147483648) {
            return -2147483648;     // minint
        } else {
            // The above should exhaust all possibilities
            return 0;
        }
#ifdef __INTEL_COMPILER
#pragma warning(default: 4146)
#endif
}
*/

void *getaddress__vm_f2i()
{
    static void *addr = 0;
    return addr;    
} //getaddress__vm_f2i

static void *getaddress__vm_f2l()
{
    static void *addr = 0;
    return addr;    
} //getaddress__vm_f2l


static int64 __stdcall vm_f2l(float f) stdcall__;

static int64 __stdcall vm_f2l(float f)
{
    assert(!tmn_is_suspend_enabled());

#ifdef VM_STATS
    vm_stats_total.num_f2l++;
#endif

    int64 result;

    int64 (*gaf2l)(int, int, int, float);
    gaf2l = (int64 ( *)(int, int, int, float) )getaddress__vm_f2l();

    result = gaf2l(0, 0, 0, f);

#ifdef __INTEL_COMPILER
#pragma warning(disable: 4146)
#endif
    // 0x80000000 is the integer indefinite value
    if(0x80000000 == *(uint32*)((char*)&result+4)) {
        if(_isnan(f)) {
            return 0;
        } else if(f >= __INT64_C(0x7fffffffffffffff) ) {
            return __INT64_C(0x7fffffffffffffff);      // maxint
        } else if(f < (double)__INT64_C(-0x8000000000000000) ) {
            return __INT64_C(-0x8000000000000000);     // minint
        } else {
            // The above should exhaust all possibilities
            ASSERT(0, "All the possible cases are expected to be already covered");
            return result;
        }
    } else {
        return result;
    }
#ifdef __INTEL_COMPILER
#pragma warning(default: 4146)
#endif
} //vm_f2l


//
// If fprem succeeds in producing a remainder that is less than the
// modulus, the function is complete and the C2 flag is cleared.
// Otherwise, C2 is set, and the result on the top of the fp stack
// is the partial remainder.  We need to re-execute the fprem instruction
// (using the partial remainder) until C2 is cleared.
//


void *getaddress__vm_frem()
{
    static void *addr = 0;
    return addr;    
} //getaddress__vm_frem



void *getaddress__vm_drem()
{
    static void *addr = 0;
    return addr;    
} //getaddress__vm_drem


#ifdef VM_STATS // exclude remark in release mode (defined but not used)
// Return the log base 2 of the integer operand. If the argument is less than or equal to zero, return zero.
static int get_log2(int value)
{
    register int n = value;
    register int result = 0;

    while (n > 1) {
        n = n >> 1;
        result++;
    }
    return result;
} //get_log2
#endif

static void vm_rt_char_arraycopy_no_exc(ManagedObject *src,
                                         int32 srcOffset,
                                         ManagedObject *dst,
                                         int32 dstOffset,
                                         int32 length)
{
    // 20030303 Use a C loop to (hopefully) speed up short array copies.

    // Check that the array references are non-null.
    assert(src && dst); 
    // Check that the arrays are arrays of 16 bit characters.
    Class * UNUSED src_class = src->vt()->clss;
    assert(src_class);
    Class * UNUSED dst_class = dst->vt()->clss;
    assert(dst_class);
    assert((src_class->is_array) && (dst_class->is_array));
    assert((src_class->is_array_of_primitives) && (dst_class->is_array_of_primitives));
    assert(strcmp(src_class->name->bytes, "[C") == 0);
    assert(strcmp(dst_class->name->bytes, "[C") == 0);
    // Check the offsets
    assert(srcOffset >= 0);
    assert(dstOffset >= 0);
    assert(length >= 0);
    assert((srcOffset + length) <= get_vector_length((Vector_Handle)src));
    assert((dstOffset + length) <= get_vector_length((Vector_Handle)dst));

    tmn_suspend_disable();       // vvvvvvvvvvvvvvvvvvv

    register uint16 *dst_addr = get_vector_element_address_uint16(dst, dstOffset);
    register uint16 *src_addr = get_vector_element_address_uint16(src, srcOffset);

#ifdef VM_STATS
    vm_stats_total.num_char_arraycopies++;
    if (dst_addr == src_addr) {
        vm_stats_total.num_same_array_char_arraycopies++;
    }
    if (srcOffset == 0) {
        vm_stats_total.num_zero_src_offset_char_arraycopies++;
    }
    if (dstOffset == 0) {
        vm_stats_total.num_zero_dst_offset_char_arraycopies++;
    }
    if ((((POINTER_SIZE_INT)dst_addr & 0x7) == 0) && (((POINTER_SIZE_INT)src_addr & 0x7) == 0)) {
        vm_stats_total.num_aligned_char_arraycopies++;
    }
    vm_stats_total.total_char_arraycopy_length += length;
    vm_stats_total.char_arraycopy_count[get_log2(length)]++;
#endif //VM_STATS

    // 20030219 The length threshold 32 here works well for SPECjbb and should be reasonable for other applications.
    if (length < 32) {
        register int i;
        if (src_addr > dst_addr) {
            for (i = length;  i > 0;  i--) {
                *dst_addr++ = *src_addr++;
            }
        } else {
            // copy down, from higher address to lower
            src_addr += length-1;
            dst_addr += length-1;
            for (i = length;  i > 0;  i--) {
                *dst_addr-- = *src_addr--;
            }
        }
    } else {
        memmove(dst_addr, src_addr, (length * sizeof(uint16)));
    }

    tmn_suspend_enable();        // ^^^^^^^^^^^^^^^^^^^
} //vm_rt_char_arraycopy_no_exc


static int32 vm_rt_imul_common(int32 v1, int32 v2)
{
    return v1 * v2;
} //vm_rt_imul_common



static int32 vm_rt_idiv_common(int32 v1, int32 v2)
{
    assert(v2);
    return v1 / v2;
} //vm_rt_idiv_common



static int32 vm_rt_irem_common(int32 v1, int32 v2)
{
    assert(v2);
    return v1 % v2;
} //vm_rt_irem_common


void *get_generic_rt_support_addr_ia32(VM_RT_SUPPORT f)
{
    switch(f) {
    case VM_RT_F2I:
        return getaddress__vm_f2i();
    case VM_RT_F2L:
        return (void *)vm_f2l;
    case VM_RT_D2I:
        return getaddress__vm_d2i();
    case VM_RT_D2L:
        return (void *)vm_d2l; 
    case VM_RT_LSHL:
        return getaddress__vm_lshl_naked();
    case VM_RT_LSHR:
        return getaddress__vm_lshr_naked();
    case VM_RT_LUSHR:
        return getaddress__vm_lushr_naked();
    case VM_RT_FREM:
        return getaddress__vm_frem();
    case VM_RT_DREM:
        return getaddress__vm_drem();
    case VM_RT_LMUL:
        return (void *)vm_lmul;
#ifdef VM_LONG_OPT
    case VM_RT_LMUL_CONST_MULTIPLIER:
        return (void *)vm_lmul_const_multiplier;
#endif
    case VM_RT_CONST_LDIV:
        return getaddress__vm_const_ldiv_naked() ;
    case VM_RT_CONST_LREM:
        return getaddress__vm_const_lrem_naked() ;
    case VM_RT_DDIV:
        return (void *)vm_rt_ddiv;

    case VM_RT_IMUL:
        return (void *)vm_rt_imul_common;
    case VM_RT_IDIV:
        return (void *)vm_rt_idiv_common;
    case VM_RT_IREM:
        return (void *)vm_rt_irem_common;
    case VM_RT_CHAR_ARRAYCOPY_NO_EXC:
        return (void *)vm_rt_char_arraycopy_no_exc;

    default:
        ASSERT(0, "Unexpected helper id");
        return 0;
    }
}
