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
 * @version $Revision: 1.3.12.3.4.4 $
 */
 
/**
 * @file
 * @brief Pltform-independent code generator's stuff.
 */
 
#if !defined(__CG_H_INCLUDED__)
#define __CG_H_INCLUDED__

namespace Jitrino { namespace Jet {

/**
 * @brief Dynamically grown byte array.
 * 
 * Class CodeStream represents a dynamically growing byte array, which 
 * always provides buffer of at least minimal guaranteed size (which is 
 * #BUFFER_ZONE).
 *
 * The usage is as follows:
 * @code
 *      CodeStream cs;
 *      cs.init(INITIALLY_EXPECTED_CODE_SIZE);
 *      unsigned p_start = cs.ipoff();
 *      // get buffer to write into
 *      char * p = cs.ip();
 *      // use the buffer
 *      memcpy(p, otherP, someSize_less_than_BUFFER_ZONE);
 *      // let know how many bytes were used
 *      cs.ip(p + someSize_less_than_BUFFER_ZONE);
 *      ...
 *      unsigned next_p = cs.ipoff();
 * @endcode
 */
class CodeStream {
public:
    /**
     * @brief ctor.
     */
    CodeStream()
    {
        m_buf = NULL;
        m_size = 0;
    }
    /**
     * @brief Frees allocated memory.
     */
    ~CodeStream()
    {
        if (m_buf) {
            free(m_buf);
        }
    }

    /**
     * @brief Performs initial memory allocation. 
     *
     * The memory size allocated is 'bytes' and then grow when necessary.
     */
    void    init(unsigned bytes)
    {
        resize(bytes);
    }


    /**
     * @brief Returns address of a next available for writing byte.
     *
     * The address returned is guaranteed to contain at least #BUFFER_ZONE 
     * bytes. 
     * @note The address returned is valid only till the next call to 
     * #ip(void) where a memory reallocation can happen. So the address
     * must be used only for call to #ip(char*). 
     * #ipoff() must be used to store offsets, which are consistent during 
     * the lifetime of CodeStream object.
     */
    char *  ip(void) {
        if ((total_size - m_size) < BUFFER_ZONE) {
            resize(total_size + total_size*GROW_RATE/100);
        }
        return m_buf + m_size;
    }

    /**
     * @brief Sets current address. This must be an address of a next 
     *        available byte.
     *
     * The address is used to determine how many bytes were used since the
     * last call of #ip(void) and thus, how many free bytes left.
     */
    void    ip(char * _ip)
    {
        m_size = _ip - m_buf;
        assert(m_size < total_size);
    }

    /**
     * @brief Returns an offset the next available byte in the stream.
     *
     * #data() + #ipoff() gets the address.
     */
    unsigned    ipoff(void) const
    {
            return m_size;
    }

    /**
     * @brief Returns the size currently used in the stream.
     */
    unsigned    size(void) const
    {
        return m_size;
    }

    /**
     * @brief Provides a direct access to internal buffer.
     * 
     * @note Never use more than #size() bytes.
     */
    char *  data(void) const
    {
        return m_buf;
    }

    /**
     * @brief The minimum guaranteed size of the buffer returned by ip().
     * 
     * This is also a minimal size of the buffer which triggers 
     * reallocation of a bigger memory buf.
     * '16' here is max size of the native instruction (on IA32/EM64T).
     * 3 was chosen heuristically.
     */
    static const unsigned   BUFFER_ZONE = 16*3;
private:
    /**
     * @brief [Re-]allocates memory.
     * 
     * The previously filled memory (if any) is copied into the newly 
     * allocated buffer.
     */
    void resize(unsigned how_much)
    {
        total_size = how_much;
        m_buf = (char*)realloc(m_buf, total_size);
    }

    /**
     * @brief Pointer to the allocated buffer.
     */
    char *  m_buf;

    /**
     * @brief Total size of the allocated buffer.
     */
    unsigned    total_size;

    /**
     * @brief Size of buffer currently in use.
     */
    unsigned    m_size;

