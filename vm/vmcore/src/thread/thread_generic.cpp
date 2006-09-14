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
 * @author Andrey Chernyshev
 * @version $Revision: 1.1.2.5.4.5 $
 */  


#define LOG_DOMAIN "thread"
#include "cxxlog.h"

#include "platform.h"
#include "vm_process.h"
#include <assert.h>

//MVM
#include <iostream>

using namespace std;

#include <signal.h>
#include <stdlib.h>

#if defined (PLATFORM_NT)
#include <direct.h>
#elif defined (PLATFORM_POSIX)
#include <sys/time.h>
#include <unistd.h>
#endif

#include "open/thread_externals.h"
#include "environment.h"
#include "vm_strings.h"
#include "open/types.h"
#include "open/vm_util.h"
#include "object_layout.h"
#include "Class.h"
#include "classloader.h"
#include "open/gc.h"
#include "vm_threads.h"
#include "nogc.h"
#include "ini.h"
#include "m2n.h"
#include "exceptions.h"
#include "jit_intf.h"
#include "vm_synch.h"
#include "exception_filter.h"
#include "vm_threads.h"
#include "jni_utils.h"
#include "object.h"

#include "platform_core_natives.h"
#include "heap.h"
#include "verify_stack_enumeration.h"

#include "sync_bits.h"
#include "vm_stats.h"
#include "native_utils.h"

#ifdef PLATFORM_NT
// wjw -- following lines needs to be generic for all OSs
#include "java_lang_thread_nt.h"
#endif

#ifdef _IPF_
#include "java_lang_thread_ipf.h"
#elif defined _EM64T_
#include "java_lang_thread_em64t.h"
#else
#include "java_lang_thread_ia32.h"
#endif

#include "thread_manager.h"
#include "object_generic.h"
#include "thread_generic.h"

#include "mon_enter_exit.h"

#include "jni_direct.h"

#ifdef _IPF_
#include "../m2n_ipf_internal.h"
#elif defined _EM64T_
#include "../m2n_em64t_internal.h"
#else
#include "../m2n_ia32_internal.h"
#endif

#include "port_malloc.h"

#include "open/jthread.h"

/////////////////////////////////////////////////////////////////////
// Native lib stuff


IDATA vm_attach() {
	//hythread_suspend_disable();
	IDATA status;
	status = hythread_global_lock();
    if(status != TM_ERROR_NONE)
        return status;
    VM_thread * p_vm_thread = get_a_thread_block();
    if (NULL == p_vm_thread) {
        TRACE2("thread", "can't get a thread block for a new thread");
		status =hythread_global_unlock();
		assert (status == TM_ERROR_NONE);
        return TM_ERROR_OUT_OF_MEMORY;
    }

    set_TLS_data(p_vm_thread);
    M2nFrame* p_m2n = (M2nFrame*) STD_MALLOC(sizeof(M2nFrame));
    ObjectHandles* p_handles = (ObjectHandles*) STD_MALLOC (sizeof(ObjectHandlesNew));
    if ((p_m2n==NULL)||(p_handles==NULL))
	{
        TRACE2("thread", "can't get a thread block for a new thread");
		status =hythread_global_unlock();
		assert (status == TM_ERROR_NONE);
        return TM_ERROR_OUT_OF_MEMORY;
    }
    status =hythread_global_unlock();
    if(status != TM_ERROR_NONE)
        return status;

	hythread_suspend_disable();

    init_stack_info();

    m2n_null_init(p_m2n);
    m2n_set_last_frame(p_m2n);

    oh_null_init_handles(p_handles);

    m2n_set_local_handles(p_m2n, p_handles);
    m2n_set_frame_type(p_m2n, FRAME_NON_UNWINDABLE);
    gc_thread_init(&p_vm_thread->_gc_private_information);

    assert(!hythread_is_suspend_enabled());
    hythread_suspend_enable();
	return TM_ERROR_NONE;
    }

IDATA vm_detach() {
	IDATA status;
    VM_thread *p_vm_thread=get_thread_ptr();

    hythread_suspend_disable();
//  assert(p_vm_thread->app_status == thread_is_running);
    gc_thread_kill(&p_vm_thread->_gc_private_information);
    hythread_suspend_enable();
 
	status = hythread_global_lock();
	if(status != TM_ERROR_NONE)
        return status;
    assert(p_vm_thread->gc_frames == 0);  

    free_this_thread_block( p_vm_thread );
    set_TLS_data(NULL);
	status =hythread_global_unlock();
	return status;
    }
    

////////////////////////////////////////////////////////////////////////////////////////////
//////// CALLED by vm_init() to initialialize thread groups and create the main thread ////
