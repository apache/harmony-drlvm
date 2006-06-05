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
 * @author Sergey L. Ivashin
 * @version $Revision: 1.10.20.3 $
 */

#include "Ia32RegAllocCheck.h"
#include "Log.h"
#include "Timer.h"


namespace Jitrino
{

Timer * regAllocCheckTimer=NULL;

namespace Ia32
{

using ::std::endl;
using ::std::ostream;


//========================================================================================
// utility routines
//========================================================================================


static ostream& operator << (ostream& os, const Inst& x)
{
	return os << "I#" << x.getId();
}


static ostream& operator << (ostream& os, const Opnd& x)
{
	return os << "O#" << x.getFirstId();
}


static ostream& operator << (ostream& os, RegName x)
{
	return os << getRegNameString(x);
}


//========================================================================================
// class RegAllocCheck
//========================================================================================

bool RegAllocCheck::run (bool checkloc)
{
	PhaseTimer tm(regAllocCheckTimer, "ia32::regAllocCheck");

	opandcount = irm.getOpndCount();
	errors = 0;
	headprinted = false;

	if (checkloc)
		checkLocations();

	const Node* node;
	for (CFG::NodeIterator it(irm); (node = it.getNode()) != 0; ++it)
		if (node->hasKind(Node::Kind_BasicBlock))
		{
			bblock = static_cast<const BasicBlock*>(node);
			binsts = &bblock->getInsts();
			checkLiveness();
			checkConstraints();
		}

	if (errors != 0)
		header() << endl << "ERRORS DETECTED: " << errors << endl;	 
	else if (Log::cat_cg()->isDebugEnabled())
		header() << endl << "No errors detected" << endl;	 

	return errors == 0;
}																				


const size_t MaxRegs = IRMaxRegNamesSameKind*IRMaxRegKinds;


static RegName regName (int x)
{
	size_t k = x / IRMaxRegNamesSameKind;
	return getRegName((OpndKind)k, Constraint::getDefaultSize(k), x % IRMaxRegNamesSameKind);
}


static size_t regIdx (const Opnd* opnd)
{
	RegName rn;
	if ((rn = opnd->getRegName()) == RegName_Null)
		return MaxRegs;

	size_t x = getRegIndex(rn),
	       k = getRegKind(rn); 

	assert(x < IRMaxRegNamesSameKind && k < IRMaxRegKinds); 

	return k*IRMaxRegNamesSameKind + x;
}


void RegAllocCheck::checkLiveness ()
{
	lastbb = 0;

	LiveSet lives(mm, opandcount);
	irm.getLiveAtExit(bblock, lives);

	Opnd* regdefs[MaxRegs],
	    * reguses[MaxRegs],
	    * regnxts[MaxRegs];

	size_t ridx;

	for (ridx = 0; ridx != MaxRegs; ++ridx)
		regdefs[ridx] = 0,
		reguses[ridx] = 0,
		regnxts[ridx] = 0;

	Opnd* opnd;
	LiveSet::IterB ls(lives);
	for (int i = ls.getNext(); i != -1; i = ls.getNext())
	{
		opnd = irm.getOpnd(i);
		if ((ridx = regIdx(opnd)) < MaxRegs)
		{
			if (regnxts[ridx] == 0)
				regnxts[ridx] = opnd;
			else
				if (regnxts[ridx] != opnd)
				{
					error() << "at end of block," << regName(ridx) 
						    << " assigned to " << *regnxts[ridx] << " and " << *opnd << endl;
				}
		}
	}

//	iterate over instructions towards the top of the block

	for (const Inst* inst = binsts->getLast(); inst != 0; inst = binsts->getPrev(inst))
	{
		const uint32 props = inst->getProperties();

	//	In general, operands can be stored in arbitrary order
		Inst::Opnds opnds(inst, Inst::OpndRole_All);
		for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it)){
			opnd = opnds.getOpnd(it);
			if ((ridx = regIdx(opnd)) < MaxRegs)
			{
				const uint32 roles = inst->getOpndRoles(it);

				if ((roles & Inst::OpndRole_Def) != 0)
				{
					if (regdefs[ridx] == 0)
						regdefs[ridx] = opnd;
					else
						if (regdefs[ridx] != opnd)
						{
							error() << "  Invalid definitions at " << *inst << " " << regName(ridx) 
								    << " of " << *regdefs[ridx] << " or " << *opnd << endl;
						}
				}

				if ((roles & Inst::OpndRole_Use) != 0)
				{
					if (reguses[ridx] == 0)
						reguses[ridx] = opnd;
					else
						if (reguses[ridx] != opnd)
						{
							error() << "  Invalid usages at " << *inst << " " << regName(ridx) 
								    << " of " << *reguses[ridx] << " or " << *opnd << endl;
						}
				}
			}
		}
		for (ridx = 0; ridx != MaxRegs; ++ridx)
		{
			if (regdefs[ridx] != 0)
			{
				//	conditional definitions are ignored
				if ((props & Inst::Properties_Conditional) == 0)
				{
					if (regdefs[ridx] != regnxts[ridx])
						if (regnxts[ridx] != 0)
						{
							error() << "  " << *inst << " " << regName(ridx) 
									<< " invalid usage of " << *regnxts[ridx]
									<< " instead of " << *regdefs[ridx] << endl;
						}
						else
						{
							//	dead def
						}

					regnxts[ridx] = 0;	// this register can used in the instruction
				}
			}

			else if (reguses[ridx] != 0)
			{
				if (reguses[ridx] != regnxts[ridx] && regnxts[ridx] != 0)
				{
					error() << "  " << *inst << " " << regName(ridx) 
							<< " invalid usage of " << *reguses[ridx]
							<< " or " << *regnxts[ridx] << endl;
				}

				regnxts[ridx] = reguses[ridx];
			}

		// clear for next instruction to be processed
			regdefs[ridx] = 0;
			reguses[ridx] = 0;
		}
	}
}


