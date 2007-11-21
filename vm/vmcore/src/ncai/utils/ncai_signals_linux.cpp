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
    jint    signal;
    char*   name;
    size_t  name_size;
};

static st_signal_info sig_table[] = {
    {SIGHUP,     STR_AND_SIZE("SIGHUP")},
    {SIGINT,     STR_AND_SIZE("SIGINT")},
    {SIGQUIT,    STR_AND_SIZE("SIGQUIT")},
    {SIGILL,     STR_AND_SIZE("SIGILL")},
    {SIGTRAP,    STR_AND_SIZE("SIGTRAP")},
    {SIGABRT,    STR_AND_SIZE("SIGABRT")},
    {SIGFPE,     STR_AND_SIZE("SIGFPE")},
    {SIGKILL,    STR_AND_SIZE("SIGKILL")},
    {SIGSEGV,    STR_AND_SIZE("SIGSEGV")},
    {SIGPIPE,    STR_AND_SIZE("SIGPIPE")},
    {SIGALRM,    STR_AND_SIZE("SIGALRM")},
    {SIGTERM,    STR_AND_SIZE("SIGTERM")},
    {SIGUSR1,    STR_AND_SIZE("SIGUSR1")},
    {SIGUSR2,    STR_AND_SIZE("SIGUSR2")},
    {SIGCHLD,    STR_AND_SIZE("SIGCHLD")},
    {SIGCONT,    STR_AND_SIZE("SIGCONT")},
    {SIGSTOP,    STR_AND_SIZE("SIGSTOP")},
    {SIGTSTP,    STR_AND_SIZE("SIGTSTP")},
    {SIGTTIN,    STR_AND_SIZE("SIGTTIN")},
    {SIGTTOU,    STR_AND_SIZE("SIGTTOU")},
};

size_t ncai_get_signal_count()
{
    return sizeof(sig_table)/sizeof(sig_table[0]);
}

static st_signal_info* find_signal(jint sig)
{
    for (size_t i = 0; i < ncai_get_signal_count(); i++)
    {
        if (sig_table[i].signal == sig)
            return &sig_table[i];
    }

    return NULL;
}

jint ncai_get_min_signal()
{
    static int min_sig_value = sig_table[1].signal;

    if (min_sig_value != sig_table[1].signal)
        return min_sig_value;

    min_sig_value = sig_table[0].signal;

    for (size_t i = 1; i < ncai_get_signal_count(); i++)
    {
        if (sig_table[i].signal < min_sig_value)
            min_sig_value = sig_table[i].signal;
    }

    return min_sig_value;
}

jint ncai_get_max_signal()
{
    static int max_sig_value = -1;

    if (max_sig_value != -1)
        return max_sig_value;

    max_sig_value = sig_table[0].signal;

    for (size_t i = 1; i < ncai_get_signal_count(); i++)
    {
        if (sig_table[i].signal > max_sig_value)
            max_sig_value = sig_table[i].signal;
    }

    return max_sig_value;
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
    return (signal >= ncai_get_min_signal() ||
            signal <= ncai_get_max_signal());
}
