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
 * @version $Revision: 1.8.40.3 $
 */  
package init;

/**
 * @keyword ignore_status
 */
public class ErrorInInitializer {
    public static void main (String[] args) {
        System.out.println("no static initialization exception, FAILED");
    }

    static {
        System.out.println("PASSED, unless something else happens");
        System.err.println("Static initializer exception should be thrown");
        if (true) {
            throw new RuntimeException("should not be caught (PASSED)");
        }
    }
}
