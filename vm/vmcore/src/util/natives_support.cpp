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
 * @author Pavel Rebriy, Ilya Berezhniuk
 * @version $Revision: 1.1.2.1.4.3 $
 */  


#include <stdio.h>
#include <string.h>
#include <vector>
#include <apr_strings.h>

#include "assert.h"
#include "port_malloc.h"
#include "port_dso.h"
#include "exceptions.h"
#include "natives_support.h"
#include "environment.h"
#include "lock_manager.h"
#include "jni_direct.h" // FIXME ???? Can we use it here ????
#include "open/vm_util.h"

#include "jni_types.h"

#define LOG_DOMAIN "natives"
#include "cxxlog.h"

typedef jint (JNICALL *f_JNI_OnLoad) (JavaVM *vm, void *reserved);
typedef void (JNICALL *f_JNI_OnUnload) (JavaVM *vm, void *reserved);

struct JNILibInfo
{
    JNILibInfo(): ppool(NULL) {}

public:
    NativeLibInfo*      lib_info_list;
    apr_pool_t*         ppool;
    Lock_Manager        lock;
};

static JNILibInfo jni_libs;


// Initializes natives_support module. Caller must provide thread safety.
bool natives_init()
{
    apr_status_t apr_status = apr_pool_create(&jni_libs.ppool, NULL);

    if (APR_SUCCESS != apr_status)
    {
        assert(jni_libs.ppool == NULL);
        return false;
    }

    return true;
}

// Searches for given name trough the list of loaded libraries
// Lock must be acquired by the caller
static NativeLibInfo* search_library_list(const char* library_name)
{
    Global_Env *ge = VM_Global_State::loader_env;
    String* name = ge->string_pool.lookup(library_name);

    for(NativeLibraryList lib = jni_libs.lib_info_list;
        lib; lib = lib->next)
    {
        if (name == lib->name)
            return lib;
    }

    return NULL;
}

// Searches for JNI_OnLoad through given library and calls it
static jint find_call_JNI_OnLoad(NativeLibraryHandle lib_handle)
{
static char* name_list[] =
    {   "JNI_OnLoad"
#ifdef PLATFORM_NT
    ,   "_JNI_OnLoad@8"
#endif
    };

    assert(lib_handle);

    for (unsigned i = 0; i < sizeof(name_list)/sizeof(name_list[0]); i++)
    {
        f_JNI_OnLoad onload_func; // = (f_JNI_OnLoad)handle;

        //apr_dso_handle_sym_t handle = NULL;
        apr_status_t apr_status =
            apr_dso_sym((apr_dso_handle_sym_t*)&onload_func, lib_handle, name_list[i]);

        if (APR_SUCCESS != apr_status)
            continue;

        assert(onload_func);

        JavaVM* vm = jni_native_intf->vm; // FIXME ???? Can we use it here ????

        assert(hythread_is_suspend_enabled());
        jint res = onload_func(vm, NULL);

        return res;
    }

    return 0;
}

// Searches for JNI_OnUnload through given library and calls it
// Lock must be acquired by the caller
// FIXME unused static
void find_call_JNI_OnUnload(NativeLibraryHandle lib_handle)
{
static char* name_list[] =
    {   "JNI_OnUnload"
#ifdef PLATFORM_NT
    ,   "_JNI_OnUnload@8"
#endif
    };

    assert(lib_handle);

    for (unsigned i = 0; i < sizeof(name_list)/sizeof(name_list[0]); i++)
    {
        f_JNI_OnUnload onunload_func;// = (f_JNI_OnUnload)handle;

        apr_status_t apr_status =
            apr_dso_sym((apr_dso_handle_sym_t*)&onunload_func, lib_handle, name_list[i]);

        if (APR_SUCCESS != apr_status)
            continue;

        assert(onunload_func);

        JavaVM* vm = jni_native_intf->vm; // FIXME ???? Can we use it here ????

        assert(hythread_is_suspend_enabled());
        onunload_func(vm, NULL);
        return;
    }
}


