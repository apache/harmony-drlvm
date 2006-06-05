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
public abstract class Reference {

    volatile Object referent;

    ReferenceQueue queue;

    Reference next;

    Reference(Object referent) {
        initReference(referent);
    }

    Reference(Object referent, ReferenceQueue q) {
        this.queue = q;
        initReference(referent);
   }

    native void initReference(Object referent);

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
    public Object get() {
        return referent;
    }

    /**
     * @com.intel.drl.spec_ref
     */
    public boolean isEnqueued() {
        return (next != null);
    }

}
