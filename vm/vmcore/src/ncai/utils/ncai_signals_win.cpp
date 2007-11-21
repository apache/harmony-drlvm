/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */

#include "ncai_utils.h"
#include "ncai_direct.h"
#include "ncai_internal.h"

#define STR_AND_SIZE(_x_) _x_, (strlen(_x_) + 1)

struct st_signal_info
{
    uint32  signal;
    char*   name;
    size_t  name_size;
};

static st_signal_info sig_table[] = {
    {0x40010005, STR_AND_SIZE("DBG_CONTROL_C")},
    {0x40010008, STR_AND_SIZE("DBG_CONTROL_BREAK")},
    {0x80000002, STR_AND_SIZE("EXCEPTION_DATATYPE_MISALIGNMENT")},
    {0xC0000005, STR_AND_SIZE("EXCEPTION_ACCESS_VIOLATION")},
    {0xC0000006, STR_AND_SIZE("EXCEPTION_IN_PAGE_ERROR")},
    {0xC0000017, STR_AND_SIZE("STATUS_NO_MEMORY")},
    {0xC000001D, STR_AND_SIZE("EXCEPTION_ILLEGAL_INSTRUCTION")},
    {0xC0000025, STR_AND_SIZE("EXCEPTION_NONCONTINUABLE_EXCEPTION")},
    {0xC0000026, STR_AND_SIZE("EXCEPTION_INVALID_DISPOSITION")},
    {0xC000008C, STR_AND_SIZE("EXCEPTION_ARRAY_BOUNDS_EXCEEDED")},
    {0xC000008D, STR_AND_SIZE("EXCEPTION_FLT_DENORMAL_OPERAND")},
    {0xC000008E, STR_AND_SIZE("EXCEPTION_FLT_DIVIDE_BY_ZERO")},
    {0xC000008F, STR_AND_SIZE("EXCEPTION_FLT_INEXACT_RESULT")},
    {0xC0000090, STR_AND_SIZE("EXCEPTION_FLT_INVALID_OPERATION")},
    {0xC0000091, STR_AND_SIZE("EXCEPTION_FLT_OVERFLOW")},
    {0xC0000092, STR_AND_SIZE("EXCEPTION_FLT_STACK_CHECK")},
    {0xC0000093, STR_AND_SIZE("EXCEPTION_FLT_UNDERFLOW")},
    {0xC0000094, STR_AND_SIZE("EXCEPTION_INT_DIVIDE_BY_ZERO")},
    {0xC0000095, STR_AND_SIZE("EXCEPTION_INT_OVERFLOW")},
    {0xC0000096, STR_AND_SIZE("EXCEPTION_PRIV_INSTRUCTION")},
    {0xC00000FD, STR_AND_SIZE("EXCEPTION_STACK_OVERFLOW")},
    {0xC0000135, STR_AND_SIZE("STATUS_DLL_NOT_FOUND")},
    {0xC0000138, STR_AND_SIZE("STATUS_ORDINAL_NOT_FOUND")},
    {0xC0000139, STR_AND_SIZE("STATUS_ENTRYPOINT_NOT_FOUND")},
    {0xC0000142, STR_AND_SIZE("STATUS_DLL_INIT_FAILED")},
    {0xC06D007E, STR_AND_SIZE("MODULE_NOT_FOUND")},
    {0xC06D007F, STR_AND_SIZE("PROCEDURE_NOT_FOUND")},
    {0xE06D7363, STR_AND_SIZE("CPP_EXCEPTION")},
};

size_t ncai_get_signal_count()
{
    return sizeof(sig_table)/sizeof(sig_table[0]);
}

static st_signal_info* find_signal(jint sig)
{
    for (size_t i = 0; i < ncai_get_signal_count(); i++)
    {
        if ((jint)sig_table[i].signal == sig)
            return &sig_table[i];
    }

    return NULL;
}

uint32 ncai_get_min_signal()
{
    return sig_table[0].signal;
}

uint32 ncai_get_max_signal()
{
    return sig_table[ncai_get_signal_count() - 1].signal;
}

char* ncai_get_signal_name(jint signal)
{
    st_signal_info* psig = find_signal(signal);
    return psig ? psig->name : NULL;
}

size_t ncai_get_signal_name_size(jint signal)
{
    st_signal_info* psig = find_signal(signal);
    return psig ? psig->name_size : 0;
}

bool ncai_is_signal_in_range(jint signal)
{
    return ((uint32)signal >= ncai_get_min_signal() ||
            (uint32)signal <= ncai_get_max_signal());
}
