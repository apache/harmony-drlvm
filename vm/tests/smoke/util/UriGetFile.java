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
 * @version $Revision: 1.7.24.4 $
 */  
package util;

import java.net.MalformedURLException;
import java.net.URL;

/**
 * @keyword golden
 */
public class UriGetFile {

    public static boolean test()
    {
        URL url = null;
        try {
            url = new URL("file://");
        } catch (MalformedURLException mue) {
        }
        System.out.println(url.getFile());

        return true;
    }

    public static void main(String[] args) {
        boolean pass = false;
        try {
            pass = UriGetFile.test();
        } catch (Throwable e) {
            System.out.println("Got exception: " + e.toString());            
        }
        System.out.println("UriGetFile " + (pass ? "passed" : "failed"));
    }
}
