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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.4 $
 */  


#include "platform_lowlevel.h"

//MVM
#include <iostream>

using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "vm_synch.h"
#include "open/vm_util.h"
#include "encoder.h"
#include "vm_stats.h"
#include "nogc.h"
#include "compile.h"

#include "exceptions.h"
#include "lil.h"
#include "lil_code_generator.h"
#include "../m2n_em64t_internal.h"
#include "object_handles.h"


extern bool dump_stubs;

char *gen_setup_j2n_frame(char *s);
char *gen_pop_j2n_frame(char *s);



// patch_addr_null_arg_ptr is the address of a variable holding the
// address of a branch instruction byte to patch with the destination
// to be taken if the struct Class* argument is NULL.
/*
static char * gen_convert_struct_class_to_object(char *ss, char **patch_addr_null_arg_ptr)
{    
    return ss;
} //gen_convert_struct_class_to_object


static char * gen_restore_monitor_enter(char *ss, char *patch_addr_null_arg)
{
    return ss;
} //gen_restore_monitor_enter
*/

void * restore__vm_monitor_enter_naked(void * code_addr)
{
    return code_addr;
} //restore__vm_monitor_enter_naked


void * restore__vm_monitor_enter_static_naked(void * code_addr)
{
    return code_addr;
} //restore__vm_monitor_enter_static_naked

/*
static char * gen_restore_monitor_exit(char *ss, char *patch_addr_null_arg)
{
    return ss;
} //gen_restore_monitor_exit
*/

void * restore__vm_monitor_exit_naked(void * code_addr)
{
    return code_addr; 
} //restore__vm_monitor_exit_naked


void * restore__vm_monitor_exit_static_naked(void * code_addr)
{
    return code_addr; 
} //restore__vm_monitor_exit_static_naked


void * getaddress__vm_monitor_enter_naked()
{
    static void *addr = NULL;
    return addr;
}


void * getaddress__vm_monitor_enter_static_naked()
{    
    static void *addr = NULL;
    return addr;
} //getaddress__vm_monitor_enter_static_naked


void * getaddress__vm_monitor_exit_naked()
{
    static void *addr = NULL;
    return addr;
} //getaddress__vm_monitor_exit_naked


void * getaddress__vm_monitor_exit_static_naked()
{
    static void *addr = NULL;
    return addr;
} //getaddress__vm_monitor_exit_static_naked

Boolean jit_may_inline_object_synchronization(unsigned *thread_id_register,
                                              unsigned *sync_header_offset,
                                              unsigned *sync_header_width,
                                              unsigned *lock_owner_offset,
                                              unsigned *lock_owner_width,
                                              Boolean  *jit_clears_ccv)
{
    return FALSE;
}
