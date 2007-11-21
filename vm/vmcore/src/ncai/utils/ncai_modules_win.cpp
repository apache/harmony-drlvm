/**
 * @author Petr Ivanov
 * @version $Revision$
 */

#include <memory.h>
#include <string.h>
#include "ncai_utils.h"
#include "ncai_internal.h"


char* ncai_parse_module_name(char* filepath)
{
    size_t length = 0;
    char* filename = strrchr(filepath, '\\');

    if (filename)
    {
        filename = filename + 1;
        length = strlen(filename);
        size_t dot_len = length - 4;
        char* dot_pos = filename + dot_len;

        if (strcmp(dot_pos, ".dll") == 0 ||
            strcmp(dot_pos, ".exe") == 0)
        {
            length = dot_len;
        }
    }
    else
    {
        filename = filepath;
        length = strlen(filename);
    }

    char* ret = (char*)ncai_alloc(length + 1);
    if (ret)
    {
        memcpy(ret, filename, length);
        ret[length] = '\0';
    }

    return ret;
}

