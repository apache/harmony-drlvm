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

#include "stack_dump.h"
#include "native_stack.h"
#include "vm_threads.h"
#include "port_malloc.h"
#include "jit_intf_cpp.h"
#include "Class.h"
#include "class_member.h"
#include "stack_trace.h"
#include "interpreter_exports.h"
#include "cci.h"
#include "m2n.h"
#include <ctype.h>

#ifdef PLATFORM_NT

#ifndef NO_DBGHELP
#include <dbghelp.h>
#pragma comment(linker, "/defaultlib:dbghelp.lib")
#endif

#else
#include <unistd.h>
#endif

// Symbolic method info: method name, source file name and a line number of an instruction within the method
struct MethodInfo {
    char* method_name;
    char* file_name;
    int line;
};

#ifdef PLATFORM_POSIX

/**
 * Find a module which a given IP address belongs to.
 * Returns module file name and offset of an IP address from beginnig of the segment (used in addr2line tool)
 */
static char* get_module(native_module_t* info, jint num_modules, void* ip, POINTER_SIZE_INT* offset) {
    for (int i = 0; i < num_modules; i++, info = info->next) {
        for (int j = 0; j < info->seg_count; j++) {
            if (info->segments[j].type != SEGMENT_TYPE_DATA) {
                if ( (POINTER_SIZE_INT) info->segments[j].base <= (POINTER_SIZE_INT) ip &&
                    (POINTER_SIZE_INT) ip < (POINTER_SIZE_INT) info->segments[j].base + info->segments[j].size) {
                    *offset = (POINTER_SIZE_INT) ip - (POINTER_SIZE_INT) info->segments[j].base;
                    return info->filename;
                }
            }
        }
    }
    return NULL;
}
#endif

static void st_get_c_method_info(MethodInfo* info, void* ip) {
    info->method_name = NULL;
    info->file_name = NULL;
    info->line = -1;
#ifdef PLATFORM_NT
#ifndef NO_DBGHELP
    if (!SymInitialize(GetCurrentProcess(), NULL, true)) {
        return;
    }
    BYTE smBuf[sizeof(SYMBOL_INFO) + 2048];
    PSYMBOL_INFO pSymb = (PSYMBOL_INFO)smBuf;
    pSymb->SizeOfStruct = sizeof(smBuf);
    pSymb->MaxNameLen = 2048;
    DWORD64 funcDispl;
    if(SymFromAddr(GetCurrentProcess(), (DWORD64)ip, &funcDispl, pSymb)) {
        info->method_name = (char*) STD_MALLOC(strlen(pSymb->Name) + 1);
        strcpy(info->method_name, pSymb->Name);        
    }
    DWORD lineOffset;
    IMAGEHLP_LINE lineInfo;
    if (SymGetLineFromAddr(GetCurrentProcess(), (DWORD)ip, &lineOffset, &lineInfo)) {
        info->line = lineInfo.LineNumber;
        info->file_name = (char*) STD_MALLOC(strlen(lineInfo.FileName) + 1);
        strcpy(info->file_name, lineInfo.FileName);

    }
#else
    return;
#endif
#else // PLATFORM_NT
    jint num_modules;
    native_module_t* modules;
    if (!get_all_native_modules(&modules, &num_modules)) {
        fprintf(stderr, "Warning: Cannot get modules info, no symbolic information will be provided\n");
        return;
    }
    
    POINTER_SIZE_INT offset;
    char* module = get_module(modules, num_modules, ip, &offset);
    if (module) {
        int pi[2];
        int po[2];
        pipe(pi);
        pipe(po);
        if (!fork()) {
            close(po[0]);
            close(pi[1]);
            dup2(po[1], 1);
            dup2(pi[0], 0);            
            execlp("addr2line", "addr2line", "-f", "-e", module, "-C", NULL);
            fprintf(stderr, "Warning: Cannot run addr2line. No symbolic information will be available\n");
            printf("??\n??:0\n"); // Emulate addr2line output
            exit(-1);
        } else {
            close(po[1]);
            close(pi[0]);
            char ip_str[9];
            sprintf(ip_str, "%08x\n", (uint32) offset); // !!!FIXME: WORKS FOR IA32 ONLY
            write(pi[1], ip_str, 9);
            close(pi[1]);
            char buf[256];
            int status;
            wait(&status);
            int count = read(po[0], buf, 255);
            close(po[0]);
            if (count >= 0) {
                while (isspace(buf[count-1])) {
                    count--;
                }
                buf[count] = '\0';
                int i = 0;
                for (; i < count; i++) {
                    if (buf[i] == '\n') { // Function name is limited by '\n'
                        buf[i] = '\0';
                        info->method_name = (char*) STD_MALLOC(strlen(buf) + 1);
                        strcpy(info->method_name, buf);
                        break;
                    }
                }
                char* fn = buf + i + 1;
                for (; i < count && buf[i] != ':'; i++) // File name and line number are separated by ':'
                    ;
                buf[i] = '\0';
                info->file_name = (char*) STD_MALLOC(strlen(fn) + 1);
                strcpy(info->file_name, fn);
                char* line = buf + i + 1;
                info->line = atoi(line);
                if (info->line == 0) {
                    info->line = -1;
                }
            } else {
                fprintf(stderr, "read() failed during execution of addr2line\n");
            }
        }
    }
#endif
}

