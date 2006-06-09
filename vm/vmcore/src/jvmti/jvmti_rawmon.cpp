/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/** 
 * @author Gregory Shimansky, Pavel Rebriy
 * @version $Revision: 1.1.2.2.4.4 $
 */  
/*
 * JVMTI raw monitor API
 */

#include <apr_time.h>

#include "platform_lowlevel.h"
#include "lock_manager.h"
#include "jvmti_direct.h"
#include "jvmti_utils.h"
#include "vm_threads.h"
#include "vm_process.h"
#include "cxxlog.h"
#include "open/thread.h"
#include "suspend_checker.h"

#define JVMTI_RAW_MONITOR_TABLE_SIZE 100

/**
 * Struct of raw monitor
 */
struct jvmtiRawMonitor
{
    // lock monitor object
    Lock_Manager m_owner_lock;
    // lock wait lock object
    Lock_Manager m_wait_lock;
    // owner thread
    VM_thread *m_owner_thread;
    // wait event
    VmEventHandle m_wait_event;
    // wait notify event
    VmEventHandle m_notify_event;
    // recursive enter counter
    unsigned m_owned_count;
    // monitor entry counter
    unsigned m_enter_count;
    // monitor waters count
    unsigned m_wait_count;
    // flag for notify
    bool m_leave;
    // flag for notifyAll
    bool m_notifyall;
    // constructor
    jvmtiRawMonitor(): m_owner_thread(NULL),
                       m_wait_event(0), m_notify_event(0),
                       m_owned_count(0), m_enter_count(0), m_wait_count(0),
                       m_leave(false), m_notifyall(false) {}
};

/**
 * Raw monitor table entry
 */
struct jvmtiRawMonitorTableEntry {
    // pointer for raw monitor
    jvmtiRawMonitor *m_monitor;
    // index of next free entry of table
    int m_nextFree;
    // constructor
    jvmtiRawMonitorTableEntry(): m_monitor(NULL), m_nextFree(-1) {}
};

/**
 * Raw monitor table
 */
static jvmtiRawMonitorTableEntry g_rawMonitors[JVMTI_RAW_MONITOR_TABLE_SIZE];

/**
 * Free entry in raw monitor table
 */
static unsigned g_freeMonitor = 1;

/**
 * Global entry locker for raw monitor
 */
static Lock_Manager jvmtiRawMonitorLock;

/**
 * Function checks valid monitor in raw monitor table
 * and checks monitor owner for thread.
 */
static jvmtiError
jvmtiCheckValidMonitor( jrawMonitorID monitor )
{
    jvmtiRawMonitor *rawmonitor;
    unsigned index;

    /**
     * Check valid monitor in raw monitor table
     */
    if( (index=(unsigned)((POINTER_SIZE_INT)monitor)) >= JVMTI_RAW_MONITOR_TABLE_SIZE
       || ( (rawmonitor = g_rawMonitors[index].m_monitor) == NULL ) )
    {
        return JVMTI_ERROR_INVALID_MONITOR;
    }
    /**
     * Check monitor owner for thread
     */
    if( rawmonitor->m_owner_thread != get_thread_ptr() ) {
        return JVMTI_ERROR_NOT_MONITOR_OWNER;
    }
    return JVMTI_ERROR_NONE;
} // jvmtiCheckValidMonitor

