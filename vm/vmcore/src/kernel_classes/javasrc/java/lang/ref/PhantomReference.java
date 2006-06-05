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
public class PhantomReference extends Reference {

    /**
     * @com.intel.drl.spec_ref 
     */
	public PhantomReference(Object referent, ReferenceQueue q) {
		super(referent, q);
	}
	
    /**
     * @com.intel.drl.spec_ref 
     */
	public Object get() {
		return null;
	}
}
