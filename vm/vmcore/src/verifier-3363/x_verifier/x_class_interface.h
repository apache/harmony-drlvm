/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
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
 * @author Mikhail Loenko
 */  

#ifndef __X_CLASS_INTERFACE_H__
#define __X_CLASS_INTERFACE_H__


/**
 * returns constantpool index for the given class
 */
unsigned short class_get_cp_class_entry(class_handler k_class, const char* name);

/**
 * removes given exception handler (handlers with greater indexes shift)
 */
void method_remove_exc_handler( method_handler method, unsigned short idx );

/**
 * modifies start_pc, end_pc 
 */
void method_modify_exc_handler_info( method_handler method, unsigned short idx, unsigned short start_pc, 
                                    unsigned short end_pc, unsigned short handler_pc, unsigned short handler_cp_index );

#endif
