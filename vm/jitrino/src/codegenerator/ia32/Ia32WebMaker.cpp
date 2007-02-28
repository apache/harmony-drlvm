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
 * @author Sergey L. Ivashin
 * @version $Revision$
 */

#include "Ia32IRManager.h"
#include "XTimer.h"
#include "Counter.h"
#include "Stl.h"

#ifdef _DEBUG__WEBMAKER
#include <iostream>
#include <iomanip>
#include "Ia32IRXMLDump.h"
#ifdef _MSC_VER
#pragma warning(disable : 4505)   //unreferenced local function has been removed
#endif //#ifdef _MSC_VER
#endif //#ifdef _DEBUG__WEBMAKER


using namespace std;

namespace Jitrino
{

namespace Ia32
{


struct WebMaker : public SessionAction
{
    MemoryManager mm;       // this is private MemoryManager, not irm.getMemoryManager()

    struct OpDef
    {
        int globid;         // -1 for local definition or uniquue id (starting from 0)
        Inst* defp;         // defining instruction
        size_t linkx;
        bool visited;

        typedef StlVector<Inst*> Instps;
        Instps* useps;      // for local operands always 0
    };

    struct Opndx
    {
        Opndx (MemoryManager& mm)           :opdefs(mm), globdefsp(0), cbbp(0), webscount(0) {}

        typedef StlVector<OpDef> OpDefs;
        OpDefs opdefs;      // array of all definitions

        BitSet* globdefsp;  // bitmask of all global definitions or 0

        const Node* cbbp;
        OpDef* copdefp;
        int webscount;
        Opnd* newopndp;
    };

    typedef StlVector <Opndx*> Opndxs;
    Opndxs opndxs;


    struct Nodex
    {
        Nodex (MemoryManager& mm, size_t s) :globentrys(mm,(uint32)s), 
                                             globdefsp (0), 
                                             globkillsp(0),
                                             globexitsp(0) {}

        BitSet globentrys;  // all global definitions available at the block entry
        BitSet* globdefsp,  // can be 0
              * globkillsp, // can be 0
              * globexitsp; // 0 if globdefsp = globkillsp = 0 (globentrys must be used instead)
    };

    typedef StlVector <Nodex*> Nodexs;
    Nodexs nodexs;

    /*const*/ size_t opandcount;
    /*const*/ size_t nodecount;
    size_t splitcount;
    size_t globcount;


    WebMaker ()                     :mm(1000, "WebMaker"), opndxs(mm), nodexs(mm) {}

    uint32 getNeedInfo () const     {return NeedInfo_LivenessInfo;}
    uint32 getSideEffects () const  {return splitcount == 0 ? 0 : SideEffect_InvalidatesLivenessInfo;}

    void runImpl();

    void phase1();
    void phase2();
    void phase3();
    void phase4();
    void linkDef(Opndx::OpDefs&, OpDef*, OpDef*);
    BitSet* bitsetp (BitSet*&);
    Opnd* splitOpnd (const Opnd*);
};


static ActionFactory<WebMaker> _webmaker("webmaker");

static Counter<size_t> count_splitted("ia32:webmaker:splitted", 0);


//========================================================================================
// Internal debug helpers
//========================================================================================


#ifdef _DEBUG__WEBMAKER

using std::endl;
using std::ostream;

static void onConstruct (const IRManager&);
static void onDestruct  ();

struct Sep
{
    Sep ()      :first(true) {}

