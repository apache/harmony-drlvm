/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */

#include <open/types.h>
//#include "m2n.h"
//#include "m2n_ia32_internal.h"
#include <open/ncai_thread.h>
#include "jit_export_rt.h"

#include "ncai_internal.h"


struct NcaiRegisters
{
    U_32  eax;
    U_32  ebx;
    U_32  ecx;
    U_32  edx;
    U_32  esp;
    U_32  ebp;
    U_32  esi;
    U_32  edi;
    uint16  ds;
    uint16  es;
    uint16  fs;
    uint16  gs;
    uint16  ss;
    uint16  cs;
    U_32  eip;
    U_32  eflags;
};

#define REGSIZE(_field_) ((jint)sizeof(((NcaiRegisters*)0)->_field_))
#define REGOFF(_field_) ((POINTER_SIZE_INT)&(((NcaiRegisters*)0)->_field_))

NcaiRegisterTableItem g_ncai_reg_table[] = {
    {"eax",     REGSIZE(eax),   REGOFF(eax)   },
    {"ebx",     REGSIZE(ebx),   REGOFF(ebx)   },
    {"ecx",     REGSIZE(ecx),   REGOFF(ecx)   },
    {"edx",     REGSIZE(edx),   REGOFF(edx)   },
    {"esp",     REGSIZE(esp),   REGOFF(esp)   },
    {"ebp",     REGSIZE(ebp),   REGOFF(ebp)   },
    {"esi",     REGSIZE(esi),   REGOFF(esi)   },
    {"edi",     REGSIZE(edi),   REGOFF(edi)   },
    {"ds",      REGSIZE(ds),    REGOFF(ds)    },
    {"es",      REGSIZE(es),    REGOFF(es)    },
    {"fs",      REGSIZE(fs),    REGOFF(fs)    },
    {"gs",      REGSIZE(gs),    REGOFF(gs)    },
    {"ss",      REGSIZE(ss),    REGOFF(ss)    },
    {"cs",      REGSIZE(cs),    REGOFF(cs)    },
    {"eip",     REGSIZE(eip),   REGOFF(eip)   },
    {"eflags",  REGSIZE(eflags),REGOFF(eflags)},
};

size_t ncai_get_reg_table_size()
{
    return sizeof(g_ncai_reg_table)/sizeof(g_ncai_reg_table[0]);
}

#ifdef PLATFORM_POSIX

static void ncai_context_to_registers(ucontext_t* pcontext, NcaiRegisters* pregs)
{
    pregs->eax  = pcontext->uc_mcontext.gregs[REG_EAX];
    pregs->ebx  = pcontext->uc_mcontext.gregs[REG_EBX];
    pregs->ecx  = pcontext->uc_mcontext.gregs[REG_ECX];
    pregs->edx  = pcontext->uc_mcontext.gregs[REG_EDX];
    pregs->esp  = pcontext->uc_mcontext.gregs[REG_ESP];
    pregs->ebp  = pcontext->uc_mcontext.gregs[REG_EBP];
    pregs->esi  = pcontext->uc_mcontext.gregs[REG_ESI];
    pregs->edi  = pcontext->uc_mcontext.gregs[REG_EDI];
    pregs->ds = pcontext->uc_mcontext.gregs[REG_DS];
    pregs->es = pcontext->uc_mcontext.gregs[REG_ES];
    pregs->fs = pcontext->uc_mcontext.gregs[REG_FS];
    pregs->gs = pcontext->uc_mcontext.gregs[REG_GS];
    pregs->ss = pcontext->uc_mcontext.gregs[REG_SS];
    pregs->cs = pcontext->uc_mcontext.gregs[REG_CS];
    pregs->eip    = pcontext->uc_mcontext.gregs[REG_EIP];
    pregs->eflags = pcontext->uc_mcontext.gregs[REG_EFL];
}

