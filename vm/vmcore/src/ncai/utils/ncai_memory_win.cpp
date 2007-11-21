/**
 * @author Ilya Berezhniuk
 * @version $Revision$
 */
#include "ncai_direct.h"
#include "ncai_internal.h"


ncaiError ncai_read_memory(void* addr, size_t size, void* buf)
{
    if (!buf || !addr)
        return NCAI_ERROR_NULL_POINTER;

    if (size == 0)
        return NCAI_ERROR_ILLEGAL_ARGUMENT;

    SIZE_T bytes_read;

    BOOL result = ReadProcessMemory(GetCurrentProcess(),
                                    addr, buf, size, &bytes_read);
    assert(bytes_read == size);

    if (!result)
        return NCAI_ERROR_ACCESS_DENIED;

    return NCAI_ERROR_NONE;
}

ncaiError ncai_write_memory(void* addr, size_t size, void* buf)
{
    if (!buf || !addr)
        return NCAI_ERROR_NULL_POINTER;

    if (size == 0)
        return NCAI_ERROR_ILLEGAL_ARGUMENT;

    SIZE_T bytes_written;

    BOOL result = WriteProcessMemory(GetCurrentProcess(),
                                    addr, buf, size, &bytes_written);
    assert(bytes_written == size);

    if (!result)
        return NCAI_ERROR_ACCESS_DENIED;

    FlushInstructionCache(GetCurrentProcess(), addr, size);

    return NCAI_ERROR_NONE;
}