    bool first;
};

static ostream& operator << (ostream&, Sep&);

static ostream& operator << (ostream&, const Inst&);

static ostream& operator << (ostream&, const Opnd&);

static ostream& operator << (ostream&, /*const*/ BitSet*);

struct Dbgout : public  ::std::ofstream
{
    Dbgout (const char* s)          {open(s);}
    ~Dbgout ()                      {close();}
};

static Dbgout dbgout("WebMaker.txt");

#define DBGOUT(s) dbgout << s
//#define DBGOUT(s) std::cerr << s

#else

#define DBGOUT(s) 

#endif


//========================================================================================
//  WebMaker implementation
//========================================================================================


void WebMaker::runImpl()
{
#ifdef _DEBUG__WEBMAKER
    onConstruct(irm);
#endif

    opandcount = irManager->getOpndCount();
    nodecount = irManager->getFlowGraph()->getMaxNodeId();

    splitcount = 0;

    phase1();

//TBD   Local definitions also can be splitted!

    if (globcount != 0)
    {
        phase2();
        phase3();
        phase4();
    }

    count_splitted += splitcount;
    DBGOUT("***splitcount=" << splitcount << "/" << count_splitted << endl;)

#ifdef _DEBUG__WEBMAKER
    onDestruct();
#endif
}


void WebMaker::phase1()
{
    static CountTime phase1Timer("timer:ia32::webmaker:phase1");
    AutoTimer tm(phase1Timer);

    opndxs.resize(opandcount);
    for (size_t i = 0; i != opandcount; ++i)
    {
        Opndx* opndxp = 0;
        if (!irManager->getOpnd((uint32)i)->hasAssignedPhysicalLocation())
            opndxp = new Opndx(mm);
        opndxs[i] = opndxp;
    }

    BitSet lives(mm, (uint32)opandcount);
    globcount = 0;

    const Nodes& postOrder = irManager->getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = postOrder.begin(), end = postOrder.end(); it!=end; ++it) {
        Node* nodep = *it;
        
        if (nodep->isBlockNode())
        {
            for (Inst* instp = (Inst*)nodep->getFirstInst(); instp!=NULL; instp = instp->getNextInst()) {
                const uint32 iprops = instp->getProperties();
                Inst::Opnds defs(instp, Inst::OpndRole_AllDefs);
                size_t itx = 0;
                for (Inst::Opnds::iterator it = defs.begin(); it != defs.end(); it = defs.next(it), ++itx)
                {
                    Opnd* opndp = instp->getOpnd(it);
                    Opndx* opndxp = opndxs.at(opndp->getId());
                    if (opndxp != 0)
                    {
                        const uint32 oprole = const_cast<const Inst*>(instp)->getOpndRoles((uint32)itx);
                        const bool isdef = ((oprole & Inst::OpndRole_UseDef) == Inst::OpndRole_Def)
                                        && ((iprops & Inst::Properties_Conditional) == 0 );
                        if (isdef)
                        {// register the new definition
                            opndxp->opdefs.push_back(OpDef());
                            OpDef& opdef = opndxp->opdefs.back();
                            opdef.visited = false;
                            opdef.linkx = &opdef - &opndxp->opdefs.front();
                            opdef.globid = -1;
                            opdef.defp   = instp;
                            opdef.useps  = 0;
                        }
                    }
                }
            }

            irManager->getLiveAtExit(nodep, lives);
            BitSet::IterB bsk(lives);
            for (int i = bsk.getNext(); i != -1; i = bsk.getNext())
            {
                Opnd* opndp = irManager->getOpnd(i);
                Opndx*& opndxp = opndxs.at(opndp->getId());
                if (opndxp != 0 && !opndxp->opdefs.empty())
                {
                    OpDef& opdef = opndxp->opdefs.back();
                    if (opdef.defp->getNode() == nodep)
                    {
                        opdef.globid = (int)globcount++;
                        bitsetp(opndxp->globdefsp)->setBit(opdef.globid, true);
                    }
                }
            }
        }
    }

#ifdef _DEBUG__WEBMAKER
/*
    dbgout << "--- phase1 ---" << endl;
    for (size_t i = 0; i != opandcount; ++i)
        if (opndxs[i] != 0)
        {
            dbgout << " O#" << irManager->getOpnd(i)->getFirstId() << " (#" << i << ")" << endl;
            Opndx::OpDefs opdefs = opndxs[i]->opdefs;
            for (Opndx::OpDefs::iterator it = opdefs.begin(); it != opdefs.end(); ++it)
            {
                OpDef& opdef = *it;
                dbgout << "  linkx#" << opdef.linkx << " globid#" << opdef.globid 
                    << " opdef B#" << opdef.defp->getBasicBlock()->getId() << " " << *opdef.defp << endl;
            }
        }
    dbgout << "--- opandcount:" << opandcount << " globcount:" << globcount << endl;
*/
#endif
}


void WebMaker::phase2()
{
    static CountTime phase2Timer("timer:ia32::webmaker:phase2");
    AutoTimer tm(phase2Timer);

    nodexs.resize(nodecount);
    for (size_t n = 0; n != nodecount; ++n)
        nodexs[n] = new Nodex(mm, globcount);

    Opndx* opndxp;
    for (size_t i = 0; i != opandcount; ++i)
        if ((opndxp = opndxs[i]) != 0  && !opndxp->opdefs.empty())
        {
            Opndx::OpDefs::iterator it = opndxp->opdefs.begin(),
                                   end = opndxp->opdefs.end();
            for (; it != end; ++it)
            {
                OpDef& opdef = *it;
                Nodex* nodexp = nodexs.at(opdef.defp->getNode()->getId());

                if (opdef.globid != -1)
                    bitsetp(nodexp->globdefsp)->setBit(opdef.globid, true);

                if (opndxp->globdefsp != 0)
                    bitsetp(nodexp->globkillsp)->unionWith(*bitsetp(opndxp->globdefsp));
            }

        //  prepare for pass3
            opndxp->cbbp = 0;
            opndxp->copdefp = 0;
        }

    
    
    BitSet wrkbs(mm, (uint32)globcount);
    bool   wrkbsvalid;
    size_t passnb = 0;
    const Nodes& postOrder = irManager->getFlowGraph()->getNodesPostOrder();
    for (bool changes = true; changes; ++passnb)
    {
        changes = false;
        
        for (Nodes::const_reverse_iterator it = postOrder.rbegin(),end = postOrder.rend(); it!=end; ++it) 
        {
            Node* nodep = *it;
            Nodex* nodexp = nodexs[nodep->getId()];

            wrkbsvalid = false;

            const Edges& edges = nodep->getInEdges();
            size_t edgecount = 0;
            for (Edges::const_iterator ite = edges.begin(), ende = edges.end(); ite!=ende; ++ite, ++edgecount) {
                Edge* edgep = *ite;
                Node*  predp  = edgep->getSourceNode();
                Nodex* predxp = nodexs[predp->getId()];

                BitSet* predbsp = predxp->globexitsp;
                if (predbsp == 0)
                    predbsp = &predxp->globentrys;

                if (edgecount == 0) 
                {
                    wrkbs.copyFrom(*predbsp);
                    wrkbsvalid = true;
                }
                else 
                    wrkbs.unionWith(*predbsp);
            }

            if (passnb > 0 && (!wrkbsvalid || nodexp->globentrys.isEqual(wrkbs)))
                continue;

            if (!wrkbsvalid)
                wrkbs.clear();

            if (!nodexp->globentrys.isEqual(wrkbs))
            {
                nodexp->globentrys.copyFrom(wrkbs);
                changes = true;
            }

            if (nodexp->globkillsp != 0 && nodexp->globdefsp != 0)
            {
                if (nodexp->globkillsp != 0)
                    wrkbs.subtract(*nodexp->globkillsp);

                if (nodexp->globdefsp != 0)
                    wrkbs.unionWith(*nodexp->globdefsp);

                if (!bitsetp(nodexp->globexitsp)->isEqual(wrkbs))
                {
                    nodexp->globexitsp->copyFrom(wrkbs);
                    changes = true;
                }
            }
        }
    }

#ifdef _DEBUG__WEBMAKER
/*
    dbgout << "--- total passes:" << passnb << endl;
    for (size_t n = 0; n != nodecount; ++n)
    {
        Nodex* nodexp = nodexs[n];
        dbgout <<"  node#" << n << endl;
        dbgout <<"    entry " << &nodexp->globentrys << endl;
        if (nodexp->globexitsp != 0)
            dbgout <<"    exit  " << nodexp->globexitsp << endl;
    }
*/
#endif
}


void WebMaker::phase3()
{
    static CountTime phase3Timer("timer:ia32::webmaker:phase3");
    AutoTimer tm(phase3Timer);

    DBGOUT("--- phase3 ---" << endl;)

    const Nodes& postOrder  = irManager->getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = postOrder.begin(), end = postOrder.end(); it!=end; ++it) {
        Node* nodep = *it;
        if (nodep->isBlockNode())
        {
            const BitSet*  globentryp = &nodexs.at(nodep->getId())->globentrys;
            
            for (Inst* instp = (Inst*)nodep->getFirstInst(); instp!=NULL; instp = instp->getNextInst()) {
            {
                const uint32 iprops = instp->getProperties();
                Inst::Opnds all(instp, Inst::OpndRole_All);
                size_t itx = 0;
                for (Inst::Opnds::iterator it = all.begin(); it != all.end(); it = all.next(it), ++itx)
                {
                    Opnd* opndp = instp->getOpnd(it);
                    Opndx* opndxp = opndxs.at(opndp->getId());
                    if (opndxp != 0)
                    {
                        OpDef* opdefp = 0;
                        const uint32 oprole = const_cast<const Inst*>(instp)->getOpndRoles((uint32)itx);
                        const bool isdef = ((oprole & Inst::OpndRole_UseDef) == Inst::OpndRole_Def)
                                        && ((iprops & Inst::Properties_Conditional) == 0 );
                        DBGOUT(" O#" << opndp->getFirstId() << "(#" << opndp->getId() << ") def:" << isdef;)
                        DBGOUT(" B#" << instp->getBasicBlock()->getId() << " " << *instp;)

                        if (isdef) 
                        {// opand definition here
                            Opndx::OpDefs::iterator it = opndxp->opdefs.begin(),
                                                    end = opndxp->opdefs.end();
                            //if (opndxp->copdefp != 0)
                            //  it = opndxp->copdefp;
                            for (; it != end && it->defp != instp; ++it)
                                ;

                            assert(it != end);
                            opdefp = &*it;
                            opndxp->copdefp = opdefp;
                            opndxp->cbbp = nodep;
                            DBGOUT(" found1 globid#" << opdefp->globid << " def B#" << opdefp->defp->getBasicBlock()->getId() << " " << *opdefp->defp << endl;)
                        }
                        else
                        {// opand usage here
                            if (opndxp->cbbp != nodep)
                            {// it must be usage of global definition
                                OpDef* lastdefp = 0;
                                Opndx::OpDefs::iterator it = opndxp->opdefs.begin(),
                                                        end = opndxp->opdefs.end();
                                for (; it != end; ++it)
                                    if (it->globid != -1 && globentryp->getBit(it->globid))
                                    {
                                        opdefp = &*it;
                                        opndxp->copdefp = opdefp;
                                        opndxp->cbbp = nodep;

                                        if (lastdefp != 0)
                                            linkDef(opndxp->opdefs, lastdefp, opdefp);
                                        lastdefp = opdefp;
                                    }
                                assert(lastdefp != 0);
                                DBGOUT(" found2 globid#" << opdefp->globid << " def B#" << opdefp->defp->getBasicBlock()->getId() << " " << *opdefp->defp << endl;)
                            }
                            else
                            {// it can be usage of global or local definition
                                opdefp = opndxp->copdefp;
                                DBGOUT(" found3 globid#" << opdefp->globid << " def B#" << opdefp->defp->getBasicBlock()->getId() << " " << *opdefp->defp << endl;)
                            }
                        }
                        assert(opdefp != 0);

                        if (opdefp->globid == -1)
                        {
                            if (isdef)
                            {
                                ++opndxp->webscount;
                                if (opndxp->webscount > 1)
                                {
                                    opndxp->newopndp = splitOpnd(opndp);
                                    DBGOUT("**new local web found O#" << opndxp->newopndp->getFirstId() << "(#" << opndxp->newopndp->getId() << ")" << endl;)
                                }
                            }

                            if (opndxp->webscount > 1 && opdefp->defp->getNode() == nodep)
                            {
                                instp->replaceOpnd(opndp, opndxp->newopndp);
                                DBGOUT("  opand replaced by O#" << opndxp->newopndp->getFirstId() << "(#" << opndxp->newopndp->getId() << ")" << endl;)
                            }
                        }
                        else
                        {
                            if (opdefp->useps == 0)
                                opdefp->useps = new OpDef::Instps(mm);
                            opdefp->useps->push_back(instp);
                        }
                    }
                }
            }
        }
        }
    }
}


