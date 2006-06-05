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
 * @version $Revision: 1.13.20.3 $
 */

#include "Ia32RegAlloc2.h"
#include "Ia32RegAllocCheck.h"
#include "Stl.h"
#include "Log.h"

namespace Jitrino
{

namespace Ia32
{


//========================================================================================
// class RegAlloc2::Span
//========================================================================================

struct RegAlloc2::Span
{
	Instnb beg, end;
};


//========================================================================================
// class RegAlloc2::Opand
//========================================================================================

struct RegAlloc2::Opand
{
	Opnd* opnd;				// 0 for fictious (preassigned) operand or pointer to LIR
	Register* assigned;		// 0 or register assigned

//	No two intervals can have one common point, so they can be ordered with Span::Less
	typedef StlVector<Span> Spans;
	Spans spans;			// liveness data in form of list of intervals

	Instnb beg, end;		// start point of first interval and end point of last interval
	size_t length;			// total length of all intervals
	size_t weight;			// weight coeff (taken from LIR)

    Opand* parentOpand; 	// another opand which is a source in a copy instruction defining this opand, hint for coalescing

	Opand (MemoryManager& mm)							:spans(mm) {}

	bool	startOrExtend  (Instnb instIdx);
	void    stop   (Instnb instIdx);

	void update ();
	bool conflict (const Opand*, int&) const;

	static bool smaller (const RegAlloc2::Opand*& x, const RegAlloc2::Opand*& y)
	{	return x->end - x->beg < y->end - y->beg;	}

	static bool lighter (const RegAlloc2::Opand* x, const RegAlloc2::Opand* y)
	{	return x->weight < y->weight;	}

};


//========================================================================================
// class RegAlloc2::Register
//========================================================================================

struct RegAlloc2::Register
{
	RegMask regmask;		// mask to identify this register (search key)
	RegName regname;
	Constraint constraint;	// corresponding to this register

	Opand  preassigned;		// fictious operand to describe assignments by code selector

	typedef StlList<Opand*> Opands;
	Opands assigned;		// all operands assigned to this register by allocator

	size_t length;			// total length of intervals of all operands

	Instnb beg, end;		// start point of first interval and end point of last interval	of all assigned operands

	Register (MemoryManager& mm)						:preassigned(mm), assigned(mm) {}

