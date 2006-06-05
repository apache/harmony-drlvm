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
 * @author Salikh Zakirov
 * @version $Revision: 1.3.12.1.4.3 $
 */
package gc;

import java.lang.ref.PhantomReference;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;

/**
 * @keyword
 */
public class RefRemove {

    public static void main(String[] args) throws Exception {
        ReferenceQueue queue = new ReferenceQueue();
        Object o = new RefRemove();
        Reference ref = new PhantomReference(o, queue);
        o = null;
        System.gc();
        System.err.println("waiting for a reference..");
        Reference enqueued = queue.remove();
        if (enqueued == ref) {
            System.out.println("PASSED, " + ref);
        } else {
            System.out.println("FAILED, wrong reference enqueued " + ref);
        }
    }
}
