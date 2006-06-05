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
 * @author Mikhail Y. Fursov
 * @version $Revision: 1.1.2.2.4.4 $
 */
#ifndef _EM_PROFILE_ACCESS_H_
#define _EM_PROFILE_ACCESS_H_

#include "open/types.h"
#include "open/em.h"

#ifdef __cplusplus
extern "C" {
#endif

enum EM_PCTYPE {
    EM_PCTYPE_ENTRY_BACKEDGE=1
};

typedef struct EM_ProfileAccessInterface {

    EM_PCTYPE               (*get_pc_type) (
                                EM_Handle _this,
                                PC_Handle pc
                            );

    Method_Profile_Handle   (*get_method_profile)(
                                EM_Handle _this,
                                PC_Handle pc,
                                Method_Handle mh
                            );

    PC_Handle               (*get_pc)(
                                EM_Handle _this,
                                EM_PCTYPE profile_type,
                                JIT_Handle jh,
                                EM_JIT_PC_Role jit_role
                            );


    //to be moved to separate ENTRY-BACKAGE profile interface-struct in OPEN
    //EB (entry-backedge) profile collector interface

    // method to create new EB profile. This method fill if called twice
    // for the same pair of (profile collector, method handle)
    Method_Profile_Handle (*eb_profiler_create_profile) (PC_Handle ph, Method_Handle mh);

    // the address of entry counter
    // JIT must generate a code to increment this counter on every method call
    void* (*eb_profiler_get_entry_counter_addr)(Method_Profile_Handle mph);

    // the address of backedge counter
    // JIT must generate a code to increment this counter on every backedge
    void* (*eb_profiler_get_backedge_counter_addr)(Method_Profile_Handle mph);

    // EB profile collector supports both synchronous (in-user-threads)
    // and asynchronous (em-helper-thread)method profile checks.
    // JIT must use this method to check the profile collector mode
    char (*eb_profiler_is_in_sync_mode)(PC_Handle pch);

    void (*eb_profiler_sync_mode_callback)(Method_Profile_Handle mph);

    uint32 (*eb_profiler_get_entry_threshold)(PC_Handle pch);

    uint32 (*eb_profiler_get_backedge_threshold)(PC_Handle pch);

} EM_ProfileAccessInterface;


#ifdef __cplusplus
}
#endif


#endif

