/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef _VM_UTILS_H_
#define _VM_UTILS_H_

#include <apr_general.h>
#include "port_malloc.h"
#include "open/types.h"

#include "hythread_ext.h"

inline IDATA wait_for_multiple_semaphores(int num, hysem_t *sems) {
    for (int i = 0; i < num; i ++) {
           IDATA stat = hysem_wait(sems[i]);
           if (stat !=TM_ERROR_NONE) {
                return stat;
           }
    }
    return TM_ERROR_NONE;
}

typedef struct String String;

// #include "String_Pool.h"
// #include "Class.h"
// #include "object_layout.h"
#include "vm_threads.h"

unsigned sizeof_java_lang_class();


struct Global_Env;

class VM_Global_State {
public:
    VMEXPORT static Global_Env *loader_env;
}; //VM_Global_State


extern VTable *cached_object_array_vtable_ptr;

/**
 * Runtime support functions exported directly, because they 
 * may be called from native code.
 */

Boolean class_is_subtype(Class *sub, Class *super);

/**
 * Like <code>class_is_subtype</code>, but <code>sub</code> must not be an 
 * interface class.
 */
Boolean class_is_subtype_fast(VTable *sub, Class *super);


#ifdef _DEBUG
void __stdcall vm_dump_object_and_return_ip(void *obj, void *eip);
#endif

class ExpandableMemBlock
{
public:
    ExpandableMemBlock(long nBlockLen = 2000, long nInc = 1000)
            : m_nBlockLen(nBlockLen), m_nCurPos(0), m_nInc(nInc){
        assert(nInc > 0);
        m_pBlock = STD_MALLOC(m_nBlockLen);
        assert(m_pBlock);
    }
    ~ExpandableMemBlock(){
        if(m_pBlock)
            STD_FREE(m_pBlock);
    }
    void AppendBlock(char *szBlock, long nLen = -1){
        if(!szBlock)return;
        if(nLen <= 0)nLen = (long) strlen(szBlock);
        if(!nLen)return;
        long nOweSpace = (m_nCurPos + nLen) - m_nBlockLen;
        if(nOweSpace >= 0){ //change > 0 to >= 0, prevents assert in m_free(m_pBlock)
            m_nBlockLen += (nOweSpace / m_nInc + 1)*m_nInc;
            m_pBlock = STD_REALLOC(m_pBlock, m_nBlockLen);
            assert(m_pBlock);
        }
        //memmove((char*)m_pBlock + m_nCurPos, szBlock, nLen);
        memcpy((char*)m_pBlock + m_nCurPos, szBlock, nLen);
        m_nCurPos += nLen;
    }
    void AppendFormatBlock(char *szfmt, ... ){
        va_list arg;
        //char *buf = (char*)calloc(1024, 1);
        char buf[1024];
        va_start( arg, szfmt );
        vsprintf(buf, szfmt, arg );
        va_end( arg );
        AppendBlock(buf);
        //m_free(buf);
    }
    void SetIncrement(long nInc){
        assert(nInc > 0);
        m_nInc = nInc;
    }
    void SetCurrentPos(long nPos){
        assert((nPos >= 0) && (nPos < m_nBlockLen));
        m_nCurPos = nPos;
    }
    long GetCurrentPos(){
        return m_nCurPos;
    }
    const void *AccessBlock(){
        return m_pBlock;
    }
    const char *toString(){
        *((char*)m_pBlock + m_nCurPos) = '\0';
        return (const char*)m_pBlock;
    }
    void EnsureCapacity(long capacity){
        long nOweSpace = capacity - m_nBlockLen;
        if(nOweSpace >= 0){ //change > 0 to >= 0, prevents assert in m_free(m_pBlock)
            m_nBlockLen += (nOweSpace / m_nInc + 1)*m_nInc;
            m_pBlock = STD_REALLOC(m_pBlock, m_nBlockLen);
            assert(m_pBlock);
        }
    }
    void CopyTo(ExpandableMemBlock &mb, long len = -1){
        if(len == -1)
            len = m_nBlockLen;
        mb.SetCurrentPos(0);
        mb.AppendBlock((char*)m_pBlock, len);
    }
protected:
    void *m_pBlock;
    long m_nBlockLen;
    long m_nCurPos;
    long m_nInc;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generates an VM's helper to invoke the provided function.
 *
 * The helper takes the <code>void*</code> parameter, which is passed 
 * to the function after some preparation made (namely GC and stack 
 * info are prepared to allow GC to work properly).
 *
 * The function must follow stdcall convention, which takes <code>void*</code> and
 * returns <code>void*</code>, so does the helper.
 * On a return from the function, the helper checks whether an exception
 * was raised for the current thread, and rethrows it if necessary.
 */
VMEXPORT void * vm_create_helper_for_function(void* (*fptr)(void*));

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VM_UTILS_H_ */



