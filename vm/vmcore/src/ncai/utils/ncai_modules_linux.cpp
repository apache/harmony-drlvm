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
    char* filename = strrchr(filepath, '/');
    filename = filename ? (filename + 1) : filepath;
    char* dot_so = strstr(filename, ".so");

    if (dot_so != NULL) // We have shared library, cut off 'lib' too
    {
        if (memcmp(filename, "lib", 3) == 0)
            filename += 3;

        length = dot_so - filename;
    }
    else
    {
        dot_so = strstr(filename, ".exe");

        if (dot_so != NULL)
            length = dot_so - filename;
        else
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
