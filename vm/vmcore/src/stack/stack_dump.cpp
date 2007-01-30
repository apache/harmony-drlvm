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

static void st_get_java_method_info(MethodInfo* info, Method* m, void* ip, bool is_ip_past) {
    info->file_name = NULL;
    info->line = -1;
    info->method_name = NULL;
    assert(m);
    if (m->get_class()->has_source_information() && m->get_class()->get_source_file_name()) {
        const char* fname = m->get_class()->get_source_file_name();
        size_t flen = m->get_class()->get_source_file_name_length();
        info->file_name = (char*) STD_MALLOC(flen + 1);
        strcpy(info->file_name, fname);
    }

    const char* mname = m->get_name()->bytes;
    size_t mlen = m->get_name()->len;
    const char* cname = m->get_class()->get_name()->bytes;
    size_t clen = m->get_class()->get_name()->len;
    const char* descr = m->get_descriptor()->bytes;
    size_t dlen = m->get_descriptor()->len;
    
    info->method_name = (char*) STD_MALLOC(mlen + clen + dlen + 2);
    memcpy(info->method_name, cname, clen);
    info->method_name[clen] = '.';
    memcpy(info->method_name + clen + 1, mname, mlen);
    memcpy(info->method_name + clen + mlen + 1, descr, dlen);
    info->method_name[clen + mlen + dlen + 1] = '\0';

    const char* f;
    get_file_and_line(m, ip, is_ip_past, &f, &info->line);
}

static void st_print_line(int count, MethodInfo* m) {
    fprintf(stderr, "\t%d: %s (%s:%d)\n",
        count,
        m->method_name ? m->method_name : "??",
        m->file_name ? m->file_name : "??",
        m->line);
    if (m->file_name) {
        STD_FREE(m->file_name);
    }
    if (m->method_name) {
        STD_FREE(m->method_name);
    }
}

void st_print_stack(Registers* regs) {
    if(interpreter_enabled()) {
       interpreter.stack_dump(get_thread_ptr());
       return;
    }
    jint num_frames;
    native_frame_t* frames;
    num_frames = walk_native_stack_registers(regs, p_TLS_vmthread, -1, NULL);
    frames = (native_frame_t*) STD_ALLOCA(sizeof(native_frame_t) * num_frames);
    num_frames = walk_native_stack_registers(regs, p_TLS_vmthread, num_frames, frames);
    StackIterator* si = si_create_from_native();
    fprintf(stderr, "Stack trace:\n");
    for (int i = 0; i < num_frames; i++) {		
        static int count = 0;
        MethodInfo m;
        if (frames[i].java_depth == -1 && !native_is_ip_stub(frames[i].ip)) { // Pure native method
            st_get_c_method_info(&m, frames[i].ip);
        } else if (frames[i].java_depth == -1) { // Generated stub
            fprintf(stderr, "\t%d: IP is 0x%08X <native code>\n", ++count, (POINTER_SIZE_INT) frames[i].ip); //FIXME: IA32 ONLY
            continue;
        } else { // Java/JNI native method
            CodeChunkInfo* cci = si_get_code_chunk_info(si);
            if (!cci) { // JNI native method
                st_get_c_method_info(&m, frames[i].ip);
            } else { // Java method
                uint32 inlined_depth = si_get_inline_depth(si);
                uint32 offset = (POINTER_SIZE_INT)si_get_ip(si) - (POINTER_SIZE_INT)cci->get_code_block_addr();
                for (uint32 j = 0; j < inlined_depth; j++) {
                    Method *real_method = cci->get_jit()->get_inlined_method(cci->get_inline_info(), offset, j);
                    st_get_java_method_info(&m, real_method, frames[i].ip, 0 == i);
                    st_print_line(++count, &m);
                }
                st_get_java_method_info(&m, cci->get_method(), frames[i].ip, 0 == i);
          }
          si_goto_previous(si);
      }
      st_print_line(++count, &m);
  }
  fprintf(stderr, "<end of stack trace>\n");
}
