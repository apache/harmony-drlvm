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
 * @author Alexei Fedotov, Salikh Zakirov
 * @version $Revision: 1.4.20.3 $
 */  
package stress;

/**
 * @keyword slow
 */
public class BigStringBuffer {

    synchronized boolean test(int ignore) {

        StringBuffer buf = new StringBuffer();

        for (int i = 0; i < 20000; i++) {
           buf.append("token" + i + "\n");
        }
        String s = buf.toString();
        System.out.println(s);            
        return true;
    }

    public static void main(String[] args) {
        boolean pass = false;

        try {
            BigStringBuffer test = new BigStringBuffer();
            pass = test.test(0);
        } catch (Throwable e) {
            System.out.println("Got exception: " + e.toString());            
        }
        System.out.println("BigStringBuffer " + (pass ? "passed" : "failed"));
    }
}