void WebMaker::phase4()
{
    static CountTime phase4Timer("timer:ia32::webmaker:phase4");
    AutoTimer tm(phase4Timer);

    DBGOUT("--- phase4 ---" << endl;)

    Opndx* opndxp;
    for (size_t i = 0; i != opandcount; ++i)
        if ((opndxp = opndxs[i]) != 0  && !opndxp->opdefs.empty())
        {
            Opnd* opndp = irManager->getOpnd((uint32)i);
            Opndx::OpDefs& opdefs = opndxp->opdefs;
            DBGOUT(" O#" << opndp->getFirstId() << "(#" << i << ")" << endl;)

            for (size_t itx = 0; itx != opdefs.size(); ++itx)
            {
                OpDef* opdefp = &opdefs[itx];
                DBGOUT("  " <<  itx << "->" << opdefp->linkx << " globid#" << opdefp->globid << endl;)
                if (opdefp->globid != -1 && !opdefp->visited)
                {
                    Opnd* newopndp = 0;
                    ++opndxp->webscount;
                    if (opndxp->webscount > 1)
                    {
                        newopndp = splitOpnd(opndp);
                        DBGOUT("**new global web found O#" << newopndp->getFirstId() << "(#" << newopndp->getId() << ")" << endl;)
                    }

                    while (!opdefp->visited)
                    {
                        DBGOUT("   -visited globid#" << opdefp->globid << endl;)

                        if (newopndp != 0 && opdefp->useps != 0)
                        {
                            OpDef::Instps::iterator it = opdefp->useps->begin(),
                                                   end = opdefp->useps->end();
                            for (; it != end; ++it)
                            {
                                (*it)->replaceOpnd(opndp, newopndp);
                                DBGOUT(" replace B#" << (*it)->getBasicBlock()->getId() << " " << *(*it);)
                                DBGOUT(" O#" << opndp->getFirstId() << "(#" << opndp->getId() << ")" << endl;)
                            }
                        }

                        opdefp->visited = true;
                        opdefp = &opdefs.at(opdefp->linkx);
                        assert(opdefp->globid != -1);
                    }
                }
            }
        }
}


