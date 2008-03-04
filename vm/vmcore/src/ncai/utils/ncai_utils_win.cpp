/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */

#include "open/ncai_thread.h"
#include "port_thread.h"
#include "ncai_internal.h"

bool ncai_get_generic_registers(hythread_t thread, Registers* regs)
{
    if (regs == NULL)
        return false;

    CONTEXT context;
    context.ContextFlags = CONTEXT_FULL; // CONTEXT_ALL
    IDATA status = hythread_get_thread_context(thread, &context);

    if (status != TM_ERROR_NONE)
        return false;

    port_thread_context_to_regs(regs, &context);
    return true;
}
