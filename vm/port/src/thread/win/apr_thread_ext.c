#include <windows.h>
#include <stdio.h>
#include "apr_thread_ext.h"
//#include "apr_arch_threadproc.h"

static int convert_priority(apr_int32_t priority);

APR_DECLARE(apr_status_t) apr_thread_set_priority(apr_thread_t *thread, 
                apr_int32_t priority) 
{
    HANDLE *os_thread;
    apr_status_t status;
    
    if (status = apr_os_thread_get(&((apr_os_thread_t *)os_thread), thread)) {
        return status;
    }
    
    if (SetThreadPriority(os_thread, (int)convert_priority(priority))) {
        return APR_SUCCESS;
    } else {
        return apr_get_os_error();
    }
    
    
}

static int convert_priority(apr_int32_t priority) {
    return (int)priority;
}

// touch thread to flash memory
APR_DECLARE(apr_status_t) apr_thread_yield_other(apr_thread_t* thread) {
    HANDLE *os_thread = NULL;
    apr_status_t status;   
    if (status = apr_os_thread_get(&((apr_os_thread_t *)os_thread), thread)) {
        return status;
    }
        if(!os_thread) {
//        printf ("detached thread\n");
              return status;
        }
       //printf("suspending %d\n", os_thread);
    if(-1!=SuspendThread(os_thread)) {
         ResumeThread(os_thread);
 //      printf("resuming %d\n", os_thread);
        } else {
  //            printf("fail to suspend %d\n", os_thread);
        }
  return APR_SUCCESS; 
}

APR_DECLARE(void) apr_memory_rw_barrier() {
    __asm mfence;
}

APR_DECLARE(apr_status_t) apr_thread_times(apr_thread_t *thread, 
                                apr_time_t * kernel_time, apr_time_t * user_time){
    FILETIME creationTime;
    FILETIME exitTime;
    FILETIME kernelTime;
    FILETIME userTime;
    HANDLE *hThread;
    SYSTEMTIME sysTime;
    int res;
    __int64 xx;
    __int32 * pp;
    apr_status_t status;

    if (status = apr_os_thread_get(&((apr_os_thread_t *)hThread), thread)) {
        return status;
    }
                    
    res = GetThreadTimes(
        hThread,
        &creationTime,
        &exitTime,
        &kernelTime,
        &userTime
    );
    
    printf( "Creation time = %08x %08x\n", creationTime.dwHighDateTime, creationTime.dwLowDateTime);
    printf( "Exit     time = %08x %08x\n", exitTime.dwHighDateTime, exitTime.dwLowDateTime);
    printf( "Kernrl   time = %08x %08x %08d\n", kernelTime.dwHighDateTime
                                      , kernelTime.dwLowDateTime, kernelTime.dwLowDateTime);
    printf( "User     time = %08x %08x %08d\n", userTime.dwHighDateTime
                                      , userTime.dwLowDateTime, userTime.dwLowDateTime);
    printf("%d\n", 
        ((unsigned)exitTime.dwLowDateTime - (unsigned)creationTime.dwLowDateTime)/10000000);
    
    FileTimeToSystemTime(&creationTime, &sysTime);
    printf("%d %d %d %d %d %d \n", sysTime.wYear, sysTime.wMonth,
        sysTime.wHour + 3, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
    
    pp = (int*)&xx;
    *pp = kernelTime.dwLowDateTime;
    *(pp + 1) = kernelTime.dwHighDateTime;
    *kernel_time = xx;
    pp = (int*)&xx;
    *pp = userTime.dwLowDateTime;
    *(pp + 1) = userTime.dwHighDateTime;
    *user_time = xx;

    return APR_SUCCESS; 
}

APR_DECLARE(apr_status_t) apr_get_thread_time(apr_thread_t *thread, unsigned long long* nanos_ptr)
{
    HANDLE *os_thread;
    apr_status_t status;   
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (status = apr_os_thread_get(&((apr_os_thread_t *)os_thread), thread)!=APR_SUCCESS) {
        return status;
    }
    GetThreadTimes(os_thread, &creation_time, 
        &exit_time, &kernel_time, 
        &user_time);

    *nanos_ptr=(Int64ShllMod32((&user_time)->dwHighDateTime, 32)|(&user_time)->dwLowDateTime);//*100;
   // *nanos_ptr = user_time * 100; // convert to nanos
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_thread_cancel(apr_thread_t *thread) {
    HANDLE *os_thread;
    apr_status_t status;   
    if (status = apr_os_thread_get(&((apr_os_thread_t *)os_thread), thread)) {
        return status;
    }
    
    if (TerminateThread(os_thread, 0)) {
        return APR_SUCCESS;
    } else {
        return apr_get_os_error();
    }
}
