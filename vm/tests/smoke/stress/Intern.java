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
package stress;

/**
 * Tests the correctness of string interning.
 */
public class Intern {
    public static void main(String[] args) {
        String s = "abc";
        try {
            for (int i = 0; i < 100000; i++) {
                s = (s + i + s).intern();
                if (s.length() > 65536) s = "abc" + i;
                if (i % 1000 == 0) trace(".");
            }
            System.out.println("\nPASSED");
        } catch (OutOfMemoryError oome) {
            System.out.println("\nFAILED");
        }
    }

    public static void trace(Object o) {
        System.out.print(o);
        System.out.flush();
    }
}
