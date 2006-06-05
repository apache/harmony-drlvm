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
 * @author Pavel Rebriy
 * @version $Revision: 1.1.2.1.4.4 $
 */  

#ifndef _VERIFIER_STUB_H_
#define _VERIFIER_STUB_H_

#include "environment.h"
#include "Class.h"

bool class_verify(const Global_Env* env, Class* klass);
bool class_verify_constraints(const Global_Env* env, Class* klass);

#endif // _VERIFIER_STUB_H_
