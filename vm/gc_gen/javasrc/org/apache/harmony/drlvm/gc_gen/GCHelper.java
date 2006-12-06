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

public class GCHelper {

    static {System.loadLibrary("gc_gen");}

    public static final int TLS_GC_OFFSET = TLSGCOffset();

    public static Address alloc(int objSize, int allocationHandle) {
  
        Address TLS_BASE = VMHelper.getTlsBaseAddress();

        Address allocator_addr = TLS_BASE.plus(TLS_GC_OFFSET);
        Address allocator = allocator_addr.loadAddress();
        Address free_addr = allocator.plus(0);
        Address free = free_addr.loadAddress();
        Address ceiling = allocator.plus(4).loadAddress();
        
        Address new_free = free.plus(objSize);

        if (new_free.LE(ceiling)) {
            free_addr.store(new_free);
            free.store(allocationHandle);
            return free;
        }
                
        return VMHelper.newResolvedUsingAllocHandleAndSize(objSize, allocationHandle);    
    }
    
    private static native int TLSGCOffset();
}

