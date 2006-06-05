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
 * @version $Revision: 1.14.14.3.4.4 $
 */

#include "Stl.h"
#include "Ia32StackInfo.h"
#include "Ia32RuntimeInterface.h"
#include "Ia32InternalTrace.h"
#include <math.h>

namespace Jitrino
{
namespace Ia32 {

#define CALL_MIN_SIZE 2
#define CALL_MAX_SIZE 6

uint32 hashFunc(uint32 key, uint32 hashTableSize) {
	return (key % hashTableSize);
}

	StackDepthInfo hashGet(DepthEntry** entries, uint32 key, uint32 hashTableSize) {
		DepthEntry * e = entries[hashFunc(key,hashTableSize)];
		assert(e);
		for(;e !=NULL;) {
			if(e->eip == key) 
				return e->info;
			else
				e = e->next;
			assert(e);
		}
		return e->info;
	}

	void hashSet(DepthEntry** entries, uint32 eip, uint32 hashTableSize,StackDepthInfo info, MemoryManager& mm) {
		uint32 key = hashFunc(eip,hashTableSize);
		DepthEntry * e = entries[key];
		if(!e) {
			entries[key] = new(mm) DepthEntry(eip, info, NULL);
		} else {
			for(;e->next!=NULL;e = e->next) ;
			e->next =  new(mm) DepthEntry(eip, info, NULL);
		}
	}

uint32 StackInfo::getByteSize() const 	{
	return byteSize ? 
			byteSize : 
			(sizeof(StackInfo)+(stackDepthInfo?stackDepthInfo->size()*sizeof(DepthEntry):0))+hashTableSize*4;
}

uint32 StackInfo::getByteSize(MethodDesc* md) {
    Byte* data =  md->getInfoBlock();
    return *(uint32*)data;
}

uint32 StackInfo::readByteSize(const Byte* bytes) const{
    uint32* data = (uint32 *) bytes;
    uint32 sizeInBytes = *data;

    return sizeInBytes;
}

typedef DepthEntry * EntryPtr;
void StackInfo::write(Byte* bytes) {
    Byte* data = bytes;
    StackInfo* serializedInfo = (StackInfo*)data;
    *serializedInfo = *this;
    serializedInfo->byteSize = getByteSize();
    data+=sizeof(StackInfo);
    MemoryManager mm(0x100, "DepthInfo");
	EntryPtr * entries = new(mm) EntryPtr[hashTableSize];
	for(uint32 i = 0; i< hashTableSize; i++)
		entries[i] = NULL;
	for(DepthMap::iterator dmit = stackDepthInfo->begin(); dmit != stackDepthInfo->end(); dmit++) {
		hashSet(entries, dmit->first, hashTableSize, dmit->second, mm);
	}
	Byte* next = data + hashTableSize * 4;
	for(uint32 i = 0; i< hashTableSize; i++) {
		DepthEntry * e = entries[i];
        uint32 serializedEntryAddr = 0;
		if(entries[i]) {
			serializedEntryAddr = (uint32)next;
            for(; e != NULL; e = e->next) {
                DepthEntry* serialized = (DepthEntry*)next;
                *serialized = *e;
                next+=sizeof(DepthEntry);
                serialized->next = e->next ? (DepthEntry*)next : NULL;
			}
		}
		*((uint32*)data)= serializedEntryAddr;
		data+=4;
	}
	assert(getByteSize() == (uint32) (((Byte*) next) - bytes));
}

DepthEntry * getHashEntry(Byte * data, uint32 eip, uint32 size) 
{
	if(!size)
		return NULL;
	uint32 key = hashFunc(eip,size);
   	data += sizeof(StackInfo) + key * sizeof(uint32);
	DepthEntry * entry = (DepthEntry *)*(uint32*)data;
	for(;entry && entry->eip != eip; entry = entry->next) {
		assert(entry!=entry->next);
	}
	return entry;
}

void StackInfo::read(MethodDesc* pMethodDesc, uint32 eip, bool isFirst) {
    Byte* data = pMethodDesc->getInfoBlock();
	byteSize = ((StackInfo *)data)->byteSize;
	hashTableSize = ((StackInfo *)data)->hashTableSize;
	frameSize = ((StackInfo *)data)->frameSize;
	icalleeOffset = ((StackInfo *)data)->icalleeOffset;
	icalleeMask = ((StackInfo *)data)->icalleeMask;
	localOffset = ((StackInfo *)data)->localOffset;
	offsetOfThis = ((StackInfo *)data)->offsetOfThis;
	itraceMethodExitString = ((StackInfo *)data)->itraceMethodExitString;
	
	if (!isFirst){
		DepthEntry * entry = getHashEntry(data, eip, hashTableSize);
		assert(entry);
		if (!entry){
			if (Log::cat_rt()->isFailEnabled())
				Log::cat_rt()->out()<<"eip=0x"<<(void*)eip<<" not found during stack unwinding!"<<::std::endl;
			::std::cerr<<"eip=0x"<<(void*)eip<<" not found during stack unwinding!"<<::std::endl;
		}
		callerSaveRegsConstraint =entry->info.callerSaveRegs;
		calleeSaveRegsMask = entry->info.calleeSaveRegs;
		stackDepth = entry->info.stackDepth;
	}else{
		DepthEntry * entry = NULL;
		uint32 i = CALL_MIN_SIZE;
		for (; i<= CALL_MAX_SIZE; i++) {
			entry = getHashEntry(data, eip + i, hashTableSize);
			if(entry)
				break;
		}
		if (entry && (entry->info.callSize == i)) {
			stackDepth = entry->info.stackDepth;
		} else {
			stackDepth = frameSize;
		}

		if (Log::cat_rt()->isDebugEnabled())
			Log::cat_rt()->out()<<"Hardware exception from eip=0x"<<(void*)eip<<::std::endl;
	}
}


void StackInfo::unwind(MethodDesc* pMethodDesc, JitFrameContext* context, bool isFirst) const {
   
	if (itraceMethodExitString!=NULL){
		Log::cat_rt()->out()<<"__UNWIND__:"
			<<(itraceMethodExitString!=(const char*)1?itraceMethodExitString:"")
			<<"; unwound from EIP="<<(void*)*context->p_eip
			<<::std::endl;
   	}

    context->esp += stackDepth;

	uint32 offset = context->esp;
	context->p_eip = (uint32 *) offset;

	offset += icalleeOffset; 
	if(getRegMask(RegName_EDI) & icalleeMask) {
		context->p_edi = (uint32 *)  offset;
		offset += 4;
	}
	if(getRegMask(RegName_ESI) & icalleeMask) {
		context->p_esi = (uint32 *)  offset;
		offset += 4;
	}
	if(getRegMask(RegName_EBP) & icalleeMask) {
		context->p_ebp = (uint32 *)  offset;
		offset += 4;
	}
	if(getRegMask(RegName_EBX) & icalleeMask) {
		context->p_ebx = (uint32 *)  offset;
	}
	context->esp += sizeof(POINTER_SIZE_INT);//EIP size == 4
}

uint32 * StackInfo::getRegOffset(const JitFrameContext* context, RegName reg) const
{
	assert(stackDepth >=0);
	if(calleeSaveRegsMask & getRegMask(reg)) { //callee save regs
        //regnames are hardcoded -> unwind() has hardcoded regs too.
		//return register offset for previous stack frame. 
		//MUST be called before unwind()
        switch(reg) {
            case RegName_ESI:
                return context->p_esi;
            case RegName_EDI:
                return context->p_edi;
            case RegName_EBP:
                return context->p_ebp;
            case RegName_EBX:
                return context->p_ebx;
	    default:
		assert(0);
		return NULL;
        }
    } else { //caller save regs
		assert(0);
		VERIFY_OUT("Caller save register requested!");
		exit(1);
	}
}

void StackInfo::fixHandlerContext(JitFrameContext* context)
{
	if (itraceMethodExitString!=NULL){
		Log::cat_rt()->out()<<"__CATCH_HANDLER__:"
			<<(itraceMethodExitString!=(const char*)1?itraceMethodExitString:"")
			<<"; unwound from EIP="<<(void*)*context->p_eip
			<<::std::endl;
	}
	context->esp -= frameSize - stackDepth;
}

void StackInfo::registerInsts(IRManager& irm) 
{
	if (!irm.getMethodDesc().isStatic()) {
		EntryPointPseudoInst * entryPointInst = irm.getEntryPointInst();
		offsetOfThis = (uint32)entryPointInst->getOpnd(0)->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement)->getImmValue();
	}
	for (CFG::NodeIterator it(irm); it!=NULL; ++it){
		if (((Node*)it)->hasKind(Node::Kind_BasicBlock)){
			BasicBlock * bb=(BasicBlock * )(Node*)it;
			const Insts& insts=bb->getInsts();
			for (Inst * inst=insts.getFirst(); inst!=NULL; inst=insts.getNext(inst)){
				if(inst->getMnemonic() == Mnemonic_CALL) {
					(*stackDepthInfo)[(uint32)inst->getCodeStartAddr()+inst->getCodeSize()]=
						StackDepthInfo(Constraint(),
						((CallInst *)inst)->getCallingConventionClient().getCallingConvention()->getCalleeSavedRegs(OpndKind_GPReg), 
						inst->getStackDepth(),
						inst->getCodeSize());
				} 
			}
		}
	}
	hashTableSize = stackDepthInfo->size();
}

void StackInfo::setMethodExitString(IRManager& irm)
{
	ConstantAreaItem * cai=(ConstantAreaItem *)irm.getInfo("itraceMethodExitString");
	if (cai!=NULL){
		itraceMethodExitString=(const char*)cai->getAddress();
		if (itraceMethodExitString==NULL)
			itraceMethodExitString=(const char*)1;
	}
}


}} //namespace

