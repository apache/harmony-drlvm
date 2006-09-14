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
package org.apache.harmony.test;

import java.security.Permission;

/**
 * Security Manager implementation which does not prevent
 * re-setting of the system security manager.
 * 
 * @author Alexey V. Varlamov
 * @version $Revision$
 */
public class ReversibleSecurityManager extends SecurityManager {
    public static final RuntimePermission RESET_PERMISSION = 
        new RuntimePermission("setSecurityManager");

    public void checkPermission(Permission perm) {
        if (!RESET_PERMISSION.equals(perm)) {
            super.checkPermission(perm);
        }
    }
}