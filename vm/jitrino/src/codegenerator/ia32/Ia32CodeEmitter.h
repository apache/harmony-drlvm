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
 * @version $Revision: 1.8.22.3 $
 */

#ifndef _IA32_CODEEMITTER_H_
#define _IA32_CODEEMITTER_H_

#include "Ia32IRManager.h"
#include "CGSupport.h"
namespace Jitrino
{
namespace Ia32{

//========================================================================================
// class CodeEmitter
//========================================================================================
/**
	class CodeEmitter
	
*/
BEGIN_DECLARE_IRTRANSFORMER(CodeEmitter, "emitter", "Code emitter")

	CodeEmitter(IRManager & irm, const char * params=0)
		:IRTransformer(irm), memoryManager(0x1000, "CodeEmitter"),
		exceptionHandlerInfos(memoryManager), constantAreaLayout(irManager, memoryManager)
	{
        if (irManager.getCompilationInterface().isBCMapInfoRequired()) {
            MethodDesc* meth = irManager.getCompilationInterface().getMethodToCompile();
            bc2LIRMapHandler = new(memoryManager) VectorHandler(bcOffset2LIRHandlerName, meth);
        }
    }


	CompilationInterface::CodeBlockHeat getCodeSectionHeat(uint32 sectionID)const;

protected:
	uint32 getNeedInfo()const{ return NeedInfo_LivenessInfo; }
	uint32 getSideEffects()const{ return 0; }

	void runImpl();
	bool verify(bool force=false);

	//------------------------------------------------------------------------------------
	void emitCode();
	void registerExceptionHandlers();
	void registerExceptionRegion(void * regionStart, void * regionEnd, DispatchNode * regionDispatchNode);
	void postPass();
	void registerDirectCall(Inst * inst);
	void registerInlineInfoOffsets( void );


	//------------------------------------------------------------------------------------
	class ConstantAreaLayout
	{
	public:
		ConstantAreaLayout(IRManager& irm, MemoryManager& mm)
			:irManager(irm), memoryManager(mm), items(memoryManager){};

		void collectItems();
		void calculateItemOffsets();
		void doLayout();
		void finalizeSwitchTables();

	protected:
		IRManager&						irManager;
		MemoryManager&					memoryManager;
		StlVector<ConstantAreaItem*>	items;

		uint32				dataSize;

		const static uint32 blockAlignment=16;
	private:

	};


	//------------------------------------------------------------------------------------
	struct ExceptionHandlerInfo
	{
		void *			regionStart;
		void *			regionEnd;
		void *			handlerAddr;
		ObjectType *	exceptionType;
		bool			exceptionObjectIsDead;
		
		ExceptionHandlerInfo(
			void *			_regionStart,
			void *			_regionEnd,
			void *			_handlerAddr, 
			ObjectType *	_exceptionType, 
			bool			_exceptionObjectIsDead=false
			):
			regionStart(_regionStart),
			regionEnd(_regionEnd),
			handlerAddr(_handlerAddr),
			exceptionType(_exceptionType),
			exceptionObjectIsDead(_exceptionObjectIsDead){}

	};
    
    // bc to native map stuff
    VectorHandler* bc2LIRMapHandler;

	MemoryManager					memoryManager;
	StlVector<ExceptionHandlerInfo> exceptionHandlerInfos;
	ConstantAreaLayout				constantAreaLayout;

END_DECLARE_IRTRANSFORMER(CodeEmitter)

}; // namespace Ia32
}
#endif
