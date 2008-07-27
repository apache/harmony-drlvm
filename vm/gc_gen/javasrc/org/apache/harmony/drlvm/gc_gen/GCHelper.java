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

/**
 * GCHelper contains GC methods written in Java, that can be jitted by JIT as
 * normal Java code. The GC helper supports common GC functionalities such as
 * allocation and write barriers. The GC module is built into a shared object
 * binary exporting a set of well defined VM/GC interface. Symbols in dynamic
 * shared objects can only be accessed indirectly, which leads to a runtime 
 * overhead of indirect function calling and parameter serializing. The GC
 * helper is designed to solve these boundary crossing problem.
 *
 * @author Xiao-Feng Li
 */
public class GCHelper {

    static {
        if (VMHelper.COMPRESSED_REFS_MODE) {
            System.loadLibrary("gc_gen");
        } else {
            System.loadLibrary("gc_gen_uncomp");
        }
        helperCallback();
    }

    /** Offset from the base pointer of the GC's Thread Local Storage Structure. */
    public static final int TLS_GC_OFFSET = TLSGCOffset();

    /** 
     * Number of Bytes prefetched in advance. The default prefetch distance is 1KB.
     * The prefetch distance can be set using the -XX:gc.prefetch_distance=N JRE
     * Option. N is the number of bytes. 
     */
    public static final int PREFETCH_DISTANCE = getPrefetchDist();    

    /**
     * Number of bytes cleared in forward direction when allocating a new memory block
     * for a thread. The default zeroing size is 2KB. The zeroing size can be set using
     * the -XX:gc.zeroing_size=N JRE Option. N is the number of bytes.
     */
    public static final int ZEROING_SIZE = getZeroingSize();

    /**
     * The number of bytes of prefetch stride. The default prefetch stride is 64 bytes.
     * The prefetch stride can be set using the -XX:gc.prefetch_stride=N JRE Option.
     * N is the number of bytes.
     */
    public static final int PREFETCH_STRIDE = getPrefetchStride();

    /**
     * Offset of member free of the Allocator structure.
     */
    public static final int TLA_FREE_OFFSET = getTlaFreeOffset();

    /**
     * Offset of member ceiling of the Allocator structure.
     */
    public static final int TLA_CEILING_OFFSET = getTlaCeilingOffset();

    /**
     * Offset of member end of the Allocator structure.
     */
    public static final int TLA_END_OFFSET = getTlaEndOffset();

    /**
     * Large Object Size Threshold value. Objects that are of size greater than this
     * threshold value are allocated in LOS (large object space). This value is used
     * to divide the heap into spaces.
     */
    public static final int LARGE_OBJECT_SIZE = getLargeObjectSize();

    /**
     * States whether allocation prefetch is enabled or disabled. Allocation prefetch is
     * disabled by default. However, enabling it may improve application performance.
     * Enabling/Disabling prefetch is done by the -XX:gc.prefetch=true|false JRE Option.
     * To enable prefetching a platform-specific distance ahead when allocating in the
     * Thread Local Allocation buffer, set the -XX:gc.prefetch option to true.
     */
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

    /**
     * Allocates memory requested by a Java Class. This method uses a class handle
     * to obtain the allocation handle and size of object to allocate. Once memory
     * has been allocated for the object, the address at which memory was allocated
     * is returned.
     *
     * @param classHandle The C native struct corresponding to java.lang.Class
     *                    uses a class handle to refer to a specific instance of a
     *                    java.lang.Class. In here we pass in the class handle of
     *                    corresponding java.lang.Class to which we wish to allocate
     *                    memory.
     * @return            Address of the allocated block of memory as seen by the
     *                    java side of this interface.
     * @see               Address
     */
    @Inline
    public static Address alloc(Address classHandle) {
        int objSize = VMHelper.getTypeSize(classHandle);
        int allocationHandle = VMHelper.getAllocationHandle(classHandle);
        return alloc(objSize, allocationHandle);
    }


    private static final int ARRAY_LEN_OFFSET = 8;
    private static final int GC_OBJECT_ALIGNMENT = getGCObjectAlignment();

    /**
     * Allocates memory requested by a Java Array Class or Java Vector Class. This 
     * method uses a element class handle to obtain the handle of the array (or vector)
     * class corresponding to the given element. The handle of the array (or vector) is 
     * used to obtain the allocation handle and the size of an element of the array (or 
     * vector). It then allocates enough memory to hold arrayLen number of elements. If 
     * arrayLen is negative a new Vector is obtained by using the allocation handle and 
     * if not a new Array is obtained. Once memory has been allocated for the objects,
     * the address at which memory was allocated is returned.
     *
     * @param elemClassHandle The C native struct corresponding to java.lang.Class
     *                        uses a class handle to refer to a specific instance of a
     *                        java.lang.Class. In here we pass in the class handle of
     *                        the element of the type which corresponds to the desired
     *                        array (or vector) class.
     * @param arrayLen        If greater than or equal to zero, this indicates the 
     *                        number of elements in the array. Else, it indicates the 
     *                        number of elements in the vector.
     * @return                Address of the allocated block of memory as seen by the
     *                        java side of this interface. This is the address of the 
     *                        array class for a arrayLen greater than or equal to zero,
     *                        or is the address of a vector created using the allocation 
     *                        handle.
     * @see                   Address
     * @see                   VMHelper#newVectorUsingAllocHandle(int, int)
     */
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

    /** Address of Boundary of the NOS (nursery object space). */
    public static Address NOS_BOUNDARY = Address.fromLong(getNosBoundary());

    /** 
     * States whether GC is in Generational Mode.
     * True if GC is in Generational Mode or False if GC is in Non-Generational Mode
     */
    public static boolean GEN_MODE = getGenMode();

    /**
     * Write Barrier for GC. This method is used to update the slot with the value 
     * provided. 
     *
     * @param p_target  This contains the value being written to the given slot.
     * @param p_objSlot Address of a memory location being written to.
     * @param p_objBase Address of the base of the object (or array) being written to.
     */
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






