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
import org.vmmagic.unboxed.Address;
import org.vmmagic.unboxed.ObjectReference;
import org.vmmagic.unboxed.Offset;
import org.vmmagic.pragma.InlinePragma;

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
    private static final int CLASS_DEPTH_OFFSET = getClassDepthOffset();

    private static final int POINTER_SIZE=4;//todo: em64 & ipf incompatible!

    private VMHelperFastPath() {}


//TODO: leave only one version 
//TODO: refactor code to use getVtableIntfTableOffset method (+ avoid extra comparisons while refactoring)

    public static Address getInterfaceVTable3(Object obj, int intfType)  {

        Address objAddr = ObjectReference.fromObject(obj).toAddress();
        Address vtableAddr = objAddr.loadAddress(Offset.fromInt(OBJ_VTABLE_OFFSET));

        int inf0Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_0_OFFSET));
        if (inf0Type == intfType) {
            return vtableAddr.loadAddress(Offset.fromInt(CLASS_INF_TABLE_0_OFFSET));               
        }
    
        int inf1Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_1_OFFSET));
        if (inf1Type == intfType) {
            return vtableAddr.loadAddress(Offset.fromInt(CLASS_INF_TABLE_1_OFFSET));               
        }

        int inf2Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_2_OFFSET));
        if (inf2Type == intfType) {
            return vtableAddr.loadAddress(Offset.fromInt(CLASS_INF_TABLE_2_OFFSET));               
        }
        //slow path version
        return VMHelper.getInterfaceVTable(obj, intfType);
    }

    public static Address getInterfaceVTable2(Object obj, int intfType)  {
        Address objAddr = ObjectReference.fromObject(obj).toAddress();
        Address vtableAddr = objAddr.loadAddress(Offset.fromInt(OBJ_VTABLE_OFFSET));

        int inf0Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_0_OFFSET));
        if (inf0Type == intfType) {
            return vtableAddr.loadAddress(Offset.fromInt(CLASS_INF_TABLE_0_OFFSET));               
        }
    
        int inf1Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_1_OFFSET));
        if (inf1Type == intfType) {
            return vtableAddr.loadAddress(Offset.fromInt(CLASS_INF_TABLE_1_OFFSET));               
        }

        //slow path version
        return VMHelper.getInterfaceVTable(obj, intfType);
    }

    public static Address getInterfaceVTable1(Object obj, int intfType)  {
        Address objAddr = ObjectReference.fromObject(obj).toAddress();
        Address vtableAddr = objAddr.loadAddress(Offset.fromInt(OBJ_VTABLE_OFFSET));

        int inf0Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_0_OFFSET));
        if (inf0Type == intfType) {
            return vtableAddr.loadAddress(Offset.fromInt(CLASS_INF_TABLE_0_OFFSET));               
        }
    
        //slow path version
        return VMHelper.getInterfaceVTable(obj, intfType);
    }

    public static boolean instanceOf(Object obj, int castType, boolean isArray, boolean isInterface, boolean isFinalTypeCast, int fastCheckDepth) throws InlinePragma {
        if (obj == null) {
            return false;
        }
        if (isInterface) {
            Address objAddr = ObjectReference.fromObject(obj).toAddress();
            Address vtableAddr = objAddr.loadAddress(Offset.fromInt(OBJ_VTABLE_OFFSET));


            int inf0Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_0_OFFSET));
            if (inf0Type == castType) {
                return true;
            }
    
            int inf1Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_1_OFFSET));
            if (inf1Type == castType) {
                return true;
            }
        } else if (!isArray && fastCheckDepth!=0) {
            return fastClassInstanceOf(obj, castType, isFinalTypeCast, fastCheckDepth);
        } 
        return VMHelper.instanceOf(obj, castType);
        
    }

    public static Object checkCast(Object obj, int castType, boolean isArray, boolean isInterface, boolean isFinalTypeCast, int fastCheckDepth) throws InlinePragma {
        if (obj == null) {
            return obj;
        }
        if (isInterface) {
            Address objAddr = ObjectReference.fromObject(obj).toAddress();
            Address vtableAddr = objAddr.loadAddress(Offset.fromInt(OBJ_VTABLE_OFFSET));


            int inf0Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_0_OFFSET));
            if (inf0Type == castType) {
                return obj;
            }
    
            int inf1Type = vtableAddr.loadInt(Offset.fromInt(CLASS_INF_TYPE_1_OFFSET));
            if (inf1Type == castType) {
                return obj;
            }
        } else if (!isArray && fastCheckDepth!=0 && fastClassInstanceOf(obj, castType, isFinalTypeCast, fastCheckDepth)) {
            return obj;
        } 
        return VMHelper.checkCast(obj, castType);
    }

    public static boolean fastClassInstanceOf(Object obj, int castType, boolean isFinalTypeCast, int fastCheckDepth) throws InlinePragma {
        Address objAddr = ObjectReference.fromObject(obj).toAddress();
        Address objVtableAddr = objAddr.loadAddress(Offset.fromInt(OBJ_VTABLE_OFFSET));
        int objClassType  = objVtableAddr.loadInt(Offset.fromInt(VTABLE_CLASS_OFFSET));//todo em64t & ipf incompat

        if (objClassType == castType) {
            return true;
        }
        if (isFinalTypeCast) {
            return false;
        }
       
        int depthSubType = objVtableAddr.loadInt(Offset.fromInt(VTABLE_SUPERCLASSES_OFFSET + POINTER_SIZE*(fastCheckDepth-1)));
        return depthSubType == castType;
    }


    private static native int getObjectVtableOffset();
    private static native int getVtableIntfTypeOffset(int n);
    private static native int getVtableIntfTableOffset(int n);
    private static native int getVtableClassOffset();
    private static native int getVtableSuperclassesOffset();
    private static native int getClassDepthOffset();
}
