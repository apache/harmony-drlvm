/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */

#include "open/ncai_thread.h"
#include "ncai_internal.h"

void ucontext_to_regs(Registers* regs, ucontext_t *uc);
void regs_to_ucontext(ucontext_t *uc, Registers* regs);

bool ncai_get_generic_registers(hythread_t thread, Registers* regs)
{
    if (regs == NULL)
        return false;

    ucontext_t uc;
    IDATA status = hythread_get_thread_context(thread, &uc);

    if (status != TM_ERROR_NONE)
        return false;

    ucontext_to_regs(regs, &uc);
    return true;
}
