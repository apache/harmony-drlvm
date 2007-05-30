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
* @author Intel, Vitaly N. Chaiko
* @version $Revision: 1.6.14.1.4.5 $
*/

#ifndef _IA32_BC_MAP_H_
#define _IA32_BC_MAP_H_

#include "Stl.h"
#include "MemoryManager.h"
#include "Ia32IRManager.h"

namespace Jitrino {

namespace Ia32 {

const static int byteCodeOffsetSize = sizeof(POINTER_SIZE_INT); // byteCodeAddrSize should be 2, 4 will allow easy mem alignment

typedef StlHashMap<POINTER_SIZE_INT, uint16>      BCByNCMap;
/**
   * Bcmap is simple storage with precise mapping between native address to
   * byte code, i.e. if there is no byte code for certain native address then
   * invalid value is returned.
   */

class BcMap {
public:
    BcMap(MemoryManager& memMgr) : theMap(memMgr) {}

    POINTER_SIZE_INT getByteSize() {
        POINTER_SIZE_INT mapSize = (POINTER_SIZE_INT)theMap.size();
        //TODO: use only 2 byte to keep BC offset but not 4 nor 8!!
        return  (mapSize * (byteCodeOffsetSize + sizeof(POINTER_SIZE_INT)) + sizeof(POINTER_SIZE_INT));
    }

    void write(Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)output;
        BCByNCMap::const_iterator citer;
        POINTER_SIZE_INT mapSize;
        POINTER_SIZE_INT i = 0;

        mapSize = (POINTER_SIZE_INT)theMap.size();
        data[0] = mapSize; //store map size
        data = data + 1;

        for (citer = theMap.begin(); citer != theMap.end(); citer++) {
            data[i*2] = (POINTER_SIZE_INT)citer->first;  // write key i.e. native addr
            data[i*2+1] = (POINTER_SIZE_INT)citer->second;  // write value i.e. bc offset
            i++;
        }
        return;
    }

    POINTER_SIZE_INT readByteSize(const Byte* input) const {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)input;
        POINTER_SIZE_INT sizeOfMap = data[0];

        return (sizeOfMap * (byteCodeOffsetSize + sizeof(POINTER_SIZE_INT)) + sizeof(POINTER_SIZE_INT));
    }

    /** read is deprecated method since creating HashMap is too cost */
    void read(const Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)output;
        POINTER_SIZE_INT mapSize;
        POINTER_SIZE_INT i = 0;

        mapSize = data[0]; //read map size
        data = data + 1;

        for (i = 0; i < mapSize; i++) {
            POINTER_SIZE_INT ncAddr = data[i * 2];
            uint16 bcOffset = (uint16)data[i * 2 + 1];
            setEntry(ncAddr, bcOffset);  // read key i.e. native addr and read value i.e. bc offset
        }
        return;
    }

    void writeZerroSize(Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)(output);
        data[0] = 0;

        return;
    }
    void setEntry(POINTER_SIZE_INT key, uint16 value) {
        theMap[key] =  value;
    }

    
    static uint16 get_bc_location_for_native(POINTER_SIZE_INT ncAddress, Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)output;
        POINTER_SIZE_INT mapSize;
        POINTER_SIZE_INT i = 0;

        mapSize = data[0]; //read map size 
        data = data + 1;

        for (i = 0; i < mapSize; i++) {
            POINTER_SIZE_INT ncAddr = data[i * 2];
            uint16 bcOffset = (uint16)data[i * 2 + 1];
            if (ncAddr == ncAddress) return bcOffset;
        }
        return ILLEGAL_BC_MAPPING_VALUE;
    }

    static POINTER_SIZE_INT get_native_location_for_bc(uint16 bcOff, Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)output;
        POINTER_SIZE_INT mapSize;
        POINTER_SIZE_INT i = 0;

        mapSize = data[0]; //read map size 
        data = data + 1;

        POINTER_SIZE_INT ncAddress = 0xdeadbeef;

        for (i = 0; i < mapSize; i++) {
            POINTER_SIZE_INT ncAddr = data[i * 2];
            uint16 bcOffset = (uint16)data[i * 2 + 1];
            if (bcOffset == bcOff) ncAddress = ncAddr;
        }

        return ncAddress;
    }

private:
    BCByNCMap theMap;
};

}} //namespace

#endif /* _IA32_BC_MAP_H_ */
