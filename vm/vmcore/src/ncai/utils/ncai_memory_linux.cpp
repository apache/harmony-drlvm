/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */
#include <sys/mman.h>
#include <limits.h>

#include "port_vmem.h"
#ifdef _IA32_
#include "encoder.h"
#include "jit_intf_cpp.h"
#endif // #ifdef _IA32_
#include "nogc.h"
#include "dump.h"
#include "ncai_direct.h"
#include "ncai_internal.h"


typedef void* (__cdecl *get_return_address_stub_type)();


#ifdef _IA32_

static get_return_address_stub_type gen_get_return_address_stub()
{
    static get_return_address_stub_type addr = NULL;

    if (addr)
        return addr;

    const int stub_size = 0x04;
    char *stub = (char *)malloc_fixed_code_for_jit(stub_size, DEFAULT_CODE_ALIGNMENT, CODE_BLOCK_HEAT_COLD, CAA_Allocate);
#ifdef _DEBUG
    memset(stub, 0xcc /*int 3*/, stub_size);
#endif
    char *ss = stub;

    M_Base_Opnd m1(esp_reg, 0);
    ss = mov(ss,  eax_opnd,  m1);
    ss = ret(ss);

    addr = (get_return_address_stub_type)stub;
    assert(ss - stub <= stub_size);

    /*
       The following code will be generated:

        mov         eax,dword ptr [esp]
        ret
    */

    DUMP_STUB(stub, "get_return_address", ss - stub);

    return addr;
}
#endif // #ifdef _IA32_


ncaiError ncai_read_memory_internal(void* addr, size_t size, void* buf, bool UNUSED calc_pass = false)
{
#ifndef _IA32_
    // SIGSEGV processing is not implemented for EM64T/IPF
    memcpy(buf, addr, size);
    return NCAI_ERROR_NONE;
#else // #ifndef _IA32_

static void* read_memory_1st_label = NULL;
static void* read_memory_2nd_label = NULL;

    char dummy_str[3];

    if (calc_pass)
    {
        addr = dummy_str;
        buf = dummy_str;
        *dummy_str = 'a'; // To avoid warnings
        size = 1;
    }

    jvmti_thread_t jvmti_thread = &p_TLS_vmthread->jvmti_thread;

    unsigned char* src_ptr = (unsigned char*)addr;
    unsigned char* dest_ptr = (unsigned char*)buf;

    jvmti_thread->violation_flag = 1;
    jvmti_thread->violation_restart_address = read_memory_1st_label;

    size_t i;
    for (i = 0; i < size; i++)
    {
        *dest_ptr = *src_ptr;

        void* label = (gen_get_return_address_stub())();
        read_memory_1st_label = label;

        if (jvmti_thread->violation_flag == 0)
            break;

        ++src_ptr;
        ++dest_ptr;
    }

    if (jvmti_thread->violation_flag == 0 || calc_pass)
    {
        if (!calc_pass)
        {
            size_t* page_sizes = port_vmem_page_sizes();
            size_t page_size = page_sizes[0];

            POINTER_SIZE_INT start =
                ((POINTER_SIZE_INT)addr + i) & ~(page_size - 1);
            POINTER_SIZE_INT pastend =
                ((POINTER_SIZE_INT)addr + size + page_size - 1) & ~(page_size - 1);

            int result = mprotect((void*)start, pastend - start,
                                    PROT_READ | PROT_WRITE | PROT_EXEC);

            if (result == EAGAIN)
            {
                timespec delay = {0, 10};
                nanosleep(&delay, NULL);
                result = mprotect((void*)start, pastend - start,
                                    PROT_READ | PROT_WRITE | PROT_EXEC);
            }

            if (result != 0)
                return NCAI_ERROR_ACCESS_DENIED;
        }

        if (calc_pass)
            ++size;

        jvmti_thread->violation_flag = 1;
        jvmti_thread->violation_restart_address = read_memory_2nd_label;

        for (; i < size; i++)
        {
            *dest_ptr++ = *src_ptr++;

            void* label = (gen_get_return_address_stub())();
            read_memory_2nd_label = label;

            if (jvmti_thread->violation_flag == 0)
                return NCAI_ERROR_ACCESS_DENIED;
        }
    }

    jvmti_thread->violation_flag = 0;
    return NCAI_ERROR_NONE;
#endif // #ifndef _IA32_
}


