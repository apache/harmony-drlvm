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
 * @author Euguene Ostrovsky
 * @version $Revision: 1.1.2.1.4.6 $
 */  


#ifndef _INIT_H
#define _INIT_H

#include "environment.h"

bool vm_init(Global_Env *env);
int run_java_main(char *classname, char **j_argv, int j_argc, Global_Env *p_env);
int run_main_platform_specific(char *classname, char **j_argv, int j_argc, Global_Env *p_env);

JavaVMInitArgs* parse_cmd_arguments(int argc, char *argv[], char **class_name, char **jar_file, int *p_java_arg_num);
void clear_vm_arguments(JavaVMInitArgs* vm_args);
void initialize_vm_cmd_state(Global_Env *p_env, JavaVMInitArgs* arguments);
void set_log_levels_from_cmd(JavaVMInitArgs* vm_arguments);
void parse_vm_arguments(Global_Env *p_env);
void parse_jit_arguments(JavaVMInitArgs* vm_arguments);
void print_generic_help();

#endif //_INIT_H
