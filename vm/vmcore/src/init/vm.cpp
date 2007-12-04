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

#include "Class.h"
#include "environment.h"
#include "compile.h"
#include "object_layout.h"


VMEXPORT Global_Env *VM_Global_State::loader_env = 0;

// tag pointer is not allocated by default, enabled by TI
VMEXPORT bool ManagedObject::_tag_pointer = false;


/////////////////////////////////////////////////////////////////
// begin Class
/////////////////////////////////////////////////////////////////


Field* class_resolve_nonstatic_field(Class* clss, unsigned cp_index)
{
    Compilation_Handle ch;
    ch.env = VM_Global_State::loader_env;
    ch.jit = NULL;
    Field_Handle fh = resolve_field(&ch, (Class_Handle)clss, cp_index);
    if(!fh || field_is_static(fh))
        return NULL;
    return fh;
} // class_resolve_nonstatic_field


/////////////////////////////////////////////////////////////////
// end Class
/////////////////////////////////////////////////////////////////