/*
 * Create Raw Monitor
 *
 * Create a raw monitor
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiCreateRawMonitor(jvmtiEnv* env,
                      const char* name,
                      jrawMonitorID* monitor_ptr)
{
    /**
     * Monitor trace
     */
    TRACE2("jvmti.monitor", "CreateRawMonitor called, name = " << name);
    SuspendEnabledChecker sec;

    /**
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    /**
     * Check valid name and monitor_ptr
     */
    if( !name || !monitor_ptr ) {
        return JVMTI_ERROR_NULL_POINTER;
    }
    *monitor_ptr = 0;
    /**
     * Check valid index in raw monitor table
     */
    if( g_freeMonitor >= JVMTI_RAW_MONITOR_TABLE_SIZE ) {
        return JVMTI_ERROR_OUT_OF_MEMORY;
    }
    /**
     * Only one thread can run next block
     * another will be stopped at global raw monitor lock
     */
    {
        VmEventHandle handle;
        /**
         * Lock global monitor lock
         */
        LMAutoUnlock aulock( &jvmtiRawMonitorLock );
        /**
         * Create monitor
         */
        jvmtiRawMonitor *rawmonitor = new jvmtiRawMonitor;
        if( rawmonitor == NULL ) {
            return JVMTI_ERROR_OUT_OF_MEMORY;
        }
        /**
         * Create wait event handle
         */
        handle = vm_create_event( NULL, TRUE, FALSE, NULL );
        if( handle == 0 ) {
            // FIXME - What do we have to do if we cannot create event object?
            return JVMTI_ERROR_INTERNAL;
        }
        rawmonitor->m_wait_event = handle;
        /**
         * Create notify event handle
         */
        handle = vm_create_event( NULL, TRUE, FALSE, NULL );
        if( handle == 0 ) {
            // FIXME - What do we have to do if we cannot create event object?
            return JVMTI_ERROR_INTERNAL;
        }
        rawmonitor->m_notify_event = handle;
        /**
         * Set monitor into raw monitor table and
         * set monitor_ptr as index in raw monitor table
         */
        g_rawMonitors[g_freeMonitor].m_monitor = rawmonitor;
        *monitor_ptr = (jrawMonitorID)((POINTER_SIZE_INT)g_freeMonitor);
        /**
         * Change raw monitor table free
         */
        if( g_rawMonitors[g_freeMonitor].m_nextFree == -1 ) {
            // created monitor is the last in raw monitor table
            g_freeMonitor++;
        } else {
            // get next free monitor from raw monitor table entry
            g_freeMonitor = g_rawMonitors[g_freeMonitor].m_nextFree;
        }
    }

    /**
     * Monitor trace
     */
    TRACE2("jvmti.monitor", "CreateRawMonitor finished, name = "
            << name << " id = " << *monitor_ptr);

    return JVMTI_ERROR_NONE;
} // jvmtiCreateRawMonitor

/*
 * Destroy Raw Monitor
 *
 * Destroy the raw monitor. If the monitor being destroyed has
 * been entered by this thread, it will be exited before it is
 * destroyed. If the monitor being destroyed has been entered by
 * another thread, an error will be returned and the monitor will
 * not be destroyed.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiDestroyRawMonitor(jvmtiEnv* env,
                       jrawMonitorID monitor)
{
    unsigned index;
    jvmtiRawMonitor *rawmonitor;

    /**
     * Monitor trace
     */
    TRACE2("jvmti.monitor", "DestroyRawMonitor called, id = " << monitor);
    SuspendEnabledChecker sec;

    /**
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    /**
     * Check valid monitor in raw monitor table
     */
    if( (index = (unsigned)((POINTER_SIZE_INT)monitor)) >= JVMTI_RAW_MONITOR_TABLE_SIZE
       || ( (rawmonitor = g_rawMonitors[index].m_monitor) == NULL ) )
    {
        return JVMTI_ERROR_INVALID_MONITOR;
    }

    /**
     * Lock global raw monitor enter lock
     * It cannot allow enter monitor and stop on monitor lock
     */
    LMAutoUnlock aulock( &jvmtiRawMonitorLock );

    /**
     * Check monitor owner for thread
     */
    if( rawmonitor->m_owner_thread != get_thread_ptr()
        && !(rawmonitor->m_owned_count == 0 && rawmonitor->m_enter_count == 0) )
    {
        return JVMTI_ERROR_NOT_MONITOR_OWNER;
    }

    /**
     * Check monitor has only 1 enter, the owner enter
     * and monitor owner has no recursion
     */
    if( (rawmonitor->m_owned_count == 1 && rawmonitor->m_enter_count == 1) ||
        (rawmonitor->m_owned_count == 0 && rawmonitor->m_enter_count == 0)) {
        // delete monitor from raw monitor raw table
        delete rawmonitor;
        // null pointer to deleted monitor in the table
        g_rawMonitors[index].m_monitor = NULL;
        // set free element in table
        g_rawMonitors[index].m_nextFree = g_freeMonitor;
        // modify next free monitor variable
        g_freeMonitor = index;
        return JVMTI_ERROR_NONE;
    } else { // monitor cannot be destroyed
        return JVMTI_ERROR_NOT_MONITOR_OWNER;
    }
} // jvmtiDestroyRawMonitor

