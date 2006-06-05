/*
 *  Copyright 2005-2006 The Apache Software Foundation or its licensors, as applicable.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
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
 * @author Sergey V. Dmitriev
 * @version $Revision: 1.1.2.1.4.4 $
 */
package java.util.concurrent.locks;

import java.util.Hashtable;
import java.util.Vector;

public class LockSupport {

    /**
     * Thread that have permits.
     */
    static Vector permits = new Vector();
    
    /**
     * Mapping of parked threads:
     * thread -> TinyLock(ACTIVE or PARKED)
     */
    static Hashtable parked = new Hashtable();
    
    /**
     * @com.intel.drl.spec_ref
     */
    public static native void unpark(Thread thread);    
    
	/**
     * @com.intel.drl.spec_ref
     */
    public static native void park();    
    
	/**
     * @com.intel.drl.spec_ref
     */
    public static native void parkNanos(long nanos);    
    
	/**
     * @com.intel.drl.spec_ref
     */
    public static native void parkUntil(long deadline);
    
	/**
     * Class used in parked mapping: thread -> TinyLock.
     */
    static class TinyLock {
        public static final int ACTIVE = 0;
        public static final int PARKED = 1;
        
        public volatile int value;
        
        public TinyLock(int value) {
            this.value = value;
        }
    }
}