void WebMaker::linkDef(Opndx::OpDefs& opdefs, OpDef* lastdefp, OpDef* opdefp)
{
    for (OpDef* p = lastdefp;;)
    {
        if (p == opdefp)
            return;
        if ((p = &opdefs.at(p->linkx)) == lastdefp)
            break;
    }

    size_t wx = lastdefp->linkx;
    lastdefp->linkx = opdefp->linkx;
    opdefp->linkx = wx;
    DBGOUT("** glob def linked " << opdefp->globid << " + " << lastdefp->globid << endl;)
}


BitSet* WebMaker::bitsetp (BitSet*& bsp)
{
    if (bsp == 0)
        bsp = new BitSet(mm, (uint32)globcount);
    else
        bsp->resize((uint32)globcount);
    return bsp;
}


Opnd* WebMaker::splitOpnd (const Opnd* op)
{
    Opnd* np = irManager->newOpnd(op->getType(), op->getConstraint(Opnd::ConstraintKind_Initial));
    np->setCalculatedConstraint( op->getConstraint(Opnd::ConstraintKind_Calculated)); 
    ++splitcount;
    return np;
}


//========================================================================================
// Internal debug helpers
//========================================================================================


#ifdef _DEBUG__WEBMAKER

static void onConstruct (const IRManager& irm)
{
    MethodDesc& md = irm.getMethodDesc();
    const char * methodName = md.getName();
    const char * methodTypeName = md.getParentType()->getName();
    DBGOUT(endl << "Constructed " << methodTypeName  << "." << methodName 
           << "(" << md.getSignatureString() << ")" << endl;)

//  sos = strcmp(methodTypeName, "spec/benchmarks/_213_javac/BatchEnvironment") == 0 &&
//        strcmp(methodName,     "flushErrors") == 0;
}


