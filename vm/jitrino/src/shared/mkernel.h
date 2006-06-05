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
 * @author Alexander V. Astapchuk
 * @version $Revision: 1.2.8.2.4.4 $
 */

/**
 * @file
 * @brief 'micro-kernel' - contains kernel-like utilities - synchronization, 
          system info, etc.
 */

#if !defined(__MKERNEL_H_INCLUDED__)
#define __MKERNEL_H_INCLUDED__

#ifdef PLATFORM_POSIX
#include <pthread.h>
#include <semaphore.h>
#else
// <winsock2.h> must be included before <windows.h> to avoid compile-time 
// errors with winsock.h. For more details, see:
//www.microsoft.com/msdownload/platformsdk/sdkupdate/2600.2180.7/contents.htm
#include <winsock2.h>
#include <windows.h>

#endif

namespace Jitrino { 

/**
 * @brief Exclusive lock.
 *
 * Class Mutex represents an exclusive lock. Interface is obvious.
 * 
 * There are no 'try_lock' or 'is_locked' methods, this absence is 
 * intentional.
 *
 * @note On both Linux and Windows platforms, the mutex object can be 
 *       recursively taken by the same thread (and, surely, must be released
 *       exactly the same number of times).
 *
 * @note On Linux platform, Mutex must be unlocked before destruction (that
 *       is, the Mutex object goes out of scope) - this is requirement of 
 *       LinuxThreads implementation.
 *
 * @note Can be used only to synchronize inside an application. Cannot be 
 *       used (and was not intended to do so) for inter-process 
 *       synchronization.
 *
 * @note On Windows platform the underlying OS object is not Mutex, but 
 *       CriticalSection instead, as it's a bit lighter and faster.
 *
 * @see #AutoUnlock
 */
class Mutex {
#ifdef PLATFORM_POSIX 
    //
    // *nix implementation
    //
public:
    /**
     * @brief Frees resources associated with the mutex.
     */
    Mutex()
    {
        pthread_mutexattr_t attrs;
        pthread_mutexattr_init(&attrs);
        pthread_mutexattr_settype(&attrs, PTHREAD_MUTEX_RECURSIVE_NP);
        pthread_mutex_init(&m_handle, &attrs);
        pthread_mutexattr_destroy(&attrs);
    }
    /**
     * @brief Destructs the object.
     */
    ~Mutex()            { pthread_mutex_destroy(&m_handle); }
    
    /**
     * @brief Acquires a lock.
     */
    void lock(void)     { pthread_mutex_lock(&m_handle);    }
    
    /**
     * @brief Releases a lock.
     */
    void unlock(void)   { pthread_mutex_unlock(&m_handle);  }
private:
    pthread_mutex_t m_handle;
    
#else   // not PLATFORM_POSIX

    //
    // Win* implementation
    //
public:
    Mutex()             { InitializeCriticalSection(&m_cs); }
    ~Mutex()            { DeleteCriticalSection(&m_cs);     }
    void lock(void)     { EnterCriticalSection(&m_cs);      }
    void unlock(void)   { LeaveCriticalSection(&m_cs);      }
private:
    CRITICAL_SECTION    m_cs;

#endif  // ~ifdef PLATFORM_POSIX
    /**
     * @brief Disallows copying.
     */
    Mutex(const Mutex&);
    /**
     * @brief Disallows copying.
     */
    const Mutex& operator=(const Mutex&);
};

/**
 * @brief Automatically unlocks a #Mutex when goes out of scope.
 *
 * Class AutoUnlock is an utility class to handy acquire and [automagically]
 * release Mutex lock.
 *
 * A trivial C++ trick, which, I believe, is used everywhere with Mutexes - 
 * holds the lock in the ctor and releases lock in dtor - when AutoUnlock
 * object goes out of scope. 
 *
 * Safely accepts NULL-s - and performs nothing in this case.
 */
class AutoUnlock {
public:
    /**
     * @brief Locks the mutex.
     */
    AutoUnlock(Mutex& m) : m_mutex(&m)
    {
        m_mutex->lock();
    }
    
    /**
     * @brief Locks the mutex, or does nothing if \c pm is \b NULL.
     */
    AutoUnlock(Mutex * pm) : m_mutex(pm)
    {
        if(m_mutex) {
            m_mutex->lock();
        }
    }
    
    /**
     * @brief Forces mutex (if any) to be unlocked. 
     * 
     * When called, clears internal pointer to a mutex object, so the 
     * following sequential calls to forceUnlock and destructor are noops.
     */    
    void forceUnlock(void) 
    {
        if(m_mutex) {
            m_mutex->unlock();
            m_mutex = NULL;
        }
    }
    
    /**
     * @brief Unlocks mutex (if any).
     */
    ~AutoUnlock()
    {
        if(m_mutex) {
            m_mutex->unlock();
        }
    }
private:
    AutoUnlock(const AutoUnlock&);
    AutoUnlock& operator=(const AutoUnlock&);
    Mutex * m_mutex;
};


/**
 * @brief Class Runtime provides an info about environment the application
 *        is currently running.
 */
class Runtime {
public:
    /**
     * @brief Returns number of CPUs available in the system.
     *
     * In theory, may be 0, if underlying system does not provide the info.
     *
     * Does not even try to distinguish HyperThreading and 'phisical CPU', so
     * a CPU with HT enabled is seen as 2 CPUs.
     */
    static unsigned cpus(void)      { return num_cpus;      }
    
    /**
     * @brief Tests whether we're running in single-CPU machine.
     */
    static bool     is_one_cpu(void)     { return cpus() == 1;   }
    
    /**
     * @brief Tests whether we're running in multi-CPU box.
     */
    static bool     is_smp(void)    { return cpus() > 1;    }
private:
    /**
     * @brief Number of CPUs.
     *
     * Initialized once at startup and never changes.
     */
    static const unsigned num_cpus;
    
    /**
     * @brief Initializes \link #num_cpus number of CPUs\endlink.
     */
    static unsigned init_num_cpus(void);
    
    /** 
     * @brief Disallows creation, interface is only through static functions.
     */
    Runtime();
    
    /**
     * @brief Disallows copying.
     */
    Runtime(const Runtime&);
    
    /**
     * @brief Disallows copying.
     */
    Runtime& operator=(const Runtime&);
};


}; // ~namespace Jitrino

#endif  // ~ifndef __MKERNEL_H_INCLUDED__