    /**
     * @brief A rate how to increase the already allocated buffer, in 
     *        percent.
     *
     * The default value 25 means that the buffer will grow by 25% each
     * allocation:
     * i.e. if the first size passed to #init() was 32, the allocations 
     * will be: 32 ; 40 (=32+32*0.25) ; 50 (=40+50*0.25) etc.
     */
    static const unsigned   GROW_RATE = 25;
};

/**
 * @brief Info used to identify a native instruction for patching.
 *
 * An instance of CodePatchItem used for 'code patching'. The code patching
 * is a process of updating a code after it has been generated. For 
 * example, on IA32/EM64T a native instruction 'CALL offset' specifies an 
 * offset to the target relative to the instruction's IP. Thus, the real 
 * offset can be counted only when the instruction is placed on its real
 * location (in the current generation scheme - after the code layout).
 * 
 * The CodePatchItem instance carries info needed to find a native 
 * instruction after its repositioning (after code layout) and also some
 * other info to calculate a data to be written.
 *
 * #bb and #bboff are used to identify the native instruction to be 
 * patched, other fields hold instruction-specific info.
 */

struct CodePatchItem {
    /**
     * @brief Points to the byte code basic block, the instruction belongs 
     *        to.
     */
    unsigned    bb;

    /**
     * @brief An offset of the native code inside the basic block.
     */
    unsigned    bboff;
    /**
     * @brief PC of the byte code instruction.
     */
    unsigned    pc;
    /**
     * @brief The real IP of the instruction. Only valid during 
     *        gen_patch() call.
     */
    char *      ip;

    /**
     * @brief Length of native instruction.
     */
    unsigned    instr_len;

    /**
     * @brief Target pc (if applicable).
     */
    unsigned    target_pc;
    
    /**
     * @brief Shows whether address to calculate from #target_pc must be 
     *        relative to instruction's to patch address or not.
     *
     * @note Only applicable to #target_pc.
     */
    bool relative;

    /**
     * @brief Address of target (if applicable).
     */
    char *      target_ip;
    
    /**
     * @brief Offset of target.
     */
    int         target_offset;

    /**
    * @brief Offset between instruction and its target.
    * 
    * @note Only used when instruction and its target are in the same 
    *       basic block !
    */
    unsigned    target_ipoff;

    /**
     * @brief Address for data patching (used in lazy resolution scheme).
     */
    char *      data_addr;

    /**
     * @brief A key for data patching (used in lazy resolution scheme).
     */
    unsigned    data;
    /**
     * @brief \b true if this item refers to TABLESWITCH instruction 
     *        implementation, which requires additional allocation of table.
     */
     bool   is_table_switch;
};


/**
 * @brief VM constants available at runtime - helper addresses, offsets,
 *        etc.
 *
 * The class acts mostly as a namespace around the constants. It's not 
 * supposed to create instances of the class, thus all members are static.
 *
 * The names are quite self-descriptive.
 */
struct VMRuntimeConsts {
public:
    static char *       rt_helper_throw;
    static char *       rt_helper_throw_out_of_bounds;
    static char *       rt_helper_throw_linking_exc;
    static char *       rt_helper_throw_npe;
    static char *       rt_helper_throw_div_by_zero_exc;

    static char *       rt_helper_new;
    static char *       rt_helper_new_array;
    static char *       rt_helper_aastore;

    static char *       rt_helper_monitor_enter;
    static char *       rt_helper_monitor_exit;
    static char *       rt_helper_monitor_enter_static;
    static char *       rt_helper_monitor_exit_static;

    static char *       rt_helper_ldc_string;
    static char *       rt_helper_init_class;
    static char *       rt_helper_multinewarray;
    static char *       rt_helper_get_vtable;
    static char *       rt_helper_checkcast;
    static char *       rt_helper_instanceof;

    static char*        rt_helper_ti_method_exit;
    static char*        rt_helper_ti_method_enter;

    static char*        rt_helper_gc_safepoint;
    static char*        rt_helper_get_thread_suspend_ptr;

    static unsigned     rt_array_length_offset;
    static unsigned     rt_suspend_req_flag_offset;

protected:
    /**
     * @brief Noop.
     */
    VMRuntimeConsts(void) {};
private:
    /**
     * @brief Disallow copying.
     */
    VMRuntimeConsts(const VMRuntimeConsts&);
    
    /**
     * @brief Disallow copying.
     */
    const VMRuntimeConsts& operator=(const VMRuntimeConsts&);
};

}}; // ~namespace Jitrino::Jet



#endif  // ~ __CG_H_INCLUDED__
