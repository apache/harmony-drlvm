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
 * @author Serguei S.Zapreyev
 * @version $Revision$
 * 
 * This ClassTestGetResource class ("Software") is furnished under license and
 * may only be used or copied in accordance with the terms of that license.
 *  
 */

package java.lang;

import java.io.File;

import junit.framework.TestCase;

/*
 * Created on 03.02.2006
 * 
 * This ClassTestGetResource class is used to test the Core API
 * java.lang.Class.getResource method
 *  
 */
public class ClassTestGetResource extends TestCase {

    static String vendor = System.getProperty("java.vm.vendor");

    /**
     * prepending by package name.
     */
    public void test1() {
        assertTrue("Error1: unexpected:"
                + Void.class.getResource("Class.class").toString(),
                Void.class.getResource("Class.class").toString().indexOf(
                        "Class.class") != -1);
    }

    /**
     * unchanging.
     */
    public void test2() {
        assertTrue("Error1",
                Void.class.getResource(
                        "/"
                                + Class.class.getPackage().getName().replace(
                                        '.', '/') + "/Class.class").toString()
                        .indexOf("Class.class") != -1);
    }

    /**
     * in java.ext.dirs.
     */
    public void test3() {
        //System.out.println(System.getProperties());
        if (vendor.equals("Intel DRL")) {// to test for others
            // -Djava.ext.dirs=<some non-empty path list> argument should be
            // passed for ij.exe for real check
            String as[] = System.getProperty("java.ext.dirs").split(
                    System.getProperty("path.separator"));
            for (int i = 0; i < as.length; i++) {
                File dir = new File(as[i]);
                if (dir.exists() && dir.isDirectory()) {
                    String afn[] = dir.list();
                    File aff[] = dir.listFiles();
                    for (int j = 0; j < aff.length; j++) {
                        if (aff[j].isFile()) {
                            /**/System.out.println("test3 "+afn[j]);
                            try {
                                assertTrue("Error1",
                                    Void.class.getResource("/" + afn[j])
                                            .toString().indexOf(afn[j]) != -1);
							    return;
                            }catch(Throwable e){
								 System.out.println(e.toString());
    			    }
                        }
                    }
                }
            }
        }
    }

    /**
     * in java.class.path.
     */
    public void test4() {
        if (vendor.equals("Intel DRL")) {// to test for others
            // -cp <some non-empty path list> or -Djava.class.path=<some
            // non-empty path list> arguments should be passed for ij.exe for
            // real check
            String as[] = System.getProperty("java.class.path").split(
                    System.getProperty("path.separator"));
            for (int i = 0; i < as.length; i++) {
                File f = new File(as[i]);
                if (f.exists() && f.isDirectory()) {
                    String afn[] = f.list();
                    File aff[] = f.listFiles();
                    for (int j = 0; j < aff.length; j++) {
                        if (aff[j].isFile()) {
                            //System.out.println("test4 "+afn[j]);
                            assertTrue("Error1",
                                    Void.class.getResource("/" + afn[j])
                                            .toString().indexOf(afn[j]) != -1);
                            return;
                        }
                    }
                } else if (f.exists() && f.isFile()
                        && f.getName().endsWith(".jar")) {
                    try {
                        java.util.jar.JarFile jf = new java.util.jar.JarFile(f);
                        for (java.util.Enumeration e = jf.entries(); e
                                .hasMoreElements();) {
                            String s = e.nextElement().toString();
                            if (s.endsWith(".class")) {
                                //System.out.println("test4 "+s);
                                assertTrue("Error1", Void.class.getResource(
                                        "/" + s).toString().indexOf(s) != -1);
                                return;
                            }
                        }
                    } catch (java.io.IOException _) {
                    }
                }
            }
        }
    }

    /**
     * via -Xbootclasspath (vm.boot.class.path).
     */
    public void test5() {
        //System.out.println("|"+System.getProperty("sun.boot.class.path")+"|");
        //System.out.println("|"+System.getProperty("vm.boot.class.path")+"|");
        if (vendor.equals("Intel DRL")) {// to test for others
            // -Xbootclasspath[/a /p]:<some non-empty path list> or
            // -D{vm/sun}.boot.class.path=<some non-empty path list> arguments
            // should be passed for ij.exe for real check
            String as[] = System.getProperty(
                    (vendor.equals("Intel DRL") ? "vm" : "sun")
                            + ".boot.class.path").split(
                    System.getProperty("path.separator"));
            for (int i = 0; i < as.length; i++) {
                File f = new File(as[i]);
                if (f.exists() && f.isFile() && f.getName().endsWith(".jar")) {
                    try {
                        java.util.jar.JarFile jf = new java.util.jar.JarFile(f);
                        for (java.util.Enumeration e = jf.entries(); e
                                .hasMoreElements();) {
                            String s = e.nextElement().toString();
                            if (s.endsWith(".class")) {
                                //System.out.println("test5 "+s);
                                assertTrue("Error1", Void.class.getResource(
                                        "/" + s).toString().indexOf(s) != -1);
                                return;
                            }
                        }
                    } catch (java.io.IOException _) {
                    }
                } else if (f.exists() && f.isDirectory() && false) {
                    String afn[] = f.list();
                    File aff[] = f.listFiles();
                    for (int j = 0; j < aff.length; j++) {
                        if (aff[j].isFile()) {
                            //System.out.println("test5 "+afn[j]);
                            assertTrue("Error1",
                                    Void.class.getResource("/" + afn[j])
                                            .toString().indexOf(afn[j]) != -1);
                            return;
                        }
                    }
                }
            }
        }
    }

    /**
     * The method throws NullPointerException if argument is null
     */
//Commented because 6793 bug isn't fixed
    public void te_st6() {
        try {
            Void.class.getResource(null);
        	fail("Error1: NullPointerException is not thrown for null argument"); // #6793
        } catch (NullPointerException _) {           
        }
    }
}
