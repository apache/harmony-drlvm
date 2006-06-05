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
 * @author Intel, Alexei Fedotov
 * @version $Revision: 1.10.20.4 $
 */  
package util;
import java.util.logging.Level;

/**
 * Test a logger.
 * @keyword golden X_Linux_bug_5898
 */
public class Logger {
    private final static java.util.logging.Logger LOG =
        java.util.logging.Logger.getAnonymousLogger();
    private final static String MESSAGE = "logging message";

    public static void main(String[] args) {
        LOG.setLevel(Level.ALL);
        LOG.info(MESSAGE);
        System.out.println("passed");
    }
}