	bool canBeAssigned (const Opand*, int&) const;
	void assign (Opand*);
};


//========================================================================================
// Internal debug helpers
//========================================================================================
using ::std::endl;
using ::std::ostream;


#ifdef _DEBUG_REGALLOC
#	define DBGOUT(x) if (Log::cat_cg()->isDebugEnabled()) Log::out()<<x;
#else
#	define DBGOUT(x) 
#endif


static ostream& operator << (ostream& os, const RegAlloc2::Opand::Spans& x)
{
	for (RegAlloc2::Opand ::Spans::const_iterator i = x.begin(); i != x.end(); ++i)
		os << " [" << i->beg << "," << i->end << "]";
	return os;
}

static ostream& operator << (ostream& os, const RegAlloc2::Opand& x)
{
	os << "Opand{";

	if (x.opnd != 0)
		os << "#" << x.opnd->getFirstId()<<"("<<x.opnd->getId()<<")";
	else
		os << "~";

	os << " W:" << x.weight << " L:" << x.length;
	os << " beg:" << x.beg << " end:" << x.end << " " << x.spans ;

	if (x.assigned != 0)
		os << " in " << getRegNameString(x.assigned->regname);

	return os << "}";
}

static ostream& operator << (ostream& os, const RegAlloc2::Register& x)
{
	return os << "Reg{" << getRegNameString(x.regname) << " L:" << x.length << " beg:" << x.beg << " end:" << x.end << "}";
}


//========================================================================================
// class Ia32RegAlloc2::Opand
//========================================================================================

bool	RegAlloc2::Opand::startOrExtend (Instnb instIdx)
{	
	Span * last = spans.size() > 0 ? &spans.back() : NULL;

	if ( last != NULL && instIdx == last->beg ){ // open the last span 
		last->beg=0; 
		return true;
	}
	if (last == NULL || last->beg != 0)
	{// insert open span
		Span tmp={ 0, instIdx };
		spans.push_back(tmp);
		return true;
	}
//  continue the same open use
	return false;
}

void    RegAlloc2::Opand::stop (Instnb instIdx)
{	
	Span * last=spans.size()>0?&spans.back():NULL;
	if ( last==NULL || ( last->beg != 0 && last->beg !=instIdx ) ){ // closed
		Span tmp={ instIdx, instIdx }; // probably dead def, add the shortest possible span
		spans.push_back(tmp);
	}else{ // last!=NULL && ( last->beg == 0 || last->beg ==instIdx  ) 
		last->beg=instIdx;
	}	
}	

//	Helper function for update()
//
static bool less (const RegAlloc2::Span& x, const RegAlloc2::Span& y)	
{
	return x.beg < y.end; // to order properly spans like [124,130] [124,124] although there should not be such things
}

//	Complete liveness calculation - sort intervals and set endpoints
//
void RegAlloc2::Opand::update ()
{
	if ( opnd != NULL ){  // pre-assigned operands are not candidates anyway, no need to update their weight
		Constraint c = opnd->getConstraint(Opnd::ConstraintKind_Calculated, OpndSize_Default);
		weight *= (17 - countOnes(c.getMask())) * ( (c & Constraint(OpndKind_Mem, c.getSize())).isNull() ? 2 : 1);
	}

	if (spans.size() != 0)
	{
		Span * pspans = &spans.front();
		sort(spans.begin(), spans.end(), less);
		beg = spans.front().beg,
		end = spans.back().end;
		length=0;
		for (int i=0, n=spans.size(); i<n; i++){
			assert(pspans[i].beg != 0);
			length += pspans[i].end - pspans[i].beg; // no +1 as this is the number of instructions "hovered" by this opand
#ifdef _DEBUG
			assert(pspans[i].beg <= pspans[i].end);
			if ( i < n-1 ){
				assert(pspans[i].end < pspans[i+1].beg);
			}
#endif
		}
		assert( opnd == NULL || beg > 0);
	}
	else
	{
		assert(0);
	}
	assert(beg <= end);
}


//	Determinate if two operands conflict (i.e. can be assigned to the same register)
//  The algorithm executes in linear time
//
bool RegAlloc2::Opand::conflict (const RegAlloc2::Opand* x, int& adj) const
{
	int d;
	if (beg <= x->end && x->beg <= end)
	{
		Spans::const_iterator 
			i = spans.begin(),		iend = spans.end(), 
			k = x->spans.begin(),	kend = x->spans.end(); 

		while (i != iend && k != kend)
		{
			if ((d = i->end - k->beg) < 0)
			{
				if (d == -1)
					++adj;
				++i;
			}
			else if ((d = i->beg - k->end) > 0)
			{
				if (d == 1)
					++adj;
				++k;
			}
			else
				return true;
		}
	}

	return false;
}


//========================================================================================
// class Ia32RegAlloc2::Register
//========================================================================================


//	Test if this register can be assigned to the operand 'x'
//
bool RegAlloc2::Register::canBeAssigned (const RegAlloc2::Opand* x, int& adj) const
{
	DBGOUT("    try " << *this;)

	if (!x->opnd->canBePlacedIn(constraint))
	{
		DBGOUT(" -constraint" << endl;)
		return false;
	}

	if (preassigned.conflict(x, adj))
	{
		DBGOUT(" -preassigned" << endl;)
		return false;
	}

	adj = 0;

	int d;
	if ( (d = x->end - beg) < 0 ){
		if (d==-1)
			adj=1;
		DBGOUT(" -free (below lower bound)!" << endl;)
		return true;
	}
	if ( (d = x->beg - end) > 0 ){
		if (d==1)
			adj=1;
		DBGOUT(" -free (above upper bound)!" << endl;)
		return true;
	}
		
	for (Opands::const_iterator i = assigned.begin(); i != assigned.end(); ++i)
		if ((*i)->conflict(x, adj))
		{
			DBGOUT(" -conflict " << **i << endl;)
			return false;
		}

	DBGOUT(" -free (full check)!" << endl;)
	return true;
}


//	Assign opand 'x' to this register
//
void RegAlloc2::Register::assign (RegAlloc2::Opand* x) 
{
	assigned.push_back(x);
	x->assigned = this;
	if (x->beg<beg)
		beg=x->beg;
	if (x->end>end)
		end=x->end;
	length += x->length;
}

//========================================================================================
// class Ia32RegAlloc2
//========================================================================================


RegAlloc2::RegAlloc2 (IRManager& irm, const char* params)	
	:IRTransformer(irm, params), mm(1000, "RegAlloc2"), registers(mm), opandmap(mm)
{
	if (Log::cat_cg()->isDebugEnabled()){
		MethodDesc& md=irManager.getMethodDesc();
		const char * methodName = md.getName();
		const char * methodTypeName = md.getParentType()->getName();
		const char * methodSignature= md.getSignatureString();
		Log::out() << "Contructed <" << methodTypeName << "::" << methodName << methodSignature << ">" << endl;
	}
}


RegAlloc2::~RegAlloc2 ()	
{
	if (Log::cat_cg()->isDebugEnabled()){
		Log::out() << "Destructed" << endl;
	}
}


void RegAlloc2::runImpl()
{
	assert(parameters);
	if (strcmp(parameters, "EAX|ECX|EDX|EBX|ESI|EDI|EBP") == 0)
		constrs = Constraint(RegName_EAX)
				 |Constraint(RegName_ECX)
				 |Constraint(RegName_EDX)
				 |Constraint(RegName_EBX)
				 |Constraint(RegName_ESI)
				 |Constraint(RegName_EDI)
				 |Constraint(RegName_EBP);

	else if (strcmp(parameters, "XMM0|XMM1|XMM2|XMM3|XMM4|XMM5|XMM6|XMM7") == 0)
		constrs = Constraint(RegName_XMM1)
				 |Constraint(RegName_XMM2)
				 |Constraint(RegName_XMM3)
				 |Constraint(RegName_XMM4)
				 |Constraint(RegName_XMM5)
				 |Constraint(RegName_XMM6)
				 |Constraint(RegName_XMM7);

	else
	{
		constrs = Constraint(parameters);
		constrs.intersectWith(Constraint(OpndKind_Reg, Constraint::getDefaultSize(constrs.getKind())));
	}
	assert(!constrs.isNull());

	buildRegs();

	buildOpands();

	if (candidateCount == 0)
		return;

	allocateRegs();

	if (Log::cat_cg()->isDebugEnabled()){
		Log::out() << endl << "Result" << endl;

		int count1 = 0, count2 = 0;

		Opand* opand;
		for (size_t i = 0; i != opandmap.size(); ++i)
			if ((opand = opandmap[i]) != 0 && opand->opnd != 0)
			{
				Log::out() << "  " << i << ") " << *opand << endl;
				if (opand->assigned != 0)
				{
					Log::out() << "    +assigned " << *opand->assigned << endl;
					++count1;
				}
				else
				{
					Log::out() << "    -spilled " << endl;
					++count2;
				}
			}

		Log::out() << endl << "Registers" << endl;
		for (Registers::const_iterator it = registers.begin(); it != registers.end(); ++it)
		{
			Register& r = **it;
			Log::out() << r;
	
			if (!r.preassigned.spans.empty())
				Log::out() << " preassigned:" << r.preassigned.spans;
	
			if (!r.assigned.empty())
			{
				Log::out() << " assigned:";
				for (Register::Opands::const_iterator kt = r.assigned.begin(); kt != r.assigned.end(); ++kt)
					Log::out() << (*kt)->spans;
			}
			Log::out() << endl;
		}

		Log::out() << endl << "Assigned: " << count1 << "  spilled: " << count2 << endl;
	}
}

bool RegAlloc2::verify (bool force)
{	
	bool failed=false;
	if (force||irManager.getVerificationLevel()>=2){
		RegAllocCheck chk(irManager);
		if (!chk.run(false))
			failed=true;
		if (!IRTransformer::verify(force))
			failed=true;
	}
	return !failed;
}	

//	Initialize list of all available registers
//
void RegAlloc2::buildRegs ()	
{
	if (Log::cat_cg()->isDebugEnabled()){
		Log::out() << endl << "buildRegs" << endl;
	}

	OpndKind k = (OpndKind)constrs.getKind(); 
	OpndSize s = constrs.getSize();

	registers.clear();
	RegMask mask = constrs.getMask();
	for (RegMask m = 1, x = 0; mask != 0; m <<= 1, ++x)
		if ((mask & m))
		{   
			mask ^= m;
			Register* r = new (mm) Register(mm);
			r->regmask = m,
			r->regname = getRegName(k, s, x);
			r->constraint = Constraint(k, s, m);
			r->length = 0;
			r->beg=(Instnb)UINT_MAX;
			r->end=(Instnb)0;

			Opand& op = r->preassigned;
			op.opnd   = 0;	// because this is a fictious operand
			op.length = 0;
			op.weight = 0;
			op.assigned = r;
			op.parentOpand = 0;
			registers.push_back(r);
		}
}


RegAlloc2::Register* RegAlloc2::findReg (RegMask mask) const
{
	for (Registers::const_iterator it = registers.begin(); it != registers.end(); ++it)
		if ((*it)->regmask == mask)
			return *it;

	return 0;
}


void RegAlloc2::buildOpands ()	
{
	if (Log::cat_cg()->isDebugEnabled()){
		Log::out() << endl << "buildOpands" << endl;
	}

	opandcount = irManager.getOpndCount();
    candidateCount = 0;
	int registerPressure = 0;
	opandmap.clear();
	opandmap.reserve(opandcount);

	for (uint32 i = 0; i != opandcount; ++i)
	{
		Opnd*  opnd  = irManager.getOpnd(i);
		Opand* opand = 0;

		if (opnd->isPlacedIn(constrs))
		{
			Register* r = findReg(getRegMask(opnd->getRegName()));
			if (r != 0)
				opand = &r->preassigned;
		}

		else if (opnd->isAllocationCandidate(constrs))
		{
			opand = new (mm) Opand(mm);
			opand->opnd = opnd;
			opand->length = 0;
			opand->weight = 0;
			opand->assigned = 0;
   			opand->parentOpand = 0;
            ++candidateCount;
		}

		opandmap.push_back(opand);
	}
	if (candidateCount == 0)
		return;

	const Node* node;
	LiveSet lives(mm, opandcount);

	for (CFG::NodeIterator it(irManager, CFG::OrderType_Postorder); (node = it.getNode()) != 0; ++it)
		if (node->hasKind(Node::Kind_BasicBlock))
		{
			int execCount = (unsigned)node->getExecCnt();
			if (execCount < 1)
				execCount = 1;
			execCount *= 100; 

			const Insts& insts = ((BasicBlock*)node)->getInsts();
			const Inst*  inst  = insts.getLast();
			if (inst == 0)
				continue;

			uint32 instIndex=inst->getIndex();
			Opand * opand;

		//	start with the operands at the block bottom
			irManager.getLiveAtExit(node, lives);
			LiveSet::IterB ib(lives);
			for (int x = ib.getNext(); x != -1; x = ib.getNext())
				if ( (opand=opandmap[x]) != 0 ){
					if (opand->startOrExtend(instIndex + 1))
						++registerPressure;
				}

		//	iterate over instructions towards the top of the block

			for (; inst != 0; inst = insts.getPrev(inst))
			{
				instIndex=inst->getIndex();
				const Opnd* opnd;
				Opand* definedInCopyOpand = 0;
				Inst::Opnds opnds(inst, Inst::OpndRole_All);
				for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it)){
					opnd = opnds.getOpnd(it);
					if ( (opand=opandmap[opnd->getId()]) != 0 )
					{
						opand->weight += execCount * (registerPressure > (int)registers.size() ? 4 : 1);
						if (Log::cat_cg()->isDebugEnabled()){
							Log::out() << " Pressure: " << registerPressure << "/" << registers.size() << " Opand: " << *opand << endl;
						}

						if (inst->isLiveRangeEnd(it)){
							opand->stop(instIndex + 1);
							--registerPressure;
							if (inst->getMnemonic() == Mnemonic_MOV)
								definedInCopyOpand = opand;
						}else{
							if (opand->startOrExtend(instIndex)){
								++registerPressure;
								if ( definedInCopyOpand != 0 && inst->getMnemonic() == Mnemonic_MOV && definedInCopyOpand->parentOpand == 0){
                                    definedInCopyOpand->parentOpand = opand;
									if (Log::cat_cg()->isDebugEnabled()){
										Log::out() << *definedInCopyOpand << " => " << *opand << endl;
									}
								}
							}
						}
					}
					if (registerPressure<0) // if there were dead defs
						registerPressure=0;
				}
			}

