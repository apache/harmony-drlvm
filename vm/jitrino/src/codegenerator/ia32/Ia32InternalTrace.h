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
 * @author Vyacheslav P. Shakin
 * @version $Revision: 1.5.22.3 $
 */

#ifndef _IA32_INTERNAL_TRACE_H_
#define _IA32_INTERNAL_TRACE_H_

#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32{



//========================================================================================
// class InternalTrace
//========================================================================================
/**
	class InternalTrace inserts trace calls 
	
*/
BEGIN_DECLARE_IRTRANSFORMER(InternalTrace, "itrace", "Internal trace")
	IRTRANSFORMER_CONSTRUCTOR(InternalTrace)
	void runImpl();
END_DECLARE_IRTRANSFORMER(InternalTrace)

void __stdcall methodExit(const char * methodName) stdcall__; 
void __stdcall methodEntry(const char * methodName, uint32 argInfoCount, CallingConvention::OpndInfo * argInfos) stdcall__;
void __stdcall fieldWrite(const void * address) stdcall__;

}}; // namespace Ia32


#endif
