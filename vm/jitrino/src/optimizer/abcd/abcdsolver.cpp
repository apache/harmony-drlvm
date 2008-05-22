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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.14.24.4 $
 *
 */

#include <assert.h>
#include <ostream>
#include "Stl.h"
#include "Log.h"
#include "constantfolder.h"
#include "abcd.h"
#include "abcdbounds.h"
#include "abcdsolver.h"
#include "opndmap.h"
#include "optarithmetic.h"

namespace Jitrino {

AbcdSolver::AbcdSolver(Abcd *pass) : 
    thePass(pass), cache(pass->mm), active(pass->mm), indent("")
{
}

AbcdSolver::~AbcdSolver()
{
}


// try to prove (var2 - var1 <= c)
bool AbcdSolver::demandProve(bool prove_lower_bound,
                             const VarBound &var1,
                             const VarBound &var2,
                             int64 c,
                             AbcdReasons *why)
{
    return (prove(prove_lower_bound, var1, var2, ConstBound(c), why) 
            != ProofLattice::ProvenFalse);
}

// if prove_lower_bound, 
//     try to prove var2 - var1 >= c
// else
//     try to prove var2 - var1 <= c
ProofLattice AbcdSolver::prove(bool prove_lower_bound,
                               const VarBound &var1, // a
                               const VarBound &var2, // v
                               ConstBound c,
                               AbcdReasons *why)
{
    pushindent saveindent(this);
    if (prove_lower_bound) {
        ConstBound negC = -c;
        if (Log::isEnabled()) {
            Log::out() << indent.c_str() << "Trying to prove ";
            var2.print(Log::out());
            Log::out() << " - ";
            var1.print(Log::out());
            Log::out() << " >= ";
            c.print(Log::out());
            Log::out() << " : ";
            Log::out() << indent.c_str() << "Rephrased as  ";
            var1.print(Log::out());
            Log::out() << " - ";
            var2.print(Log::out());
            Log::out() << " <= ";
            negC.print(Log::out());
            Log::out() << " : ";
        }
        // v2 - v1 >= c
        // same as
        //   v1 - v2 <= -c
        return prove(false, var2, var1, negC, why);
    } else {
        MemoizedBounds &mb 
            = cache[::std::make_pair(var2, var1)]; // bounds on (var2-var1)
        if (Log::isEnabled()) {
            Log::out() << indent.c_str() << "Trying to prove ";
            var2.print(Log::out());
            Log::out() << " - ";
            var1.print(Log::out());
            Log::out() << " <= ";
            c.print(Log::out());
            Log::out() << " : ";
        }
        if (mb.leastTrueUB <= c) { // already proved a lower UB.
            if (Log::isEnabled()) {
                Log::out() << "Case 1: mb.leastTrueUB ==";
                mb.leastTrueUB.print(Log::out());
                Log::out() << " => TRUE" << ::std::endl;
            }                
            if (mb.leastTrueUBwhy) {
                if (thePass->flags.useReasons) {
                    assert(why);
                    why->addReasons(*mb.leastTrueUBwhy);
                }
            }
            return ProofLattice::ProvenTrue;
        } else if (mb.greatestFalseUB >= c) { // already disproved a higher UB
            if (Log::isEnabled()) {
                Log::out() << "Case 2: mb.greatestFalseUB ==";
                mb.greatestFalseUB.print(Log::out());
                Log::out() << " => FALSE" << ::std::endl;
            }                
            return ProofLattice::ProvenFalse;
        } else if (mb.leastReducedUB <= c) { // has cycle with lower UB
            if (Log::isEnabled()) {
                Log::out() << "Case 3: mb.leastReducedUB ==";
                mb.leastReducedUB.print(Log::out());
                Log::out() << " => REDUCED" << ::std::endl;
            }                
            if (mb.leastReducedUBwhy) {
                if (thePass->flags.useReasons) {
                    assert(why);
                    why->addReasons(*mb.leastReducedUBwhy);
                }
            }
            return ProofLattice::ProvenReduced;
        } else if ((var1 == var2) && 
                   (c >= ConstBound(I_32(0)))) { // reached source:
            if (Log::isEnabled()) {
                Log::out() << "Case 4: => TRUE" << ::std::endl;
            }                
            // don't need a reason, it's self-evident
            return ProofLattice::ProvenTrue; // (a-a <= c), success
        };

        // check for a loop in proof
        ConstBound &active_var = active[::std::make_pair(var1,var2)];
        
        if (!active_var.isNull()) { // a cycle
            if (Log::isEnabled()) {
                Log::out() << "Case 5" << ::std::endl;
            }                
            if (c < active_var) {
                if (Log::isEnabled()) {
                    Log::out() << indent.c_str() << "Case 5a : active[";
                    var1.print(Log::out());
                    Log::out() << ", ";
                    var2.print(Log::out());
                    Log::out() << "]==";
                    active_var.print(Log::out());
                    Log::out() << " => FALSE" << ::std::endl;
                }                
                // skip memorization step below by returning immediately
                return ProofLattice::ProvenFalse; // a reducing cycle for lb
            } else {
                if (Log::isEnabled()) {
                    Log::out() << indent.c_str() << "Case 5b : active[";
                    var1.print(Log::out());
                    Log::out() << ", ";
                    var2.print(Log::out());
                    Log::out() << "]==";
                    active_var.print(Log::out());
                    Log::out() << " => REDUCED" << ::std::endl;
                }                
                // skip memorization step below by returning immediately
                return ProofLattice::ProvenReduced;
            }
        } else {
            if (Log::isEnabled()) {
                Log::out() << "Case 5c: => ..." << ::std::endl;
            }
        }
        
        if (Log::isEnabled()) {
            Log::out() << indent.c_str() << "  Setting active[";
            var1.print(Log::out());
            Log::out() << ", ";
            var2.print(Log::out());
            Log::out() << "] to ";
            c.print(Log::out());
            Log::out() << ::std::endl;
        }
        active_var = c;
        
        ProofLattice result = ProofLattice::ProvenFalse;

        bool var1Blocked = var1.isEmpty();
        AbcdReasons *subWhy = (thePass->flags.useReasons
                               ? new (thePass->mm) AbcdReasons(thePass->mm)
                               : 0);
        if (!var1Blocked) {
            result =
                proveForPredecessors(var1, var2, c, true, var1Blocked, subWhy); //var1

            bool noSpecialCases = false;
            if (result == ProofLattice::ProvenFalse) {
                result = 
                    proveForSpecialCases(var1, var2, c, true,
                                         noSpecialCases,
                                         subWhy); //var1
            }
            var1Blocked = var1Blocked && noSpecialCases;
        }
        if (var1Blocked) {
            // 
            // Unlike ABCD paper, we do not build an explicit constraint
            // graph, but instead just use the use->def edges in the IR.
            // However, since we don't have def->use edges, we can only
            // go one way in the graph.
            // 
            // Therefore, unlike them, we must get predecessors of var1
            // here, to dereference def->use edges from the var1 side.
            // This allows us to find more paths.
            // 
            // Note that we are safe from infinite recursion since we
            // don't move to var2 until var1 is blocked.
            //
            bool ignore;
            result =
                proveForPredecessors(var1, var2, 
                                     c, false, ignore, subWhy); //var2
            
            if (result == ProofLattice::ProvenFalse) {
                result = proveForSpecialCases(var1, var2, c, false,
                                              ignore, subWhy); //var2
            }
        }
    
        active_var.setNull(); // reset it to Null
        if (Log::isEnabled()) {
            Log::out() << indent.c_str() << "Finished trying to prove ";
            var2.print(Log::out());
            Log::out() << " - ";
            var1.print(Log::out());
            Log::out() << " <= ";
            c.print(Log::out());
            Log::out() << " : ";
        }
        switch (result.value) {
        case ProofLattice::ProvenTrue:
            if (Log::isEnabled()) {
                Log::out() << " TRUE " << ::std::endl;
            }
            if (c < mb.leastTrueUB) {
                if (thePass->flags.useReasons) {
                    if (!mb.leastTrueUBwhy)
                        mb.leastTrueUBwhy = new (thePass->mm) AbcdReasons(thePass->mm);
                    mb.leastTrueUBwhy->clear();
                    assert(subWhy);
                    mb.leastTrueUBwhy->addReasons(*subWhy);
                } else {
                    mb.leastTrueUBwhy = 0;
                }
                mb.leastTrueUB = c; break;
            }
        case ProofLattice::ProvenFalse:
            if (Log::isEnabled()) {
                Log::out() << " FALSE " << ::std::endl;
            }
            if (c > mb.greatestFalseUB) {
                mb.greatestFalseUB = c; break;
            }
        case ProofLattice::ProvenReduced:
            if (Log::isEnabled()) {
                Log::out() << " REDUCED " << ::std::endl;
            }
            if (c < mb.leastReducedUB) {
                if (thePass->flags.useReasons) {
                    if (!mb.leastReducedUBwhy)
                        mb.leastReducedUBwhy = new (thePass->mm) AbcdReasons(thePass->mm);
                    mb.leastReducedUBwhy->clear();
                    assert(subWhy);
                    mb.leastReducedUBwhy->addReasons(*subWhy);
                }
                mb.leastReducedUB = c; break;
            }
        }
        if (thePass->flags.useReasons) {
            assert(why && subWhy);
            why->addReasons(*subWhy);
        }
        return result;
    }
}

void AbcdSolver::tryToEliminate(Inst *theInst)
{
    if (theInst->getOpcode() == Op_TauCheckBounds) {
        assert(theInst->getNumSrcOperands() == 2);
        Opnd *arrayOp = theInst->getSrc(0);
        Opnd *idxOp = theInst->getSrc(1);

        if (Log::isEnabled()) {
            Log::out() << "Checking checkbounds instruction ";
            theInst->print(Log::out());
            Log::out() << " for redundancy" << ::std::endl;
        }

        bool successUB = true;
        bool successLB = true;
        AbcdReasons *why = (thePass->flags.useReasons
                            ? new (thePass->mm) AbcdReasons(thePass->mm)
                            : 0);
        // idx - a.size <= -1
        if (demandProve(false,
                        VarBound(arrayOp), // may want lower bound of range
                        VarBound(idxOp),   // may want upper bound of range
                        -1,
                        why)) {
            if (Log::isEnabled()) {
                Log::out() << "We can eliminate UB check of ";
                theInst->print(Log::out());
                if (thePass->flags.useReasons) {
                    Log::out() << " because of ";
                    why->print(Log::out());
                }
                Log::out() << ::std::endl;
            }
        } else {
            if (Log::isEnabled()) {
                Log::out() << "We cannot eliminate UB check of ";
                theInst->print(Log::out());
                Log::out() << ::std::endl;
            }
            successUB = false;
        }
        // idx - 0 >= 0
        // same as
        //     0 - idx <= 0
        if (demandProve(false,
                        VarBound(idxOp), // may want lower bound of range
                        VarBound(),      // upper bound of 0
                        0,
                        why)) {
            if (Log::isEnabled()) {
                Log::out() << "We can eliminate LB check of ";
                theInst->print(Log::out());
                if (thePass->flags.useReasons) {
                    Log::out() << " because of ";
                    why->print(Log::out());
                }
                Log::out() << ::std::endl;
            }
        } else {
            if (Log::isEnabled()) {
                Log::out() << "We cannot eliminate LB check of ";
                theInst->print(Log::out());
                Log::out() << ::std::endl;
            }
            successLB = false;
        }
        if (successUB) {
            if (successLB) {
                if (Log::isEnabled()) {
                    Log::out() << "!!! We can eliminate boundscheck of ";
                    theInst->print(Log::out());
                    if (thePass->flags.useReasons) {
                        Log::out() << " because of ";
                        why->print(Log::out());
                    }
                    Log::out() << ::std::endl;
                }
                AbcdReasons *ignore;
                if (!thePass->isMarkedToEliminate(theInst, ignore))
                    if (why)
                        thePass->markCheckToEliminateAndWhy(theInst, why);
                    else
                        thePass->markCheckToEliminate(theInst);
            } else {
                if (Log::isEnabled()) {
                    Log::out() << "!!! We can eliminate UB check of ";
                    theInst->print(Log::out());
                    if (thePass->flags.useReasons) {
                        Log::out() << " because of ";
                        why->print(Log::out());
                    }
                    Log::out() << ::std::endl;
                }
                AbcdReasons *ignore;
                if (!thePass->isMarkedToEliminateUB(theInst, ignore))
                    if (why)
                        thePass->markCheckToEliminateUBAndWhy(theInst, why);
                    else 
                        thePass->markCheckToEliminateUB(theInst);
            }
        } else {
            if (successLB) {
                if (Log::isEnabled()) {
                    Log::out() << "!!! We can eliminate LB check of ";
                    theInst->print(Log::out());
                    if (thePass->flags.useReasons) {
                        Log::out() << " because of ";
                        why->print(Log::out());
                    }
                    Log::out() << ::std::endl;
                }
                AbcdReasons *ignore;
                if (!thePass->isMarkedToEliminateLB(theInst, ignore))
                    if (why)
                        thePass->markCheckToEliminateLBAndWhy(theInst, why);
                    else
                        thePass->markCheckToEliminateLB(theInst);
            }
        }
        if (!(successLB && successUB)) {
            // see if we can disprove overflow
            // idx >= 0
            if (demandProve(true, // lower obund
                            VarBound(),
                            VarBound(idxOp),
                            0,
                            why)) {
                
                U_32 elemSize = 16;
                uint64 overflowSize = ((uint64)1 << 31) / elemSize;
                
                if (demandProve(false, // upper bound
                                VarBound(),
                                VarBound(idxOp),
                                overflowSize,
                                why)) {
                    // overflow can't happen
                    theInst->setOverflowModifier(Overflow_None);
                }
            }
        }
    } else if (thePass->flags.remConv && 
               (theInst->getOpcode() == Op_Conv)) {
        assert(theInst->getNumSrcOperands() == 1);
        Opnd *srcOp = theInst->getSrc(0);
        Opnd *dstOp = theInst->getDst();
        AbcdReasons *ignore;
        if (Abcd::hasCheckableType(srcOp) &&
            Abcd::hasCheckableType(dstOp) &&
            Abcd::isCheckableType(theInst->getType()) &&
            (srcOp->getType()->tag == dstOp->getType()->tag) &&
            !thePass->isMarkedToEliminate(theInst, ignore)) {
            
            if (Log::isEnabled()) {
                Log::out() << "Checking conv instruction ";
                theInst->print(Log::out());
                Log::out() << " for redundancy" << ::std::endl;
            }

            if (Abcd::convPassesSource(dstOp)) {
                if (Log::isEnabled()) {
                    Log::out() << "We can trivially remove conv instruction ";
                    theInst->print(Log::out());
                    Log::out() << ::std::endl;
                }
                thePass->markInstToEliminate(theInst);
            } else {
                VarBound dstBound = VarBound(dstOp);
                VarBound srcBound = VarBound(srcOp);
                
                PiCondition convBounds = PiCondition::convBounds(dstBound);
                PiBound lb = convBounds.getLb();
                PiBound ub = convBounds.getUb();
                ProofLattice res1;
                AbcdReasons *why = (thePass->flags.useReasons
                                    ? new (thePass->mm) AbcdReasons(thePass->mm)
                                    : 0);
                
                if (lb.isConstant()) {
                    ConstBound clb = lb.getConst();
                    res1 = prove(true, // lower bound
                                 VarBound(),
                                 srcBound, 
                                 clb,
                                 why);
                } else if (lb.isUndefined()) {
                    res1 = ProofLattice::ProvenTrue;
                } else {
                    res1 = ProofLattice::ProvenFalse;
                }
                if (res1 != ProofLattice::ProvenFalse) {
                    if (ub.isConstant()) {
                        ConstBound cub = ub.getConst();
                        ProofLattice res2 = prove(false, // upper bound
                                                  VarBound(),
                                                  srcBound, 
                                                  cub,
                                                  why);
                        res1.meetwith(res2);
                    } else if (!ub.isUndefined()) {
                        res1 = ProofLattice::ProvenFalse;
                    }
                }
                if (res1 != ProofLattice::ProvenFalse) {
                    // note that though these are constant bounds, 
                    // they can result in reduced proofs
                    if (Log::isEnabled()) {
                        Log::out() << "!!! We can eliminate conversion: ";
                        theInst->print(Log::out());
                        if (thePass->flags.useReasons) {
                            Log::out() << " because of ";
                            why->print(Log::out());
                        }
                        Log::out() << ::std::endl;
                    }

                    if (why)
                        thePass->markInstToEliminateAndWhy(theInst, why);
                    else
                        thePass->markInstToEliminate(theInst);
                }
            }
        }
    } else if (thePass->flags.unmaskShifts && 
               ((theInst->getOpcode() == Op_Shr) ||
                (theInst->getOpcode() == Op_Shl)) &&
               (theInst->getShiftMaskModifier() == ShiftMask_Masked)) {
        assert(theInst->getNumSrcOperands() == 2);
        Opnd *shiftByOp = theInst->getSrc(1);
        AbcdReasons *ignore;
        if (Abcd::hasCheckableType(shiftByOp) &&
            !thePass->isMarkedToEliminate(theInst, ignore)) {
            if (Log::isEnabled()) {
                Log::out() << "Checking shift instruction ";
                theInst->print(Log::out());
                Log::out() << " to eliminate mask" << ::std::endl;
            }
            I_32 mask;
            switch (theInst->getType()) {
            case Type::Int32:
                mask = 31; break;
            case Type::Int64:
                mask = 63; break;
            default:
                mask = -1; break;
            }
            
            AbcdReasons *why = (thePass->flags.useReasons
                                ? new (thePass->mm) AbcdReasons(thePass->mm)
                                : 0);
            if (mask > 0) {
                VarBound shiftByVar = VarBound(shiftByOp);
                ConstBound zerob(I_32(0));
                ProofLattice res1 = prove(true, // lower bound
                                          VarBound(),
                                          shiftByVar,
                                          zerob, why);
                if (res1 != ProofLattice::ProvenFalse) {
                    ConstBound upperb(mask);
                    ProofLattice res2 = prove(false, // upper bound
                                              VarBound(),
                                              shiftByVar,
                                              upperb, why);
                    if (res2 != ProofLattice::ProvenFalse) {
                        // note that though these are constant bounds, 
                        // they can result in reduced proofs
                        if (Log::isEnabled()) {
                            Log::out() << "!!! We can eliminate shift mask: ";
                            theInst->print(Log::out());
                            if (thePass->flags.useReasons) {
                                Log::out() << " because of ";
                                why->print(Log::out());
                            }
                            Log::out() << ::std::endl;
                        }

                        if (why)
                            thePass->markInstToEliminateAndWhy(theInst, why);
                        else
                            thePass->markInstToEliminate(theInst);
                    }
                }
            }
        }
    } else if (thePass->flags.remBr &&
               (theInst->getOpcode() == Op_Branch)) {
        if (Abcd::hasCheckableType(theInst->getSrc(0)))
            tryToFoldBranch(theInst);
    } else if (thePass->flags.remCmp &&
               (theInst->getOpcode() == Op_Cmp)) {
        if (Abcd::hasCheckableType(theInst->getSrc(0)))
            tryToFoldCompare(theInst);
    } else if (thePass->flags.remOverflow &&
               Abcd::isCheckableType(theInst->getType()) &&
               ((theInst->getOpcode() == Op_Add) ||
                (theInst->getOpcode() == Op_Sub))) {
        assert(theInst->getNumSrcOperands() == 2);
        Modifier mod = theInst->getModifier();
        AbcdReasons *why = (thePass->flags.useReasons
                            ? new (thePass->mm) AbcdReasons(thePass->mm)
                            : 0);
        if ((mod.getOverflowModifier() == Overflow_None) ||
            (mod.getExceptionModifier() == Exception_Sometimes)) {
            // check whether we can overflow
            bool isSigned = mod.getOverflowModifier() != Overflow_Unsigned;
            Opnd *opnd0 = theInst->getSrc(0);
            Opnd *opnd1 = theInst->getSrc(1);

            int64 lb, ub;
            Type::Tag instType = theInst->getType();
            if (Abcd::hasTypeBounds(instType, lb, ub) &&
                (theInst->getType() != Type::Int64)) { // is somewhat unbounded if int64
                Opnd *constOpnd = 0;
                Opnd *varOpnd = 0;
                bool negate = theInst->getOpcode() == Op_Sub;
                bool negateConstant = false;
                if (ConstantFolder::isConstant(opnd0)) {
                    constOpnd = opnd0;
                    varOpnd = opnd1;
                } else if (ConstantFolder::isConstant(opnd1)) {
                    constOpnd = opnd1;
                    varOpnd = opnd0;
                    negateConstant = negate;
                }
                bool provenNoOverflow = false;
                if (constOpnd) {
                    // we can be fairly precise
                    // for var+constOpnd, bounds on var are [lb-constOpnd, ub-constOpnd],
                    // for var-constOpnd, bounds on var are [lb+constOpnd, ub+constOpnd]
                    // for constOpnd-var, bounds on var are [constOpnd-ub, constOpnd-lb]
                    // (with values in I_32, but calculations performed in int64)
                    ConstInst *constInst = constOpnd->getInst()->asConstInst();
                    assert(constInst);
#ifndef NDEBUG
                    Type::Tag constInstType = constInst->getType();
#endif
                    assert(isSigned ?
                           ((constInstType == Type::Int8)||(constInstType == Type::Int16)||
                            (constInstType == Type::Int32)) :
                           ((constInstType == Type::UInt8)||
                            (constInstType == Type::UInt16)||(constInstType == Type::UInt32)));
                    ConstInst::ConstValue constValue = constInst->getValue();
                    I_32 constValue32 = constValue.i4;
                    int64 varlb, varub;
                    int64 constValue64 = isSigned ? (int64)constValue32 : (int64)(uint64)(U_32)constValue32;
                    if (negateConstant) { 
                        varlb = lb + constValue64 + 1; // tweak by 1 in case I have a fencepost problem
                        varub = ub + constValue64 - 1;
                    } else if (negate) {
                        varlb = constValue64 - ub + 1;
                        varub = constValue64 - lb - 1;
                    } else {
                        varlb = lb - constValue64 + 1;
                        varub = ub - constValue64 - 1;
                    }
                    VarBound varBound = VarBound(varOpnd);
                    lb = ::std::max(varlb, lb);
                    ub = ::std::min(varub, ub);
                    ProofLattice res1 = prove(true, // lower bound
                                              VarBound(),
                                              varBound,
                                              lb, why);
                    if (res1 != ProofLattice::ProvenFalse) {
                        res1 = prove(false, // upper bound
                                     VarBound(),
                                     varBound,
                                     ub, why);
                    }
                    if (res1 != ProofLattice::ProvenFalse) {
                        // we have proved that overflow cannot happen.
                        provenNoOverflow = true;
                    }
                } else {
                    // we have to be more conservative
                    // for Add, we must prove
                    //    lb <= opnd1 + opnd2 <= ub
                    // but we can't represent that, so instead just try for
                    //    lb/2 +1 <= opnd1,opnd2 <= ub/2 -1
                    // which is sufficient to ensure no overflow
                    if (isSigned) {
                        assert((instType == Type::Int8)||(instType == Type::Int16)||
                               (instType == Type::Int32));
                        lb = lb / 2 + 1;
                        ub = ub / 2 - 1;
                    } else {
                        assert((instType == Type::UInt8)||
                               (instType == Type::UInt16)||(instType == Type::UInt32));
                        assert(lb ==(int64) 0);
                        ub = ub / 2 - 1;
                    }
                    VarBound varBound0 = VarBound(opnd0);
                    VarBound varBound1 = VarBound(opnd1);
                    ProofLattice res1 = prove(true, // lower bound
                                              VarBound(),
                                              varBound0,
                                              lb, why);
                    if (res1 != ProofLattice::ProvenFalse) {
                        res1 = prove(false, // upper bound
                                     VarBound(),
                                     varBound0,
                                     ub, why);
                    }
                    if (res1 != ProofLattice::ProvenFalse) {
                        res1 = prove(true, // lower bound
                                     VarBound(),
                                     varBound1,
                                     lb, why);
                    }
                    if (res1 != ProofLattice::ProvenFalse) {
                        res1 = prove(false, // upper bound
                                     VarBound(),
                                     varBound1,
                                     ub, why);
                    }
                    provenNoOverflow = true;
                }

                if (provenNoOverflow) {
                    Modifier newmod = mod;
                    newmod.setExceptionModifier(Exception_Never);
                    if (newmod.getOverflowModifier() == Overflow_None) {
                        if (isSigned) {
                            newmod.setOverflowModifier(Overflow_Signed);
                        } else {
                            newmod.setOverflowModifier(Overflow_Unsigned);
                        }
                        if (Log::isEnabled()) {
                            Log::out() << "Disproved overflow of add/sub instruction ";
                            theInst->print(Log::out());
                            Log::out() << " so marking it overflow but exception-free" << ::std::endl;
                        }
                    } else {
                        if (Log::isEnabled()) {
                            Log::out() << "Disproved overflow of add/sub instruction ";
                            theInst->print(Log::out());
                            Log::out() << " so marking it exception-free" << ::std::endl;
                        }
                    }
                    theInst->setModifier(newmod);
                }
            }
        }
    } else if (thePass->flags.remOverflow &&
               Abcd::isCheckableType(theInst->getType()) &&
               (theInst->getOpcode() == Op_Mul)) {
        assert(theInst->getNumSrcOperands() == 2);
        Modifier mod = theInst->getModifier();
        if ((mod.getOverflowModifier() == Overflow_None) ||
            (mod.getExceptionModifier() == Exception_Sometimes)) {
            // check whether we can overflow
            bool isSigned = mod.getOverflowModifier() != Overflow_Unsigned;
            Opnd *opnd0 = theInst->getSrc(0);
            Opnd *opnd1 = theInst->getSrc(1);

            int64 lb, ub;
            Type::Tag instType = theInst->getType();
            AbcdReasons *why 
                = (thePass->flags.useReasons
                   ? new (thePass->mm) AbcdReasons(thePass->mm)
                   : 0);
            if (Abcd::hasTypeBounds(instType, lb, ub) &&
                (theInst->getType() != Type::Int64)) { // is somewhat unbounded if int64
                Opnd *constOpnd = 0;
                Opnd *varOpnd = 0;
                if (ConstantFolder::isConstant(opnd0)) {
                    constOpnd = opnd0;
                    varOpnd = opnd1;
                } else if (ConstantFolder::isConstant(opnd1)) {
                    constOpnd = opnd1;
                    varOpnd = opnd0;
                }
                bool provenNoOverflow = false;
                if (constOpnd) {
                    // we can hope to do something
                    // if constOpnd > 0, bounds on var are [lb/constOpnd+1, ub/constOpnd-1],
                    // if constOpnd < 0, bounds on var are [ub/constOpnd+1, lb/constOpnd+1]
                    ConstInst *constInst = constOpnd->getInst()->asConstInst();
                    assert(constInst);
#ifndef NDEBUG
                    Type::Tag constInstType = constInst->getType();
                    assert(isSigned ?
                           ((constInstType == Type::Int8)||(constInstType == Type::Int16)||
                            (constInstType == Type::Int32)) :
                           ((constInstType == Type::UInt8)||
                            (constInstType == Type::UInt16)||(constInstType == Type::UInt32)));
#endif
                    ConstInst::ConstValue constValue = constInst->getValue();
                    I_32 constValue32 = constValue.i4;
                    int64 varlb, varub;
                    if (isSigned) {
                        if (constValue32 < 0) {
                            varlb = lb/constValue32 + 1;
                            varub = ub/constValue32 - 1;
                        } else {
                            assert(constValue32 > 0); // inst should have been simplified otherwise
                            varlb = ub/constValue32 + 1;
                            varub = lb/constValue32 - 1;
                        }
                    } else {
                        assert(constValue32 > 0); // inst should have been simplified otherwise
                        varlb = 0;
                        varub = ub/(U_32)constValue32;
                    }
                    VarBound varBound = VarBound(varOpnd);
                    lb = ::std::max(varlb, lb);
                    ub = ::std::min(varub, ub);
                    ProofLattice res1 = prove(true, // lower bound
                                              VarBound(),
                                              varBound,
                                              lb, why);
                    if (res1 != ProofLattice::ProvenFalse) {
                        res1 = prove(false, // upper bound
                                     VarBound(),
                                     varBound,
                                     ub, why);
                    }
                    if (res1 != ProofLattice::ProvenFalse) {
                        // we have proved that overflow cannot happen.
                        provenNoOverflow = true;
                    }
                } else {
                    // we have to be much more conservative
                    // for Mul, we must prove
                    //    lb <= opnd1 * opnd2 <= ub
                    // but it could work to use
                    //    sqrt(-lb)+1 <= opnd1, opnd2 <= sqrt(ub)-1     if signed
                    // or
                    //    0 <= opnd1, opnd2 <= sqrt(ub)-1               if unsigned
                    if (isSigned) {
                        assert((instType == Type::Int8)||(instType == Type::Int16)||
                               (instType == Type::Int32));
                        lb = 1 - isqrt<uint64>(uint64(-(lb+1)));
                        ub = isqrt<uint64>(uint64(ub)) - 1;
                    } else {
                        lb = 0;
                        ub = isqrt<uint64>(uint64(ub)) - 1;
                    }
                    VarBound varBound0 = VarBound(opnd0);
                    VarBound varBound1 = VarBound(opnd1);
                    ProofLattice res1 = prove(true, // lower bound
                                              VarBound(),
                                              varBound0,
                                              lb, why);
                    if (res1 != ProofLattice::ProvenFalse) {
                        res1 = prove(false, // upper bound
                                     VarBound(),
                                     varBound0,
                                     ub, why);
                    }
                    if (res1 != ProofLattice::ProvenFalse) {
                        res1 = prove(true, // lower bound
                                     VarBound(),
                                     varBound1,
                                     lb, why);
                    }
                    if (res1 != ProofLattice::ProvenFalse) {
                        res1 = prove(false, // upper bound
                                     VarBound(),
                                     varBound1,
                                     ub, why);
                    }
                    provenNoOverflow = true;
                }

                if (provenNoOverflow) {
                    Modifier newmod = mod;
                    newmod.setExceptionModifier(Exception_Never);
                    if (newmod.getOverflowModifier() == Overflow_None) {
                        if (isSigned) {
                            newmod.setOverflowModifier(Overflow_Signed);
                        } else {
                            newmod.setOverflowModifier(Overflow_Unsigned);
                        }
                        if (Log::isEnabled()) {
                            Log::out() << "Disproved overflow of mul instruction ";
                            theInst->print(Log::out());
                            Log::out() << " so marking it overflow but exception-free" << ::std::endl;
                        }
                    } else {
                        if (Log::isEnabled()) {
                            Log::out() << "Disproved overflow of mul instruction ";
                            theInst->print(Log::out());
                            Log::out() << " so marking it exception-free" << ::std::endl;
                        }
                    }
                    theInst->setModifier(newmod);
                }
            }
        }
    }
}

void AbcdSolver::tryToFoldBranch(Inst *) {}
void AbcdSolver::tryToFoldCompare(Inst *) {}

// try to prove var2 - var1 <= c
// by considering either var1 or var2, depending on derefVar1
// if (!checkVar1), var2 should have been fully dereferenced and checked first.
ProofLattice AbcdSolver::proveForSpecialCases(const VarBound &var1,
                                              const VarBound &var2,
                                              ConstBound c,
                                              bool checkVar1,
                                              bool &noneApply,
                                              AbcdReasons *why)
{
    const VarBound &theVar = (checkVar1 ? var1 : var2);
    const VarBound &otherVar = (checkVar1 ? var2 : var1);
    ProofLattice result = ProofLattice::ProvenFalse;

    if (Log::isEnabled()) {
        Log::out() << indent.c_str() << "Checking special cases for: ";
        theVar.print(Log::out());
        Log::out() << ::std::endl;
    }

    AbcdReasons *subWhy = 
        (thePass->flags.useReasons
         ? new (thePass->mm) AbcdReasons(thePass->mm)
         : 0);
    if (thePass->flags.useShr) {
        if (otherVar.isEmpty() && theVar.isConvexFunction()) {
            assert(theVar.the_var != 0);
            Inst *the_inst = theVar.the_var->getInst();
            
            if (Log::isEnabled()) {
                Log::out() << indent.c_str() << "ConvexFunction: ";
                the_inst->print(Log::out());
                Log::out() << ::std::endl;
            }

            if (checkVar1) {
                // we have var2 = 0, so
                // we're trying to prove
                //    -var1 <= c
                // or
                //     var1 >= -c
                // where
                //    var1 = f(var3)
                // by constraining input by
                //    var3 >= inputC = finv(-c)
                ConstBound negC = -c;
                VarBound var3;
                
                ConstBound inputC =
                    var1.getConvexInputBound(true, negC, var3);
                if (!inputC.isNull()) {
                    result = prove(true, // lower bound on var3
                                   VarBound(),
                                   var3,
                                   inputC,
                                   subWhy);
                    if (result != ProofLattice::ProvenFalse) {
                        if (thePass->flags.useReasons) {
                            assert(why && subWhy);
                            why->addReasons(*subWhy);
                        }
                        return result;
                    }
                }
            } else {
                // here we have var1=0, so
                // we're trying to prove
                //    var2 <= c
                // where
                //    var2 = f(var3)
                // by constraining input by
                //    var3 <= inputC = finv(-c)
                VarBound var3;
                
                ConstBound inputC =
                    var2.getConvexInputBound(false, c, var3);
                if (!inputC.isNull()) {
                    result = prove(false, // upper bound on var3
                                   VarBound(),
                                   var3,
                                   inputC,
                                   subWhy);
                    if (result != ProofLattice::ProvenFalse) {
                        if (thePass->flags.useReasons) {
                            assert(why && subWhy);
                            why->addReasons(*subWhy);
                        }
                        return result;
                    }
                }
            }
        }
    }
    
    if (thePass->flags.useConv
        && (result == ProofLattice::ProvenFalse)
        && (theVar.isConvVar())) {
        // for a Conv, predecessors just are type-based bounds
        // let's see if we can do better looking at the predecessor
        VarBound var3 = theVar.getConvSource();
        assert(theVar.the_var != 0);
        Inst *the_inst = theVar.the_var->getInst();
        
        if (Log::isEnabled()) {
            Log::out() << indent.c_str() << "Have conv : ";
            the_inst->print(Log::out());
            Log::out() << ::std::endl;
        }

        bool passesSource = theVar.convPassesSource();
        AbcdReasons *markedWhy = 0;
        bool alreadyMarked = thePass->isMarkedToEliminate(the_inst, markedWhy);
        if (passesSource) {
            if (Log::isEnabled()) {
                Log::out() << indent.c_str() << "Passes Source: ";
                the_inst->print(Log::out());
                Log::out() << ::std::endl;
            }
        };
        if (alreadyMarked) {
            if (Log::isEnabled()) {
                Log::out() << indent.c_str() << "Already marked : ";
                the_inst->print(Log::out());
                Log::out() << ::std::endl;
            }
        };

        if (passesSource || alreadyMarked) {
            if (checkVar1) {
                // no need to check whether source is in range
                result = prove(false,
                               var3,
                               var2,
                               c,
                               subWhy);
                if ((result != ProofLattice::ProvenFalse) && alreadyMarked) {
                    // need to add reasons why it was marked;
                    if (thePass->flags.useReasons) {
                        assert(subWhy && why);
                        subWhy->addReasons(*why);
                    }
                }
            } else {
                // no need to check whether source is in range
                result = prove(false,
                               var1,
                               var3,
                               c,
                               subWhy);
                if ((result != ProofLattice::ProvenFalse) && alreadyMarked) {
                    // need to add reasons why it was marked;
                    if (thePass->flags.useReasons) {
                        assert(subWhy && why);
                        subWhy->addReasons(*why);
                    }
                }
            }
        } else {
            // check whether source is in range
            PiCondition convBounds = PiCondition::convBounds(theVar);
            
            if (Log::isEnabled()) {
                Log::out() << indent.c_str() << "Checking for source in range : ";
                the_inst->print(Log::out());
                Log::out() << ::std::endl;
            }

            PiBound lb = convBounds.getLb();
            PiBound ub = convBounds.getUb();
            ProofLattice res1;
            if (lb.isConstant()) {
                ConstBound clb = lb.getConst();
                if (checkVar1)
                    res1 = prove(true, var3, VarBound(), clb, subWhy);
                else
                    res1 = prove(true, VarBound(), var3, clb, subWhy);
            } else if (lb.isUndefined()) {
                // no need to add a reason, LB is unconstrained
                res1 = ProofLattice::ProvenTrue;
            } else {
                res1 = ProofLattice::ProvenFalse;
            }
            if (res1 != ProofLattice::ProvenFalse) {
                if (ub.isConstant()) {
                    ConstBound cub = ub.getConst();
                    ProofLattice res2;
                    if (checkVar1)
                        res2 = prove(false, var3, VarBound(), cub, subWhy);
                    else
                        res2 = prove(false, VarBound(), var3, cub, subWhy);
                    res1.meetwith(res2);
                } else if (!ub.isUndefined()) {
                    res1 = ProofLattice::ProvenFalse;
                }
            }
            if (res1 != ProofLattice::ProvenFalse) {
                
                if (Log::isEnabled()) {
                    Log::out() << indent.c_str() << "Source is in range, passing constraint: ";
                    the_inst->print(Log::out());
                    if (thePass->flags.useReasons) {
                        Log::out() << " because of ";
                        subWhy->print(Log::out());
                    }
                    Log::out() << ::std::endl;
                }
                // note that though these are constant bounds, 
                // they can result in reduced proofs
                if (why)
                    thePass->markInstToEliminateAndWhy(the_inst, subWhy);
                else
                    thePass->markInstToEliminate(the_inst);
                // source is in range, visit it
                if (checkVar1)
                    result = prove(false,
                                   var3,
                                   var2,
                                   c,
                                   subWhy);
                else
                    result = prove(false,
                                   var1,
                                   var3,
                                   c,
                                   subWhy);

                if (Log::isEnabled()) {
                    Log::out() << indent.c_str() << "Special case conv result = ";
                    result.print(Log::out());
                    Log::out() << ::std::endl;
                }
                
                if (result != ProofLattice::ProvenFalse) {
                    if (thePass->flags.useReasons) {
                        assert(why && subWhy);
                        why->addReasons(*subWhy);
                    }
                }
                return result;
            }
        }
    }
    noneApply = true;
    if (Log::isEnabled()) {
        Log::out() << indent.c_str() << "NoneApply, special case conv result = ";
        result.print(Log::out());
        Log::out() << ::std::endl;
    }
    if (result != ProofLattice::ProvenFalse) {
        if (thePass->flags.useReasons) {
            assert(why && subWhy);
            why->addReasons(*subWhy);
        }
    }
    return result;
}

// try to prove var2 - var1 <= c
// by dereferencing either Var1 (if derefVar1==true) or Var2 (otherwise)
ProofLattice AbcdSolver::proveForPredecessors(const VarBound &var1,
                                              const VarBound &var2,
                                              ConstBound c,
                                              bool derefVar1,
                                              bool &predsAreEmpty,
                                              AbcdReasons *why)
{
    PiBoundIter preds = (derefVar1 
                         ? var1.getPredecessors(true, thePass->flags.useReasons, thePass->mm) // boundBelow
                         : var2.getPredecessors(false, thePass->flags.useReasons, thePass->mm)); // boundAbove
    ProofLattice result = ProofLattice::ProvenFalse;
    VarBound derefVar = (derefVar1 ? var1 : var2);
    bool boundDirection = derefVar1; // preds are lower bounds, MIN acts like PHI
    if (preds.isEmpty()) {
        if (Log::isEnabled()) {
            Log::out() << indent.c_str() << "    Predecessors of ";
            if (derefVar1) {
                Log::out() << "var1=";
            } else {
                Log::out() << "var2=";
            }
            derefVar.print(Log::out());
            Log::out() << " are EMPTY";
            Log::out() << ::std::endl;
        }
        predsAreEmpty = true;
    } else {
        if (derefVar.isPhiVar()) {
            if (Log::isEnabled()) {
                Log::out() << indent.c_str()
                           << "  Case 5c : Phi function" << ::std::endl;
            }                
            result = ProofLattice::ProvenTrue;
            AbcdReasons *subWhy 
                = (thePass->flags.useReasons
                   ? new (thePass->mm) AbcdReasons(thePass->mm)
                   : 0);
            StlVector<AbcdReasons *> *subSubWhys
                = (thePass->flags.useReasons
                   ? new (thePass->mm) StlVector<AbcdReasons *>(thePass->mm)
                   : 0);
            StlVector<Opnd *> *subSubVars
                = (thePass->flags.useReasons
                   ? new (thePass->mm) StlVector<Opnd *>(thePass->mm)
                   : 0);
            for ( ; !preds.isEmpty(); ++preds) {
                PiBound pred = preds.getBound(subWhy);
                
                assert(pred.getType() == Type::Int32);
                assert(pred.isVar());
                VarBound var3 = pred.getVar();
                AbcdReasons *subSubWhy 
                    = (thePass->flags.useReasons
                       ? new (thePass->mm) AbcdReasons(thePass->mm)
                       : 0);
                if (derefVar1) {
                    // we want to prove
                    //   var2 <= c + var1
                    // we know
                    //   var1 >= var3
                    // so sufficient to prove
                    //   var2 <= c + var3
                    ProofLattice res1 = prove(false,
                                              var3,
                                              var2,
                                              c,
                                              subSubWhy);
                    result.meetwith(res1);
                } else {
                    // we want to prove
                    //   var2 <= c + var1
                    // we know
                    //   var2 <= var3
                    // so sufficient to prove
                    //   var3 <= c + var1
                    ProofLattice res1 = prove(false,
                                              var1,
                                              var3,
                                              c,
                                              subSubWhy);
                    result.meetwith(res1);
                }
                if (result == ProofLattice::ProvenFalse) break;
                if (thePass->flags.useReasons) {
                    assert(subSubWhys && subSubVars);
                    subSubWhys->push_back(subSubWhy);
                    subSubVars->push_back(var3.the_var);
                }
            }
            if (result != ProofLattice::ProvenFalse) {
                if (thePass->flags.useReasons) {
                    assert(subSubWhys && subSubVars);
                    SsaTmpOpnd *tauOpnd = thePass->makeReasonPhi(derefVar.the_var, *subSubWhys, *subSubVars);
                    assert(why && tauOpnd);
                    why->addReason(tauOpnd);
                }
            }
            return result;
        } else {
            bool andConditions = derefVar.isMinMax(boundDirection);
            result = (andConditions 
                      ? ProofLattice::ProvenTrue
                      : ProofLattice::ProvenFalse);
            AbcdReasons *subWhy = 
                ((andConditions && thePass->flags.useReasons)
                 ? new (thePass->mm) AbcdReasons(thePass->mm)
                 : 0);
            StlVector<AbcdReasons *> *subSubWhys
                = ((andConditions && thePass->flags.useReasons)
                   ? new (thePass->mm) StlVector<AbcdReasons *>(thePass->mm)
                   : 0);
            if (Log::isEnabled()) {
                Log::out() << indent.c_str()
                           << "  Case 5d : non-Phi" << ::std::endl;
            }                
            for ( ; !preds.isEmpty(); ++preds) {
                AbcdReasons *subSubWhy = 
                    (thePass->flags.useReasons
                     ? new (thePass->mm) AbcdReasons(thePass->mm)
                     : 0);
                PiBound pred = preds.getBound(subSubWhy);
                
                if ((pred.getType() == Type::Int32) &&
                    (pred.isVarPlusConst() || pred.isConst())) {
                    
                    VarBound var3 = pred.getVar();
                    int64 d1 = pred.getConst();
                    I_32 d = (I_32)d1;
                    assert(d1 == int64(d)); // check for overflow
                    ConstBound db(d);
                    
                    if (derefVar1) {
                        // trying to prove
                        //   var2 <= c + var1
                        // we know that 
                        //   var1 >= pred
                        //   pred = var3 + d
                        //   var1 + c >= var3 + d + c
                        // sufficient to prove
                        //   var2 <= var3 + d + c
                        // or
                        //   var2 - var3 <= d + c
                        ProofLattice res1 = prove(false,
                                                  var3,
                                                  var2,
                                                  c + db,
                                                  subSubWhy);
                        if (andConditions)
                            result.meetwith(res1);
                        else
                            result.joinwith(res1);
                    } else {
                        // trying to prove
                        //   var2 <= c + var1
                        // we know that 
                        //   var2 <= pred
                        //   pred = var3 + d
                        //   var2 - c <= var3 + d - c
                        // sufficient to prove
                        //   var1 >= var3 + d - c
                        // or
                        //   var3 - var1 <= c - d
                        ProofLattice res1 = prove(false,
                                                  var1,
                                                  var3,
                                                  c - db,
                                                  subSubWhy);
                        if (andConditions)
                            result.meetwith(res1);
                        else
                            result.joinwith(res1);
                    }
                    if (andConditions) {
                        if (result == ProofLattice::ProvenFalse) {
                            break;
                        }
                        if (thePass->flags.useReasons) {
                            assert(subSubWhys);
                            subSubWhys->push_back(subSubWhy);
                        }
                    } else {
                        if (result == ProofLattice::ProvenTrue) {
                            // and with
                            if (thePass->flags.useReasons){
                                assert(subWhy && subSubWhy);
                                subWhy->addReasons(*subSubWhy);
                            }
                            break;
                        }
                    }
                }
            }
            if (Log::isEnabled()) {
                Log::out() << indent.c_str()
                           << "  returning ";
                result.print(Log::out());
                Log::out() << ::std::endl;
            }                
            if (andConditions) {
                if (result != ProofLattice::ProvenFalse) {
                    if (thePass->flags.useReasons) {
                        assert(why && subSubWhys);
                        why->addReasons(subSubWhys);
                    }
                }
            } else {
                if (result == ProofLattice::ProvenTrue)
                    if (thePass->flags.useReasons) {
                        assert(why && subWhy);
                        why->addReasons(*subWhy);
                    }
            }
            return result;
        }
    }
    if (Log::isEnabled()) {
        Log::out() << indent.c_str()
                   << "  returning FALSE";
        Log::out() << ::std::endl;
    }                
    return ProofLattice::ProvenFalse;
}


} //namespace Jitrino 
