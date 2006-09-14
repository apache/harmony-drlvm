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
 * @version $Revision: $
 */
 
/**
 * @file
 * @brief Main decoding (disassembling) routines and structures.
 *
 * @note Quick and rough implementation, subject for a change.
 */
 
#ifndef __DEC_BASE_H_INCLUDED__
#define __DEC_BASE_H_INCLUDED__


#include "enc_base.h"
#include "enc_prvt.h"

#define IF_CONDITIONAL  (0x00000000)
#define IF_SYMMETRIC    (0x00000000)
#define IF_BRANCH       (0x00000000)

struct Inst {
    Mnemonic mn;
    unsigned flags;
    unsigned size;
    int      offset;
    void *   direct_addr;
    const EncoderBase::OpcodeDesc * odesc;
};

inline bool is_jcc(Mnemonic mn)
{
    return Mnemonic_JO <= mn && mn<=Mnemonic_JG;
}

class DecoderBase {
public:
    static unsigned decode(const void * addr, Inst * pinst);
private:
    static bool decodeModRM(const EncoderBase::OpcodeDesc& odesc,
                            const unsigned char ** pbuf, Inst * pinst);
    static bool decode_aux(const EncoderBase::OpcodeDesc& odesc,
                        unsigned aux, const unsigned char ** pbuf, 
                        Inst * pinst);
    static bool try_mn(Mnemonic mn, const unsigned char ** pbuf, Inst * pinst);

    static bool is_prefix(const unsigned char * bytes);
};

#endif  // ~ __DEC_BASE_H_INCLUDED__

