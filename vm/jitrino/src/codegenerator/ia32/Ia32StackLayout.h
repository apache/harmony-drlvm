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
 * @author Intel, Nikolay A. Sidelnikov
 * @version $Revision: 1.8.22.4 $
 */

#ifndef _IA32_STACK_LAYOUT_H_
#define _IA32_STACK_LAYOUT_H_

#include "Ia32IRManager.h"
#include "Ia32StackInfo.h"
namespace Jitrino
{
namespace Ia32{

//========================================================================================
// class StackLayouter
//========================================================================================
/**
 *	Class StackLayouter performs forming of stack memory for a method, e.g.
 *	allocates memory for stack variables and input arguments, inserts 
 *	saving/restoring callee-save registers. Also it fills StackInfo object
 *	for runtime access to information about stack layout
 *
 *	This transformer ensures that
 *
 *	1)	All input argument operands and stack memory operands have appropriate
 *		displacements from stack pointer
 *	
 *	2)	There are save/restore instructions for all callee-save registers
 *
 *	3)	There are save/restore instructions for all caller-save registers for
 *		all calls in method
 *
 *	4)	ESP has appropriate value throughout whole method
 *
 *	Stack layout illustration:
 *
 *	+-------------------------------+	inargEnd
 *	|								|
 *	|								|
 *	|								|
 *	+-------------------------------+	inargBase, eipEnd
 *	|			eip					|
 *	+-------------------------------+	eipBase,icalleeEnd		<--- "virtual" ESP
 *	|			EBX					|
 *	|			EBP					|
 *	|			ESI					|
 *	|			EDI					|
 *	+-------------------------------+	icalleeBase, fcalleeEnd
 *	|								|
 *	|								|
 *	|								|
 *	+-------------------------------+	fcalleeBase, acalleeEnd
 *	|								|
 *	|								|
 *	|								|
 *	+-------------------------------+	acalleeBase, localEnd
 *	|								|
 *	|								|
 *	|								|
 *	+-------------------------------+	localBase				<--- "real" ESP
 *	|			EAX					|
 *	|			ECX					|
 *	|			EDX					|
 *	+-------------------------------+	base of caller-save regs
 *	
*/

BEGIN_DECLARE_IRTRANSFORMER(StackLayouter, "stack", "Stack Layout")
	StackLayouter (IRManager& irm, const char * params);
	StackInfo * getStackInfo() { return stackInfo; }
	void runImpl();

	uint32 getNeedInfo()const{ return 0; }
	uint32 getSideEffects()const{ return 0; }

protected:
	void checkUnassignedOpnds();

	/**	Computes all offsets for stack areas and stack operands,
		inserts pushs for callee-save registers
	*/
	void createProlog();

	/**	Restores stack pointer if needed,
		inserts pops for callee-save registers
	*/
	void createEpilog();

	int getLocalBase(){ return  localBase; }
	int getLocalEnd(){ return  localEnd; }
	int getApplCalleeBase(){ return  acalleeBase; }
	int getApplCalleeEnd(){ return  acalleeEnd; }
	int getFloatCalleeBase(){ return  fcalleeBase; }
	int getFloatCalleeEnd(){ return  fcalleeEnd; }
	int getIntCalleeBase(){ return  icalleeBase; }
	int getIntCalleeEnd(){ return  icalleeEnd; }
    int getRetEIPBase(){ return  retEIPBase; } 
	int getRetEIPEnd(){ return  retEIPEnd; }
    int getInArgBase(){ return  inargBase; } 
	int getInArgEnd(){ return  inargEnd; }
	uint32 getFrameSize(){ return  frameSize; }
	uint32 getOutArgSize(){ return  outArgSize; }

	int localBase;
	int localEnd;
	int fcalleeBase;
	int fcalleeEnd;
	int acalleeBase;
	int acalleeEnd;
	int icalleeBase;
	int icalleeEnd;
	int icallerBase;
	int icallerEnd;

	int retEIPBase;
	int retEIPEnd;
    int inargBase;
	int inargEnd;
	uint32 frameSize;
	uint32 outArgSize;

	StackInfo * stackInfo;

	MemoryManager memoryManager;

END_DECLARE_IRTRANSFORMER(StackLayouter)

}}; //namespace Ia32
#endif //_IA32_STACK_LAYOUT_H_
