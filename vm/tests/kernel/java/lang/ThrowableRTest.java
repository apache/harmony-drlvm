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
 * @author Elena Semukhina
 * @version $Revision$
 */

package java.lang;

import junit.framework.TestCase;

public class ThrowableRTest extends TestCase {

    private class initCauseExc extends RuntimeException {
        
        private static final long serialVersionUID = 0L;

        initCauseExc(Throwable cause){
            super(cause);
        }
        
        public Throwable initCause(Throwable cause) {
            return cause;
        }
    }

    /*
	 * Test the Throwable(Throwable) constructor when the initCause() method
     * is overloaded
	 */
	public void testThrowableThrowableInitCause() {
        NullPointerException nPE = new NullPointerException();
        initCauseExc iC = new initCauseExc(nPE);
        assertTrue("Assert 0: The cause has not been set",
                   iC.getCause() != null); 
        assertTrue("Assert 1: The invalid cause has been set", 
                   iC.getCause() == nPE);        
    }
}