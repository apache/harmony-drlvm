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
 * @version $Revision: 1.7.20.1.4.4 $
 */  
package outofmemory;

/**
 * Out of memory tests have no dependecy on class library.
 * @keyword slow
 */
public class Vector {
    public static void main (String[] args) {
        int size = 1000*1048576; // a little less than 1G
        try {
            System.out.println("allocating byte[" + size + "]");
            byte[] b = new byte[size];
            System.out.println("allocation succeeded");
            b[0] = 1;
            System.out.println("write to start succeeded");
            b[b.length-1] = 1;
            System.out.println("write to end succeeded");
        } catch (OutOfMemoryError e) {
            System.out.println("\nPASS"); System.out.flush();
            e.printStackTrace(System.out);
        }
    }
}
