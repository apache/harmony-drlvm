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
 * @author Vladimir Nenashev
 * @version $Revision$
 */  

#include <ctype.h>
#include "vm_threads.h"
#include "port_malloc.h"
#include "port_dso.h"
#include "jit_intf_cpp.h"
#include "Class.h"
#include "class_member.h"
#include "exceptions.h"
#include "stack_trace.h"
#include "interpreter_exports.h"
#include "cci.h"
#include "m2n.h"
#include "native_stack.h"
#include "native_modules.h"
#include "natives_support.h"
#include "exception_filter.h"

#include "stack_dump.h"


static native_module_t* g_modules = NULL;

// Is called to fill modules list (should be called under lock)
static void sd_fill_modules()
{
    if (g_modules)
        return;

    int count;
    bool res = get_all_native_modules(&g_modules, &count);
    assert(res && g_modules && count);
}

#ifdef SD_UPDATE_MODULES
// Is called to update modules info
void sd_update_modules()
{
    hymutex_t* sd_lock;
    bool res = sd_initialize(&sd_lock);

    if (!res)
        return;

    hymutex_lock(sd_lock);

    if (g_modules)
        clear_native_modules(&g_modules);

    int count;
    res = get_all_native_modules(&g_modules, &count);
    assert(res && g_modules && count);

    hymutex_unlock(sd_lock);
}
#endif // SD_UPDATE_MODULES


static char* sd_construct_java_method_name(Method* m, char* buf)
{
    if (!m || !buf)
    {
        *buf = 0;
        return NULL;
    }

    char* ptr = buf;

    const char* err_str = "<--Truncated: too long name";
    size_t err_len = strlen(err_str);

    size_t remain = SD_MNAME_LENGTH - 1;
    const char* cname = m->get_class()->get_name()->bytes;
    size_t clen = m->get_class()->get_name()->len;
    const char* mname = m->get_name()->bytes;
    size_t mlen = m->get_name()->len;
    const char* descr = m->get_descriptor()->bytes;
    size_t dlen = m->get_descriptor()->len;

    if (clen + 1 > remain)
    {
        size_t len = remain - err_len;
        memcpy(ptr, cname, len);
        strcpy(ptr + len, err_str);
        return buf;
    }

    memcpy(ptr, cname, clen);
    ptr += clen;
    *ptr++ = '.';
    remain -= clen + 1;

    if (mlen > remain)
    {
        if (remain > err_len)
            memcpy(ptr, mname, remain - err_len);

        strcpy(ptr + remain - err_len, err_str);
        return buf;
    }

    memcpy(ptr, mname, mlen);
    ptr += mlen;
    remain -= mlen;

    if (dlen > remain)
    {
        if (remain > err_len)
            memcpy(ptr, descr, remain - err_len);

        strcpy(ptr + remain - err_len, err_str);
        return buf;
    }

    strcpy(ptr, descr);
    return buf;
}

static void sd_get_java_method_info(MethodInfo* info, Method* m, void* ip,
                                    bool is_ip_past, int inl_depth)
{
    *info->method_name = 0;
    *info->file_name = 0;
    info->line = -1;

    if (!m || !method_get_class(m))
        return;

    if (!sd_construct_java_method_name(m, info->method_name))
        return;

    const char* fname = NULL;
    get_file_and_line(m, ip, is_ip_past, inl_depth, &fname, &info->line);

    if (fname)
    {
        if (strlen(fname) >= sizeof(info->file_name))
        {
            memcpy(info->file_name, fname, sizeof(info->file_name));
            info->file_name[sizeof(info->file_name) - 1] = 0;
        }
        else
            strcpy(info->file_name, fname);
    }
}

static void sd_print_line(int count, MethodInfo* m) {

    fprintf(stderr, "%3d: %s (%s:%d)\n",
        count,
        *m->method_name ? m->method_name : "??",
        *m->file_name ? m->file_name : "??",
        m->line);
}


