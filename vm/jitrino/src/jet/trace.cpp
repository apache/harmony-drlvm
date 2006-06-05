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
 * @author Alexander V. Astapchuk
 * @version $Revision: 1.4.12.3.4.4 $
 */
 
/**
 * @file
 * @brief Implementation of debug tracing utilities.
 */
#include "trace.h"
#include "compiler.h"
#include "cg_ia32.h"

#include "jit_intf.h"

#if !defined(PLATFORM_POSIX)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif // !defined(PLATFORM_POSIX)

#include <stdio.h>
#include <stdarg.h>


namespace Jitrino
{
namespace Jet
{

/**
 * @brief Disassembling function prototype.
 * @todo document lwdis library usage
 */
typedef unsigned (*DISFUNC)(const char *kode, char *buf, unsigned buf_len);

/**
 * @brief Returns directory of currently running module, ended with file 
 *        separator.
 *
 * The function returns path to a directory, where the currently running 
 * module is located. The path guaranteed to end with file separator for 
 * current platform (that is '/' on Linux and '\\' on Windows).
 *
 * The function never fails. If it fails to find true path for the module, 
 * it returns a path pointing to current directory - './' or '.\\'.
 *
 * The function always returns the same address which points to internal 
 * static buffer. The buffer get initialized once, on the first call. 
 *
 * @note If called from within dynamic library, the function returns path for
 *       the 'main' executable module, not for the library itself.
 * 
 * @return directory of currently running module, ended with file separator
 */
const char * get_exe_dir(void)
{
#ifdef PLATFORM_POSIX
    static char buf[PATH_MAX+1];
    static const char FSEP = '/';
#else
    static char buf[_MAX_PATH+1];
    static const char FSEP = '\\';
#endif // ~PLATFORM_POSIX
    
    static bool path_done = false;
    if (path_done) {
        return buf;
    }
    
#ifdef PLATFORM_POSIX
    static const char self_path[] = "/proc/self/exe";
    // Resolve full path to module
    if (readlink(self_path, buf, sizeof(buf)) < 0) {
#else
    if (!GetModuleFileName(NULL, buf, sizeof(buf))) {
#endif
        // Something wrong happened. Trying last resort path.
        // buf <= "./"
        buf[0] = '.'; buf[1] = FSEP; buf[2] = '\0';
    }
    else {
        // Extract directory path
        char * slash = strrchr(buf, FSEP);
        if (NULL == slash) { // Huh ? How comes ?
            // Trying last resort path.
            // buf <= "./"
            buf[0] = '.'; buf[1] = FSEP; buf[2] = '\0';
        }
        else {
            *(slash+1) = '\0';
        }
    }
    path_done = true;
    return buf;
}

void dbg(const char *frmt, ...)
{
    static FILE *fout = fopen(LOG_FILE_NAME, "w");
    assert(fout != NULL);
    va_list valist;
    va_start(valist, frmt);
    vfprintf(fout, frmt, valist);
    fflush(fout);
    //vprintf(frmt, valist);
}

void __stdcall rt_dbg(const char *msg)
{
    static FILE *f = NULL;
    if (f == NULL) {
        f = fopen(RUNTIME_LOG_FILE_NAME, "w");
        assert(f);
    }
    static int cnt = 0;
    ++cnt;
    static int depth = 0;
    if (!strncmp(msg, "enter", 5)) {
        ++depth;
    }
    // print out:
    // [call depth] message <total count>
    // precede the whole string with several spaces, so elements 
    // with big differences in the depth will be visually separated
    fprintf(f, "%*c [%u] %s <%u>\n", depth%10, ' ', depth, msg, cnt);
    fflush(f);
    if (!strncmp(msg, "exit", 4)) {
        --depth;
    }
    if (depth<0) {
        depth = 0;
    }
}

/**
 * @brief Returns a pointer to a disassembling function.
 * @return - pointer to disassembling function, or NULL if disassembling 
 *           is not supported/presented.
 * @todo document lwdis
 */
static DISFUNC get_disfunc(void)
{
#if !defined(PLATFORM_POSIX)
    static HMODULE hDll = LoadLibrary("lwdis.dll");
    static void *disfunc =
    hDll == NULL ? NULL : GetProcAddress(hDll, "disasm");
#else // if !platform_posix
    static bool load_done = false;
    static void *disfunc = NULL;
    if (!load_done) {
        char buf[PATH_MAX+1];
        snprintf(buf, sizeof(buf), "%sliblwdis.so", get_exe_dir());
        void * handle = dlopen(buf, RTLD_NOW);
        disfunc = handle == NULL ? NULL : dlsym(handle, "disasm");
        load_done = true;
    }
#endif
    return (DISFUNC)disfunc;
}

/**
 * @brief Formats a string to describe the passed Slot.
 * 
 * Returns a string formatted with a description of a slot passed.
 * The string contains 3 numbers: \b vslot.regid.id and some characters
 * which describe the state of the slot.
 * 
 * The following legend is used:
 *
 *   - * - an item was changed, need to be sync-ed to memeory
 *   - mem - an item is currently swapped to the memory
 *   - {} - an item represents a high part of a wide type (#dbl64 or #i64)
 *   - + - an item is positive or zero
 *   - nz - an item was tested against zero (for int-s) or against null 
 *           (for jobj)
 *   - ! - an item need swapping (i.e. an overflow in stack regs cache 
 *         happend)
 */
static ::std::string toStr2(const JFrame::Slot & s, bool stackSlot)
{
    ::std::string str;
    if (s.jt == jvoid) {
        str = "[x]";
        return str;
    }
    str = "[";

    //if( s.half )    str += '{';
    if (s.state() & SS_HI_PART) {
        str += '{';
    }

    if (s.attrs() & SA_NOT_NEG) {
        str += '+';
    };
    str += jtypes[s.jt].name;

    str += ',';
    if (s.state() & SS_SWAPPED) {
        str += "mem";
    }
    
    if (s.state() & SS_NEED_SWAP) {
        str += '!';
    }
    
    if (s.state() & SS_CHANGED) {
        str += '*';
    }
    
    if (s.attrs() & SA_NZ) {
        str += "nz";
    }

    str += "#";
    char buf[30];
    if (s.vslot() == -1) {
        str += "_";
    }
    else {
        sprintf(buf, "%d", s.vslot());
        str += buf;
    }

    str += '/';
    if (s.regid() == -1) {
        str += "_";
    }
    else if (!(s.state() & SS_SWAPPED)) {
        const RegName * regs;
        regs = is_f(s.type()) ? Compiler::g_frnames : Compiler::g_irnames;
        if (!stackSlot) {
            regs += is_f(s.type()) ? 
                   Compiler::F_STACK_REGS : Compiler::I_STACK_REGS;
        }
        sprintf(buf, "%s", getRegNameString(regs[s.regid()]));
        str += buf;
    }
    else {
        sprintf(buf, "%d", s.regid());
        str += buf;
    }

    str += '/';
    if (s.id() == (unsigned) -1) {
        str += "_";
    }
    else {
        sprintf(buf, "%d", s.id());
        str += buf;
    }

    //if (s.changed) str += '*';
    //if (s.swapped) str += '!';
    //if (s.half) str += '}';
    if (s.state() & SS_HI_PART) {
        str += '}';
    }
    str += "]";
    return str;
}

::std::string trim(const char *p) {
    const char *start = NULL, *end = NULL;
    for (const char *pp = p; *pp; pp++) {
        if (!isspace(*pp)) {
            if (start == NULL) {
                start = pp;
            }
            end = pp;
        }
    }
    ::std::string s;
    if (!start) {
        return s;
    }
    s.assign(start, end - start + 1);
    return s;
}

void dump_frame(char *ptr)
{
    // outdated
    assert(false);  // also need method_get_info_block() to collect the complete data
    
    // presuming we have a EBP value
    MethodInfoBlock rtinfo(NULL);
    StackFrame stackframe(rtinfo.get_num_locals(),
                rtinfo.get_stack_max(), rtinfo.get_in_slots());
    int *p = (int *) ptr;

    unsigned num_locals = rtinfo.get_num_locals();
    unsigned stack_max = rtinfo.get_stack_max();

    unsigned stack_depth = *(p + stackframe.info_gc_stack_depth());

    dbg("EBP             = %p\n", p);
    dbg("NUM_LOCALS      = %d\n", num_locals);
    dbg("MAX_STACK_DEPTH = %d\n", stack_max);
    dbg("NUM_IN_SLOTS    = %d\n", rtinfo.get_in_slots());
    dbg("GC_STACK_DEPTH  = %d\n", stack_depth);
    dbg("counted.esp     = %X (*ESP=%d/%X)\n",
    p + stackframe.native_stack_bot(),
    *(int *) (p + stackframe.native_stack_bot()),
    *(int *) (p + stackframe.native_stack_bot()));

    dbg("GC.Locals: %X @ %X =>", *(p + stackframe.info_gc_locals()),
    p + stackframe.info_gc_locals());
    for (unsigned i = 0; i < num_locals; i++) {
        unsigned bit = bit_no(i);
        unsigned off = word_no(i);
        //unsigned data = *(p+frame.info_gc_locals() + off);
        bool b = 0 != ((1 << bit) & *(p + stackframe.info_gc_locals() + off));
        dbg(b ? "*" : ".");
        if (i && !(i % 5)) {
            dbg(" | ");
        }
        if (i > 65536) {
            assert(false);
        }
    }
    dbg("\n");
    dbg("GC.Stack:       =>");
    for (unsigned i = 0; i < stack_depth; i++) {
        unsigned bit = bit_no(i);
        unsigned off = word_no(i);
        //unsigned data = *(p+frame.info_gc_stack() + off);
        bool b = (1 << bit) & *(p + stackframe.info_gc_stack() + off);
        dbg(b ? "*" : ".");
        if (i && !(i % 5)) {
            dbg(" | ");
        };
        if (i > 65536) {
            assert(false);
        }
    }
    dbg("\n");
    //
    //
    dbg("===========================\n");
    for (unsigned i = 0; i < num_locals; i++) {
        dbg("local.%d = %d (%X)\n", 
                i, *(p + stackframe.local(i)), *(p + stackframe.local(i)));
    }
    dbg("===========================\n");
    dbg("note: the stack is printed from bottom (=0) to top(=max_stack)\n");
    for (unsigned i = 0; i < stack_depth; i++) {
        dbg("(%d) = %d (%X)\n", 
                i, 
                *(p + stackframe.native_stack_bot() + i), 
                *(p + stackframe.native_stack_bot() + i));
    }
    dbg("===========================\n");
}

void Compiler::dbg_trace_comp_start(void)
{
    // start ; counter ; klass::method ; bytecode size ; signature
    dbg("start |%5d| %s::%s | bc.size=%d | %s\n",
        g_methodsSeen,
        class_get_name(m_klass), method_get_name(m_method),
        method_get_byte_code_size(m_method),
        method_get_descriptor(m_method));
}

void Compiler::dbg_trace_comp_end(bool success, const char * reason)
{
    // end   ; counter ; klass::method ; 
    //          [REJECTED][code start ; code end ; code size] ; signature

    dbg("end   |%5d| %s::%s | ", 
        g_methodsSeen, class_get_name(m_klass), method_get_name(m_method));
        
    if (success) {
        void * start = method_get_code_block_addr_jit(m_method, jit_handle);
        unsigned size = method_get_code_block_size_jit(m_method, jit_handle);
        
        dbg("code.start=%p | code.end=%p | code.size=%d", 
             start, (char*)start + size, size);    
    }
    else {
        dbg("[REJECTED:%s]", reason);
    }
    dbg("| %s\n", method_get_descriptor(m_method));
}


::std::string Compiler::toStr(const JInst & jinst, bool show_names)
{
    char buf[10240], tmp0[1024] = { 0 }, tmp1[1024] = { 0 };
    
    const InstrDesc & idesc = instrs[jinst.opcode];

    if (jinst.op0 != (int) NOTHING) {
        const char * lpClass = NULL;
        const char * lpItem = NULL;
        const char * lpDesc = NULL;
        if (show_names) {
            const JavaByteCodes opc = jinst.opcode;
            
            switch(opc) {
            case OPCODE_INVOKESPECIAL:
            case OPCODE_INVOKESTATIC:
            case OPCODE_INVOKEVIRTUAL:
                lpClass = const_pool_get_method_class_name(m_klass, jinst.op0);
                lpItem = const_pool_get_method_name(m_klass, jinst.op0);
                lpDesc = const_pool_get_method_descriptor(m_klass, jinst.op0);
            break;
            case OPCODE_INVOKEINTERFACE:
                lpClass = 
                const_pool_get_interface_method_class_name(m_klass, jinst.op0);
                lpItem = const_pool_get_interface_method_name(m_klass, jinst.op0);
                lpDesc = const_pool_get_interface_method_descriptor(m_klass, jinst.op0);
                break;
            case OPCODE_GETFIELD:
            case OPCODE_PUTFIELD:
            case OPCODE_GETSTATIC:
            case OPCODE_PUTSTATIC:
                lpClass = const_pool_get_field_class_name(m_klass, jinst.op0);
                lpItem = const_pool_get_field_name(m_klass, jinst.op0);
                lpDesc = const_pool_get_field_descriptor(m_klass, jinst.op0);
                break;
            case OPCODE_NEW:
            case OPCODE_INSTANCEOF:
            case OPCODE_CHECKCAST:
                lpClass = const_pool_get_class_name(m_klass, jinst.op0);
                break;
            default: break;
            }
        }
        if (lpClass || lpItem || lpDesc) {
            snprintf(tmp0, sizeof(tmp0)-1, "%-2d {%s%s%s %s}",
                jinst.op0, 
                lpClass ? lpClass : "", lpClass ? "::" : "",
                lpItem ? lpItem : "", 
                lpDesc ? lpDesc : "");
        }
        else if (jinst.is_branch()) {
            sprintf(tmp0, "->%d<-", jinst.get_target(0));
        }
        else {
            sprintf(tmp0, "%-2d", jinst.op0);
        }
    }

    if (jinst.op1 != (int) NOTHING) {
        sprintf(tmp1, " %d ", jinst.op1);
    }

    snprintf(buf, sizeof(buf)-1, "%c%2d) %-15s %-6s %-6s",
        idesc.flags & OPF_GC_PT ? '*' : ' ', jinst.pc, idesc.name,
        tmp0, tmp1);

    return::std::string(buf);
}

void Compiler::dbg_dump_bbs(void)
{
    static const char *bb_delim = "======================================\n";

    BBInfo bb = m_bbs[0];
    JInst jinst;
    unsigned pc = 0;
    while (NOTHING != (pc = fetch(pc, jinst))) {
        bool bb_head = m_bbs.find(jinst.pc) != m_bbs.end();
        char jmptarget = ' ';
        if (bb_head) {
            bb = m_bbs[jinst.pc];
            if (bb.jmp_target) {
                jmptarget = '>';
            }
            dbg(bb_delim);
        }
        if (bb_head) {
            const BBInfo& bbinfo = m_bbs[jinst.pc];
            dbg("ref.count=%d, %s, last=%u, next=%u\n", bbinfo.ref_count, 
                bbinfo.jsr_target ? "#JSR#" : "",
                bbinfo.last_pc, bbinfo.next_bb);
            dbg("%c%s\n", jmptarget, toStr(jinst, false).c_str());
        }
        else {
            dbg("%c%s\n", jmptarget, toStr(jinst, false).c_str());
        }
    }
    dbg(bb_delim);
}

void dbg_dump_jframe(const char *name, const JFrame * pjframe)
{
    const JFrame& jframe = *pjframe;
    dbg("\n;; frame.%s, need_update=%d\n;;\n", name, jframe.need_update());
    //
    unsigned num_locals = jframe.num_vars();
    dbg(";; locals.total=%d\n;; ", num_locals);
    
    // Dump local variables first
    for (unsigned i = 0; i < num_locals; i++) {
        if (i && !(i % 5)) {
            dbg("\n;;  ");
        }
        const JFrame::Slot & s = jframe.get_var(i);
        dbg(" %s", toStr2(s, false).c_str());
    }
    
    dbg("\n;;\n");
    // ... and stack after that
    dbg(";; stack.size=%d\n;;  ", jframe.size());
    for (unsigned i = 0; i < jframe.size(); i++) {
        if (i && !(i % 5)) {
            dbg("\n;;  ");
        }
        const JFrame::Slot & s = jframe.get_stack(i);
        dbg(" %s", toStr2(s, true).c_str());
    }
    dbg("\n;;\n");
}

void Compiler::dbg_dump_code(const char *code, unsigned length, 
                             const char *name)
{
    if (name != NULL) {
        dbg("; .%s.start\n", name);
    }

    DISFUNC disf = get_disfunc();
    if (disf != NULL) {
    for (unsigned i = 0; i < length; /**/) {
        char buf[1024];
        unsigned bytes = (disf) (code + i, buf, sizeof(buf));
        assert(bytes);  // cant be 0, or we'll fall into infinite loop
        i += bytes ? bytes : 1;
        dbg("\t%s\n", buf);
    }
    }
    else {
        // if disassembler shared library not present, simply 
        // dump out the code
        dbg("\t");
        for (unsigned i = 0; i < length; i++) {
            dbg(" %02X", (unsigned) (unsigned char) code[i]);
            unsigned pos = (i + 1) % 16;
            if (0 == pos) {
                // output by 16 bytes per line
                dbg("\n\t");
            }
            else if (7 == pos) {
                // additional space in the middle of the output
                dbg(" ");
            }
        }
    }
    dbg("\n");
    if (name != NULL) {
        dbg("; .%s.end\n", name);
    }
}

void Compiler::dbg_dump_code(void)
{
    const char * code = (const char*)
                        method_get_code_block_addr_jit(m_method, jit_handle);
    const unsigned codeLen = 
                        method_get_code_block_size_jit(m_method, jit_handle);
    unsigned pc = NOTHING;
    DISFUNC disf = get_disfunc();
    for (unsigned i=0; i<codeLen; /**/) {
        const char * ip = code + i;
        unsigned tmp = m_infoBlock.get_pc(ip);
        if (pc != tmp) {
            // we just passed to a new byte code instruction, print it out
            pc = tmp;
            JInst jinst;
            unsigned next_pc = fetch(pc, jinst);
            dbg(";; %s\n", toStr(jinst, true).c_str());
            // check whether we have a code for this instruction - read next
            // item and compare its IP. If they are the same, then 'jinst' 
            // represents an empty instruction with no real code - NOP, POP, 
            // etc. In this case, switch to the nearest instruction with a 
            // code.
            for(;next_pc<m_infoBlock.get_bc_size();) {
                const char * next_ip = m_infoBlock.get_ip(next_pc);
                if (next_ip != ip) {
                    break;
                }
                next_pc = fetch(next_pc, jinst);
                dbg(";; %s\n", toStr(jinst, true).c_str());
            }
        }
        if (disf) {
            char buf[1024];
            unsigned bytes = disf(code + i, buf, sizeof(buf));
            assert(bytes != 0); // cant be 0
            dbg("%p\t%s\n", code + i, buf);
            i += bytes;
        }
        else {
            i += 1;
        }
    }
}

}
};  // ~namespace Jitrino::Jet
