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
 * @author Ivan Volosyuk
 * @version $Revision: 1.2.32.1.4.3 $
 */  
package exception;

public class NPE {
    private static void f(String s) {
        System.out.println("String length = " + s.length());
    }

    private static void npe() {
        System.out.println("Testing NPE");
        try {
            f(null);
            System.out.println("Didn\'t catch NPE, FAILED");
        } catch (NullPointerException e) {
            System.out.println("Caught NPE, PASSED");
        }
    }

    private static void ae() {
        System.out.println("Testing AE");
        try {
            int a = 1 / 0;
            System.out.println("Didn\'t caught AE, a = " + a + ", FAILED");
        } catch (ArithmeticException e) {
            System.out.println("Caught AE, PASSED");
        }
    }

    public static void main(String args[]) {
        npe();
        ae();
    }
}
