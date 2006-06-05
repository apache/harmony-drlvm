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
 * @author Pavel Pervov
 * @version $Revision: 1.7.28.4 $
 */  
package classloader;
import java.io.InputStream;

/**
 * Test resource reading.
 * @keyword golden 
 */
public class SysRes {
    public static void main(String[] s) {
            InputStream is = ClassLoader.getSystemResourceAsStream(
                "classloader/SysRes.class");
            int c;
            try {
                while ((c = is.read()) != -1) {
                    System.out.println(c);
                }
            } catch (java.io.IOException ioe) {
                System.out.println("IOE = " + ioe);
            }
    }
}
