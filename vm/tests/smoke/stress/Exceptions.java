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
 * @author Pavel Afremov
 * @version $Revision: 1.6.20.4 $
 */  
package stress;
import java.util.Vector;

/**
 * @keyword golden slow
 */
public class Exceptions {

    Exception e[] = {
        new RuntimeException("RuntimeException"),
        new NullPointerException("NullPointerException"),
        new ClassNotFoundException("ClassNotFoundException"),
        new NoSuchFieldException("NoSuchFieldException"),
        new NoSuchMethodException("NoSuchMethodException"),
        new InterruptedException("NoSuchMethodException"),
     };

    synchronized boolean test(int ignore) {

        for (int i = e.length - 1; i >= 0; i--) {        
            try {

                System.out.println("Throw exception: " + e[i].toString());

                throw e[i];
            
            } catch (Throwable t) {
                System.out.println("  Got exception: " + t.toString());            
            }
        }

        final int size = 1000;
        Vector v = new Vector();

        String s[] = new String[size];

        for (int i = 0; i < 1000000; i++) {
            try {
                String tmp = new String("iteration " + i);
                s[i % (size + 1)] = tmp;
                v.add(tmp);
            } catch (ArrayIndexOutOfBoundsException foo) {
                 System.out.println("Got ArrayIndexOutOfBoundsException");
            }

            if ((i % 10000) == 0) {
                System.out.println("Git GC on " + i + " iteration");
                v.removeAllElements();                
                System.gc();
            }
        }
        return true;
    }

    public static void main(String[] args) {
        boolean pass = false;

        try {
            Exceptions test = new Exceptions();
            pass = test.test(0);
        } catch (Throwable e) {
            System.out.println("Got exception: " + e.toString());            
        }
        System.out.println("Exceptions test " + (pass ? "passed" : "failed"));
    }
}