static inline bool is_name_lowercase(const char* name)
{
    for(; *name; name++)
    {
        if (isalpha(*name) && !islower(*name))
            return false;
    }

    return true;
}


// Function loads native library with a given name.
NativeLibraryHandle
natives_load_library(const char* library_name, bool* just_loaded,
                     NativeLoadStatus* pstatus)
{
    assert(NULL != library_name);
    assert(NULL != pstatus);
#ifdef PLATFORM_NT
    assert(is_name_lowercase(library_name));
#endif

    jni_libs.lock._lock();

    NativeLibInfo* pfound = search_library_list(library_name);

    if (pfound)
    {
        jni_libs.lock._unlock();

        *just_loaded = false;
        *pstatus = APR_SUCCESS;

        return pfound->handle;
    }

    *just_loaded = true;

    // library was not loaded previously, try to load it
    NativeLibraryHandle handle;
    apr_status_t apr_status = port_dso_load_ex(&handle, library_name,
                                               PORT_DSO_BIND_DEFER, jni_libs.ppool);
    if (APR_SUCCESS != apr_status)
    {
        char buf[1024];
        apr_dso_error( handle, buf, 1024 );
        TRACE( "Natives: could not load library " << library_name << "; " << buf );

        jni_libs.lock._unlock();

        *pstatus = apr_status;
        return NULL;
    }

    NativeLibInfo* pinfo = (NativeLibInfo*)apr_palloc(jni_libs.ppool, sizeof(NativeLibInfo));
    if (NULL == pinfo)
    {
        apr_dso_unload(handle);
        jni_libs.lock._unlock();

        DIE("natives_load_library: apr_palloc failed");
        return NULL;
    }

    pinfo->handle = handle;

    Global_Env *ge = VM_Global_State::loader_env;
    pinfo->name = ge->string_pool.lookup(library_name);

    pinfo->next = jni_libs.lib_info_list;
    jni_libs.lib_info_list = pinfo;

    jni_libs.lock._unlock();

    jint UNREF res = find_call_JNI_OnLoad(pinfo->handle); // What to do with result???

    *pstatus = APR_SUCCESS;

    return pinfo->handle;
} // natives_load_library


// Function unloads native library.
static void
natives_unload_library_internal(NativeLibInfo* pfound)
{
    assert(pfound);

    // FIXME find_call_JNI_OnUnload(pfound->handle);

    apr_dso_unload(pfound->handle);
} // natives_unload_library

// Function unloads native library.
void
natives_unload_library(NativeLibraryHandle library_handle)
{
    assert(NULL != library_handle);

    NativeLibInfo* prev = NULL;
    for(NativeLibInfo* lib = jni_libs.lib_info_list;
        lib; lib = lib->next)
    {
        if(lib->handle == library_handle)
        {
            jni_libs.lock._lock();
            if(jni_libs.lib_info_list == lib) {
                jni_libs.lib_info_list = lib->next;
            } else {
                prev->next = lib->next;
            }
            natives_unload_library_internal(lib);
            jni_libs.lock._unlock();
            return;
        }

        prev = lib;
    }
} // natives_unload_library

// Function looks for loaded native library with a given name.
bool
natives_is_library_loaded(const char* library_name)
{
    return (search_library_list(library_name) != NULL);
} // natives_is_library_loaded

// Cleanups natives_support module. Cleans all remaining libraries.
// Is tread safety provided?????
void
natives_cleanup()
{
    for(NativeLibraryList lib = jni_libs.lib_info_list;
        lib; lib = lib->next)
    {
        natives_unload_library_internal(lib);
    }

    apr_pool_destroy(jni_libs.ppool);
    jni_libs.ppool = NULL;
} // natives_cleanup

// Function calculates mangled string length.
static inline int
natives_get_mangled_string_length( const char *string )
{
    int added = 0;

    assert(string);
    // calculate mangled string length
    int len = (int)strlen(string); 
    for( int index = 0; index < len; index++ ) {
        switch( string[index] ) {
        case '_':
        case ';':
        case '[':
            added++;
            break;
        case '$':
            added += 6 - 1;
            break;
        case '(':
            added--;
            continue;
        case ')':
            added -= len - index;
            index = len;
            break;
        default:
            break;
        }
    }
    return added + len;
} // natives_get_mangled_string_length

