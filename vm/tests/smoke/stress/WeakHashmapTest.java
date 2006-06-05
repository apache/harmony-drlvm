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
 * @author Ivan Volosyuk, Salikh Zakirov
 * @version $Revision: 1.7.8.1.4.3 $
 */  

package stress;
import java.util.*;

/**
 * @keyword XXX_bug_6164
 */

public class WeakHashmapTest {
    private String str;
    private static WeakHashMap map = new WeakHashMap();
    private static HashMap map2 = new HashMap();

    WeakHashmapTest(String s) {
        str = s;
    }

    public boolean equals(Object o) {
        if (o == null) return false;
        if (o instanceof WeakHashmapTest) {
            WeakHashmapTest t = (WeakHashmapTest) o;
            return str.equals(t.str);
        } else {
            System.out.println("ERROR: object of unknown type received");
            System.out.println("object = " + o);
            return false;
        }
    }

    public int hashCode() {
        return str.hashCode();
    }

    public static void main(String[] args) {
        Thread t1 = new Thread() {
            public void run() {
                while (true) {
                    System.gc();
                    try { Thread.sleep(3000); } catch (Throwable t) {}
                }
            }
        };
        t1.setDaemon(true);
        t1.start();

        new Thread() {
            public void run() {
                for(int j = 0; j < 50; j++) {
                    for(int i = 0; i < 1000; i++) {
                        String s = "" + i;
                        map.put(new WeakHashmapTest(s), s);
                    }
                    trace("+");
                    map.clear();
                }
            }
        }.start();

        new Thread() {
            public void run() {
                for(int j = 0; j < 50; j++) {
                    for(int i = 1000; i >= 0; --i) {
                        String s = "" + i;
                        WeakHashmapTest t = new WeakHashmapTest(s);
                        map.put(t, s);
                        if ((i & 3) == 0) map2.put(t, s);
                    }
                    trace("-");
                }
            }
        }.start();

        new Thread() {
            public void run() {
                for(int j = 0; j < 50; j++) {
                    for(int i = 0; i < 1000; i++) {
                        map.get(new WeakHashmapTest("" + i));
                    }
                    trace("g");
                }
            }
        }.start();


        for(int j = 0; j < 50; j++) {
            for(int i = 0; i < 1000; i++) {
                map.get(new WeakHashmapTest("" + i));
            }
            trace(".");
        }

        System.out.println("PASSED");
    }

    public static void trace(Object o) {
        System.out.print(o);
        System.out.flush();
    }
}
