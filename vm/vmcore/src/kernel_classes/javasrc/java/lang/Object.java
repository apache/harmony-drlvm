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
 * @author Roman S. Bushmanov
 * @version $Revision: 1.1.2.1.4.3 $
 */

 package java.lang;

/**
 * @com.intel.drl.spec_ref
 *
 */
public class Object {

	public final Class getClass() {
		return VMClassRegistry.getClass(this);
	}

	public int hashCode() {
		return VMMemoryManager.getIdentityHashCode(this);
	}

	public boolean equals(Object object) {
		return this == object;
	}

	protected Object clone() throws CloneNotSupportedException {
		if (!(this instanceof Cloneable)) {
			throw new CloneNotSupportedException(
					"Doesn't implement Cloneable interface!");
		}
		return VMMemoryManager.clone(this);
	}

	public String toString() {
		return getClass().getName() + '@' + Integer.toHexString(hashCode());
	}

	public final void notify() {
		VMThreadManager.notify(this);
	}

	public final void notifyAll() {
		VMThreadManager.notifyAll(this);
	}

	public final void wait(long millis, int nanos) throws InterruptedException {
		if(millis < 0 || nanos < 0 || nanos > 999999 ){
			throw new IllegalArgumentException("Arguments don't match the expected range!");
		}
		VMThreadManager.wait(this, millis, nanos);
	}

	public final void wait(long millis) throws InterruptedException {
		wait(millis, 0);
	}

	public final void wait() throws InterruptedException {
		VMThreadManager.wait(this, 0, 0);
	}

	protected void finalize() throws Throwable {
	}
}