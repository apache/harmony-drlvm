/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */

#include "open/types.h"
#include <open/ncai_thread.h>

#include "ncai_internal.h"


struct NcaiRegisters
{
    uint64  rax;
    uint64  rbx;
    uint64  rcx;
    uint64  rdx;
    uint64  rsp;
    uint64  rbp;
    uint64  rsi;
    uint64  rdi;
    uint64  r8;
    uint64  r9;
    uint64  r10;
    uint64  r11;
    uint64  r12;
    uint64  r13;
    uint64  r14;
    uint64  r15;
    uint16  ds;
    uint16  es;
    uint16  fs;
    uint16  gs;
    uint16  ss;
    uint16  cs;
    uint64  rip;
    U_32  eflags;
};

#define REGSIZE(_field_) ((jint)sizeof(((NcaiRegisters*)0)->_field_))
#define REGOFF(_field_) ((POINTER_SIZE_INT)&(((NcaiRegisters*)0)->_field_))

NcaiRegisterTableItem g_ncai_reg_table[] = {
    {"rax",     REGSIZE(rax),   REGOFF(rax)   },
    {"rbx",     REGSIZE(rbx),   REGOFF(rbx)   },
    {"rcx",     REGSIZE(rcx),   REGOFF(rcx)   },
    {"rdx",     REGSIZE(rdx),   REGOFF(rdx)   },
    {"rsp",     REGSIZE(rsp),   REGOFF(rsp)   },
    {"rbp",     REGSIZE(rbp),   REGOFF(rbp)   },
    {"rsi",     REGSIZE(rsi),   REGOFF(rsi)   },
    {"rdi",     REGSIZE(rdi),   REGOFF(rdi)   },
    {"ds",      REGSIZE(ds),    REGOFF(ds)    },
    {"es",      REGSIZE(es),    REGOFF(es)    },
    {"fs",      REGSIZE(fs),    REGOFF(fs)    },
    {"gs",      REGSIZE(gs),    REGOFF(gs)    },
    {"ss",      REGSIZE(ss),    REGOFF(ss)    },
    {"cs",      REGSIZE(cs),    REGOFF(cs)    },
    {"rip",     REGSIZE(rip),   REGOFF(rip)   },
    {"eflags",  REGSIZE(eflags),REGOFF(eflags)},
};

size_t ncai_get_reg_table_size()
{
    return sizeof(g_ncai_reg_table)/sizeof(g_ncai_reg_table[0]);
}

#ifdef PLATFORM_POSIX

static void ncai_context_to_registers(ucontext_t* pcontext, NcaiRegisters* pregs)
{
    pregs->rax  = pcontext->uc_mcontext.gregs[REG_RAX];
    pregs->rbx  = pcontext->uc_mcontext.gregs[REG_RBX];
    pregs->rcx  = pcontext->uc_mcontext.gregs[REG_RCX];
    pregs->rdx  = pcontext->uc_mcontext.gregs[REG_RDX];
    pregs->rsp  = pcontext->uc_mcontext.gregs[REG_RSP];
    pregs->rbp  = pcontext->uc_mcontext.gregs[REG_RBP];
    pregs->rsi  = pcontext->uc_mcontext.gregs[REG_RSI];
    pregs->rdi  = pcontext->uc_mcontext.gregs[REG_RDI];
    pregs->r8   = pcontext->uc_mcontext.gregs[REG_R8];
    pregs->r9   = pcontext->uc_mcontext.gregs[REG_R9];
    pregs->r10  = pcontext->uc_mcontext.gregs[REG_R10];
    pregs->r11  = pcontext->uc_mcontext.gregs[REG_R11];
    pregs->r12  = pcontext->uc_mcontext.gregs[REG_R12];
    pregs->r13  = pcontext->uc_mcontext.gregs[REG_R13];
    pregs->r14  = pcontext->uc_mcontext.gregs[REG_R14];
    pregs->r15  = pcontext->uc_mcontext.gregs[REG_R15];
    pregs->fs = ((uint16*)&pcontext->uc_mcontext.gregs[REG_CSGSFS])[2];
    pregs->gs = ((uint16*)&pcontext->uc_mcontext.gregs[REG_CSGSFS])[1];
    pregs->ss   = 0;
    pregs->cs = ((uint16*)&pcontext->uc_mcontext.gregs[REG_CSGSFS])[0];
    pregs->rip    = pcontext->uc_mcontext.gregs[REG_RIP];
    pregs->eflags = pcontext->uc_mcontext.gregs[REG_EFL];
}

