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
 * @author Intel, Evgueni Brevnov
 * @version $Revision: 1.1.2.1.4.3 $
 */  


#define N_T_S_H NULL
#define E_H_M NULL
#define E_H_S NULL
#define E_H_I NULL
#define E_H_S0 NULL
#define G_S_R_E_H NULL
#define J_V_M_T_I_E_H NULL
#define E_H_RECOMP NULL

#define OS_THREAD_INIT_1()

#define OS_SYNC_SUPPORT()


#define OS_THREAD_INIT_2() \
    p_TLS_vmthread = p_vm_thread; 


#define THREAD_CLEANUP() 

#define BEGINTHREADEX_SUSPENDSTATE 1

#define SET_THREAD_DATA_MACRO()

    
#define THREAD_ACTIVE_IN_OS_MACRO()

#define CHECK_HIJACK_SUPPORT_MACRO()

