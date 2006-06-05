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
 * @author Dmitry B. Yershov
 * @version $Revision: 1.1.2.1.4.3 $
 */

package java.lang.ref;

/**
 * @com.intel.drl.spec_ref 
 */
public class ReferenceQueue extends Object {

    private Reference firstReference;

    /**
     * @com.intel.drl.spec_ref 
     */
    public ReferenceQueue() {
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized Reference poll() {
        if (firstReference == null)
            return null;
        Reference ref = firstReference;
        firstReference = (firstReference.next == firstReference ? null
                : firstReference.next);
        ref.next = null;
        return ref;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public synchronized Reference remove(long timeout)
            throws IllegalArgumentException, InterruptedException {
        if (firstReference == null)
            wait(timeout);
        if (firstReference == null)
            return null;
        Reference ref = firstReference;
        firstReference = (firstReference.next == firstReference ? null
                : firstReference.next);
        ref.next = null;
        return ref;
    }

    /**
     * @com.intel.drl.spec_ref 
     */
    public Reference remove() throws InterruptedException {
        return remove(0L);
    }

    synchronized boolean enqueue(Reference ref) {
        ref.next = (firstReference == null ? ref : firstReference);
        firstReference = ref;
        notify();
        return true;
    }
}
