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

public class VMHelper {

    private VMHelper() {}
    


    public static Address getTlsBaseAddress() {fail(); return null;}

    public static Address newResolvedUsingAllocHandleAndSize(int objSize, int allocationHandle) {fail(); return null;}
    
    public static Address newVectorUsingAllocHandle(int arrayLen, int elemSize, int allocationHandle) {fail(); return null;}

    public static void monitorEnter(Object obj) {fail();}

    public static void monitorExit(Object obj) {fail();}

    public static void writeBarrier(Address objBase, Address objSlot, Address source) {fail();}

    public static Address getInterfaceVTable(Object obj, int intfTypeId) {fail(); return null;}
 
    public static Object checkCast(Object obj, int castType) {fail(); return null;}
 
    public static boolean instanceOf(Object obj, int castType) {fail(); return false;}


    protected static void fail() {throw new RuntimeException("Not supported!");}

}