void RegAllocCheck::checkLocations ()
{
	for (size_t i = 0; i != opandcount; ++i)
	{
		Opnd* opnd = irm.getOpnd(i);
		if (!opnd->hasAssignedPhysicalLocation())
		{
			header() << "Not assigned opand " << *opnd << endl;
			errors++;
		}
	}
}


void RegAllocCheck::checkConstraints ()
{
	lastbb = 0;
	for (const Inst* inst = binsts->getFirst(); inst != 0; inst = binsts->getNext(inst))
	{
		Opnd* opnd;
		Inst::Opnds opnds(inst, Inst::OpndRole_AllDefs);
		for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it))
		{
			opnd = opnds.getOpnd(it);
			Constraint cloc = opnd->getConstraint(Opnd::ConstraintKind_Location);
			Constraint c = inst->getConstraint(Inst::ConstraintKind_Current, it, cloc.getSize());
			if (!cloc.isNull())
				if (c.getSize() == cloc.getSize() && !c.contains(cloc))
					error() << "Constraint error " << *inst << " " << *opnd << endl;
		}
	}
}


ostream& RegAllocCheck::error ()
{
	if (lastbb != bblock)
	{
		lastbb = bblock;
		header() << "BasicBlock " << bblock->getId() 
			     << "[" << binsts->getFirst()->getId() << " .. " 
				 << binsts->getLast() ->getId() << "]" 
				 << endl ;
	}
	errors++;
	return Log::out();
}


ostream& RegAllocCheck::header ()
{
	if (!headprinted)
	{
		headprinted = true;

		MethodDesc& md=irm.getMethodDesc();
		Log::out() << endl << "RegAllocCheck for " 
		           << md.getParentType()->getName() 
				   << "." << md.getName() << "(" << md.getSignatureString() << ")"
				   << endl;	 
	}
	return Log::out();
}


} //namespace Ia32
} //namespace Jitrino
