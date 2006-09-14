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
 * @author Intel, Konstantin M. Anisimov, Igor V. Chebykin
 * @version $Revision$
 *
 */

#include "IpfCfg.h"
#include "IpfOpndManager.h"
#include "IpfIrPrinter.h"

namespace Jitrino {
namespace IPF {

//============================================================================//
// Constant
//============================================================================//

Constant::Constant(DataKind dataKind_) {
    offset   = LOCATION_INVALID;
    size     = 0;
    dataKind = dataKind_;
}

//============================================================================//
// SwitchConstant
//============================================================================//

SwitchConstant::SwitchConstant() : Constant(DATA_U64) { 
}

//----------------------------------------------------------------------------//

void SwitchConstant::addEdge(Edge *edge) { 
    edgeList.push_back(edge); 
}

//----------------------------------------------------------------------------//

int16 SwitchConstant::getSize() { 
    return edgeList.size() * sizeof(uint64);
}

//----------------------------------------------------------------------------//

void *SwitchConstant::getData(void *p) {
    return NULL;
}

//----------------------------------------------------------------------------//

uint16 SwitchConstant::getChoice(Edge *edge) {
    
    for (uint16 i=0; i < edgeList.size(); i++) {
        if (edgeList[i] == edge) {
           return i;
        }
    }

    return (uint16)-1;
}

//============================================================================//
// Float Constants
//============================================================================//

FloatConstant::FloatConstant(float value_) : Constant(DATA_S) { 
    value = value_; 
    setSize(sizeof(float)); 
}

//----------------------------------------------------------------------------//

void *FloatConstant::getData() {
    return NULL;
}

//============================================================================//
// Double Constants
//============================================================================//

DoubleConstant::DoubleConstant(double value_) : Constant(DATA_D) { 
    value = value_; 
    setSize(sizeof(double)); 
}

//----------------------------------------------------------------------------//

void *DoubleConstant::getData() {
    return NULL;
}

//============================================================================//
// Opnd
//============================================================================//

Opnd::Opnd(uint32 id_, OpndKind opndKind_, DataKind dataKind_, int64 value_) :
    id(id_), 
    opndKind(opndKind_), 
    dataKind(dataKind_), 
    value(value_) { 
}

//----------------------------------------------------------------------------//

bool Opnd::isWritable() {
    if (isReg()      == false) return false;
    if (isConstant() == true)  return false;
    return true;
}

//----------------------------------------------------------------------------//
// true if opnd resides on memory stack

bool Opnd::isMem() {
    
    if (isReg() == false)          return false;
    if (value < S_BASE)            return false;
    if (value >= LOCATION_INVALID) return false;
    return true;
    
    return true;
}

//----------------------------------------------------------------------------//

bool Opnd::isConstant() {

    if (value==0 && opndKind==OPND_P_REG) return true;  // p0
    if (value==0 && opndKind==OPND_G_REG) return true;  // r0
    if (value==0 && opndKind==OPND_F_REG) return true;  // f0
    if (value==1 && opndKind==OPND_F_REG) return true;  // f1

    return false;
}

//----------------------------------------------------------------------------//

bool Opnd::isImm(int size) {

    if (isImm() == false) return false;
    
    Opnd* imm = (Opnd*) this;
    if (Opnd::isFoldableImm(imm->getValue(), size)) return true;
    return false;
}

//----------------------------------------------------------------------------//

bool Opnd::isFoldableImm(int64 imm, int16 size) {
    uint64 max  = ((uint64)0x1 << (size - 1)) - 1;
    uint64 min  = ~max;
    return (imm >= (int64)min) && (imm <= (int64)max);
}
    
//============================================================================//
// RegOpnd
//============================================================================//

RegOpnd::RegOpnd(uint32 id_, OpndKind opndKind_, DataKind dataKind_, int32 value_) : 
    Opnd(id_, opndKind_, dataKind_, value_) { 
    
    spillCost     = 0;
    crossCallSite = false;
}

//----------------------------------------------------------------------------//

int64 RegOpnd::getValue() {
    
    if (isMem() == false)       return value;
    if (value >= S_OUTARG_BASE) return value;
    return value - S_BASE;
}

//----------------------------------------------------------------------------//

void RegOpnd::insertDepOpnd(RegOpnd *depOpnd) { 
    
    if (depOpnd->isMem() == true)           return;
    if (depOpnd->getOpndKind() != opndKind) return;
    depOpnds.insert(depOpnd); 
}

//============================================================================//
// NodeRef
//============================================================================//

int64 NodeRef::getValue() { return node->getAddress(); }

//============================================================================//
// MethodRef
//============================================================================//

int64 MethodRef::getValue() { 

   int64 *indirectAddress = (int64 *)method->getIndirectAddress();
   return *indirectAddress; 
}

} //Jitrino
} // IPF
