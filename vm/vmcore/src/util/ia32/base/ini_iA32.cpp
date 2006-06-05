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
 * @version $Revision: 1.1.2.1.4.3 $
 */  



//MVM
#include <iostream>

using namespace std;

#include <assert.h>

#include "open/types.h"
#include "open/vm_util.h"
#include "Class.h"
#include "exceptions.h"
#include "vm_threads.h"
#include "open/thread.h"
#include "compile.h"
#include "nogc.h"
#include "encoder.h"
#include "ini.h"
#include "environment.h"
#include "lil.h"
#include "lil_code_generator.h"

#include "interpreter.h"

#include "port_malloc.h"

#define LOG_DOMAIN "invoke"
#include "cxxlog.h"

#ifdef _WIN32
static int64 __declspec(naked) __stdcall
vm_invoke_native_array_stub(uint32 *args,
                            int sz,
                            void* f)
{
    __asm {
        push ebp
        mov ebp, esp
        push ebx // FIXME: check jit calling conventions,
        push esi // is it necessary to save the registers here
        push edi

        mov eax, [ebp+8]
        mov ecx, [ebp+12]
        lea eax, [eax+ecx*4-4]
        sub eax, esp
        or ecx, ecx
        je e
        l:
        push [esp+eax]
        loop l
        e:
        mov eax, [ebp+16]
        call eax
        lea esp, [ebp-12]
        pop edi
        pop esi
        pop ebx
        leave
        ret
    }
}
#else /* Linux */
extern "C" {
    int64 vm_invoke_native_array_stub(uint32 *args,
                                      int sz,
                                      void *func);
}

#endif

typedef double (*DoubleFuncPtr)(uint32*,int,void*);
typedef ManagedObject* (*RefFuncPtr)(uint32*,int,void*);
typedef float (*FloatFuncPtr)(uint32*,int,void*);
typedef int32 (*IntFuncPtr)(uint32*,int,void*);

DoubleFuncPtr vm_invoke_native_array_stub_double =
        (DoubleFuncPtr) vm_invoke_native_array_stub;
RefFuncPtr vm_invoke_native_array_stub_ref =
        (RefFuncPtr) vm_invoke_native_array_stub;
IntFuncPtr vm_invoke_native_array_stub_int =
        (IntFuncPtr) vm_invoke_native_array_stub;
FloatFuncPtr vm_invoke_native_array_stub_float =
        (FloatFuncPtr) vm_invoke_native_array_stub;


void
JIT_execute_method_default(JIT_Handle jit, jmethodID methodID, jvalue *return_value, jvalue *args) {

    // Detecting errors with object headears on stack when using destructive
    // unwinding.
    void *lastFrame = p_TLS_vmthread->lastFrame;
    p_TLS_vmthread->lastFrame = (void*)&lastFrame;
    //printf("execute: push: prev = 0x%p, curr=0x%p\n", lastFrame, &lastFrame);

//    fprintf(stderr, "Not implemented\n");

    Method *method = (Method*) methodID;
    TRACE("enter method "
            << method->get_class()->name->bytes << " "
            << method->get_name()->bytes << " "
            << method->get_descriptor()->bytes);
    int sz = method->get_num_arg_bytes() >> 2;
    void *meth_addr = method->get_code_addr();
    uint32 *arg_words = (uint32*) STD_ALLOCA(sz * sizeof(uint32));

    int argId = sz;
    int pos = 0;

    assert(!tmn_is_suspend_enabled());
    if (!method->is_static()) {
        ObjectHandle handle = (ObjectHandle) args[pos++].l;
        assert(handle);
        arg_words[--argId] = (unsigned) handle->object;
    }

    const char *mtype = method->get_descriptor()->bytes + 1;
    assert(mtype != 0);

    for(; *mtype != ')'; mtype++) {
        switch(*mtype) {
            case JAVA_TYPE_CLASS:
            case JAVA_TYPE_ARRAY:
                {
                    ObjectHandle handle = (ObjectHandle) args[pos++].l;
                    arg_words[--argId] = (unsigned) (handle ? handle->object : 0);

                    while(*mtype == '[') mtype++;
                    if (*mtype == 'L')
                        while(*mtype != ';') mtype++;
                }
                break;

            case JAVA_TYPE_SHORT:
                // sign extend
                arg_words[--argId] = (uint32)(int32) args[pos++].s;
                break;
            case JAVA_TYPE_BYTE:
                // sign extend
                arg_words[--argId] = (uint32)(int32) args[pos++].b;
                break;
            case JAVA_TYPE_INT:
                // sign extend
                arg_words[--argId] = (uint32)(int32) args[pos++].i;
                break;

            case JAVA_TYPE_FLOAT:
                arg_words[--argId] = (int32) args[pos++].i;
                break;
            case JAVA_TYPE_BOOLEAN:
                arg_words[--argId] = (int32) args[pos++].z;
                break;
            case JAVA_TYPE_CHAR:
                // zero extend
                arg_words[--argId] = (int32) args[pos++].c;
                break;

            case JAVA_TYPE_LONG:
            case JAVA_TYPE_DOUBLE:
                *(jlong*)&arg_words[argId-2] = args[pos++].j;
                argId -= 2;
                break;
            default:
                ABORT("Unexpected java type");
        }
    }
    assert(argId >= 0);

    jvalue *resultPtr = (jvalue*) return_value;
    Java_Type ret_type = method->get_return_java_type();

    arg_words += argId;
    argId = sz - argId;

    switch(ret_type) {
        case JAVA_TYPE_VOID:
            vm_invoke_native_array_stub(arg_words, argId, meth_addr);
            break;

        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
        case JAVA_TYPE_STRING:
            {
                ManagedObject *ref = vm_invoke_native_array_stub_ref(arg_words, argId, meth_addr);
                ObjectHandle h = oh_allocate_local_handle();

                if (ref != NULL) {
                    h->object = ref;
                    resultPtr->l = h;
                } else {
                    resultPtr->l = NULL;
                }
            }
            break;

        case JAVA_TYPE_BOOLEAN:
        case JAVA_TYPE_BYTE:
        case JAVA_TYPE_CHAR:
        case JAVA_TYPE_SHORT:
        case JAVA_TYPE_INT:
            resultPtr->i = vm_invoke_native_array_stub_int(arg_words, argId, meth_addr);
            break;

        case JAVA_TYPE_FLOAT:
            resultPtr->f = vm_invoke_native_array_stub_float(arg_words, argId, meth_addr);
            break;

        case JAVA_TYPE_LONG:
            resultPtr->j = vm_invoke_native_array_stub(arg_words, argId, meth_addr);
            break;

        case JAVA_TYPE_DOUBLE:
            resultPtr->d = vm_invoke_native_array_stub_double(arg_words, argId, meth_addr);
            break;

        default:
            ABORT("Unexpected java type");
    }

    if (exn_raised()) {
        if ((resultPtr != NULL) && (ret_type != JAVA_TYPE_VOID)) {   
            resultPtr->l = 0; //clear result
        }
    }
 
    TRACE("exit method "
            << method->get_class()->name->bytes << " "
            << method->get_name()->bytes << " "
            << method->get_descriptor()->bytes);

    // Detecting errors with object headears on stack when using destructive
    // unwinding.
    //printf("execute:  pop: prev = 0x%p, curr=0x%p\n", &lastFrame, lastFrame);
    p_TLS_vmthread->lastFrame = lastFrame;
}




