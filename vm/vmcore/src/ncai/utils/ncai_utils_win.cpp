/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */

#include "open/ncai_thread.h"
#include "ncai_internal.h"

void nt_to_vm_context(PCONTEXT pcontext, Registers* regs);
void vm_to_nt_context(Registers* regs, PCONTEXT pcontext);

bool ncai_get_generic_registers(hythread_t thread, Registers* regs)
{
    if (regs == NULL)
        return false;

    CONTEXT context;
    context.ContextFlags = CONTEXT_FULL; // CONTEXT_ALL
    IDATA status = hythread_get_thread_context(thread, &context);

    if (status != TM_ERROR_NONE)
        return false;

    nt_to_vm_context(&context, regs);
    return true;
}
