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
 * @author Pavel Afremov, Vera Volynets
 * @version $Revision: 1.3.12.1.4.3 $
 */  
 
package perf;

/**
 * @keyword perf exc
 */
public class ThrowMany {

    private final static int MAX_THROW = 100000; 
        
    static class TestLazyException extends Exception {
        public static final long serialVersionUID = 0L;
    }
    
    private final static TestLazyException testLazyException = new TestLazyException();

    private void runTest() {
	for (int i = 0; i < MAX_THROW; i++) {
	    try {
                throw testLazyException;
            } catch (TestLazyException tle) {}
        }

    }

     
    public static void main(String argv[]) {
	ThrowMany test = new ThrowMany();
	test.runTest();
	System.out.println("PASSED");
    }
}