static void sd_print_stack_jit(VM_thread* thread,
                        native_frame_t* frames, jint num_frames)
{
    jint frame_num = 0;
    jint count = 0;
    StackIterator* si = NULL;

    if (thread)
        si = si_create_from_native(thread);

    while ((si && !si_is_past_end(si)) || frame_num < num_frames)
    {
        MethodInfo m;
        void* cur_ip = frames[frame_num].ip;

        if (frame_num < num_frames && frames[frame_num].java_depth < 0)
        {
            if (native_is_ip_stub(cur_ip)) // Generated stub
            {
                char buf[81];
                char* stub_name =
                    native_get_stub_name(cur_ip, buf, sizeof(buf));

                fprintf(stderr, "%3d: 0x%"W_PI_FMT": stub '%s'\n",
                    count++, (POINTER_SIZE_INT)cur_ip,
                    stub_name ? stub_name : "??");
                ++frame_num;
                continue;
            }

            // pure native frame
            native_module_t* module = find_native_module(g_modules, cur_ip);
            sd_get_c_method_info(&m, module, cur_ip);
            sd_print_line(count++, &m);
            ++frame_num;
            continue;
        }

        // Java/JNI frame, look into stack iterator

        // If iterator is exhausted
        if (si_is_past_end(si) ||
            (si_is_native(si) && !m2n_get_previous_frame(si_get_m2n(si))))
            break;

        if (si_is_native(si) && frame_num < num_frames)
        {
            // Print information from native stack trace for JNI frames
            native_module_t* module = find_native_module(g_modules, cur_ip);
            sd_get_c_method_info(&m, module, cur_ip);
            sd_print_line(count, &m);
        }
        else if (si_is_native(si) && frame_num >= num_frames)
        {
            // Print information about JNI frames from iterator
            // when native stack trace is not available
            Method* method = m2n_get_method(si_get_m2n(si));
            void* ip = m2n_get_ip(si_get_m2n(si));
            sd_get_java_method_info(&m, method, ip, false, -1);
            sd_print_line(count, &m);
        }
        else // !si_is_native(si)
        {
            // Print information about Java method from iterator
            CodeChunkInfo* cci = si_get_code_chunk_info(si);
            Method* method = cci->get_method();
            void* ip = (void*)si_get_ip(si);

            uint32 inlined_depth = si_get_inline_depth(si);
            uint32 offset = (uint32)((POINTER_SIZE_INT)ip -
                (POINTER_SIZE_INT)cci->get_code_block_addr());
            bool is_ip_past = (frame_num != 0);

            if (inlined_depth)
            {
                for (uint32 i = inlined_depth; i > 0; i--)
                {
                    Method* inl_method = cci->get_jit()->get_inlined_method(
                                            cci->get_inline_info(), offset, i);

                    sd_get_java_method_info(&m, inl_method, ip, is_ip_past, i);
                    sd_print_line(count++, &m);

                    if (frame_num < num_frames)
                        ++frame_num; // Go to the next native frame
                }
            }

            sd_get_java_method_info(&m, method, ip, is_ip_past, -1);
            sd_print_line(count, &m);
        }

        ++count;
        si_goto_previous(si);

        if (frame_num < num_frames)
            ++frame_num; // Go to the next native frame
    }

    if (si)
        si_free(si);
}

static void sd_print_stack_interpreter(VM_thread* thread,
    native_frame_t* frames, jint num_frames)
{
    FrameHandle* frame = interpreter.interpreter_get_last_frame(thread);
    jint frame_num = 0;
    jint count = 0;

    while (frame || frame_num < num_frames)
    {
        MethodInfo m;

        if (frame_num < num_frames && frames[frame_num].java_depth < 0)
        { // pure native frame
            native_module_t* module = find_native_module(g_modules, frames[frame_num].ip);
            sd_get_c_method_info(&m, module, frames[frame_num].ip);
            sd_print_line(count++, &m);
            ++frame_num;
            continue;
        }

        // Java/JNI frame, look into stack iterator

        Method* method = (Method*)interpreter.interpreter_get_frame_method(frame);
        uint8* bc_ptr = interpreter.interpreter_get_frame_bytecode_ptr(frame);

        // Print information from native stack trace
        // when method is not available or is native
        if (frame_num < num_frames &&
            (!method || method_is_native(method)))
        {
            native_module_t* module = find_native_module(g_modules, frames[frame_num].ip);
            sd_get_c_method_info(&m, module, frames[frame_num].ip);
            sd_print_line(count, &m);
        }

        // Print information about method from iterator
        // when is Java method or when native stack trace is not available
        if (method &&
            (!method_is_native(method) || frame_num >= num_frames))
        {
            sd_get_java_method_info(&m, method, (void*)bc_ptr, false, -1);
            sd_print_line(count, &m);
        }

        ++count;
        frame = interpreter.interpreter_get_prev_frame(frame);

        if (frame_num < num_frames)
            ++frame_num; // Go to the next native frame
    }
}

const char* sd_get_module_type(const char* short_name)
{
    char name[256];

    if (strlen(short_name) > 255)
        return "Too long short name";

    strcpy(name, short_name);
    char* dot = strchr(name, '.');

    // Strip suffix/extension
    if (dot)
        *dot = 0;

    // Strip prefix
    char* nameptr = name;

    if (!memcmp(short_name, PORT_DSO_PREFIX, strlen(PORT_DSO_PREFIX)))
        nameptr += strlen(PORT_DSO_PREFIX);

    char* vm_modules[] = {"java", "em", "encoder", "gc_gen", "gc_gen_uncomp", "gc_cc",
        "harmonyvm", "hythr", "interpreter", "jitrino", "vmi"};

    for (size_t i = 0; i < sizeof(vm_modules)/sizeof(vm_modules[0]); i++)
    {
        if (!strcmp_case(name, vm_modules[i]))
            return "VM native code";
    }

    if (natives_is_library_loaded_slow(short_name))
        return "JNI native library";

    return "Unknown/system native module";
}