static void ncai_registers_to_context(NcaiRegisters* pregs, ucontext_t* pcontext)
{
    pcontext->uc_mcontext.gregs[REG_EAX]  = pregs->eax;
    pcontext->uc_mcontext.gregs[REG_EBX]  = pregs->ebx;
    pcontext->uc_mcontext.gregs[REG_ECX]  = pregs->ecx;
    pcontext->uc_mcontext.gregs[REG_EDX]  = pregs->edx;
    pcontext->uc_mcontext.gregs[REG_ESP]  = pregs->esp;
    pcontext->uc_mcontext.gregs[REG_EBP]  = pregs->ebp;
    pcontext->uc_mcontext.gregs[REG_ESI]  = pregs->esi;
    pcontext->uc_mcontext.gregs[REG_EDI]  = pregs->edi;
    pcontext->uc_mcontext.gregs[REG_DS] = pregs->ds;
    pcontext->uc_mcontext.gregs[REG_ES] = pregs->es;
    pcontext->uc_mcontext.gregs[REG_FS] = pregs->fs;
    pcontext->uc_mcontext.gregs[REG_GS] = pregs->gs;
    pcontext->uc_mcontext.gregs[REG_SS] = pregs->ss;
    pcontext->uc_mcontext.gregs[REG_CS] = pregs->cs;
    pcontext->uc_mcontext.gregs[REG_EIP]  = pregs->eip;
    pcontext->uc_mcontext.gregs[REG_EFL]  = pregs->eflags;
}

#else // #ifdef PLATFORM_POSIX

#ifndef __INTEL_COMPILER
//4244 - conversion from 'int' to 'unsigned short', possible loss of data
    #pragma warning (disable:4244)
#endif

static void ncai_context_to_registers(CONTEXT* pcontext, NcaiRegisters* pregs)
{
    pregs->eax    = pcontext->Eax;
    pregs->ebx    = pcontext->Ebx;
    pregs->ecx    = pcontext->Ecx;
    pregs->edx    = pcontext->Edx;
    pregs->esp    = pcontext->Esp;
    pregs->ebp    = pcontext->Ebp;
    pregs->esi    = pcontext->Esi;
    pregs->edi    = pcontext->Edi;
    pregs->ds     = pcontext->SegDs;
    pregs->es     = pcontext->SegEs;
    pregs->fs     = pcontext->SegFs;
    pregs->gs     = pcontext->SegGs;
    pregs->ss     = pcontext->SegSs;
    pregs->cs     = pcontext->SegCs;
    pregs->eip    = pcontext->Eip;
    pregs->eflags = pcontext->EFlags;
}

static void ncai_registers_to_context(NcaiRegisters* pregs, CONTEXT* pcontext)
{
    pcontext->Eax     = pregs->eax;
    pcontext->Ebx     = pregs->ebx;
    pcontext->Ecx     = pregs->ecx;
    pcontext->Edx     = pregs->edx;
    pcontext->Esp     = pregs->esp;
    pcontext->Ebp     = pregs->ebp;
    pcontext->Esi     = pregs->esi;
    pcontext->Edi     = pregs->edi;
    pcontext->SegDs   = pregs->ds;
    pcontext->SegEs   = pregs->es;
    pcontext->SegFs   = pregs->fs;
    pcontext->SegGs   = pregs->gs;
    pcontext->SegSs   = pregs->ss;
    pcontext->SegCs   = pregs->cs;
    pcontext->Eip     = pregs->eip;
    pcontext->EFlags  = pregs->eflags;
}

#endif // #ifdef PLATFORM_POSIX

bool ncai_get_register_value(hythread_t thread, jint reg_number, void* buf_ptr)
{
    thread_context_t context;
    IDATA status = hythread_get_thread_context(thread, &context);

    if (status != TM_ERROR_NONE)
        return false;

    NcaiRegisters regs;
    ncai_context_to_registers(&context, &regs);

    memcpy(
        buf_ptr,
        ((U_8*)&regs) + g_ncai_reg_table[reg_number].offset,
        g_ncai_reg_table[reg_number].size);

    return true;
}

bool ncai_set_register_value(hythread_t thread, jint reg_number, void* buf_ptr)
{
    thread_context_t context;
    IDATA status = hythread_get_thread_context(thread, &context);

    if (status != TM_ERROR_NONE)
        return false;

    NcaiRegisters regs;
    ncai_context_to_registers(&context, &regs);

    memcpy(
        ((U_8*)&regs) + g_ncai_reg_table[reg_number].offset,
        buf_ptr,
        g_ncai_reg_table[reg_number].size);

    ncai_registers_to_context(&regs, &context);

    status = hythread_set_thread_context(thread, &context);

    return (status == TM_ERROR_NONE);
}

void* ncai_get_instruction_pointer(hythread_t thread)
{
    thread_context_t context;
    IDATA status = hythread_get_thread_context(thread, &context);

    if (status != TM_ERROR_NONE)
        return NULL;

    NcaiRegisters regs;
    ncai_context_to_registers(&context, &regs);

    return (void*)regs.eip;
}