		//	process operands at the top of the block
			inst = insts.getFirst();
			instIndex=inst->getIndex();
			LiveSet* tmp = irManager.getLiveAtEntry(node);

			ib.init(*tmp);
			for (int x = ib.getNext(); x != -1; x = ib.getNext())
				if ( (opand=opandmap[x]) != 0 ){
					opand->stop(instIndex);
					--registerPressure;
				}

			if (registerPressure<0) // TODO: why?
				registerPressure=0;
		}

//	for each operand, sort all its intervals (spans)

	for (Opands::iterator it = opandmap.begin(); it != opandmap.end(); ++it)
		if (*it != 0)
			(*it)->update();

	for (Opands::iterator it = opandmap.begin(); it != opandmap.end(); ++it){
		Opand * opand = *it;
		if (opand != 0){
			int adj;
			if (opand->parentOpand != 0 &&
				opand->length < opand->parentOpand->length &&
				opand->length < 3 && 
				!opand->conflict(opand->parentOpand, adj)
				){
				// can coalesce
            	if (opand->weight >= opand->parentOpand->weight){
	            	opand->weight = opand->parentOpand->weight; // make sure it is handled after its parent operand
               		++opand->parentOpand->weight;
	            }
			}
		}
	}

	if (Log::cat_cg()->isDebugEnabled()){
		Log::out() << endl << "opandmap" << endl;
		for (Opands::iterator it = opandmap.begin(); it != opandmap.end(); ++it)
		{
			Log::out() << "  "; 
			if (*it != 0)
				Log::out() << **it;
			else
				Log::out() << "-";
			Log::out() << endl;
		}
	}
}

