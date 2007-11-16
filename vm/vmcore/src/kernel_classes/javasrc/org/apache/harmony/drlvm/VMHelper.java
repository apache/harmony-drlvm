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

package org.apache.harmony.drlvm;

import org.vmmagic.unboxed.Address;
/**
    Core class for DRLVM's vmmagic based helpers.
    Resolved and initilized during VM startup
    
    Note: All classes with vmmagic based helpers registred in VM are also resolved and initialized at VM startup
    Note: If you need to initialize another DRLVM's specific utility class related to vmmagic infrastructure
          refer to it from static section of this class: and it will also be automatically initialized
*/
public class VMHelper {

    private VMHelper() {}
    


    public static Address getTlsBaseAddress() {fail(); return null;}

    //TODO: allocation handle is int only on 32bit OS or (64bit OS && compressed mode)
    public static Address newResolvedUsingAllocHandleAndSize(int objSize, int allocationHandle) {fail(); return null;}
    
    //TODO: allocation handle is int only on 32bit OS or (64bit OS && compressed mode)
    public static Address newVectorUsingAllocHandle(int arrayLen, int allocationHandle) {fail(); return null;}

    public static void monitorEnter(Object obj) {fail();}

    public static void monitorExit(Object obj) {fail();}

    public static void writeBarrier(Address objBase, Address objSlot, Address source) {fail();}

    public static Address getInterfaceVTable(Object obj, Address intfTypePtr) {fail(); return null;}
 
    public static Object checkCast(Object obj, Address castTypePtr) {fail(); return null;}
 
    public static boolean instanceOf(Object obj, Address castTypePtr) {fail(); return false;}

    protected static void fail() {throw new RuntimeException("Not supported!");}


    public static final int POINTER_TYPE_SIZE          = getPointerTypeSize();
    public static final boolean COMPRESSED_REFS_MODE   = isCompressedRefsMode();
    public static final boolean COMPRESSED_VTABLE_MODE = isCompressedVTableMode();
    public static final long COMPRESSED_VTABLE_BASE_OFFSET    = getCompressedModeVTableBaseOffset();
    public static final long COMPRESSED_REFS_OBJ_BASE_OFFSET  = getCompressedModeObjectBaseOffset();

    /** @return pointer-type size. 4 or 8 */
    private static native int getPointerTypeSize();

    /** @return true if VM is run in compressed reference mode */
    private static native boolean isCompressedRefsMode();

    /** @return true if VM is run in compressed vtables mode */
    private static native boolean isCompressedVTableMode();

    /** @return vtable base offset if is in compressed-refs mode or -1*/
    private static native long getCompressedModeVTableBaseOffset();

    /** @return object base offset if is in compressed-refs mode or -1*/
    private static native long getCompressedModeObjectBaseOffset();
}

