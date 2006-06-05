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
 * @version $Revision: 1.13.16.3 $
 */

#ifndef _IA32_CODE_GENERATOR_
#define _IA32_CODE_GENERATOR_

#include "CodeGenIntfc.h"
#include "PropertyTable.h"
#include "Ia32CFG.h"
#include "Ia32IRManager.h"
#include "Timer.h"
namespace Jitrino
{
namespace Ia32{

//========================================================================================================
class MethodSplitter;

//
// Optimization levels
//
enum OptLevel {
    OptLevel_Low,
    OptLevel_High
};




    //-----------------------------------------------------------------------------------
    /** struct Flags is a bit set defining various options controling 
	the Ia32 code generator

	These flags can be set directly in the code of from the VM command line
	if VM supports this
	*/
struct CGFlags {
    CGFlags () {
			dumpdot = false;
            useOptLevel = true;
			earlyDCEOn = false;
			verificationLevel = 1;
        }
	
		bool	dumpdot					: 1;
        bool	useOptLevel             : 1;// Use fast compilation for first compile
		bool	earlyDCEOn				: 1;
		uint32	verificationLevel		: 2;
		
	};

//========================================================================================================
// class CodeGenerator  -- main class for IA32 back end
//========================================================================================================
/** 
class CodeGenerator is the main class for IA32 back end.
It servers as a driver controlling the whole process of native code generation.
*/

class CodeGenerator : public ::Jitrino::CodeGenerator {
public:

    //-----------------------------------------------------------------------------------
    CodeGenerator(MemoryManager &mm, CompilationInterface& compInterface);
	
    virtual ~CodeGenerator() {}

    bool genCode(MethodCodeSelector& inputProvider);
	
	bool runIRTransformer(const char * tag, const char * params=0);

    
    static void readFlagsFromCommandLine(const CompilationContext* context);
    static void showFlagsFromCommandLine();

    OptLevel getOptLevel()               { return optLevel; }
    void        setOptLevel(OptLevel o)  { optLevel = o; }


	//--------------------------------------------------------------
	class IRTransformerPath
	{
	public:
		struct Step
		{
			enum Switch
			{
				Switch_Off=0,
				Switch_On=1,
				Switch_AlwaysOn=11
			};


			const char *	tag;
			const char *	parameters;
			Switch			tagSwitch;
			uint32			extentionIndex;
		
			Step()
				:tag(NULL), parameters(NULL), tagSwitch(Switch_AlwaysOn), extentionIndex(0){}

			Step(const char * _tag)
				:tag(_tag), parameters(NULL), tagSwitch(Switch_AlwaysOn), extentionIndex(0)
			{ assert(tag!=NULL&&tag[0]!=0); }

		};

		IRTransformerPath(MemoryManager& mm)
			:memoryManager(mm), steps(mm), error(0)
		{}

		IRTransformerPath()
			:memoryManager(*new MemoryManager(0x1000, "StaticIRTransformerPath")), steps(memoryManager), error(0)
		{}

		bool parse(const char * str);

		bool parseOption(const char * str, Step& result);

		void readFlagsFromCommandLine(const JitrinoParameterTable *params);

		const StlVector<Step>& getSteps()const
		{ return steps; }

	protected:
		const char * parseTag(const char * str);
		const char * parseSwitch(const char * str, Step& step, bool startsWithColon=true);
		const char * parseParams(const char * str, Step& step);

		MemoryManager&						memoryManager;
		StlVector<Step>						steps;

		uint32								error;

	};

	//--------------------------------------------------------------
protected:
	bool runIRTransformerPath();
	bool createIRTransformerPath();
	const char * getDefaultIRTransformerPathName();

	//--------------------------------------------------------------
private:
    MemoryManager&			methodMemManager;
	CompilationInterface&	compilationInterface;
    TypeManager&			typeManager;

	IRTransformerPath		irTransformerPath;

	IRManager				irManager;

	void *                  instrMap;          
    OptLevel				optLevel;

	static const char *						defaultPathString;
	static const char *						fastPathString;
	static Timer *							paramsTimer;
};


}} // namespace Ia32
#endif // _IA32_CODE_GENERATOR
