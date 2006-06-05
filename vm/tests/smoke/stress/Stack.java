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
 * @author Alexei Fedotov
 * @version $Revision: 1.5.20.3 $
 */  
package stress;

/**
 * @keyword XXX_bug_2887
 */
public class Stack {

    static final int depth = 7000; // the external java crashes if depth = 200000

    synchronized boolean test(int i) {
        System.out.println("" + i);

        if (i < depth && test(++i)) {
            return true;
        }
        return false; // Should got stack overflow?
    }

    public static void main(String[] args) {
        boolean pass = false;

        try {
            Stack test = new Stack();
            pass = test.test(0);
        } catch (Throwable e) {
            System.out.println("Got exception: " + e.toString());            
        }
        System.out.println("Stack test " + (pass ? "passed" : "failed"));
    }
}
