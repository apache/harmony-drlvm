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
 *  @author Gregory Shimansky
 *  @version $Revision: 1.1.2.1.4.4 $
 */  

#ifndef _JVMTI_INTF_H
#define _JVMTI_INTF_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Method enter callback which is called from all JIT compiled methods,
 * This callback is called before synchronization is done for synchronized
 * methods. Garbage collector should be enabled.
 * @param method - handle of the called method
 */
void jvmti_method_enter_callback(Method_Handle method);

/**
 * Method exit callback which is called from all JIT compiled methods.
 * This callback is called after synchronization is done for synchronized
 * methods. Garbage collector should be enabled.
 * @param method - handle of the exiting method
 * @param return_value - the return value of the method if method is not void
 */
void jvmti_method_exit_callback(Method_Handle method, jvalue* return_value);

#ifdef __cplusplus
}
#endif

#endif // _JVMTI_INTF_H