// Function mangles string.
static inline char *
natives_get_mangled_string( char *buf,
                            const char *string)
{
    assert(buf);
    assert(string);

    // mangle string
    int len = (int)strlen(string); 
    for( int index = 0; index < len; index++ ) {
        switch( string[index] ) {
        case '_':
            *buf = '_';
            *(buf + 1) = '1';
            buf += 2;
            break;
        case ';':
            *buf = '_';
            *(buf + 1) = '2';
            buf += 2;
            break;
        case '[':
            *buf = '_';
            *(buf + 1) = '3';
            buf += 2;
            break;
        case '/':
            *buf = '_';
            buf++;
            break;
        case '$':
            {
                int len = sprintf(buf, "_0%04x", '$' );
                assert(len == 6);
                buf += len;
            }
            break;
        case '(':
            continue;
        case ')':
            index = len;
            break;
        default:
            *buf = string[index];
            buf++;
            break;
        }
    }
    return buf;
} // natives_get_mangled_string

// Function returns mangled symbol name length based on class and method names.
static inline int
natives_get_mangled_symbol_name_length( const char* klass,
                                        const char* name)
{
    // 1. prefix 'Java_'
    int len = 5;

    // 2. class name
    len += natives_get_mangled_string_length( klass );

    // 3. an underscore ('_') separator
    len++;

    // 4. method name
    len += natives_get_mangled_string_length( name );

    return len;
} // natives_get_mangled_symbol_name_length

// Function extends mangled symbol name length to mandled overloaded symbol
// name length which is based on class name method name and descriptor of method.
static inline int
natives_get_overloaded_length( int mandled_len, 
                               const char* desc)
{
    // 5. for overloaded an underscore ('__') separator
    mandled_len += 2;

    // 6. method descriptor
    mandled_len += natives_get_mangled_string_length( desc );
    
    return mandled_len;
} // natives_get_overloaded_length

// Function returns mangled symbol name based on class and method names.
static void
natives_get_mangled_symbol_name( char *buf,
                                 const char* klass,
                                 const char* name)
{
    // 1. prefix 'Java_'
    memcpy( buf, "Java_", 5 );
    buf += 5;

    // 2. class name
    buf = natives_get_mangled_string( buf, klass );

    // 3. an underscore ('_') separator
    *buf = '_';
    buf++;

    // 4. method name
    buf = natives_get_mangled_string( buf, name );

    // 5. end of string
    *buf = '\0';

    return;
} // natives_get_mangled_symbol_name

// Function extends mangled symbol name to mangled overloaded symbol name
// based on class name, method name and descriptor of method.
static void
natives_get_overloaded_name( char *buf,
                             const char* desc)
{
    // 5. for overloaded an underscore ('__') separator
    buf[0] = '_';
    buf[1] = '_';
    buf += 2;

    // 6. method descriptor
    buf = natives_get_mangled_string( buf, desc );

    // 7. end of string
    *buf = '\0';

    return;
} // natives_get_overloaded_name

#ifdef PLATFORM_NT

// Function calculates function arguments length in bytes
//      and writes string to the buffer.
static inline unsigned
natives_get_args_len(const char* desc)
{
    // calculate argument length
    unsigned value = 8;
    int len = (int)strlen(desc); 
    bool is_array = false;
    for( int index = 0; index < len; index++ ) {
        switch( desc[index] ) {
        case 'L':
            // skip class name
            do {
                index++;
            } while( desc[index] != ';' );
        case 'B':
        case 'C':
        case 'F':
        case 'I':
        case 'S':
        case 'Z':
            value += 4;
            is_array = false;
            break;
        case 'D':
        case 'J':
            if( is_array ) {
                value += 4;
            } else {
                value += 8;
            }
            is_array = false;
            break;
        case '[':
            is_array = true;
            break;
        case '(':
            break;
        case ')':
            index = len;
            break;
        default:
            break;
        }
    }

    return value;
} // natives_append_args_size

