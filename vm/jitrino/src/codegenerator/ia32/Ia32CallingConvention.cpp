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
 * @version $Revision: 1.6.22.3 $
 */

#include "Ia32CallingConvention.h"
#include "Ia32IRManager.h"

namespace Jitrino{
namespace Ia32{

const CallingConvention * CallingConvention::str2cc(const char * cc_name) {
	if( NULL == cc_name ) { // default
		return &CallingConvention_STDCALL;
	}

	if( !strcmpi(cc_name, "stdcall") ) {
		return &CallingConvention_STDCALL;
	}

	if( !strcmpi(cc_name, "drl") ) {
		return &CallingConvention_DRL;
	}

	if( !strcmpi(cc_name, "cdecl") ) {
		return &CallingConvention_CDECL;
	}
	assert( false );
	return NULL;
}


//========================================================================================
STDCALLCallingConvention		CallingConvention_STDCALL;
DRLCallingConvention			CallingConvention_DRL;
CDECLCallingConvention			CallingConvention_CDECL;


//========================================================================================
// class STDCALLCallingConvention
//========================================================================================

//______________________________________________________________________________________
void	STDCALLCallingConvention::getOpndInfo(ArgKind kind, uint32 count, OpndInfo * infos)const
{
	if (kind==ArgKind_InArg){
		for (uint32 i=0; i<count; i++){
			Type::Tag typeTag=(Type::Tag)infos[i].typeTag;
	
			OpndSize size=IRManager::getTypeSize(typeTag);
			assert(size!=OpndSize_Null && size<=OpndSize_64);
	
			infos[i].slotCount=1;
			infos[i].slots[0]=RegName_Null;
	
			if (size==OpndSize_64){
				infos[i].slotCount=2;
				infos[i].slots[1]=RegName_Null;
			}
		}
	}else{
		for (uint32 i=0; i<count; i++){
			Type::Tag typeTag=(Type::Tag)infos[i].typeTag;
			if (i>0){
				infos[i].slotCount=0;
			}else{
				switch(typeTag){
					case Type::Void:
						infos[i].slotCount=0;
						break;
					case Type::Float:
					case Type::Double:
					case Type::Single:
						infos[i].slotCount=1;
						infos[i].slots[0]=RegName_FP0;
						break;
					default:
					{
						OpndSize size=IRManager::getTypeSize(typeTag);
						assert(size!=OpndSize_Null && size<=OpndSize_64);
	
						infos[i].slotCount=1;
						infos[i].slots[0]=RegName_EAX;
		
						if (size==OpndSize_64){
							infos[i].slotCount=2;
							infos[i].slots[1]=RegName_EDX;
						}
					}
				}
			}
		}
	}
}

//______________________________________________________________________________________
uint32	STDCALLCallingConvention::getCalleeSavedRegs(OpndKind regKind)const
{
	switch (regKind){
		case OpndKind_GPReg:
			return (Constraint(RegName_EBX)|RegName_EBP|RegName_ESI|RegName_EDI).getMask();
		default:
			return 0;
	}
}

//______________________________________________________________________________________


};  // namespace Ia32
}
