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
/**
   * Bcmap is simple storage with precise mapping between native address to
   * byte code, i.e. if there is no byte code for certain native address then
   * invalid value is returned.
   */

class BcMap {
public:
    BcMap() {}
    BcMap(MemoryManager& memMgr) {
        theMap = new(memMgr) StlHashMap<uint64, uint64>(memMgr);
#ifdef _DEBUG
        revMultiMap = new(memMgr) StlHashMultiMap<uint64, uint64>(memMgr);
#endif
    }

    POINTER_SIZE_INT getByteSize() {
        POINTER_SIZE_INT mapSize = (POINTER_SIZE_INT)theMap->size();

        return  (mapSize * (byteCodeOffsetSize + wordSize) + wordSize);
    }

    void write(Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)output;
        StlHashMap<uint64, uint64>::const_iterator citer;
        POINTER_SIZE_INT mapSize;
        POINTER_SIZE_INT i = 0;

        mapSize = (POINTER_SIZE_INT)theMap->size();
        data[0] = mapSize; //store map size
        data = data + 1;

        for (citer = theMap->begin(); citer != theMap->end(); citer++) {
            data[i*2] = (POINTER_SIZE_INT)citer->first;  // write key i.e. native addr
            data[i*2+1] = (POINTER_SIZE_INT)citer->second;  // write value i.e. bc offset
            i++;
        }
        return;
    }

    POINTER_SIZE_INT readByteSize(const Byte* input) const {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)input;
        POINTER_SIZE_INT sizeOfMap = data[0];

        return (sizeOfMap * (byteCodeOffsetSize + wordSize) + wordSize);
    }
    /** read is deprecated method since creating HashMap is too cost */
    void read(const Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)output;
        POINTER_SIZE_INT mapSize;
        POINTER_SIZE_INT i = 0;

        mapSize = data[0]; //read map size
        data = data + 1;

        for (i = 0; i < mapSize; i++) {
            uint64 ncAddr, bcOffset;
            ncAddr = data[i * 2];
            bcOffset = data[i * 2 + 1];
            setEntry(ncAddr, bcOffset);  // read key i.e. native addr and read value i.e. bc offset
        }
        return;
    }

    void writeZerroSize(Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)(output);
        data[0] = 0;

        return;
    }
    void setEntry(uint64 key, uint64 value) {
        (*theMap)[key] =  value;
#ifdef _DEBUG
        revMultiMap->insert(IntPair(value, key));
#endif
    }

    /** this method is deprecated  since creating HashMap is too cost */
    uint64 get_bc_location_for_native_prev(uint64 ncAddr) {
        StlHashMap<uint64, uint64>::const_iterator citer = theMap->find(ncAddr);
        if ( citer!= theMap->end()) {
            return citer->second;
        } else return ILLEGAL_VALUE;
    }

    static uint64 get_bc_location_for_native(uint64 ncAddress, Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)output;
        POINTER_SIZE_INT mapSize;
        POINTER_SIZE_INT i = 0;

        mapSize = data[0]; //read map size 
        data = data + 1;

        for (i = 0; i < mapSize; i++) {
            uint64 ncAddr, bcOffset;
            ncAddr = data[i * 2];
            bcOffset = data[i * 2 + 1];
            if (ncAddr == ncAddress) return bcOffset;
        }
        return ILLEGAL_VALUE;
    }

    static uint64 get_native_location_for_bc(uint64 bcOff, Byte* output) {
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)output;
        POINTER_SIZE_INT mapSize;
        POINTER_SIZE_INT i = 0;

        mapSize = data[0]; //read map size 
        data = data + 1;

        uint64 ncAddress = ILLEGAL_VALUE;

        for (i = 0; i < mapSize; i++) {
            POINTER_SIZE_INT ncAddr, bcOffset;
            ncAddr = data[i * 2];
            bcOffset = data[i * 2 + 1];
            if (bcOffset == bcOff) ncAddress = ncAddr;
        }

        return ncAddress;
    }

    /** this method is deprecated  since creating HashMap is too cost */
    uint64 get_native_location_for_bc_prev(uint64 bcOff, Byte* output) {
        uint64 ncAddress = ILLEGAL_VALUE;
#ifdef _DEBUG
        POINTER_SIZE_INT* data = (POINTER_SIZE_INT*)output;
        POINTER_SIZE_INT mapSize;
        POINTER_SIZE_INT i = 0;

        mapSize = data[0]; //read map size
        data = data + 1;

        for (i = 0; i < mapSize; i++) {
            POINTER_SIZE_INT ncAddr, bcOffset;
            ncAddr = data[i * 2];
            bcOffset = data[i * 2 + 1];
            setEntry(ncAddr, bcOffset);  // read key i.e. native addr and read value i.e. bc offset
        }

        StlHashMultiMap<uint64, uint64>::const_iterator citer = revMultiMap->find(bcOff);
        for ( citer = revMultiMap->begin(); citer != revMultiMap->end(); citer++) {
            if (ncAddress > citer->second) ncAddress = citer->second;
        }
#endif
        return ncAddress;
    }

    bool isBcOffsetExist(uint64 bcOff) {
#ifdef _DEBUG
        if (revMultiMap->has(bcOff)) return true;
#endif
        return false;
    }
protected:
private:
    POINTER_SIZE_INT sizeInBytes;
    POINTER_SIZE_INT mapSize;
    StlHashMap<uint64, uint64>* theMap;
    StlHashMultiMap<uint64, uint64>* revMultiMap;
    const static int wordSize = sizeof(POINTER_SIZE_INT); // 4 bytes for ia32
    const static int byteCodeOffsetSize = sizeof(POINTER_SIZE_INT); // byteCodeAddrSize should be 2, 4 will allow easy mem alignment
    typedef ::std::pair <uint64, uint64> IntPair;
};

}} //namespace

#endif /* _IA32_BC_MAP_H_ */