static char* construct_java_method_name(Method* m)
{
    if (!m)
        return NULL;

    const char* mname = m->get_name()->bytes;
    size_t mlen = m->get_name()->len;
    const char* cname = m->get_class()->get_name()->bytes;
    size_t clen = m->get_class()->get_name()->len;
    const char* descr = m->get_descriptor()->bytes;
    size_t dlen = m->get_descriptor()->len;
    
    char* method_name = (char*)STD_MALLOC(mlen + clen + dlen + 2);
    if (!method_name)
        return NULL;

    char* ptr = method_name;
    memcpy(ptr, cname, clen);
    ptr += clen;
    *ptr++ = '.';
    memcpy(ptr, mname, mlen);
    ptr += mlen;
    memcpy(ptr, descr, dlen);
    ptr[dlen] = '\0';

    return method_name;
}

static void st_get_java_method_info(MethodInfo* info, Method* m, void* ip,
                                    bool is_ip_past, int inl_depth)
{
    info->method_name = NULL;
    info->file_name = NULL;
    info->line = -1;

    if (!m || !method_get_class(m))
        return;

    info->method_name = construct_java_method_name(m);
    const char* fname = NULL;
    get_file_and_line(m, ip, is_ip_past, inl_depth, &fname, &info->line);

    if (fname)
    {
        size_t fsize = strlen(fname) + 1;
        info->file_name = (char*)STD_MALLOC(fsize);
        if (info->file_name)
            memcpy(info->file_name, fname, fsize);
    }
}

static void st_print_line(int count, MethodInfo* m) {
    fprintf(stderr, "\t%d: %s (%s:%d)\n",
        count,
        m->method_name ? m->method_name : "??",
        m->file_name ? m->file_name : "??",
        m->line);

    if (m->method_name)
        STD_FREE(m->method_name);

    if (m->file_name)
        STD_FREE(m->file_name);
}