/*
 * Raw Monitor Enter
 *
 * Gain exclusive ownership of a raw monitor.
 * The same thread may enter a monitor more then once.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiRawMonitorEnter(jvmtiEnv* env,
                     jrawMonitorID monitor)
{
    unsigned index;
    jvmtiRawMonitor *rawmonitor;

    /**
     * Monitor trace
     */
    TRACE2("jvmti.monitor", "RawMonitorEnter called, id = " << monitor);
    SuspendEnabledChecker sec;
    
    /**
     * Check given env & current phase.
     */
    jvmtiPhase* phases = NULL;

    CHECK_EVERYTHING();

    /**
     * Check valid monitor
     */
    if( (index = (unsigned)((POINTER_SIZE_INT)monitor)) >= JVMTI_RAW_MONITOR_TABLE_SIZE
       || ( (rawmonitor = g_rawMonitors[index].m_monitor) == NULL ) )
    {
        return JVMTI_ERROR_INVALID_MONITOR;
    }
    // set monitor pointer
    rawmonitor = g_rawMonitors[index].m_monitor;

    /**
     * If current thread isn't owner of monitor try to lock monitor
     */
    if( rawmonitor->m_owner_thread != get_thread_ptr() ) {
        /**
         * Increase monitor enter count
         * Only 1 thread can execute this block
         */
        {
            // lock global raw monitor enter lock
            jvmtiRawMonitorLock._lock();
            // during lock monitor monitor can be destroyed
            if( (rawmonitor = g_rawMonitors[index].m_monitor) == NULL ) {
                // monitor was destroyed
                return JVMTI_ERROR_INVALID_MONITOR;
            }
            // increase monitor enter count
            rawmonitor->m_enter_count++;
            // unlock global raw monitor enter lock
            jvmtiRawMonitorLock._unlock();
        }

        /**
         * Try to lock monitor
         * This is stop point for all thread, only 1 thread can pass it
         */
        // set suspend thread flag
        assert(tmn_is_suspend_enabled());
        p_TLS_vmthread->is_suspended = true;
        tmn_safe_point();
        // all thread stoped at this point trying to lock monitor
        rawmonitor->m_owner_lock._lock();
        // only owner of monitor can execute in the following instructions

        // set monitor owner
        rawmonitor->m_owner_thread = get_thread_ptr();
        // lock wait lock object
        rawmonitor->m_wait_lock._lock();
        // suspend thread if needed
        //tmn_suspend_disable();
        //tmn_suspend_enable();
        // remove suspend thread flag
        p_TLS_vmthread->is_suspended = false;
    }
    // increase monitor recursion
    rawmonitor->m_owned_count++;

    return JVMTI_ERROR_NONE;
} // jvmtiRawMonitorEnter

/*
 * Raw Monitor Exit
 *
 * Release exclusive ownership of a raw monitor.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiRawMonitorExit(jvmtiEnv* env,
                    jrawMonitorID monitor)
{
    jvmtiRawMonitor *rawmonitor;
    jvmtiError result;

    /**
     * Monitor trace
     */
    TRACE2("jvmti.monitor", "RawMonitorExit called, id = " << monitor);
    SuspendEnabledChecker sec;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase* phases = NULL;

    CHECK_EVERYTHING();

    /**
     * Check valid monitor
     */
    if( ( result = jvmtiCheckValidMonitor( monitor ) ) != JVMTI_ERROR_NONE ) {
        return result;
    }
    // set monitor pointer
    rawmonitor = g_rawMonitors[(unsigned)((POINTER_SIZE_INT)monitor)].m_monitor;

    /**
     * Only ower of monitor can execute this block
     */
    {
        // decrease monitor recursion
        rawmonitor->m_owned_count--;

        /**
        * If there is no monitor recurtion, release monitor
        */
        if( !rawmonitor->m_owned_count ) {
            // decrease monitor enter count
            rawmonitor->m_enter_count--;
            // remove monitor owner
            rawmonitor->m_owner_thread = NULL;
            // unlock wait lock object
            rawmonitor->m_wait_lock._unlock();
            // unlock owner lock object
            rawmonitor->m_owner_lock._unlock();
        }
    }

    return JVMTI_ERROR_NONE;
} // jvmtiRawMonitorExit

