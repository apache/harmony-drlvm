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
/** 
 * @author Mikhail Y. Fursov
 * @version $Revision: 1.1.2.1.4.4 $
 */  
#ifndef _DLL_EM_INTF_H_
#define _DLL_EM_INTF_H_

#include "em_intf_cpp.h"
#include <apr_dso.h>

bool vm_is_a_em_dll(const char *dll_filename);

class Dll_EM: public EM {

public:

    Dll_EM(const char *dll_filename);

    ~Dll_EM() { 
        _deinit();
        if (pool != NULL) apr_pool_destroy(pool);
    }

    virtual bool init() { 
        return (bool)_init(); 
    }

    virtual void execute_method(jmethodID methodID, jvalue *return_value, jvalue *args) { 
        return _execute_method(methodID, return_value, args);
    }

    virtual JIT_Result compile_method(Method_Handle method) { 
        return _compile_method(method); 
    }

    virtual bool need_profiler_thread_support() const {
        return _need_profiler_thread_support();
    }    

    virtual void profiler_thread_timeout() {
        _profiler_thread_timeout();
    }

    virtual int get_profiler_thread_timeout() const {
        return _get_profiler_thread_timeout();      
    }

private:
    char (*_init)();

    void (*_deinit)();

    void (*_execute_method)(jmethodID methodID, jvalue *return_value, jvalue *args);

    JIT_Result (*_compile_method)(Method_Handle mh);

    bool (*_need_profiler_thread_support)();

    void (*_profiler_thread_timeout)();

    int (*_get_profiler_thread_timeout)();

    apr_pool_t *pool;
};




#endif
