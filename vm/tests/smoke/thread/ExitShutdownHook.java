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
package thread;

/**
 * @keyword
 */
public class ExitShutdownHook extends Thread {

    public static void main(String[] args) throws Exception {
        System.out.println("addShutdownHook()");
        Runtime.getRuntime().addShutdownHook(new ExitShutdownHook());
        Runtime.getRuntime().exit(0);
    }

    public void run() {
        System.out.println("Passed");
    }

}
