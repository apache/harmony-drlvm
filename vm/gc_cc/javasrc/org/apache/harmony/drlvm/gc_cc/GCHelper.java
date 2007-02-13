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
 */ 

package org.apache.harmony.drlvm.gc_cc;

import org.apache.harmony.drlvm.VMHelper;
import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;

public class GCHelper {

    static {System.loadLibrary("gc_cc");}

    private static final int TLS_CURRENT_OFFSET = getCurrentOffset();
    private static final int TLS_CLEANED_OFFSET = getCleanedOffset();


    private static final int ARRAY_LEN_OFFSET = 8;
    private static final int GC_OBJECT_ALIGNMENT = 4; //TODO: EM64 or IPF could have 8!


    @Inline
    public static Address alloc(int objSize, int allocationHandle) {
        Address tlsAddr = VMHelper.getTlsBaseAddress();

        Address tlsFreeFieldAddr = tlsAddr.plus(TLS_CURRENT_OFFSET);
        Address tlsCleanedFieldAddr = tlsAddr.plus(TLS_CLEANED_OFFSET);

        Address tlsFreeAddr = tlsFreeFieldAddr.loadAddress();
        Address tlsCleanedAddr = tlsCleanedFieldAddr.loadAddress ();

        Address tlsNewFreeAddr = tlsFreeAddr.plus(objSize);

        // the fast path without cleaning
        if (tlsNewFreeAddr.LE(tlsCleanedAddr)) {
            tlsFreeFieldAddr.store (tlsNewFreeAddr);
            tlsFreeAddr.store(allocationHandle);
            return tlsFreeAddr;
        }
        return VMHelper.newResolvedUsingAllocHandleAndSize(objSize, allocationHandle);    
    }


    @Inline   
    public static Address allocArray(int arrayLen, int elemSize, int allocationHandle) {
        if (arrayLen >= 0) {
            int firstElementOffset = ARRAY_LEN_OFFSET + (elemSize==8?8:4);
            int size = firstElementOffset + elemSize*arrayLen;
            size = (((size + (GC_OBJECT_ALIGNMENT - 1)) & (~(GC_OBJECT_ALIGNMENT - 1))));

            Address arrayAddress = alloc(size, allocationHandle); //never null!
            arrayAddress.store(arrayLen, Offset.fromIntZeroExtend(ARRAY_LEN_OFFSET));
            return arrayAddress;
        }
        return VMHelper.newVectorUsingAllocHandle(arrayLen, allocationHandle);
    }



    private static native int getCurrentOffset();
    private static native int getCleanedOffset();
}