void RegAlloc2::allocateRegs ()	
{
	if (Log::cat_cg()->isDebugEnabled()){
		Log::out() << endl << "allocateRegs" << endl;
	}

	Opands opands(mm);
	opands.reserve(opandmap.size());

	Opand* nxt;
	for (Opands::const_iterator i = opandmap.begin(); i != opandmap.end(); ++i){
		if ((nxt = *i) != 0 && nxt->assigned == 0)
			opands.push_back(nxt);
	}

	sort(opands.begin(), opands.end(), Opand::lighter);

	for (Opands::reverse_iterator i = opands.rbegin(); i != opands.rend(); ++i)
	{
		nxt = *i;

		DBGOUT("  nxt " << *nxt << endl;)

	//	try to find free regsiter for opand 'nxt'
		Register* better = 0;
		Register* best   = 0;
		int bestadj = 0, adj=0;

		for (Registers::iterator k = registers.begin(); k != registers.end(); ++k)
			if ((*k)->canBeAssigned(nxt, adj))
			{
				DBGOUT("    found " << **k << " adj:" << adj << endl;)

				if (better == 0 || better->length < (*k)->length)
					better = *k;

				if (adj != 0)
					if (best == 0 || adj > bestadj)
						best = *k, bestadj = adj;
			}

		if (best == 0)
			best = better; 

		if (best != 0)
		{
			DBGOUT("    assigned " << *best << endl;)
			best->assign(nxt);
			nxt->opnd->assignRegName(best->regname);
		}
	}
}


} //namespace Ia32
} //namespace Jitrino
