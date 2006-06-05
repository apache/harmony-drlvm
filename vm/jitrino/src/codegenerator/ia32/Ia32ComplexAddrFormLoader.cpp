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
 * @version $Revision: 1.10.12.2.4.3 $
 */


#include "Ia32ComplexAddrFormLoader.h"
namespace Jitrino
{
namespace Ia32 {

void
ComplexAddrFormLoader::runImpl() {
	if (parameters) {
		refCountThreshold = atoi(std::string(parameters).c_str());
		if(refCountThreshold < 2) {
			refCountThreshold = 2;
			assert(0);
		}
	} else {
		refCountThreshold = 2;
	}
	StlMap<Opnd *, bool> memOpnds(irManager.getMemoryManager());
	uint32 opndCount = irManager.getOpndCount();
	irManager.calculateOpndStatistics();
	for (uint32 i = 0; i < opndCount; i++) {
		Opnd * opnd = irManager.getOpnd(i);
		if(opnd->isPlacedIn(OpndKind_Mem)) {
			Opnd * baseOp = opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Base);
			Opnd * indexOp = opnd->getMemOpndSubOpnd(MemOpndSubOpndKind_Index);
			if (baseOp && (!indexOp)) {
				StlMap<Opnd *, bool>::iterator it = memOpnds.find(baseOp);
				if(it == memOpnds.end() || it->second) {
					memOpnds[baseOp]=findAddressComputation(opnd);
				}
			}
		}
	}
}

bool
ComplexAddrFormLoader::findAddressComputation(Opnd * memOp) {

	
	Opnd * disp = memOp->getMemOpndSubOpnd(MemOpndSubOpndKind_Displacement);
	Opnd * base = memOp->getMemOpndSubOpnd(MemOpndSubOpndKind_Base);
	
	Inst * inst = base->getDefiningInst();
	if (!inst)
		return true;

	BasicBlock * bb = inst->getBasicBlock();

	SubOpndsTable table(base, disp);
	walkThroughOpnds(table);
	if(!table.baseOp)
		table.baseOp = table.suspOp;
	
	if(base->getRefCount() > refCountThreshold) {
		if (table.indexOp) {
			bb->appendInsts(irManager.newInst(Mnemonic_LEA, base, irManager.newMemOpnd(irManager.getTypeManager().getIntPtrType(), table.baseOp, table.indexOp, table.scaleOp, table.dispOp)), inst);
			return false;
		}
	} else {
		if (table.baseOp) {
			memOp->setMemOpndSubOpnd(MemOpndSubOpndKind_Base, table.baseOp);
		} 
		if (table.indexOp) {
			memOp->setMemOpndSubOpnd(MemOpndSubOpndKind_Index, table.indexOp);
			if (table.scaleOp) {
				memOp->setMemOpndSubOpnd(MemOpndSubOpndKind_Scale, table.scaleOp);
			} else {
				assert(0);
			}
		}
		if (table.dispOp) {
			memOp->setMemOpndSubOpnd(MemOpndSubOpndKind_Displacement, table.dispOp);

		}
	}
	return true;
}//end ComplexAddrFormLoader::findAddressComputation

bool 
ComplexAddrFormLoader::checkIsScale(Inst * inst) {
	Opnd * opnd = inst->getOpnd(3);
	if(opnd->isPlacedIn(OpndKind_Imm)) {
		switch(opnd->getImmValue()) {
			case 1:
			case 2:
			case 4:
			case 8:	
				return true;
			default:
				return false;
		}
	}
	return false;
}

void
ComplexAddrFormLoader::walkThroughOpnds(SubOpndsTable& table) {
	
	Opnd * opnd;
	if (table.baseCand1)
		opnd = table.baseCand1;
	else if(table.baseCand2)
		opnd = table.baseCand2;
	else
		opnd = table.suspOp;
	
	Inst * instUp = opnd->getDefiningInst();

	for(;instUp!=NULL && instUp->getMnemonic() == Mnemonic_MOV;instUp = instUp->getOpnd(1)->getDefiningInst());
	if(!instUp) {
		if(!table.baseOp)
			table.baseOp = opnd;
		if (table.baseCand1) {
			table.baseCand1 = NULL;
			walkThroughOpnds(table);
		}
		return;
	} 

	uint32 defCount = instUp->getOpndCount(Inst::OpndRole_InstLevel|Inst::OpndRole_Def);
	if(instUp->getMnemonic()==Mnemonic_ADD) {
		if(instUp->getOpnd(defCount+1)->isPlacedIn(OpndKind_Imm)) {
			if(table.dispOp) {
				if (table.dispOp->getType()->isInteger() && instUp->getOpnd(defCount+1)->getType()->isInteger()) {
					table.suspOp = instUp->getOpnd(defCount);
					table.dispOp = irManager.newImmOpnd(table.dispOp->getType(), table.dispOp->getImmValue() + instUp->getOpnd(defCount+1)->getImmValue());
				}
				return;
			} else {
				table.suspOp = instUp->getOpnd(defCount);
				table.dispOp=instUp->getOpnd(defCount+1);			
			}
			if (table.baseCand1)
				table.baseCand1 = NULL;
			walkThroughOpnds(table);
		}else if(table.baseCand1) {
			assert(!table.baseOp);
			table.baseOp = table.baseCand1;
			table.baseCand1 = NULL;
			walkThroughOpnds(table);
		}else if(table.baseCand2) {
			assert(table.baseOp);
			table.baseOp = table.suspOp;
		}else if(!table.baseOp) {
			table.baseCand1 = instUp->getOpnd(defCount);
			table.baseCand2 = instUp->getOpnd(defCount+1);
			walkThroughOpnds(table);
		} else {
			table.baseOp = table.suspOp;
		}
	} else if(instUp->getMnemonic()==Mnemonic_IMUL && checkIsScale(instUp)) {
		table.indexOp = instUp->getOpnd(defCount);
		table.scaleOp =  instUp->getOpnd(defCount+1);
		if(table.baseCand1) {
			table.baseCand1 = NULL;
			table.suspOp = table.baseCand2;
			table.baseCand2 = NULL;
			walkThroughOpnds(table);
		} 
	} else {
		table.baseOp = opnd;
	}
}
} //end namespace Ia32
}
