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

import org.apache.harmony.drlvm.VMHelper;
import org.vmmagic.unboxed.*;
import org.vmmagic.pragma.*;

public class VMHelperFastPath {

    private static final int OBJ_VTABLE_OFFSET   = getObjectVtableOffset();
    private static final int VTABLE_CLASS_OFFSET = getVtableClassOffset();
    private static final int VTABLE_SUPERCLASSES_OFFSET = getVtableSuperclassesOffset();
    private static final int CLASS_INF_TYPE_0_OFFSET  = getVtableIntfTypeOffset(0);
    private static final int CLASS_INF_TABLE_0_OFFSET = getVtableIntfTableOffset(0);
    private static final int CLASS_INF_TYPE_1_OFFSET  = getVtableIntfTypeOffset(1);
    private static final int CLASS_INF_TABLE_1_OFFSET = getVtableIntfTableOffset(1);
    private static final int CLASS_INF_TYPE_2_OFFSET  = getVtableIntfTypeOffset(2);
    private static final int CLASS_INF_TABLE_2_OFFSET = getVtableIntfTableOffset(2);

    private VMHelperFastPath() {}

    @Inline
    public static Address getVTableAddress(Object obj) {
        Address objAddr = ObjectReference.fromObject(obj).toAddress();
        if (VMHelper.COMPRESSED_VTABLE_MODE) {
            int vtableOffset = objAddr.loadInt(Offset.fromIntZeroExtend(OBJ_VTABLE_OFFSET));
            Address res = Address.fromLong(VMHelper.COMPRESSED_VTABLE_BASE_OFFSET + vtableOffset);
            return res;
        }
        return objAddr.loadAddress(Offset.fromIntZeroExtend(OBJ_VTABLE_OFFSET));
        
    }


//TODO: leave only one version 
//TODO: refactor code to use getVtableIntfTableOffset method (+ avoid extra comparisons while refactoring)

    @Inline
    public static Address getInterfaceVTable3(Object obj, Address intfType)  {
        Address vtableAddr = getVTableAddress(obj);

        Address inf0Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_0_OFFSET));
        if (inf0Type.EQ(intfType)) {
            return vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TABLE_0_OFFSET));               
        }
    
        Address inf1Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_1_OFFSET));
        if (inf1Type.EQ(intfType)) {
            return vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TABLE_1_OFFSET));               
        }

        Address inf2Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_2_OFFSET));
        if (inf2Type.EQ(intfType)) {
            return vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TABLE_2_OFFSET));               
        }
        //slow path version
        return VMHelper.getInterfaceVTable(obj, intfType);
    }

    @Inline
    public static Address getInterfaceVTable2(Object obj, Address intfType)  {
        Address vtableAddr = getVTableAddress(obj);

        Address inf0Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_0_OFFSET));
        if (inf0Type.EQ(intfType)) {
            return vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TABLE_0_OFFSET));               
        }
    
        Address inf1Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_1_OFFSET));
        if (inf1Type.EQ(intfType)) {
            return vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TABLE_1_OFFSET));               
        }

        //slow path version
        return VMHelper.getInterfaceVTable(obj, intfType);
    }

    @Inline
    public static Address getInterfaceVTable1(Object obj, Address intfType)  {
        Address vtableAddr = getVTableAddress(obj);

        Address inf0Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_0_OFFSET));
        if (inf0Type.EQ(intfType)) {
            return vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TABLE_0_OFFSET));               
        }
    
        //slow path version
        return VMHelper.getInterfaceVTable(obj, intfType);
    }

    @Inline
    public static boolean instanceOf(Object obj, Address castType, boolean isArray, boolean isInterface, boolean isFinalTypeCast, int fastCheckDepth) {
        if (obj == null) {
            return false;
        }
        if (isInterface) {
            Address vtableAddr = getVTableAddress(obj);

            Address inf0Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_0_OFFSET));
            if (inf0Type.EQ(castType)) {
                return true;
            }
    
            Address inf1Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_1_OFFSET));
            if (inf1Type.EQ(castType)) {
                return true;
            }
        } else if (!isArray && fastCheckDepth!=0) {
            return fastClassInstanceOf(obj, castType, isFinalTypeCast, fastCheckDepth);
        } 
        return VMHelper.instanceOf(obj, castType);
        
    }

    @Inline
    public static Object checkCast(Object obj, Address castType, boolean isArray, boolean isInterface, boolean isFinalTypeCast, int fastCheckDepth) {
        if (obj == null) {
            return obj;
        }
        if (isInterface) {
            Address vtableAddr = getVTableAddress(obj);

            Address inf0Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_0_OFFSET));
            if (inf0Type.EQ(castType)) {
                return obj;
            }
    
            Address inf1Type = vtableAddr.loadAddress(Offset.fromIntZeroExtend(CLASS_INF_TYPE_1_OFFSET));
            if (inf1Type.EQ(castType)) {
                return obj;
            }
        } else if (!isArray && fastCheckDepth!=0 && fastClassInstanceOf(obj, castType, isFinalTypeCast, fastCheckDepth)) {
            return obj;
        } 
        return VMHelper.checkCast(obj, castType);
    }

    @Inline
    public static boolean fastClassInstanceOf(Object obj, Address castType, boolean isFinalTypeCast, int fastCheckDepth) {
        Address objVtableAddr = getVTableAddress(obj);
        Address objClassType  = objVtableAddr.loadAddress(Offset.fromIntZeroExtend(VTABLE_CLASS_OFFSET));

        if (objClassType.EQ(castType)) {
            return true;
        }
        if (isFinalTypeCast) {
            return false;
        }
       
        int subTypeOffset = VTABLE_SUPERCLASSES_OFFSET + VMHelper.POINTER_TYPE_SIZE*(fastCheckDepth-1);
        Address depthSubType = objVtableAddr.loadAddress(Offset.fromIntZeroExtend(subTypeOffset));
        return depthSubType.EQ(castType);
    }


    private static native int getObjectVtableOffset();
    private static native int getVtableIntfTypeOffset(int n);
    private static native int getVtableIntfTableOffset(int n);
    private static native int getVtableClassOffset();
    private static native int getVtableSuperclassesOffset();
}