// Function extends mangled symbol name length to mangled name length for Windows.
static inline int
natives_get_win_length( int mandled_len,
                        const char* desc)
{
    // 1. prefix '_' before 'Java_'
    mandled_len++;

    // 2. postfix '@' and bytes amount of arguments
    mandled_len += 1 + 10;

    return mandled_len;
} // natives_get_win_length

// Function extends mangled symbol name to mangled name for Windows.
static inline void
natives_get_win_name(char* buf,
                     const char* symbol, 
                     unsigned args_len)
{
    // 1. prefix '_' before 'Java_'
    *buf = '_';
    buf++;

    // 2. copy symbol name
    int len = (int)strlen(symbol);
    memcpy( buf, symbol, len);
    buf += len;

    // 3. postfix '@'
    *buf = '@';
    buf++;

    // 4. size of arguments in bytes
    sprintf(buf, "%d", args_len);

    return;
} // natives_get_win_name

#endif // PLATFORM_NT


static GenericFunctionPointer
natives_lookup_symbol(NativeLibraryList libraries, const char* symbol)
{
    for(NativeLibInfo* lib = libraries; lib; lib = lib->next) {
        apr_dso_handle_sym_t func;
        apr_status_t res = apr_dso_sym(&func, lib->handle, symbol);
        if(res == APR_SUCCESS) {
            return (GenericFunctionPointer)func;
        }
    }
    return NULL;
}


// Function looks for method with a given name and descriptor in a given native library.
GenericFunctionPointer
natives_lookup_method( NativeLibraryList libraries,
                       const char* class_name,
                       const char* method_name,
                       const char* method_desc)
{
    TRACE("Looking for native: "
        << class_name << "."
        << method_name << method_desc);

    // get mangled symbol name length and mangled overloaded symbol length
    int len = natives_get_mangled_symbol_name_length(class_name, method_name);
    int ov_len = natives_get_overloaded_length(len, method_desc);

#ifdef PLATFORM_NT
    // get lengths for Windows
    int wlen = natives_get_win_length( len, method_desc );
    int wov_len = ov_len + wlen - len;
    int wargs_len = natives_get_args_len(method_desc);
#endif // PLATFORM_NT

    // alloc memory for mangled overloaded symbol
    char *name = (char*)STD_ALLOCA( ov_len + 1 );

    // get mangled symbol name
    natives_get_mangled_symbol_name(name, class_name, method_name);
    TRACE("\ttrying: " << name);

    // find function in library
    GenericFunctionPointer func;
    if((func = natives_lookup_symbol(libraries, name)) != NULL)
        return func;

#ifdef PLATFORM_NT
    // alloc memory for mangled overloaded symbol for Windows
    char *wname = (char*)STD_ALLOCA( wov_len + 1 );

    // get mangled symbol name for Windows
    natives_get_win_name(wname, name, wargs_len);
    TRACE("\ttrying: " << wname);

    // find function in library
    if((func = natives_lookup_symbol(libraries, wname)) != NULL)
        return func;
#endif // PLATFORM_NT

    // get mangled overloaded symbol name
    natives_get_overloaded_name(name + len, method_desc);
    TRACE("\ttrying: " << name);

    // find function in library
    if((func = natives_lookup_symbol(libraries, name)) != NULL)
        return func;

#ifdef PLATFORM_NT
    // get mangled overloaded symbol name
    natives_get_win_name(wname, name, wargs_len);
    TRACE("\ttrying: " << wname);

    // find function in library
    if((func = natives_lookup_symbol(libraries, wname)) != NULL)
        return func;
#endif // PLATFORM_NT

    return NULL;
} // natives_lookup_method

void
natives_describe_error(NativeLoadStatus error, char* buf, size_t buflen)
{
    apr_strerror(error, buf, buflen);
} // natives_describe_error