/*
 * Raw Monitor Wait
 *
 * Wait for notification of the raw monitor.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiRawMonitorWait(jvmtiEnv* env,
                    jrawMonitorID monitor,
                    jlong millis)
{
    int old_owned_count;
    int64 timer;
    DWORD stat;
    VM_thread *old_owner_thread;
    jvmtiRawMonitor *rawmonitor;
    jvmtiError result;

    /**
     * Monitor trace
     */
    TRACE2("jvmti.monitor", "RawMonitorWait called, id = " << monitor << " timeout = " << millis);
    SuspendEnabledChecker sec;

    /**
     * Check given env & current phase.
     */
    jvmtiPhase* phases = NULL;

    CHECK_EVERYTHING();

    /**
     * Check valid monitor
     */
    if( ( result = jvmtiCheckValidMonitor( monitor ) ) != JVMTI_ERROR_NONE ) {
        return result;
    }
    // set monitor pointer
    rawmonitor = g_rawMonitors[(unsigned)((POINTER_SIZE_INT)monitor)].m_monitor;

    /**
     * Set old monitor values to reset monitor before wait state
     * Only owner of monitor can execute this block
     */
    {
        old_owned_count = rawmonitor->m_owned_count;
        old_owner_thread = rawmonitor->m_owner_thread;
        // reset monitor recursion
        rawmonitor->m_owned_count = 0;
        // reset monitor ower
        rawmonitor->m_owner_thread = 0;
        // increase count of waiters
        rawmonitor->m_wait_count++;
        // disallow leave wait state without notify
        rawmonitor->m_leave = false;
        // disallow leave wait state without notifyAll
        rawmonitor->m_notifyall = false;
        // reset wait event
        vm_reset_event( rawmonitor->m_wait_event );
        // unlock wait lock object
        rawmonitor->m_wait_lock._unlock();
        // unlock monitor lock object
        rawmonitor->m_owner_lock._unlock();
        // set thread suspend flag
        assert(tmn_is_suspend_enabled());
        p_TLS_vmthread->is_suspended = true;
    }

    /**
     * Wait state for all threads loop in this block
     */
    {
        while( true ) {
            // get timer
            timer = apr_time_now();
            // wait for event
#if defined (__INTEL_COMPILER) 
#pragma warning( push )
#pragma warning (disable:1683) // to get rid of remark #1683: explicit conversion of a 64-bit integral type to a smaller integral type
#endif
            stat = vm_wait_for_single_object( rawmonitor->m_wait_event,
                                               millis == 0 ? INFINITE : (DWORD)millis );

#if defined (__INTEL_COMPILER)
#pragma warning( pop )
#endif
            // get waiting time
            timer = apr_time_now() - timer;
            // check for timeout
            if( millis != 0 && stat != WAIT_TIMEOUT ) {
                millis -= timer/1000;
                if( millis <= 0 ) {
                    // set timeout state
                    stat = WAIT_TIMEOUT;
                    millis = 1;
                }
            }
            if( stat == WAIT_FAILED ) {
                // wait state was interapted
                result = JVMTI_ERROR_INTERRUPT;
            }
            // check wait object result
            if( millis != 0 && stat == WAIT_TIMEOUT ) {
                // lock wait lock object
                rawmonitor->m_wait_lock._lock();
                break;
            } else {
                // check monitor notify set
                if( rawmonitor->m_leave ) {
                    // monitor has notify, try to lock wait lock object
                    if( rawmonitor->m_wait_lock._tryLock() ) {
                        // wait lock object was locked
                        // check monitor notifyAll set
                        if( !rawmonitor->m_notifyall ) {
                            // notifyAll don't set, remove monitor notify set
                            rawmonitor->m_leave = false;
                        }
                        break;
                    }
                }
            }
        }
    }

    /**
     * Only owner of wait lock object can execute this block
     */
    {
        // decrease count of monitor waiters
        rawmonitor->m_wait_count--;
        // if wait was interupted
        if( stat != WAIT_TIMEOUT ) {
            if( rawmonitor->m_notifyall ) {
                // wait was interupted by notifyAll call
                if( rawmonitor->m_wait_count == 0 ) {
                    // last waiter sets notify event
                    vm_set_event( rawmonitor->m_notify_event );
                }
            } else {
                // wait was interupted by notify
                // reset wait event if RawMonitorNotify is called
                vm_reset_event( rawmonitor->m_wait_event );
                // waiter sets notify event
                vm_set_event( rawmonitor->m_notify_event );
            }
        } else {
            // if someone waits notify event
            if( rawmonitor->m_wait_count == 0 && rawmonitor->m_leave ) {
                // last waiter sets notify event
                vm_set_event( rawmonitor->m_notify_event );
            }
        }
        // unlock wait object
        rawmonitor->m_wait_lock._unlock();
    }

    /**
     * This is stop point for all thread, only 1 thread can pass it
     * All thread stoped at this point trying to lock monitor
     * Only owner of monitor can execute this block
     */
    {
        // suspend thread if needed
        p_TLS_vmthread->is_suspended = true;
        tmn_safe_point();
        // lock owner lock object
        rawmonitor->m_owner_lock._lock();
        // lock wait lock object
        rawmonitor->m_wait_lock._lock();
        // restore monitor threa owner
        rawmonitor->m_owner_thread = old_owner_thread;
        // restore value of monitor recurtion
        rawmonitor->m_owned_count = old_owned_count;
        // remove suspend thread flag
        p_TLS_vmthread->is_suspended = false;
    }

    return result;
} // jvmtiRawMonitorWait

