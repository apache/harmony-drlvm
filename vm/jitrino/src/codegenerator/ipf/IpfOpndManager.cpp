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
 * @author Intel, Konstantin M. Anisimov, Igor V. Chebykin
 * @version $Revision$
 *
 */

#include "IpfCfg.h"
#include "IpfOpndManager.h"
#include "DrlVMInterface.h"
#include "IpfIrPrinter.h"

namespace Jitrino {
namespace IPF {

//========================================================================================//
// RegStack
//----------------------------------------------------------------------------------------//
// For all masks "1" means reg can be used
// Scratch registers r14-r16, f32-f34, p6-p8, b6 are reserved for spill/fill 
// Preserved reg r4 is thread pointer, r5 and r6 are also busy, so we do not use preserved grs
//========================================================================================//

RegStack::RegStack() : 
    scratchGrMask(string("1111111111111111111111111111111111111111111111111111111111111111"
                         "1111111111111111111111111111111111111111111111100000111100001100")),
    preservGrMask(string("1111111111111111111111111111111111111111111111111111111111111111"
                         "1111111111111111111111111111111100000000000000000000000010000000")),
    spillGrMask  (string("0000000000000000000000000000000000000000000000011000000000000000")),
    scratchFrMask(string("1111111111111111111111111111111111111111111111111111111111111111"
                         "1111111111111111111111111111100000000000000000001111111111000000")),
    preservFrMask(string("0000000000000000000000000000000000000000000000000000000000000000"
                         "0000000000000000000000000000000011111111111111110000000000111100")),
    spillFrMask  (string("0000000000000000000000000000011100000000000000000000000000000000")),
    scratchPrMask(string("0000000000000000000000000000000000000000000000001111111000000000")),
    preservPrMask(string("1111111111111111111111111111111111111111111111110000000000111110")),
    spillPrMask  (string("0000000000000000000000000000000000000000000000000000000111000000")),
    scratchBrMask(string("10000001")),
    preservBrMask(string("00111110")),
    spillBrMask  (string("01000000")) {

    inRegSize  = 0;
    locRegSize = 0;
    outRegSize = 0;
}

//----------------------------------------------------------------------------------------//

int32 RegStack::newInReg(int32 inArgPosition) { 

    if (inRegSize < inArgPosition+1) inRegSize = inArgPosition+1;
    return G_INARG_BASE + inArgPosition;
}

//----------------------------------------------------------------------------------------//

int32 RegStack::newOutReg(int32 outArgPosition) { 

    if (outRegSize < outArgPosition+1) {
        outRegSize = outArgPosition+1;
        // out regs can not be used as preserved (but can be used as scratch)
        for (uint16 i=NUM_G_REG-outRegSize; i<NUM_G_REG; i++) preservGrMask[i] = 0;
    }

    return G_OUTARG_BASE - outArgPosition;
}

//----------------------------------------------------------------------------------------//

bool RegStack::isOutReg(RegOpnd* opnd) {

    if (opnd->getOpndKind() != OPND_G_REG) return false; // opnd is not gr - ignore

    int32 firstOutRegArg = G_OUTARG_BASE - outRegSize + 1;
    int32 location       = opnd->getLocation();
    if (location < firstOutRegArg) return false; // location is less then first outArg - ignore
    if (location >= NUM_G_REG)     return false; // location is greater then last outArg - ignore
    return true;                                 // it is out reg arg
}

//========================================================================================//
// MemStack
//========================================================================================//

MemStack::MemStack() {

    locMemSize = 0;
    outMemSize = 0;
    inBase     = 0;
    locBase    = 0;
    outBase    = 0;
}

//----------------------------------------------------------------------------------------//
// returns location (not offset!) for new in stack opnd

int32 MemStack::newInSlot(int32 inArgPosition) {
    
    int32 offset = (inArgPosition - MAX_REG_ARG) * ARG_SLOT_SIZE;
    return S_INARG_BASE + offset;
}

//----------------------------------------------------------------------------------------//
// increments locMemSize and returns location (not offset!) for new local stack opnd

int32 MemStack::newLocSlot(DataKind dataKind) {
    
    if (inBase > 0) IPF_ERR << endl;            // new loc slot makes illegal in arg offsets

    int16 size   = IpfType::getSize(dataKind);
    int32 offset = align(locMemSize, size);     // align memory address to natural boundary
    locMemSize = offset + size;                 // increase current local area size
    
    return S_LOCAL_BASE + offset;
}

//----------------------------------------------------------------------------------------//
// increments outMemSize and returns location (not offset!) for new out stack opnd

int32 MemStack::newOutSlot(int32 outArgPosition) {

    if (locBase+inBase > 0) IPF_ERR << endl;    // new out slot makes illegal in arg and local offsets
    
    int32 offset  = (outArgPosition - MAX_REG_ARG) * ARG_SLOT_SIZE;
    int32 newSize = offset + ARG_SLOT_SIZE;
    if(outMemSize < newSize) outMemSize = newSize;
    
    return S_OUTARG_BASE + offset;
}

//----------------------------------------------------------------------------------------//
// converts area local offset (location) in absolute offset + S_BASE
    
void MemStack::calculateOffset(RegOpnd* opnd) {
    
    int32 location = opnd->getLocation();
    location = calculateOffset(location);
    opnd->setLocation(location);
}
    
//----------------------------------------------------------------------------------------//
// converts area local offset (location) in absolute offset + S_BASE
    
int32 MemStack::calculateOffset(int32 location) {
    
    if(location < S_OUTARG_BASE) { // offset has been calculated
        return location;
    }
    
    outBase = outMemSize+locMemSize > 0 ? S_SCRATCH_SIZE : 0;
    if(location < S_LOCAL_BASE) {  // it is mem outArg location
        return location - S_OUTARG_BASE + outBase + S_BASE;
    }

    locBase = outBase + align(outMemSize, S_SCRATCH_SIZE);
    if(location < S_INARG_BASE) {  // it is local mem location
        return location - S_LOCAL_BASE + locBase + S_BASE;
    }

    // it is mem inArg location
    inBase = locBase + align(locMemSize, S_SCRATCH_SIZE) + S_SCRATCH_SIZE;
    return location - S_INARG_BASE + inBase + S_BASE;  
}

//----------------------------------------------------------------------------------------//
// return current location in local area
    
int32 MemStack::getSavedBase() {

    outBase   = outMemSize+locMemSize > 0 ? S_SCRATCH_SIZE : 0;
    locBase   = outBase + align(outMemSize, S_SCRATCH_SIZE);
    return locBase + align(locMemSize, S_SCRATCH_SIZE);
}

//----------------------------------------------------------------------------------------//

int32 MemStack::getMemStackSize() {

    outBase = outMemSize+locMemSize > 0 ? S_SCRATCH_SIZE : 0;
    locBase = outBase + align(outMemSize, S_SCRATCH_SIZE);
    inBase  = locBase + align(locMemSize, S_SCRATCH_SIZE) + S_SCRATCH_SIZE;

    return inBase - S_SCRATCH_SIZE;
}
    
//----------------------------------------------------------------------------------------//
    
int32 MemStack::align(int32 val, int32 size) {

    int32 mask = -size;
    int32 buf  = val & mask;
    return buf<val ? buf+size : buf;
}

//========================================================================================//
// StackInfo
//========================================================================================//

StackInfo::StackInfo() {

    rpBak        = LOCATION_INVALID;
    prBak        = LOCATION_INVALID;
    pfsBak       = LOCATION_INVALID;
    unatBak      = LOCATION_INVALID;
    savedBase    = 0; 
    savedGrMask  = 0;
    savedFrMask  = 0;
    savedBrMask  = 0;
    memStackSize = 0;
}

//========================================================================================//
// OpndManager
//========================================================================================//

OpndManager::OpndManager(MemoryManager &mm, CompilationInterface &compilationInterface) : 
    mm(mm),
    compilationInterface(compilationInterface) {

    maxOpndId = 0;

    r0  = NULL;
    f0  = NULL;
    f1  = NULL;
    p0  = NULL;
    b0  = NULL;
    r12 = NULL;
    tau = NULL;

    containCall = false;

    refsCompressed       = compilationInterface.areReferencesCompressed();
    vtablePtrsCompressed = compilationInterface.areVTablePtrsCompressed();
    heapBase             = NULL;
    vtableBase           = NULL;
    vtableOffset         = NULL;
}

//----------------------------------------------------------------------------------------//

Opnd *OpndManager::newOpnd(OpndKind opndKind) {
    return new(mm) Opnd(maxOpndId++, opndKind);
}

//----------------------------------------------------------------------------------------//

RegOpnd *OpndManager::newRegOpnd(OpndKind opndKind, DataKind dataKind, int32 location) {
    return new(mm) RegOpnd(maxOpndId++, opndKind, dataKind, location);
}

//----------------------------------------------------------------------------------------//

Opnd *OpndManager::newImm(int64 immValue) {
    return new(mm) Opnd(maxOpndId++, OPND_IMM, DATA_IMM, immValue);
}

//----------------------------------------------------------------------------------------//

ConstantRef *OpndManager::newConstantRef(Constant *constant, DataKind dataKind) {
    return new(mm) ConstantRef(maxOpndId++, constant, dataKind);
}

//----------------------------------------------------------------------------------------//

NodeRef *OpndManager::newNodeRef(BbNode *node) {
    return new(mm) NodeRef(maxOpndId++, node);
}

//----------------------------------------------------------------------------------------//

MethodRef *OpndManager::newMethodRef(MethodDesc *method) {
    return new(mm) MethodRef(maxOpndId++, method);
}

//----------------------------------------------------------------------------------------//

RegOpnd *OpndManager::getR0()  { if(r0 ==NULL) r0 =newRegOpnd(OPND_G_REG, DATA_I64,  0); return r0;  } 
RegOpnd *OpndManager::getF0()  { if(f0 ==NULL) f0 =newRegOpnd(OPND_F_REG, DATA_F,    0); return f0;  } 
RegOpnd *OpndManager::getF1()  { if(f1 ==NULL) f1 =newRegOpnd(OPND_F_REG, DATA_F,    1); return f1;  } 
RegOpnd *OpndManager::getP0()  { if(p0 ==NULL) p0 =newRegOpnd(OPND_P_REG, DATA_P,    0); return p0;  } 
RegOpnd *OpndManager::getB0()  { if(b0 ==NULL) b0 =newRegOpnd(OPND_B_REG, DATA_I64,  0); return b0;  } 
RegOpnd *OpndManager::getR12() { if(r12==NULL) r12=newRegOpnd(OPND_G_REG, DATA_I64, 12); return r12; } 
RegOpnd *OpndManager::getTau() { if(tau==NULL) tau=newRegOpnd(OPND_INVALID, DATA_INVALID); return tau; } 
RegOpnd *OpndManager::getR0(RegOpnd *ref) { return newRegOpnd(OPND_G_REG, ref->getDataKind(), 0); } 

//----------------------------------------------------------------------------------------//

RegOpnd *OpndManager::getHeapBase() { 
    if (heapBase == NULL) heapBase = newRegOpnd(OPND_G_REG, DATA_U64);
    return heapBase; 
} 

//----------------------------------------------------------------------------------------//

RegOpnd *OpndManager::getVtableBase() {
    if (vtableBase == NULL) vtableBase = newRegOpnd(OPND_G_REG, DATA_U64);
    return vtableBase;
} 

//----------------------------------------------------------------------------------------//

Opnd *OpndManager::getVtableOffset() { 

    if (vtableOffset == NULL) {
        vtableOffset = newImm(compilationInterface.getVTableOffset());
    }
    return vtableOffset;
}

//----------------------------------------------------------------------------------------//

void OpndManager::initCompBases(BbNode *enterNode) {
    
    uint64     baseValue = 0;
    Opnd       *baseImm  = NULL;
    RegOpnd    *p0       = getP0();
    InstVector &insts    = enterNode->getInsts();

    if (heapBase != NULL) {
        baseValue  = (uint64) compilationInterface.getHeapBase();
        baseImm    = newImm(baseValue);
        Inst *inst = new(mm) Inst(INST_MOVL, p0, heapBase, baseImm);
        insts.insert(insts.begin(), inst);
        IPF_LOG << "    HeapBase initialization code inserted" << endl;
    }

    if (vtableBase != NULL) {
        baseValue  = (uint64) compilationInterface.getVTableBase();
        baseImm    = newImm(baseValue);
        Inst *inst = new(mm) Inst(INST_MOVL, p0, vtableBase, baseImm);
        insts.insert(insts.begin(), inst);
        IPF_LOG << "    VtableBase initialization code inserted" << endl;
    }
}

//----------------------------------------------------------------------------------------//
// assign location for opnd

void OpndManager::assignLocation(RegOpnd *opnd) {
    
    OpndKind  opndKind    = opnd->getOpndKind();
    DataKind  dataKind    = opnd->getDataKind();
    RegBitSet &usedMask   = opnd->getBusyRegMask();
    bool      isPreserved = opnd->getCrossCallSite();
    
    int32 location = newLocation(opndKind, dataKind, usedMask, isPreserved);
    opnd->setLocation(location);
}

//----------------------------------------------------------------------------------------//
// tryes to find available location for the opndKind/dataKind taking in account mask of used regs 

int32 OpndManager::newLocation(OpndKind  opndKind, 
                               DataKind  dataKind, 
                               RegBitSet usedMask, 
                               bool      isPreserved) {
    
    RegBitSet &unusedMask = usedMask.flip(); 
    int32 location = LOCATION_INVALID;
    
    if (isPreserved == false) {                             // it is scratch location
        location = newScratchReg(opndKind, unusedMask);     // try to find scratch register
        if (location != LOCATION_INVALID) return location;  // if we succeed - return it
    }
                                                            // it is preserved location or we failed to find scratch one
    location = newPreservReg(opndKind, unusedMask);       // try to find preserved register
    if (location != LOCATION_INVALID) return location;      // if we succeed - return it
                                                            // we failed to find available register
    return newLocSlot(dataKind);                            // allocate new slot on memory stack
}

//----------------------------------------------------------------------------------------//
// tryes to find available scratch register for the opndKind taking in account mask of unused regs 

int32 OpndManager::newScratchReg(OpndKind opndKind, RegBitSet &unusedMask) {

    RegBitSet mask;
    int16     maskSize = 0;
    
    // initialise reg masks and mask size
    switch(opndKind) {
        case OPND_G_REG: mask = scratchGrMask & unusedMask; maskSize = NUM_G_REG; break;
        case OPND_F_REG: mask = scratchFrMask & unusedMask; maskSize = NUM_F_REG; break;
        case OPND_P_REG: mask = scratchPrMask & unusedMask; maskSize = NUM_P_REG; break;
        case OPND_B_REG: mask = scratchBrMask & unusedMask; maskSize = NUM_B_REG; break;
        default: IPF_ERR << " unexpected opnd kind: " << opndKind << endl;
    }

    for(int16 i=0; i<maskSize; i++) if(mask[i] == true) return i;
    return LOCATION_INVALID;
}
    
//----------------------------------------------------------------------------------------//
// tryes to find available preserved register for the opndKind taking in account mask of unused regs 

int32 OpndManager::newPreservReg(OpndKind opndKind, RegBitSet &unusedMask) {

    RegBitSet mask;
    int16     maskSize = 0;
    
    // initialise reg masks and mask size
    switch(opndKind) {
        case OPND_G_REG: mask = preservGrMask & unusedMask; maskSize = REG_STACK_BASE; break;
        case OPND_F_REG: mask = preservFrMask & unusedMask; maskSize = NUM_F_REG;      break;
        case OPND_P_REG: mask = preservPrMask & unusedMask; maskSize = NUM_P_REG;      break;
        case OPND_B_REG: mask = preservBrMask & unusedMask; maskSize = NUM_B_REG;      break;
        default: IPF_ERR << " unexpected opnd kind: " << opndKind << endl;
    }

    // general registers is special case - it is better to allocate preserved reg on dynamic subset of 
    // register stack and only if the attempt failes try to allocate it on static regs
    if (opndKind == OPND_G_REG) { 
        for(int16 i=REG_STACK_BASE; i<NUM_G_REG; i++) if(mask[i] == true) return i;
    }

    for(int16 i=0; i<maskSize; i++) if(mask[i] == true) return i;
    return LOCATION_INVALID;
}
    
//----------------------------------------------------------------------------------------//
// get offset of the first element in array object

int64 OpndManager::getElemBaseOffset() { 
    
    DrlVMTypeManager typeManager(mm);
    typeManager.init(compilationInterface);
    ArrayType *arrayType = typeManager.getArrayType(typeManager.getInt64Type());
    return arrayType->getArrayElemOffset();
}

//----------------------------------------------------------------------------------------//
// init savedBase with current location in local area
    
void OpndManager::initSavedBase() {
    savedBase = getSavedBase();
}

//----------------------------------------------------------------------------------------//
// init memStackSize 
    
void OpndManager::initMemStackSize() {
    memStackSize = getMemStackSize();
}

//----------------------------------------------------------------------------------------//

void OpndManager::printStackInfo()  { 

    IPF_LOG << "  Stack info" << endl;
    IPF_LOG << "    Register: loc=" << locRegSize << " out=" << outRegSize << endl;
    IPF_LOG << "    Memory  : loc=" << locMemSize << " out=" << outMemSize << endl;
    IPF_LOG << "    Method contains call = " << boolalpha << containCall << endl;
}

//----------------------------------------------------------------------------------------//

void OpndManager::saveThisArg()  { 

    MethodDesc *methodDesc = compilationInterface.getMethodToCompile();
    if (methodDesc->isStatic() == true) return;  // there is no "this" arg in static method 

    preservGrMask[32] = 0;                       // make r32 unavailable for reg allocator
    scratchGrMask[32] = 0;                       // make r32 unavailable for reg allocator
    IPF_LOG << endl << "    \"this\" arg is saved" << endl;
}

} // IPF
} // Jitrino