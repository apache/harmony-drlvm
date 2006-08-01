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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.10.12.2.4.4 $
 */

#include "Ia32GCMap.h"
#include "Ia32Inst.h"
#include "Ia32RuntimeInterface.h"
#include "Ia32StackInfo.h"
#include "Ia32GCSafePoints.h"

#include "Ia32Printer.h"

#ifdef _DEBUG
//#define ENABLE_GC_RT_CHECKS
#endif //default mode, but could be enabled for release too

//#define ENABLE_GC_RT_CHECKS

namespace Jitrino
{
namespace  Ia32 {


//_______________________________________________________________________
// GCMap
//_______________________________________________________________________

GCMap::GCMap(MemoryManager& memM) : mm(memM), gcSafePoints(mm), offsetsInfo(NULL) {
}

void GCMap::registerInsts(IRManager& irm) {
    assert(offsetsInfo == NULL);
    assert(gcSafePoints.empty());
    offsetsInfo = new (mm) GCSafePointsInfo(mm, irm, GCSafePointsInfo::MODE_2_CALC_OFFSETS);
    const Nodes& nodes = irm.getNodes();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (node->hasKind(Node::Kind_BasicBlock)) {
            processBasicBlock(irm, (BasicBlock*)node);
        }
    }
}

void GCMap::processBasicBlock(IRManager& irm, const BasicBlock* block) {
    uint32 safePointsInBlock = GCSafePointsInfo::getNumSafePointsInBlock(block);
    if (safePointsInBlock == 0) {
        return;
    }
    gcSafePoints.reserve(gcSafePoints.size() + safePointsInBlock);
    LiveSet ls(mm, irm.getOpndCount());
    irm.getLiveAtExit(block, ls);
    const Insts& insts = block->getInsts();
    for (Inst* inst = insts.getLast(); inst!=NULL; inst = insts.getPrev(inst)) {
        if (IRManager::isGCSafePoint(inst)) {
            registerGCSafePoint(irm, ls, inst);
#ifndef _DEBUG   //for debug mode we collect all hardware exception points too
            if (--safePointsInBlock == 0) {
                break;
            }
#else
        }  else if (isHardwareExceptionPoint(inst)){  //check if hardware exception point
            registerHardwareExceptionPoint(inst); //save empty safe-point -> used for debugging and testing
#endif
        }
        irm.updateLiveness(inst, ls);
    }
}

void  GCMap::registerGCSafePoint(IRManager& irm, const LiveSet& ls, Inst* inst) {
    uint32 eip = (uint32)inst->getCodeStartAddr()+inst->getCodeSize();
    GCSafePoint* gcSafePoint = new (mm) GCSafePoint(mm, eip);
    GCSafePointPairs& pairs = offsetsInfo->getGCSafePointPairs(inst);
    const StlSet<Opnd*>& staticFieldsMptrs = offsetsInfo->getStaticFieldMptrs();
#ifdef GC_MAP_DUMP_ENABLED
    StlVector<int>* offsets = NULL;
    StlVector<Opnd*>* basesAndMptrs = NULL;
    bool loggingGCInst = Log::cat_cg()->isIREnabled();
    if (loggingGCInst) {
        offsets = new (mm) StlVector<int>(mm);
        basesAndMptrs = new (mm) StlVector<Opnd*>(mm);
    }
#endif    
#ifdef _DEBUG
    gcSafePoint->instId = inst->getId();
#endif
    LiveSet::IterB liveOpnds(ls);
	assert(inst->hasKind(Inst::Kind_CallInst));
	Opnd * callRes = inst->getOpndCount(Inst::OpndRole_AllDefs)>0 ? inst->getOpnd(0) : NULL;
    for (int i = liveOpnds.getNext(); i != -1; i = liveOpnds.getNext()) {
        Opnd* opnd = irm.getOpnd(i);
        if (callRes == opnd) {
            continue;
        }
        if (!opnd->getType()->isManagedPtr() && !opnd->getType()->isObject()) {
            continue;
        }
        if (staticFieldsMptrs.find(opnd) != staticFieldsMptrs.end()) {
            continue;
        }
        if (opnd->getRefCount() == 0) {
            continue;
        }
        //ok register this opnd
        MPtrPair* pair = GCSafePointsInfo::findPairByMPtrOpnd(pairs, opnd);
        int32 offset = pair == NULL ? 0 : pair->getOffset();
        bool isObject = offset == 0;
        GCSafePointOpnd* gcOpnd;
        RegName reg = opnd->getRegName();
        if (reg != RegName_Null) {
            gcOpnd = new (mm) GCSafePointOpnd(isObject, TRUE, uint32(reg), offset);
        } else if (opnd->getMemOpndKind() == MemOpndKind_StackAutoLayout) {
            const Opnd* displOpnd = opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
            assert(displOpnd!=NULL);
            int offset_from_esp0 =  (int)displOpnd->getImmValue(); //opnd saving offset from the esp on method call
            int inst_offset_from_esp0 = (int)inst->getStackDepth();
            uint32 ptrToAddrOffset = inst_offset_from_esp0 + offset_from_esp0; //opnd saving offset from the esp on inst
            gcOpnd = new (mm) GCSafePointOpnd(isObject, false, ptrToAddrOffset, offset);
        } else {
            assert(opnd->getMemOpndKind() == MemOpndKind_Heap);
            continue;
        }
#ifdef _DEBUG
        gcOpnd->firstId = opnd->getFirstId();
#endif
        gcSafePoint->gcOpnds.push_back(gcOpnd);
#ifdef GC_MAP_DUMP_ENABLED
        if (loggingGCInst) {
            basesAndMptrs->push_back(opnd);
            offsets->push_back(gcOpnd->getMPtrOffset());
        }
#endif
    }
    gcSafePoints.push_back(gcSafePoint);
#ifdef GC_MAP_DUMP_ENABLED
    if (loggingGCInst && !offsets->empty()) {
        GCInfoPseudoInst* gcInst = irm.newGCInfoPseudoInst(*basesAndMptrs);
        gcInst->desc = "gcmap";
        gcInst->offsets.resize(offsets->size());
        std::copy(offsets->begin(), offsets->end(), gcInst->offsets.begin());
        inst->getBasicBlock()->appendInsts(gcInst, inst);
    }
#endif
}
bool GCMap::isHardwareExceptionPoint(const Inst* inst) const {
    Inst::Opnds opnds(inst, Inst::OpndRole_Explicit|Inst::OpndRole_UseDef);
    for (Inst::Opnds::iterator it = opnds.begin(), end = opnds.end(); it!=end; it = opnds.next(it)) {
        Opnd* opnd = inst->getOpnd(it);
        if (opnd->isPlacedIn(OpndKind_Mem) && opnd->getMemOpndKind() == MemOpndKind_Heap) {
            return true;
        }
    }
    return false;
}

void  GCMap::registerHardwareExceptionPoint(Inst* inst) {
    uint32 eip = (uint32)inst->getCodeStartAddr();
    GCSafePoint* gcSafePoint = new (mm) GCSafePoint(mm, eip);
#ifdef _DEBUG
    gcSafePoint->instId = inst->getId();
    gcSafePoint->hardwareExceptionPoint = true;
#endif
    gcSafePoints.push_back(gcSafePoint);
}


uint32 GCMap::getByteSize() const {
    uint32 size = 4/*byte number */ + 4/*number of safepoints*/ 
            + 4*gcSafePoints.size()/*space to save safepoints sizes*/;
    for (int i=0, n = gcSafePoints.size(); i<n;i++) {
        GCSafePoint* gcSite = gcSafePoints[i];
        size+= 4*gcSite->getUint32Size();
    }
    return size;
}

uint32 GCMap::readByteSize(const Byte* input) {
    uint32* data = (uint32*)input;
    uint32 gcMapSizeInBytes;
    
    gcMapSizeInBytes = data[0];
    return gcMapSizeInBytes;
}


#ifdef _DEBUG
struct hwecompare {
    bool operator() (const GCSafePoint* p1, const GCSafePoint* p2) const {
        if (p1->isHardwareExceptionPoint() == p2->isHardwareExceptionPoint()) {
            return p1 < p2;
        }
        return !p1->isHardwareExceptionPoint();
    }
};
#endif

void GCMap::write(Byte* output)  {
    uint32* data = (uint32*)output;
    data[0] = getByteSize();
    data[1] = gcSafePoints.size();
    uint32 offs = 2;

#ifdef _DEBUG
    // make sure that hwe-points are after normal gcpoints
    // this is depends on findGCSafePointStart algorithm -> choose normal gcpoint
    // if both hwe and normal points are registered for the same IP
    std::sort(gcSafePoints.begin(), gcSafePoints.end(), hwecompare());
#endif

    for (int i=0, n = gcSafePoints.size(); i<n;i++) {
        GCSafePoint* gcSite = gcSafePoints[i];
        uint32 siteUint32Size = gcSite->getUint32Size();
        data[offs++] = siteUint32Size;
        gcSite->write(data+offs);
        offs+=siteUint32Size;
    }
    assert(4*offs == getByteSize());
}


const uint32* GCMap::findGCSafePointStart(const uint32* data, uint32 ip) {
    uint32 nGCSafePoints = data[1];
    uint32 offs = 2;
    for (uint32 i = 0; i < nGCSafePoints; i++) {
        uint32 siteIP = GCSafePoint::getIP(data + offs + 1);
        if (siteIP == ip) {
            return data+offs+1;
        }
        uint32 siteSize = data[offs];
        offs+=1+siteSize;
    }
    return NULL;
}


//_______________________________________________________________________
// GCSafePoint
//_______________________________________________________________________

GCSafePoint::GCSafePoint(MemoryManager& mm, const uint32* image) : gcOpnds(mm) , ip(image[0]) {
    uint32 nOpnds = image[1];
    gcOpnds.reserve(nOpnds);
    uint32 offs = 2;
    for (uint32 i = 0; i< nOpnds; i++, offs+=3) {
        GCSafePointOpnd* gcOpnd= new (mm) GCSafePointOpnd(image[offs], int(image[offs+1]), int32(image[offs+2]));
#ifdef _DEBUG
        gcOpnd->firstId = image[offs+3];
        offs++;
#endif
        gcOpnds.push_back(gcOpnd);
    }
#ifdef _DEBUG
    instId = image[offs];
    hardwareExceptionPoint = (bool)image[offs+1];
#endif
}

uint32 GCSafePoint::getUint32Size() const {
    uint32 size = 1/*ip*/+1/*nOpnds*/+GCSafePointOpnd::IMAGE_SIZE_UINT32 * gcOpnds.size()/*opnds images*/;
#ifdef _DEBUG
    size++; //instId
    size++; //hardwareExceptionPoint
#endif
    return size;
}

void GCSafePoint::write(uint32* data) const {
    uint32 offs=0;
    data[offs++] = ip;
    data[offs++] = gcOpnds.size();
    for (uint32 i = 0, n = gcOpnds.size(); i<n; i++, offs+=3) {
        GCSafePointOpnd* gcOpnd = gcOpnds[i];
        data[offs] = gcOpnd->flags;
        data[offs+1] = gcOpnd->val;
        data[offs+2] = gcOpnd->mptrOffset;
#ifdef _DEBUG
        data[offs+3] = gcOpnd->firstId;
        offs++;
#endif
    }
#ifdef _DEBUG
    data[offs++] = instId;
    data[offs++] = (uint32)hardwareExceptionPoint;
#endif
    assert(offs == getUint32Size());
}

uint32 GCSafePoint::getIP(const uint32* image) {
    return image[0];
}

static inline void m_assert(bool cond)  {
#ifdef _DEBUG
    assert(cond);
#else
#ifdef WIN32
    if (!cond) {
        __asm {
            int 3;
        }
    }
#endif
#endif    
}

void GCMap::checkObject(DrlVMTypeManager& tm, const void* p)  {
    if (p==NULL) return;
    m_assert (!(p<(const void*)0x10000)); //(INVALID PTR)
    m_assert((((uint32)p)&0x3)==0); // check for valid alignment
    uint32 vtableOffset=tm.getVTableOffset();
    void * allocationHandle=*(void**)(((uint8*)p)+vtableOffset);
    m_assert (!(allocationHandle<(void*)0x10000 || (((uint32)allocationHandle)&0x3)!=0)); //INVALID VTABLE PTR
    ObjectType * type=tm.getObjectTypeFromAllocationHandle(allocationHandle);
    m_assert(type!=NULL); // UNRECOGNIZED VTABLE PTR;
}

void GCSafePoint::enumerate(GCInterface* gcInterface, const JitFrameContext* context, const StackInfo& stackInfo) const {
#ifdef ENABLE_GC_RT_CHECKS
    MemoryManager mm(256, "tmp");
    DrlVMTypeManager tm(mm);
#endif
    for (uint32 i=0, n = gcOpnds.size(); i<n; i++) {
        GCSafePointOpnd* gcOpnd = gcOpnds[i];
        uint32 valPtrAddr = getOpndSaveAddr(context, stackInfo, gcOpnd);
        if (gcOpnd->isObject()) {
#ifdef ENABLE_GC_RT_CHECKS
            GCMap::checkObject(tm, *(void**)valPtrAddr);
#endif
            gcInterface->enumerateRootReference((void**)valPtrAddr);
        } else {
            assert(gcOpnd->isMPtr());
            uint32 mptrAddr = *((uint32*)valPtrAddr); //
            //find base, calculate offset and report to GC
            if (gcOpnd->getMPtrOffset() == MPTR_OFFSET_UNKNOWN) {
                //we looking for a base that  a) located before mptr in memory b) nearest to mptr
                GCSafePointOpnd* baseOpnd = NULL;
                uint32 basePtrAddr = 0, baseAddr = 0;
                for (uint32 j =0; j<n; j++) {
                    GCSafePointOpnd* tmpOpnd = gcOpnds[j];   
                    if (tmpOpnd->isObject()) {
                        uint32 tmpPtrAddr = getOpndSaveAddr(context, stackInfo, tmpOpnd);
                        uint32 tmpBaseAddr = *((uint32*)tmpPtrAddr);
                        if (tmpBaseAddr <= mptrAddr) {
                            if (baseOpnd == NULL || tmpBaseAddr > baseAddr) {
                                baseOpnd = tmpOpnd;
                                basePtrAddr = tmpPtrAddr;
                                baseAddr = tmpBaseAddr;
                            } 
                        }
                    }
                }
                assert(baseOpnd!=NULL);
#ifdef ENABLE_GC_RT_CHECKS
                GCMap::checkObject(tm,  *(void**)basePtrAddr);
#endif 
                gcInterface->enumerateRootManagedReferenceWithBase((void**)valPtrAddr, (void**)basePtrAddr);
            } else { // mptr - base == static_offset
                int32 offset = gcOpnd->getMPtrOffset();
                assert(offset>=0);
#ifdef ENABLE_GC_RT_CHECKS
                char* mptrAddr = *(char**)valPtrAddr;
                GCMap::checkObject(tm, mptrAddr - offset);
#endif
                gcInterface->enumerateRootManagedReference((void**)valPtrAddr, offset);
            }
        }
    }
}

uint32 GCSafePoint::getOpndSaveAddr(const JitFrameContext* ctx,const StackInfo& stackInfo,const GCSafePointOpnd *gcOpnd)const {
    uint32 addr = 0;
    if (gcOpnd->isOnRegister()) {
        RegName regName = gcOpnd->getRegName();
        uint32* stackPtr = stackInfo.getRegOffset(ctx, regName);
        addr = (uint32)stackPtr;
    } else { 
        assert(gcOpnd->isOnStack());
        addr = ctx->esp + gcOpnd->getDistFromInstESP();
    }
    return addr;
}


void GCMapCreator::runImpl() {
    MemoryManager& mm=irManager.getMemoryManager();
    GCMap * gcMap=new(mm) GCMap(mm);
    irManager.calculateOpndStatistics();
    gcMap->registerInsts(irManager);
    irManager.setInfo("gcMap", gcMap);

    if (Log::cat_cg()->isIREnabled()) {
        gcMap->getGCSafePointsInfo()->dump(getTagName());
    }
}


//_______________________________________________________________________
// RuntimeInterface
//____________________________________________________ ___________________

static Timer *enumerateTimer=NULL;
void RuntimeInterface::getGCRootSet(MethodDesc* methodDesc, GCInterface* gcInterface, 
                                   const JitFrameContext* context, bool isFirst)
{  
    PhaseTimer tm(enumerateTimer, "ia32::gcmap::rt_enumerate");

/*    Class_Handle parentClassHandle = method_get_class(((DrlVMMethodDesc*)methodDesc)->getDrlVMMethod());
    const char* methodName = methodDesc->getName();
    const char* className = class_get_name(parentClassHandle);
    const char* methodSignature = methodDesc->getSignatureString();
    printf("enumerate: %s::%s %s\n", className, methodName, methodSignature);*/

    // Compute stack information
    uint32 stackInfoSize = StackInfo::getByteSize(methodDesc);
    Byte* infoBlock = methodDesc->getInfoBlock();
    Byte* gcBlock = infoBlock + stackInfoSize;
    const uint32* gcPointImage = GCMap::findGCSafePointStart((uint32*)gcBlock, *context->p_eip);
    if (gcPointImage != NULL) {
        MemoryManager mm(128,"RuntimeInterface::getGCRootSet");
        GCSafePoint gcSite(mm, gcPointImage);
        if (gcSite.getNumOpnds() > 0) { 
            //this is a performance filter for empty points 
            // and debug filter for hardware exception point that have no stack info assigned.
            StackInfo stackInfo(mm);
            stackInfo.read(methodDesc, *context->p_eip, false);
            gcSite.enumerate(gcInterface, context, stackInfo);
        }
    } else {
        //NPE + GC -> nothing to enumerate for this frame;
#ifdef _DEBUG
        assert(0); //in debug mode all hardware exceptions are saved as empty gcsafepoints
#endif 
    }
}

bool RuntimeInterface::canEnumerate(MethodDesc* methodDesc, NativeCodePtr eip) {  
    assert(0); 
    return FALSE;
}

}} //namespace
