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
 * @author Xiao-Feng Li
 */ 

package org.apache.harmony.drlvm.gc_gen;

import org.apache.harmony.drlvm.VMHelper;
import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;

public class GCHelper {

    static {
        if (VMHelper.COMPRESSED_REFS_MODE) {
            System.loadLibrary("gc_gen");
        } else {
            System.loadLibrary("gc_gen_uncomp");
        }
        helperCallback();
    }

    public static final int TLS_GC_OFFSET = TLSGCOffset();
    public static final int PREFETCH_DISTANCE = getPrefetchDist();    
    public static final int ZEROING_SIZE = getZeroingSize();
    public static final int PREFETCH_STRIDE = getPrefetchStride();

    public static final int TLA_FREE_OFFSET = getTlaFreeOffset();
    public static final int TLA_CEILING_OFFSET = getTlaCeilingOffset();
    public static final int TLA_END_OFFSET = getTlaEndOffset();

    public static final int LARGE_OBJECT_SIZE = getLargeObjectSize();
    public static final boolean PREFETCH_ENABLED = isPrefetchEnabled();


    @Inline
    private static Address alloc(int objSize, int allocationHandle ) {

	if (objSize > LARGE_OBJECT_SIZE) {
	    return VMHelper.newResolvedUsingAllocHandleAndSize(objSize, allocationHandle);
	}

        Address TLS_BASE = VMHelper.getTlsBaseAddress();

        Address allocator_addr = TLS_BASE.plus(TLS_GC_OFFSET);
        Address allocator = allocator_addr.loadAddress();
        Address free_addr = allocator.plus(TLA_FREE_OFFSET);
        Address free = free_addr.loadAddress();
        Address ceiling_addr = allocator.plus(TLA_CEILING_OFFSET);
        Address ceiling = ceiling_addr.loadAddress();

        Address new_free = free.plus(objSize);

        if (new_free.LE(ceiling)) {
            free_addr.store(new_free);
            free.store(allocationHandle);
            return free;
        } else if (PREFETCH_ENABLED) {
            Address end = allocator.plus(TLA_END_OFFSET).loadAddress();
            if(new_free.LE(end)) {
               // do prefetch from new_free to new_free + PREFETCH_DISTANCE + ZEROING_SIZE
               VMHelper.prefetch(new_free, PREFETCH_DISTANCE + ZEROING_SIZE, PREFETCH_STRIDE);
    
               Address new_ceiling = new_free.plus(ZEROING_SIZE);
	       // align ceiling to 64 bytes		
	       int remainder = new_ceiling.toInt() & 63;
	       new_ceiling = new_ceiling.minus(remainder);
               if( !new_ceiling.LE(end) ){
                   new_ceiling = end;
               }

               VMHelper.memset0(ceiling , new_ceiling.diff(ceiling).toInt());

               ceiling_addr.store(new_ceiling);
               free_addr.store(new_free);
               free.store(allocationHandle);
               return free;
	    }
	}    
                
        return VMHelper.newResolvedUsingAllocHandleAndSize(objSize, allocationHandle);    
    }

    @Inline
    public static Address alloc(Address classHandle) {
        int objSize = VMHelper.getTypeSize(classHandle);
        int allocationHandle = VMHelper.getAllocationHandle(classHandle);
        return alloc(objSize, allocationHandle);
    }


    private static final int ARRAY_LEN_OFFSET = 8;
    private static final int GC_OBJECT_ALIGNMENT = getGCObjectAlignment();

    @Inline
    public static Address allocArray(Address elemClassHandle, int arrayLen) {
        Address arrayClassHandle = VMHelper.getArrayClass(elemClassHandle);
        int allocationHandle = VMHelper.getAllocationHandle(arrayClassHandle);
        if (arrayLen >= 0) {
            int elemSize = VMHelper.getArrayElemSize(arrayClassHandle);
            int firstElementOffset = ARRAY_LEN_OFFSET + (elemSize==8?8:4);
            int size = firstElementOffset + elemSize*arrayLen;
            size = (((size + (GC_OBJECT_ALIGNMENT - 1)) & (~(GC_OBJECT_ALIGNMENT - 1))));

            Address arrayAddress = alloc(size, allocationHandle); //never null!
            arrayAddress.store(arrayLen, Offset.fromIntZeroExtend(ARRAY_LEN_OFFSET));
            return arrayAddress;
        }
        return VMHelper.newVectorUsingAllocHandle(arrayLen, allocationHandle);
    }

    /** NOS (nursery object space) is higher in address than other spaces.
       The boundary currently is produced in GC initialization. It can
       be a constant in future.
    */

    public static Address NOS_BOUNDARY = Address.fromLong(getNosBoundary());
    public static boolean GEN_MODE = getGenMode();

    @Inline
    public static void write_barrier_slot_rem(Address p_target, Address p_objSlot, Address p_objBase) {
      
       /* If the slot is in NOS or the target is not in NOS, we simply return*/
        if(p_objSlot.GE(NOS_BOUNDARY) || p_target.LT(NOS_BOUNDARY) || !GEN_MODE) {
            p_objSlot.store(p_target);
            return;
        }
        Address p_obj_info = p_objBase.plus(4);
        int obj_info = p_obj_info.loadInt();
        if((obj_info & 0x80) != 0){
            p_objSlot.store(p_target);
            return;
        }
         
        VMHelper.writeBarrier(p_objBase, p_objSlot, p_target);
    }

    private static native boolean isPrefetchEnabled();
    private static native int getLargeObjectSize();
    private static native int getTlaFreeOffset(); 
    private static native int getTlaCeilingOffset(); 
    private static native int getTlaEndOffset(); 
    private static native int getGCObjectAlignment(); 
    private static native int getPrefetchDist(); 
    private static native int getZeroingSize(); 
    private static native int getPrefetchStride();
    private static native int helperCallback();
    private static native boolean getGenMode(); 
    private static native long getNosBoundary();    
    private static native int TLSGCOffset();
}