ncaiError ncai_read_memory(void* addr, size_t size, void* buf)
{
    if (!buf || !addr)
        return NCAI_ERROR_NULL_POINTER;

    if (size == 0)
        return NCAI_ERROR_ILLEGAL_ARGUMENT;

    if (p_TLS_vmthread == NULL)
        return NCAI_ERROR_INTERNAL;

    if (ncai_read_memory_internal(NULL, 0, NULL, true) != NCAI_ERROR_NONE)
        return NCAI_ERROR_INTERNAL;

    return ncai_read_memory_internal(addr, size, buf, false);
}


static ncaiError ncai_write_memory_internal(void* addr, size_t size, void* buf, bool UNUSED calc_pass = false)
{
#ifndef _IA32_
    // SIGSEGV processing is not implemented for EM64T/IPF
    memcpy(addr, buf, size);
    return NCAI_ERROR_NONE;
#else // #ifndef _IA32_

static void* write_memory_1st_label = NULL;
static void* write_memory_2nd_label = NULL;

    char dummy_str[3];

    if (calc_pass)
    {
        addr = dummy_str;
        buf = dummy_str;
        *dummy_str = 'a'; // To avoid warnings
        size = 1;
    }

    jvmti_thread_t jvmti_thread = &p_TLS_vmthread->jvmti_thread;

    unsigned char* src_ptr = (unsigned char*)buf;
    unsigned char* dest_ptr = (unsigned char*)addr;

    jvmti_thread->violation_flag = 1;
    jvmti_thread->violation_restart_address = write_memory_1st_label;

    size_t i;
    for (i = 0; i < size; i++)
    {
        *dest_ptr = *src_ptr;

        void* label = (gen_get_return_address_stub())();
        write_memory_1st_label = label;

        if (jvmti_thread->violation_flag == 0)
            break;

        ++src_ptr;
        ++dest_ptr;
    }

    if (jvmti_thread->violation_flag == 0 || calc_pass)
    {
        if (!calc_pass)
        {
            size_t* page_sizes = port_vmem_page_sizes();
            size_t page_size = page_sizes[0];

            POINTER_SIZE_INT start =
                ((POINTER_SIZE_INT)addr + i) & ~(page_size - 1);
            POINTER_SIZE_INT pastend =
                ((POINTER_SIZE_INT)addr + size + page_size - 1) & ~(page_size - 1);

            int result = mprotect((void*)start, pastend - start,
                                    PROT_READ | PROT_WRITE | PROT_EXEC);

            if (result == EAGAIN)
            {
                timespec delay = {0, 10};
                nanosleep(&delay, NULL);
                result = mprotect((void*)start, pastend - start,
                                    PROT_READ | PROT_WRITE | PROT_EXEC);
            }

            if (result != 0)
                return NCAI_ERROR_ACCESS_DENIED;
        }

        if (calc_pass)
            ++size;

        jvmti_thread->violation_flag = 1;
        jvmti_thread->violation_restart_address = write_memory_2nd_label;

        for (; i < size; i++)
        {
            *dest_ptr++ = *src_ptr++;

            void* label = (gen_get_return_address_stub())();
            write_memory_2nd_label = label;

            if (jvmti_thread->violation_flag == 0)
                return NCAI_ERROR_ACCESS_DENIED;
        }
    }

#ifdef _IPF_
    asm volatile ("mf" ::: "memory");
#else
    __asm__("mfence");
#endif

    jvmti_thread->violation_flag = 0;
    return NCAI_ERROR_NONE;
#endif // #ifndef _IA32_
}

ncaiError ncai_write_memory(void* addr, size_t size, void* buf)
{
    if (!buf || !addr)
        return NCAI_ERROR_NULL_POINTER;

    if (size == 0)
        return NCAI_ERROR_ILLEGAL_ARGUMENT;

    if (p_TLS_vmthread == NULL)
        return NCAI_ERROR_INTERNAL;

    if (ncai_write_memory_internal(NULL, 0, NULL, true) != NCAI_ERROR_NONE)
        return NCAI_ERROR_INTERNAL;

    return ncai_write_memory_internal(addr, size, buf, false);
}