static void ncai_registers_to_context(NcaiRegisters* pregs, ucontext_t* pcontext)
{
    pcontext->uc_mcontext.gregs[REG_RAX]  = pregs->rax;
    pcontext->uc_mcontext.gregs[REG_RBX]  = pregs->rbx;
    pcontext->uc_mcontext.gregs[REG_RCX]  = pregs->rcx;
    pcontext->uc_mcontext.gregs[REG_RDX]  = pregs->rdx;
    pcontext->uc_mcontext.gregs[REG_RSP]  = pregs->rsp;
    pcontext->uc_mcontext.gregs[REG_RBP]  = pregs->rbp;
    pcontext->uc_mcontext.gregs[REG_RSI]  = pregs->rsi;
    pcontext->uc_mcontext.gregs[REG_RDI]  = pregs->rdi;
    pcontext->uc_mcontext.gregs[REG_R8]   = pregs->r8;
    pcontext->uc_mcontext.gregs[REG_R9]   = pregs->r9;
    pcontext->uc_mcontext.gregs[REG_R10]  = pregs->r10;
    pcontext->uc_mcontext.gregs[REG_R11]  = pregs->r11;
    pcontext->uc_mcontext.gregs[REG_R12]  = pregs->r12;
    pcontext->uc_mcontext.gregs[REG_R13]  = pregs->r13;
    pcontext->uc_mcontext.gregs[REG_R14]  = pregs->r14;
    pcontext->uc_mcontext.gregs[REG_R15]  = pregs->r15;
    ((uint16*)&pcontext->uc_mcontext.gregs[REG_CSGSFS])[2] = pregs->fs;
    ((uint16*)&pcontext->uc_mcontext.gregs[REG_CSGSFS])[1]  = pregs->gs;
    // ss register is not restored, because there is no storage for it
    ((uint16*)&pcontext->uc_mcontext.gregs[REG_CSGSFS])[0]  = pregs->cs;
    pcontext->uc_mcontext.gregs[REG_RIP]  = pregs->rip;
    pcontext->uc_mcontext.gregs[REG_EFL]  = pregs->eflags;
}

#else // #ifdef PLATFORM_POSIX

static void ncai_context_to_registers(CONTEXT* pcontext, NcaiRegisters* pregs)
{
    pregs->rax    = pcontext->Rax;
    pregs->rbx    = pcontext->Rbx;
    pregs->rcx    = pcontext->Rcx;
    pregs->rdx    = pcontext->Rdx;
    pregs->rsp    = pcontext->Rsp;
    pregs->rbp    = pcontext->Rbp;
    pregs->rsi    = pcontext->Rsi;
    pregs->rdi    = pcontext->Rdi;
    pregs->r8     = pcontext->R8;
    pregs->r9     = pcontext->R9;
    pregs->r10    = pcontext->R10;
    pregs->r11    = pcontext->R11;
    pregs->r12    = pcontext->R12;
    pregs->r13    = pcontext->R13;
    pregs->r14    = pcontext->R14;
    pregs->r15    = pcontext->R15;
    pregs->ds     = pcontext->SegDs;
    pregs->es     = pcontext->SegEs;
    pregs->fs     = pcontext->SegFs;
    pregs->gs     = pcontext->SegGs;
    pregs->ss     = pcontext->SegSs;
    pregs->cs     = pcontext->SegCs;
    pregs->rip    = pcontext->Rip;
    pregs->eflags = pcontext->EFlags;
}

static void ncai_registers_to_context(NcaiRegisters* pregs, CONTEXT* pcontext)
{
    pcontext->Rax     = pregs->rax;
    pcontext->Rbx     = pregs->rbx;
    pcontext->Rcx     = pregs->rcx;
    pcontext->Rdx     = pregs->rdx;
    pcontext->Rsp     = pregs->rsp;
    pcontext->Rbp     = pregs->rbp;
    pcontext->Rsi     = pregs->rsi;
    pcontext->Rdi     = pregs->rdi;
    pcontext->R8      = pregs->r8;
    pcontext->R9      = pregs->r9;
    pcontext->R10     = pregs->r10;
    pcontext->R11     = pregs->r11;
    pcontext->R12     = pregs->r12;
    pcontext->R13     = pregs->r13;
    pcontext->R14     = pregs->r14;
    pcontext->R15     = pregs->r15;
    pcontext->SegDs   = pregs->ds;
    pcontext->SegEs   = pregs->es;
    pcontext->SegFs   = pregs->fs;
    pcontext->SegGs   = pregs->gs;
    pcontext->SegSs   = pregs->ss;
    pcontext->SegCs   = pregs->cs;
    pcontext->Rip     = pregs->rip;
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

    return (void*)regs.rip;
}
