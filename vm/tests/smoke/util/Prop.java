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
 * @version $Revision: 1.6.24.3 $
 */  
package util;

/**
 * Test classpath and bootclasspath properties.
 */
public class Prop {
    static boolean checkProperty(String key) {
        String value = System.getProperty(key);
        System.out.println(key + " = " + value);
        if (value == null) {
            return  false;
        }
        return true;
    }

    static boolean checkPathProperty(String key) {
        return checkProperty(key);
    }

    public static void main(String[] args) {
        if (checkPathProperty("vm.boot.class.path")
            && checkPathProperty("java.class.path")
            && checkProperty("user.name")
            && checkProperty("user.home")) {
            System.out.println("PASS");
        }
    }
}
