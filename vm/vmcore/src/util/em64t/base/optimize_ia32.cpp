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
 * @version $Revision: 1.1.2.1.4.5 $
 */  


//MVM
#include <iostream>

using namespace std;

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "open/types.h"
#include "environment.h"
#include "encoder.h"
#include "open/vm_util.h"
#include "compile.h"

// *** This is for readInternal override
#include "jni_utils.h"
#include "vm_arrays.h"
#include "open/vm_util.h"

extern int readinternal_override_lil(JNIEnv *jenv, Java_java_io_FileInputStream *pThis, int fd, Vector_Handle pArrayOfByte, int  offset, int len);

// *** This is for currenttimemillis override
#include "platform_core_natives.h"
/*
static unsigned java_array_of_char_body_offset()
{
    return vector_first_element_offset(VM_DATA_TYPE_CHAR);
}

static unsigned java_array_of_char_length_offset()
{
    return vector_length_offset();
}
*/
static bool enable_fast_char_arraycopy()
{
    Global_Env *env = VM_Global_State::loader_env;
    if (env->compress_references)
        return false;
    return true;
}

/*
static ManagedObject *
get_class_ptr(ManagedObject *obj)
{
    return *(obj->vt()->clss->class_handle);
}
*/
// ****** Begin overrides
// ****** identityHashCode
#include "object_generic.h"
unsigned native_hashcode_fastpath_size(Method *m)
{
    return 20;
}
void gen_native_hashcode(Emitter_Handle h, Method *m)
{
}

// ****** newInstance
// *** This is for the newInstance override
unsigned native_newinstance_fastpath_size(Method *m)
{
    return 100;
}

void gen_native_newinstance(Emitter_Handle h, Method *m)
{
}

// ****** readInternal
unsigned native_readinternal_fastpath_size(Method *m)
{
    return 100;
}
void gen_native_readinternal(Emitter_Handle h, Method *m)
{
}

// ****** get current time millis
unsigned native_getccurrenttime_fastpath_size(Method *m)
{
    return 20;
}
void gen_native_system_currenttimemillis(Emitter_Handle h, Method *m)
{
}

// above are additions to bring original on par with LIL

unsigned native_getclass_fastpath_size(Method *m)
{
    return 20;
}


void gen_native_getclass_fastpath(Emitter_Handle h, Method *m)
{
}



// Return an upper bound on the size of the custom arraycopy routine.
unsigned native_arraycopy_fastpath_size(Method *m)
{
    if (enable_fast_char_arraycopy())
        return 350;
    return 0;
}

//MOV_PAIR must be less than 32
#define MOV_PAIR  8
void gen_native_arraycopy_fastpath(Emitter_Handle h, Method *method)
{
} //jit_inline_native_array_copy_general



int find_inline_native_method(const char* clss_name, const char* method_name,JIT_Result  (*&func)(Method& method, JIT_Flags flags))
{
    return -1 ;
} //find_inline_native_method
