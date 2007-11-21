/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */

#define LOG_DOMAIN "ncai.step"
#include "cxxlog.h"
#include "jvmti_break_intf.h"

#include "ncai_utils.h"
#include "ncai_direct.h"
#include "ncai_internal.h"


void ncai_setup_single_step(GlobalNCAI* ncai,
            const VMBreakPoint* bp, jvmti_thread_t jvmti_thread)
{
    TRACE2("ncai.step", "Setup predicted single step breakpoints: "
        << "not implemented for em64t");
}

void ncai_setup_signal_step(jvmti_thread_t jvmti_thread, NativeCodePtr addr)
{
    TRACE2("ncai.step", "Setting up single step in exception handler: "
        << "not implemented for IPF");
}