static void onDestruct ()
{
    DBGOUT(endl << "Destructed" << endl;)
}

#endif //#ifdef _DEBUG__WEBMAKER


//========================================================================================
//  Output formatters
//========================================================================================


#ifdef _DEBUG__WEBMAKER

static ostream& operator << (ostream& os, Sep& x)
{
    if (x.first)
        x.first = false;
    else
        os << ",";
    return os;
}

static ostream& operator << (ostream& os, const Inst& x)
{
    return os << "I#" << x.getId();
}


static ostream& operator << (ostream& os, const Opnd& x)
{
    os << "O#" << x.getFirstId();
    RegName rn = x.getRegName();
    if (rn != RegName_Null)
        os << "<" << getRegNameString(rn) << ">";
    if (x.isPlacedIn(OpndKind_Memory))
        os << "<mem>";
    return os;
}


static ostream& operator << (ostream& os, /*const*/ BitSet* bs)
{
    if (bs != 0)
    {
        os << "{";
        Sep s;
        BitSet::IterB bsk(*bs);
        for (int i = bsk.getNext(); i != -1; i = bsk.getNext())
            os << s << i;
        os << "}";
    }
    else
        os << "null";

    return os;
}

#endif //#ifdef _DEBUG__WEBMAKER

} //namespace Ia32
} //namespace Jitrino