static void sd_print_module_info(Registers* regs)
{
#ifdef SD_UPDATE_MODULES
    sd_fill_modules(); // Fill modules table if needed
#endif

    native_module_t* module = find_native_module(g_modules, (void*)regs->get_ip());
    sd_parse_module_info(module, (void*)regs->get_ip());
}

static void sd_print_modules()
{
    fprintf(stderr, "\nLoaded modules:\n\n");
    dump_native_modules(g_modules, stderr);
}


static void sd_print_threads_info(VM_thread* cur_thread)
{
    if (!cur_thread)
        fprintf(stderr, "\nCurrent thread is not attached to VM, ID: %d\n", sd_get_cur_tid());

    fprintf(stderr, "\nVM attached threads:\n\n");

    hythread_iterator_t it = hythread_iterator_create(NULL);
    int count = (int)hythread_iterator_size (it);

    for (int i = 0; i < count; i++)
    {
        hythread_t thread = hythread_iterator_next(&it);
        VM_thread* vm_thread = jthread_get_vm_thread(thread);

        if (!vm_thread)
            continue;

        jthread java_thread = jthread_get_java_thread(thread);
        JNIEnv* jni_env = vm_thread->jni_env;

        if (cur_thread && java_thread)
        {
            jclass cl = GetObjectClass(jni_env, java_thread);
            jmethodID id = jni_env->GetMethodID(cl, "getName","()Ljava/lang/String;");
            jstring name = jni_env->CallObjectMethod(java_thread, id);
            char* java_name = (char*)jni_env->GetStringUTFChars(name, NULL);

            fprintf(stderr, "%s[%p]  '%s'\n",
                    (cur_thread && vm_thread == cur_thread) ? "--->" : "    ",
                    thread->os_handle, java_name);

            jni_env->ReleaseStringUTFChars(name, java_name);
        }
        else
        {
            fprintf(stderr, "%s[%p]\n",
                    (cur_thread && vm_thread == cur_thread) ? "--->" : "    ",
                    thread->os_handle);
        }
    }

    hythread_iterator_release(&it);
}


void sd_print_stack(Registers* regs)
{
    hymutex_t* sd_lock;
    int disable_count;
    bool unwindable;

    VM_thread* thread = get_thread_ptr(); // Can be NULL for pure native thread

    // Enable suspend to allow working with threads
    if (thread)
        disable_count = hythread_reset_suspend_disable();

    // Acquire global lock to print threads list and stop other crashed threads
    hythread_global_lock();

    if (!sd_initialize(&sd_lock))
        return;

    hymutex_lock(sd_lock);
    if (thread)
        unwindable = set_unwindable(false); // To call Java code

    // Print register info
    print_reg_state(regs);

    // Print crashed modile info
    sd_print_module_info(regs);

    // Print program environment info
    sd_print_cwdcmdenv();

    // Print the whole list of modules
    sd_print_modules();

    native_frame_t* frames = NULL;

    // Print threads info
    sd_print_threads_info(thread);

    // We are trying to get native stack trace using walk_native_stack_registers
    // function and get corresponding Java methods for stack trace from
    // JIT/interpreter stack iterator.
    // When native stack trace is not complete (for example, when
    // walk_native_stack_registers cannot unwind frames in release build),
    // we will use JIT/interpreter stack iterator to complete stack trace.

    jint num_frames =
        walk_native_stack_registers(regs, thread, -1, NULL);

    if (num_frames)
        frames = (native_frame_t*)STD_ALLOCA(sizeof(native_frame_t)*num_frames);

    if (num_frames && frames)
        walk_native_stack_registers(regs, thread, num_frames, frames);
    else
        num_frames = 0; // Consider native stack trace empty

    fprintf(stderr, "\nStack trace:\n");

    if(interpreter_enabled() && thread)
        sd_print_stack_interpreter(thread, frames, num_frames);
    else // It should be used also for threads without VM_thread structure
        sd_print_stack_jit(thread, frames, num_frames);

    fprintf(stderr, "<end of stack trace>\n");
    fflush(stderr);

    if (thread)
        set_unwindable(unwindable);

    // Do not unlock to prevent other threads from printing crash stack
    //hymutex_unlock(sd_lock);

    hythread_global_unlock();
    if (thread)
        hythread_set_suspend_disable(disable_count);
}
