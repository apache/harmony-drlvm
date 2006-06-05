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
 * @author Nikolay A. Sidelnikov
 * @version $Revision: 1.2.12.2.4.3 $
 */

#ifndef _IA32_INTERNAL_PROFILER_H_
#define _IA32_INTERNAL_PROFILER_H_

#include "Ia32IRManager.h"
namespace Jitrino
{
namespace Ia32{

	template<class T> struct AttrDesc {
		T value;
		char * name;
	};

	struct BBStats {
		int64 *	bbExecCount;
		uint32 *	counters;

		BBStats() : bbExecCount(NULL), counters(NULL) {}
	};

	struct MethodStats {
		std::string methodName;
		StlMap<int, BBStats> bbStats; //bbID, bbExecCount, array of counters
		MethodStats(std::string s, MemoryManager& mm) : methodName(s), bbStats(mm) {}
	};

typedef StlVector<MethodStats *> Statistics;

template<class T> class FilterAttr {
	public:
		T		value;
		bool	isInitialized;
		bool	isNegative;

		FilterAttr(T v, bool i = false, bool n = false) : value(v), isInitialized(i), isNegative(n) {};
		
		FilterAttr& operator=(const FilterAttr& c) {
			FilterAttr& f = *this;
			f.value = c.value;
			f.isInitialized = c.isInitialized;
			f.isNegative = c.isNegative;
			return f;
		}
};

struct OpndFilter {
		bool							isInitialized;
		int								opNum;
		FilterAttr<Inst::OpndRole>		opndRole;
		FilterAttr<OpndKind>			opndKind;
		FilterAttr<RegName>				regName;
		FilterAttr<MemOpndKind>			memOpndKind;

		OpndFilter() : isInitialized(false), opNum(-1), opndRole(Inst::OpndRole_Null), opndKind(OpndKind_Null), regName(RegName_Null), memOpndKind(MemOpndKind_Null) {}
		
		OpndFilter& operator=(const OpndFilter& c) {
			OpndFilter& f = *this;
			f.isInitialized = c.isInitialized;		
			f.opNum = c.opNum;
			f.opndRole          =c.opndRole;    
			f.opndKind          =c.opndKind;    
			f.regName           =c.regName;     
			f.memOpndKind       =c.memOpndKind; 
			return f;
		}

};

struct Filter {
		bool	isInitialized;
		bool	isNegative;
		bool	isOR;

		FilterAttr<Mnemonic>								mnemonic;
		FilterAttr<int32>									operandNumber;
		FilterAttr<Opnd::RuntimeInfo::Kind>					rtKind;
		FilterAttr<CompilationInterface::RuntimeHelperId>	rtHelperID;
		FilterAttr<std::string>								rtIntHelperName;
		FilterAttr<bool>									isNative;
		FilterAttr<bool>									isStatic;
		FilterAttr<bool>									isSynchronized;
		FilterAttr<bool>									isNoInlining;
		FilterAttr<bool>									isInstance;
		FilterAttr<bool>									isFinal;
		FilterAttr<bool>									isVirtual;
		FilterAttr<bool>									isAbstract;
		FilterAttr<bool>									isClassInitializer;
		FilterAttr<bool>									isInstanceInitializer;
		FilterAttr<bool>									isStrict;
		FilterAttr<bool>									isRequireSecObject;
		FilterAttr<bool>									isInitLocals;
		FilterAttr<bool>									isOverridden;

		StlMap<int, OpndFilter> operandFilters;

		Filter() : isInitialized(false), isNegative(false), isOR(false), mnemonic(Mnemonic_NULL), operandNumber(-1), rtKind(Opnd::RuntimeInfo::Kind_Null),rtHelperID(CompilationInterface::Helper_Null), rtIntHelperName("none"), isNative(false), isStatic(false), isSynchronized(false), isNoInlining(false), isInstance(false), isFinal(false), isVirtual(false), isAbstract(false), isClassInitializer(false), isInstanceInitializer(false), isStrict(false), isRequireSecObject(false), isInitLocals(false), isOverridden(false), operandFilters(Jitrino::getGlobalMM()) {}

		Filter& operator=(const Filter& c) {
			Filter& f = *this;
			f.isNegative = c.isNegative;
			f.isInitialized = c.isInitialized;
			f.isOR = c.isOR;
			f.mnemonic=c.mnemonic;
			f.operandNumber=c.operandNumber;
			f.rtKind=c.rtKind;
			f.rtHelperID=c.rtHelperID;
			f.rtIntHelperName=c.rtIntHelperName;
			f.isNative=c.isNative;
			f.isStatic=c.isStatic;
			f.isSynchronized=c.isSynchronized;
			f.isNoInlining            =  c.isNoInlining;              
			f.isInstance             =  c.isInstance;                
			f.isFinal               =  c.isFinal;                   
			f.isVirtual              =  c.isVirtual;                 
			f.isAbstract              =  c.isAbstract;                
			f.isClassInitializer      =  c.isClassInitializer;        
			f.isInstanceInitializer   =  c.isInstanceInitializer;     
			f.isStrict                =  c.isStrict;                  
			f.isRequireSecObject      =  c.isRequireSecObject;        
			f.isInitLocals            =  c.isInitLocals;              
			f.isOverridden            =  c.isOverridden;              

			for(StlMap<int, OpndFilter>::const_iterator it = c.operandFilters.begin(); it !=c.operandFilters.end(); it++) {
				f.operandFilters[it->first] = it->second;
			}
			return f;
		}
};

struct Counter {
		std::string		name;
		std::string		title;
		bool			isSorting;
		Filter			filter;

		Counter() : isSorting(false) {}
};

class Config {
public:
	StlVector<Counter> counters;

	bool printBBStats;
	Config(MemoryManager& mm) : counters(mm), printBBStats(false) {};
};


//========================================================================================
// class InternalProfiler
//========================================================================================
/**
	class InternalProfiler collects information about methods 
	
*/
BEGIN_DECLARE_IRTRANSFORMER(InternalProfiler, "iprof", "Internal profiler")
	IRTRANSFORMER_CONSTRUCTOR(InternalProfiler)
public:	
	void runImpl();

	static void init();
	static void deinit() { dumpIt(); config = NULL; }

protected:
	void addCounters(MethodDesc& methodDesc);

	bool passFilter(Inst * inst, Filter& filter);
	bool passOpndFilter(Inst * inst, Opnd * opnd, Filter& filter, OpndFilter& opndFltr);	

	static void readConfig(Config * config);
	static void dumpIt();

	static 	Config * config;
	static	Statistics * statistics;

END_DECLARE_IRTRANSFORMER(InternalProfiler)


}}; // namespace Ia32


#endif