void st_print_stack_jit(VM_thread* thread,
                        native_frame_t* frames, jint num_frames)
{
    jint frame_num = 0;
    jint count = 0;
    StackIterator* si = NULL;

    if (thread)
        si = si_create_from_native(thread);

    while ((si && !si_is_past_end(si)) || frame_num < num_frames)
    {
        MethodInfo m = {NULL, NULL, 0};

        if (frame_num < num_frames && frames[frame_num].java_depth < 0)
        {
            if (native_is_ip_stub(frames[frame_num].ip)) // Generated stub
            {
                fprintf(stderr, "\t%d: <Generated stub> IP is %p\n",
                    count++, frames[frame_num].ip);
                ++frame_num;
                continue;
            }

            // pure native frame
            st_get_c_method_info(&m, frames[frame_num].ip);
            st_print_line(count++, &m);
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
            st_get_c_method_info(&m, frames[frame_num].ip);
            st_print_line(count, &m);
        }
        else if (si_is_native(si) && frame_num >= num_frames)
        {
            // Print information about JNI frames from iterator
            // when native stack trace is not available
            Method* method = m2n_get_method(si_get_m2n(si));
            void* ip = m2n_get_ip(si_get_m2n(si));
            st_get_java_method_info(&m, method, ip, false, -1);
            st_print_line(count, &m);
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

            for (uint32 i = 0; i < inlined_depth; i++)
            {
                Method* inl_method = cci->get_jit()->get_inlined_method(
                                            cci->get_inline_info(), offset, i);

                st_get_java_method_info(&m, inl_method, ip, is_ip_past, i);
                st_print_line(count++, &m);

                if (frame_num < num_frames)
                    ++frame_num; // Go to the next native frame
            }

            st_get_java_method_info(&m, method, ip, is_ip_past, -1);
            st_print_line(count, &m);
        }

        ++count;
        si_goto_previous(si);

        if (frame_num < num_frames)
            ++frame_num; // Go to the next native frame
    }

    if (si)
        si_free(si);
}

void st_print_stack_interpreter(VM_thread* thread,
    native_frame_t* frames, jint num_frames)
{
    FrameHandle* frame = interpreter.interpreter_get_last_frame(thread);
    jint frame_num = 0;
    jint count = 0;

    while (frame || frame_num < num_frames)
    {
        MethodInfo m = {NULL, NULL, 0};

        if (frame_num < num_frames && frames[frame_num].java_depth < 0)
        { // pure native frame
            st_get_c_method_info(&m, frames[frame_num].ip);
            st_print_line(count++, &m);
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
            st_get_c_method_info(&m, frames[frame_num].ip);
            st_print_line(count, &m);
        }

        // Print information about method from iterator
        // when is Java method or when native stack trace is not available
        if (method &&
            (!method_is_native(method) || frame_num >= num_frames))
        {
            st_get_java_method_info(&m, method, (void*)bc_ptr, false, -1);
            st_print_line(count, &m);
        }

        ++count;
        frame = interpreter.interpreter_get_prev_frame(frame);

        if (frame_num < num_frames)
            ++frame_num; // Go to the next native frame
    }
}

void st_print_stack(Registers* regs)
{
    // We are trying to get native stack trace using walk_native_stack_registers
    // function and get corresponding Java methods for stack trace from
    // JIT/interpreter stack iterator.
    // When native stack trace is not complete (for example, when
    // walk_native_stack_registers cannot unwind frames in release build),
    // we will use JIT/interpreter stack iterator to complete stack trace.

    VM_thread* thread = get_thread_ptr(); // Can be NULL for pure native thread
    native_frame_t* frames = NULL;

    jint num_frames =
        walk_native_stack_registers(regs, thread, -1, NULL);

    if (num_frames)
        frames = (native_frame_t*)STD_ALLOCA(sizeof(native_frame_t)*num_frames);

    if (num_frames && frames)
        walk_native_stack_registers(regs, thread, num_frames, frames);
    else
        num_frames = 0; // Consider native stack trace empty

   fprintf(stderr, "Stack trace:\n");

    if(interpreter_enabled() && thread)
        st_print_stack_interpreter(thread, frames, num_frames);
    else // It should be used also for threads without VM_thread structure
        st_print_stack_jit(thread, frames, num_frames);

  fprintf(stderr, "<end of stack trace>\n");
  fflush(stderr);
}
