/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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

public class ThreadRTest extends TestCase {

    private class ThreadRunning extends Thread {
        volatile boolean stopWork = false;
        int i = 0;

        ThreadRunning() {
            super();
        }
        
        ThreadRunning(ThreadGroup g, String name) {
            super(g, name);
        }
        public void run () {
            while (!stopWork) {
                i++;
            }
        }
    }

    public void testGetThreadGroupDeadThread() {
        ThreadRunning t = new ThreadRunning();
        t.start();
        t.stopWork = true;
        try {
            t.join();
        } catch (InterruptedException e) {
            e.printStackTrace();          
        }
        try {
            t.getThreadGroup();
            t.getThreadGroup();
        } catch (NullPointerException e) {
            fail("NullPointerException has been thrown");
        }
    }
}
