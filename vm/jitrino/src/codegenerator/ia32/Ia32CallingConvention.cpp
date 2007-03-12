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
STDCALLCallingConvention        CallingConvention_STDCALL;
DRLCallingConvention            CallingConvention_DRL;
CDECLCallingConvention          CallingConvention_CDECL;


//========================================================================================
// class STDCALLCallingConvention
//========================================================================================

#ifdef _EM64T_
#ifdef _WIN64
const RegName fastCallGPRegs[4] = {RegName_RCX, RegName_RDX, RegName_R8, RegName_R9} ;
const RegName fastCallFPRegs[4] = {RegName_XMM0,RegName_XMM1,RegName_XMM2,RegName_XMM3};
#else 
const RegName fastCallGPRegs[6] = {RegName_RDI, RegName_RSI, RegName_RDX, RegName_RCX, RegName_R8, RegName_R9} ;
const RegName fastCallFPRegs[8] = {RegName_XMM0,RegName_XMM1,RegName_XMM2,RegName_XMM3,RegName_XMM4,RegName_XMM5,RegName_XMM6,RegName_XMM7};
#endif
#endif

//______________________________________________________________________________________
void    STDCALLCallingConvention::getOpndInfo(ArgKind kind, uint32 count, OpndInfo * infos)const
{
    if (kind==ArgKind_InArg){
#ifdef _EM64T_
        uint32 gpreg = 0;
#ifdef _WIN64
#define fpreg gpreg
#else
        uint32 fpreg = 0;
#endif
#endif
        for (uint32 i=0; i<count; i++){
    
            Type::Tag typeTag=(Type::Tag)infos[i].typeTag;
#ifdef _EM64T_
            if(((typeTag>Type::Float  ||typeTag<Type::Single)  && gpreg < lengthof(fastCallGPRegs))) {
                infos[i].slotCount=1;
                infos[i].slots[0]=fastCallGPRegs[gpreg];
                infos[i].isReg=true;
                gpreg++;
            } else if(((typeTag<=Type::Float && typeTag>=Type::Single)  && fpreg < lengthof(fastCallFPRegs))) {
                infos[i].slotCount=1;
                infos[i].slots[0]=fastCallFPRegs[fpreg];
                infos[i].isReg=true;
                fpreg++;
            } else {
                infos[i].slotCount=1;
                infos[i].slots[0]=RegName_Null;
                infos[i].isReg=false;
            }
            
    
#else
            OpndSize size=IRManager::getTypeSize(typeTag);
            assert(size!=OpndSize_Null && size<=OpndSize_64);

            infos[i].slotCount=1;
            infos[i].slots[0]=RegName_Null;
            infos[i].isReg=false;
    
            if (size==OpndSize_64){
                infos[i].slotCount=2;
                infos[i].slots[1]=RegName_Null;
            }
#endif
    
        }
    }else{
        for (uint32 i=0; i<count; i++){
            Type::Tag typeTag=(Type::Tag)infos[i].typeTag;
            infos[i].isReg=true;
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
#ifdef _EM64T_
                        infos[i].slotCount=1;
                        infos[i].slots[0]=RegName_XMM0;
#else
                        infos[i].slotCount=1;
                        infos[i].slots[0]=RegName_FP0;
#endif
                        break;
                    default:
                    {
                        OpndSize size=IRManager::getTypeSize(typeTag);
#ifdef _EM64T_
                        infos[i].slotCount=1;
                        infos[i].slots[0]=RegName_RAX;
        
                        if (size==OpndSize_128){
                            infos[i].slotCount=2;
                            infos[i].slots[1]=RegName_RDX;
                        }
#else
                        assert(size!=OpndSize_Null && size<=OpndSize_64);
    
                        infos[i].slotCount=1;
                        infos[i].slots[0]=RegName_EAX;
        
                        if (size==OpndSize_64){
                            infos[i].slotCount=2;
                            infos[i].slots[1]=RegName_EDX;
                        }
#endif
                    }
                }
            }
        }
    }
}

//______________________________________________________________________________________
Constraint  STDCALLCallingConvention::getCalleeSavedRegs(OpndKind regKind)const
{
    switch (regKind){
        case OpndKind_GPReg:
#ifdef _EM64T_
            return (Constraint(RegName_RBX)|RegName_RBP|RegName_R12|RegName_R13|RegName_R14|RegName_R15);
#else
            return (Constraint(RegName_EBX)|RegName_EBP|RegName_ESI|RegName_EDI);
#endif
        default:
            return Constraint();
    }
}
#ifdef _EM64T_
//______________________________________________________________________________________
void    CDECLCallingConvention::getOpndInfo(ArgKind kind, uint32 count, OpndInfo * infos)const
{
    if (kind==ArgKind_InArg){
        for (uint32 i=0; i<count; i++){
    
                infos[i].slotCount=1;
                infos[i].slots[0]=RegName_Null;
                infos[i].isReg=false;
        }
    }else{
        for (uint32 i=0; i<count; i++){
            Type::Tag typeTag=(Type::Tag)infos[i].typeTag;
            if (i>0){
                infos[i].isReg=false;
                infos[i].slotCount=0;
            }else{
                switch(typeTag){
                    case Type::Void:
                        infos[i].isReg=false;
                        infos[i].slotCount=0;
                        break;
                    case Type::Float:
                    case Type::Double:
                    case Type::Single:
                        infos[i].isReg=true;
                        infos[i].slotCount=1;
                        infos[i].slots[0]=RegName_XMM0;
                        break;
                    default:
                    {
                        OpndSize size=IRManager::getTypeSize(typeTag);
                        infos[i].slotCount=1;
                        infos[i].slots[0]=RegName_RAX;
                        infos[i].isReg=true;
        
                        if (size==OpndSize_128){
                            infos[i].slotCount=2;
                            infos[i].slots[1]=RegName_RDX;
                        }
                    }
                }
            }
        }
    }
}

#endif

//______________________________________________________________________________________


};  // namespace Ia32
}
