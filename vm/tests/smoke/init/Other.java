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
 * @version $Revision: 1.4.32.1.4.3 $
 */  
package init;

/**
 * @keyword 
 */
public class Other {
    public static void main (String[] args) {
        try {
            Class.forName("init.OtherErrorInInitializer");
            System.out.println("FAILED");
        } catch (Throwable e) {
            System.out.println("caught " + e);
            System.out.println("PASSED");
        }
    }
}

class OtherErrorInInitializer {
    static {
        if (true) {
            throw new RuntimeException("error in initializer");
        }
    }
}
