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
public abstract class Reference<T> {

    private volatile T referent;

    ReferenceQueue<? super T> queue;

    Reference next;

    Reference(T referent) {
        this.referent = referent;
    }

    Reference(T referent, ReferenceQueue<? super T> q) {
        this.queue = q;
        this.referent = referent; 
   }

    /**
     * @com.intel.drl.spec_ref
     */
    public void clear() {
        referent = null;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean enqueue() {
        if (next == null && queue != null) {
            queue.enqueue(this);
            queue = null;
            return true;
        }
        return false;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public T get() {
        return referent;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isEnqueued() {
        return (next != null);
    }

}