/*
 * Internal Raw Monitor Notify
 */
static inline jvmtiError
InternalRawMonitorNotify(jvmtiEnv* env,
                         jrawMonitorID monitor,
                         bool notifyAll)
{
    jvmtiRawMonitor *rawmonitor;
    jvmtiError result;

    /*
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    /**
     * Check valid monitor
     */
    if( ( result = jvmtiCheckValidMonitor( monitor ) ) != JVMTI_ERROR_NONE ) {
        return result;
    }
    // set monitor pointer
    rawmonitor = g_rawMonitors[(unsigned)((POINTER_SIZE_INT)monitor)].m_monitor;

    /**
     * Only owner of monitor can execute following block
     */
    {
        // set notifyAll for waiters to allow waters to leave wait state
        rawmonitor->m_notifyall = notifyAll;
        // set notify for waiters to allow a waiter to leave wait state
        rawmonitor->m_leave = true;
        // reset notify event
        vm_reset_event( rawmonitor->m_notify_event );
        // signal event
        vm_set_event( rawmonitor->m_wait_event );
        // unlock wait lock object
        rawmonitor->m_wait_lock._unlock();

        /**
         * Wait until a waiter leaves wait state
         * It decrease number of waiter by 1
         * after which will stop on monitor lock
         */
        if( rawmonitor->m_wait_count ) {
            // set suspend thread flag
            assert(tmn_is_suspend_enabled());
            p_TLS_vmthread -> is_suspended = true;
            // wait notify event
            vm_wait_for_single_object( rawmonitor->m_notify_event, INFINITE );
            // suspend thread if needed
            tmn_suspend_disable();
            tmn_suspend_enable();
            // remove suspend thread flag
            p_TLS_vmthread->is_suspended = false;
        }

        // lock wait lock object back
        rawmonitor->m_wait_lock._lock();
        // reset notify event
        vm_reset_event( rawmonitor->m_notify_event );
    }

    return JVMTI_ERROR_NONE;
} // InternalRawMonitorNotify


/*
 * Raw Monitor Notify
 *
 * Notify a single thread waiting on the raw monitor.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiRawMonitorNotify(jvmtiEnv* env,
                      jrawMonitorID monitor)
{
    /**
     * Monitor trace
     */
    TRACE2("jvmti.monitor", "RawMonitorNotify called, id = " << monitor);
    SuspendEnabledChecker sec;

    /**
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    return InternalRawMonitorNotify( env, monitor, false );
} // jvmtiRawMonitorNotify

/*
 * Raw Monitor Notify All
 *
 * Notify all threads waiting on the raw monitor.
 *
 * REQUIRED Functionality.
 */
jvmtiError JNICALL
jvmtiRawMonitorNotifyAll(jvmtiEnv* env,
                         jrawMonitorID monitor)
{
    /**
     * Monitor trace
     */
    TRACE2("jvmti.monitor", "RawMonitorNotifyAll called, id = " << monitor);
    SuspendEnabledChecker sec;

    /**
     * Check given env & current phase.
     */
    jvmtiPhase phases[] = {JVMTI_PHASE_ONLOAD, JVMTI_PHASE_LIVE};

    CHECK_EVERYTHING();

    return InternalRawMonitorNotify( env, monitor, true );
} // jvmtiRawMonitorNotifyAll
