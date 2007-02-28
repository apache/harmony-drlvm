/**
 * @author Sergey L. Ivashin
 * @version $Revision$
 */

#include "Ia32IRManager.h"
#include "Ia32RegAllocCheck.h"
#include "Stl.h"
#include "Log.h"
#include "Ia32Printer.h"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdio.h>

#ifdef _DEBUG__REGALLOC3
#include "Ia32RegAllocWrite.h"
#ifdef _MSC_VER
#pragma warning(disable : 4505)   //unreferenced local function has been removed
#endif //#ifdef _MSC_VER
#endif //#ifdef _DEBUG__REGALLOC3


//
//  Flags to tune 
//
#define _VAR2_
#define _REGALLOC3_NEIGBH
//#define _REGALLOC3_COALESCE



using namespace std;

namespace Jitrino
{

namespace Ia32
{


//========================================================================================
// class Ia32RegAlloc3
//========================================================================================

/**
 *  This class attempts to assign register for any operand (found in LIR) that can be 
 *  allocated in register.
 *
 *  Set of registers available for allocation is specified by input arguments and saved 
 *  in 'constrs' class member. All operands that cannot be allocated in the registers 
 *  available are simply ignored.
 *
 *  So this allocator should be called for each set of the registers available (GPReg, XMM, 
 *  FP) independently.
 *
 *  It is not guaranteed that all operands which can be assigned will be assigned. 
 *  Therefore, the companion class (SpillGen) must be used after this allocator.
 *
 */

struct RegAlloc3 : public SessionAction
{
    MemoryManager mm;           // this is private MemoryManager, not irm.getMemoryManager()

    int coalesceCount;

    class BoolMatrix;
    typedef uint32 RegMask;     // used to represent set of registers

    static void merge (Constraint& c, RegMask mk)   {c.setMask(c.getMask() | mk);}

    // Table of all available registers sets.
    // Each set (GPReg, FPReg, XMMReg and so on) is represented by the corresponding 
    // Constraint object.
    struct Registers : public StlVector<Constraint>
    {
        Registers (MemoryManager& mm)       :StlVector<Constraint>(mm) {}

        void  parse (const char*);

        // register the new constraint (register) in the table.
        // if table doesn't contain constraint of the specified kind, it will be ignored
        // (add = false) or new table entry for the constraint will be created (add = true).
        int merge (const Constraint&, bool add = false);

        // returns table index of the constraint of the specified kind or -1
        int index (const Constraint&) const;

        int indexes[IRMaxRegKinds];
    };
    Registers registers;

    struct Oprole
    {
        Inst* inst;
        uint32 role;
    };
    typedef StlVector<Oprole> Oproles;

    struct Opndx
    {
        typedef StlList<int> Indexes;
        Indexes* adjacents,
               * hiddens;

        Indexes* neighbs;

        Opnd*    opnd;
        Oproles* oproles;

        int ridx;       // index in Registers of register assigned/will be assigned
        RegMask alloc,  // 0 or mask of the register assigned
                avail;  // if not assigned, then mask of the registers available    
        int nbavails;   // number of the registers available for this operand
        int spillcost;
        bool spilled;
    };

    //  Operand's graph to be colored
    struct Graph : public StlVector<Opndx>
    {
        Graph (MemoryManager& m)            : StlVector<Opndx>(m), mm(m) {}

        void connect (int x1, int x2) const;
        int  disconnect (int x) const;
        void reconnect  (int x) const;
        void moveNodes (Opndx::Indexes& from, Opndx::Indexes& to, int x) const;

        MemoryManager& mm;
    };
    Graph graph;

    int graphsize;

    StlVector<int> nstack;


    RegAlloc3 ()                    : mm(1000, "RegAlloc3"), registers(mm), graph(mm), nstack(mm) {}

    uint32 getNeedInfo () const     {return NeedInfo_LivenessInfo;}
    uint32 getSideEffects () const  {return coalesceCount == 0 ? 0 : SideEffect_InvalidatesLivenessInfo;}

    void runImpl();
    bool verify(bool force=false);

    bool buildGraph ();
    bool coalescing (int* opandmap, BoolMatrix& matrix);
    void coalesce   (int* opandmap, BoolMatrix& matrix, int, int);
    void pruneGraph ();
    void assignRegs ();
    bool assignReg (int);
};


static ActionFactory<RegAlloc3> _cg_regalloc("cg_regalloc");


static Counter<size_t> count_spilled("ia32:regalloc3:spilled", 0),
                       count_assigned("ia32:regalloc3:assigned", 0),
                       count_coalesced("ia32:regalloc3:coalesced", 0);


//========================================================================================
// Internal debug helpers
//========================================================================================


using std::endl;
using std::ostream;

#ifdef _DEBUG__REGALLOC3

#include "Ia32RegAllocWrite.h"

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

static ostream& operator << (ostream&, Constraint);

static ostream& operator << (ostream&, const RegAlloc3::Registers&);

struct RegMasks
{
    RegMasks (Constraint x, RegAlloc3::RegMask mk)  : c(x) {c.setMask(mk);}

    Constraint c;
};

static ostream& operator << (ostream&, RegMasks);

static ostream& outRegMasks (ostream&, RegAlloc3::RegMask*, const RegAlloc3::Registers&);

static ostream& operator << (ostream&, const RegAlloc3::Opndx&);

static ostream& operator << (ostream&, const RegAlloc3::Graph&);

struct Dbgout : public  ::std::ofstream
{
    Dbgout (const char* s)          {open(s);}
    ~Dbgout ()                      {close();}
};

static Dbgout dbgout("RegAlloc3.txt");

#define DBGOUT(s) dbgout << s

#else

#define DBGOUT(s) 

#endif


//========================================================================================
//  Utility
//========================================================================================


static size_t bitCount (RegAlloc3::RegMask mk)
{
    size_t count = 0;
    while (mk != 0)
    {
        if ((mk & 1) != 0)
            ++count;
        mk >>= 1;
    }
    return count;
}


static size_t bitNumber (RegAlloc3::RegMask mk)
{
    assert(mk != 0);

    size_t number = 0;
    while (mk != 1)
    {
        ++number;
        mk >>= 1;
    }
    return number;
}


static RegAlloc3::RegMask findHighest (RegAlloc3::RegMask mk)
{
    assert(mk != 0);

    RegAlloc3::RegMask high = 1,
                       highest = (RegAlloc3::RegMask)~1;

    while ((mk & highest) != 0)
    {
        high <<= 1,
        highest <<= 1;
    }

    return high;
}


//========================================================================================
//  Tokens - Utility class for (zero-terminated) strings parsing
//========================================================================================


class Tokens
{
public:

    Tokens (const char* s)          :src(s) {;}

    void init (const char* s)       {src = s;}
    bool scan ();
    bool isWord () const            {return isw;}
    const char* lex () const        {return buff;}

protected:

    const char* src;
    char* dst;
    char  buff[64];
    bool  isw;
};

//========================================================================================
//  BoolMatrix - Symmetric boolean matrix
//========================================================================================


class RegAlloc3::BoolMatrix
{
public:

    BoolMatrix (MemoryManager&, int);

    void clear ();
    void clear (int i, int j)           {at(i,j) = 0;}
    void set   (int i, int j)           {at(i,j) = 1;}
    bool test  (int i, int j) const     {return at(i,j) != 0;}

private:

    char& at (int i, int j) const
    {
        assert(0 <= i && i < dim && 0 <= j && j < dim && i != j);

        return (i < j) ? *(i*dim + j + ptr)
                       : *(j*dim + i + ptr);
    }

    int   dim, dims;
    char* ptr;
};


RegAlloc3::BoolMatrix::BoolMatrix (MemoryManager& mm, int d)
{
    assert(d > 0);
    dim = d;
    ptr = new (mm) char[dims = dim*dim];
    clear();
}


void RegAlloc3::BoolMatrix::clear ()                        
{
    memset(ptr, 0, dims);
}


//========================================================================================
//  Registers implementation
//========================================================================================


//  Parse input parameters (registers available) and build table of the regsiters
//  available for allocalion ('registers').
//
void RegAlloc3::Registers::parse (const char* params)
{
    if (params == 0 || strcmp(params, "ALL") == 0)
    {
        push_back(Constraint(RegName_EAX)
                 |Constraint(RegName_ECX)
                 |Constraint(RegName_EDX)
                 |Constraint(RegName_EBX)
                 |Constraint(RegName_ESI)
                 |Constraint(RegName_EDI)
                 |Constraint(RegName_EBP));

        push_back(Constraint(RegName_XMM1)
                 |Constraint(RegName_XMM2)
                 |Constraint(RegName_XMM3)
                 |Constraint(RegName_XMM4)
                 |Constraint(RegName_XMM5)
                 |Constraint(RegName_XMM6)
                 |Constraint(RegName_XMM7));

        push_back(Constraint(RegName_FP0));
    }
    else
    {
        Constraint c;
        for (Tokens t(params); t.scan(); )
            if (t.isWord())
            {
                RegName r = getRegName(t.lex());
                if (r != RegName_Null)
                    c = Constraint(r);

                merge(c, true);
            }
    }

    assert(!empty());

    for (size_t i = 0; i != IRMaxRegKinds; ++i)
        indexes[i] = -1;

    for (size_t i = 0; i != size(); ++i)
        indexes[operator[](i).getKind()] = (int)i;
}


int RegAlloc3::Registers::merge (const Constraint& c, bool add)
{
    if (c.getMask() != 0)
    {
        for (size_t i = 0; i != size(); ++i)
        {
            Constraint& r = operator[](i);
            if (r.getKind() == c.getKind())
            {
                r.setMask(r.getMask() | c.getMask());
                return (int)i;
            }
        }

        if (add)
            push_back(c);
    }

    return -1;
}


int RegAlloc3::Registers::index (const Constraint& c) const
{
    return indexes[c.getKind() & OpndKind_Reg];
}


//========================================================================================
//  Graph implementation
//========================================================================================


void RegAlloc3::Graph::connect (int x1, int x2) const
{
    at(x1).adjacents->push_back(x2);
    at(x2).adjacents->push_back(x1);
}


int RegAlloc3::Graph::disconnect (int x) const
{
//  Node to be disconnected
    const Opndx& opndx = at(x);
    if (opndx.adjacents->empty())
        return 0;

    int disc = 0;

    for (Opndx::Indexes::iterator k = opndx.adjacents->begin(); k != opndx.adjacents->end(); ++k)
    {
    //  this node is adjacent to the node to be disconnected
        const Opndx& adjopndx = at(*k);
        if (!adjopndx.adjacents->empty())
        {
            moveNodes(*adjopndx.adjacents, *adjopndx.hiddens, x);
            if (adjopndx.adjacents->empty())
                disc++;
        }
    }

    opndx.hiddens->splice(opndx.hiddens->begin(), *opndx.adjacents);

    return ++disc;
}


void RegAlloc3::Graph::reconnect (int x) const
{
//  Node to be reconnected
    const Opndx& opndx = at(x);

    for (Opndx::Indexes::iterator k = opndx.hiddens->begin(); k != opndx.hiddens->end(); ++k)
    {
    //  this node was adjacent to the node to be reconnected
        const Opndx& adjopndx = at(*k);
        moveNodes(*adjopndx.hiddens, *adjopndx.adjacents, x);
    }

    opndx.adjacents->splice(opndx.adjacents->begin(), *opndx.hiddens);
}


void RegAlloc3::Graph::moveNodes (Opndx::Indexes& from, Opndx::Indexes& to, int x) const
{
    Opndx::Indexes::iterator i;
    while ((i = find(from.begin(), from.end(), x)) != from.end())
        to.splice(to.begin(), from, i);
}


//========================================================================================
//  RegAlloc3 implementation
//========================================================================================


void RegAlloc3::runImpl ()
{
#ifdef _DEBUG__REGALLOC3
    onConstruct(*irManager);
#endif

    irManager->fixEdgeProfile();

    registers.parse(getArg("regs"));
    DBGOUT("parameters: " << registers << endl;)

    coalesceCount = 0;

    if (buildGraph())
    {

#ifdef _DEBUG__REGALLOC3
        dbgout << "--- graph" << endl;
        for (int x = 0; x != graphsize; ++x)
        {
            const Opndx& opndx = graph.at(x);
            dbgout << "(" << x << ") " << opndx.opnd->getId()
                   << " " << *opndx.opnd << " ridx:" << opndx.ridx 
                   << " avail:" << hex << opndx.avail << dec << " nbavails:" << opndx.nbavails
                   << " spillcost:" << opndx.spillcost;

            if (!opndx.adjacents->empty())
            {
                Sep s;
                dbgout << " adjacents{";
                for (RegAlloc3::Opndx::Indexes::const_iterator i = opndx.adjacents->begin(); i != opndx.adjacents->end(); ++i)
                    dbgout << s << *i;
                dbgout << "}";
            }

            if (opndx.neighbs != 0)
            {
                Sep s;
                dbgout << " neighbs{";
                for (RegAlloc3::Opndx::Indexes::const_iterator i = opndx.neighbs->begin(); i != opndx.neighbs->end(); ++i)
                    dbgout << s << *i;
                dbgout << "}";
            }

            dbgout << endl;
        }
        dbgout << "---------" << endl;
#endif

        pruneGraph();
        assignRegs();
    }

    count_coalesced += coalesceCount;

#ifdef _DEBUG__REGALLOC3
    RegAllocWrite raw(*irManager, "regalloc3.xml");
    raw.write();

    onDestruct();
#endif
}


bool RegAlloc3::verify (bool force)
{   
    bool failed = false;
    if (force || getVerificationLevel() >=2 )
    {
        RegAllocCheck chk(getIRManager());
        if (!chk.run(false))
            failed = true;
        if (!SessionAction::verify(force))
            failed = true;
    }

    return !failed;
}   


bool RegAlloc3::buildGraph ()   
{
    size_t opandcount = getIRManager().getOpndCount();
    graph.reserve(opandcount);

//  First, scan all the operands available and see if operand is already assigned
//  or need to be assigned

    Opndx opndx;
    int* opandmap = new (mm) int[opandcount];

    for (size_t i = 0; i != opandcount; ++i)
    {
        Opnd* opnd  = getIRManager().getOpnd((uint32)i);
        int mapto = -1;

        int ridx;
        Constraint loc = opnd->getConstraint(Opnd::ConstraintKind_Location, OpndSize_Default);
        if (!loc.isNull())
        {// this operand is already assigned to some location/register
            if ((ridx = registers.index(loc)) != -1)
            {
                opndx.opnd  = opnd;
                opndx.ridx  = ridx;
                opndx.alloc = loc.getMask();
                mapto = (int)graph.size();
                graph.push_back(opndx);
            }
        }
        else
        {// this operand is not allocated yet
            loc = opnd->getConstraint(Opnd::ConstraintKind_Calculated, OpndSize_Default);
            if ((ridx = registers.index(loc)) != -1)
            {// operand should be assigned to register
                opndx.opnd  = opnd;
                opndx.ridx  = ridx;
                opndx.alloc = 0;
                opndx.avail = loc.getMask() & registers[ridx].getMask();
                opndx.nbavails = (int)bitCount(opndx.avail);
                assert(opndx.nbavails != 0);
                opndx.spillcost = 1;
                opndx.spilled = false;
                mapto = (int)graph.size();
                graph.push_back(opndx);
            }
        }

        opandmap[i] = mapto;
    }

    if ((graphsize = (int)graph.size()) == 0)
        return false;

    for (Graph::iterator i = graph.begin(); i != graph.end(); ++i)
    {
        i->adjacents = new (mm) Opndx::Indexes(mm);
        i->hiddens   = new (mm) Opndx::Indexes(mm);
        i->oproles   = new (mm) Oproles(mm);
        i->neighbs   = 0;
    }

//  Second, iterate over all instructions in CFG and calculate which operands
//  are live simultaneouesly (result stored in matrix)

    BoolMatrix matrix(mm, graphsize);

    BitSet lives(mm, (uint32)opandcount);

    const Nodes& nodes = irManager->getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode())
        {
            Inst* inst = (Inst*)node->getLastInst();
            if (inst == 0)
                continue;

            int excount = static_cast<int>(node->getExecCount());
            if (excount < 1)
                excount = 1;
            excount *= 100;
            //int excount = 1;

        //  start with the operands at the block bottom
            getIRManager().getLiveAtExit(node, lives);

        //  iterate over instructions towards the top of the block
            for (;;)
            {
                int i, x;
#ifdef _OLD_
                Inst::Opnds defs(inst, Inst::OpndRole_AllDefs);
                for (Inst::Opnds::iterator it = defs.begin(); it != defs.end(); it = defs.next(it))
                    if ((x = opandmap[i = inst->getOpnd(it)->getId()]) != -1)
                    {
                        BitSet::IterB bsk(lives);
                        int k, y;
                        for (k = bsk.getNext(); k != -1; k = bsk.getNext())
                            if (k != i && (y = opandmap[k]) != -1)
                                matrix.set(x, y);
                    }
#else
                int defx = -1;
                Oprole oprole;
                const uint32* oproles = const_cast<const Inst*>(inst)->getOpndRoles();
                Inst::Opnds opnds(inst, Inst::OpndRole_All);
                for (Inst::Opnds::iterator it = opnds.begin(); it != opnds.end(); it = opnds.next(it))
                    if ((x = opandmap[i = inst->getOpnd(it)->getId()]) != -1)
                    {
                        Opndx& opndx = graph.at(x);

                        oprole.inst = inst;
                        oprole.role = oproles[it];
                        opndx.oproles->push_back(oprole);

                        if (oprole.role & Inst::OpndRole_Def)
                        {
                            defx = x;
                            BitSet::IterB bsk(lives);
                            int k, y;
                            for (k = bsk.getNext(); k != -1; k = bsk.getNext())
                                if (k != i && (y = opandmap[k]) != -1)
                                    matrix.set(x, y);
                        }
#ifdef _REGALLOC3_NEIGBH
                        else if (defx != -1 && !lives.getBit(opndx.opnd->getId()))
                        {
                            Opndx& defopndx = graph.at(defx);
                            if (defopndx.neighbs == 0)
                                defopndx.neighbs = new (mm) Opndx::Indexes(mm);
                            defopndx.neighbs->push_back(x);

                            if (opndx.neighbs ==0)
                                opndx.neighbs = new (mm) Opndx::Indexes(mm);
                            opndx.neighbs->push_back(defx);
                        }
#endif

                        opndx.spillcost += excount;
                    }
#endif

                if (inst->getPrevInst() == 0)
                    break;

                getIRManager().updateLiveness(inst, lives);
                inst = inst->getPrevInst();
            }
        }
    }

//  Do iterative coalescing

#ifdef _REGALLOC3_COALESCE
        while (coalescing(opandmap, matrix))
        /*nothing*/;
#endif

//  Third, connect nodes that represent simultaneouesly live operands

    for (int x1 = 1; x1 < graphsize; ++x1)
        for (int x2 = 0; x2 < x1; ++x2)
            if (matrix.test(x1, x2) && graph.at(x1).ridx == graph.at(x2).ridx)
                graph.connect(x1, x2);

    return true;
}


bool RegAlloc3::coalescing (int* opandmap, BoolMatrix& matrix)
{
    int x0, x1;

    const Nodes& nodes = irManager->getFlowGraph()->getNodesPostOrder();
    for (Nodes::const_iterator it = nodes.begin(), end = nodes.end(); it!=end; ++it) {
        Node* node = *it;
        if (node->isBlockNode())
        {
            for (const Inst*  inst = (Inst*)node->getLastInst(); inst != 0; inst = inst->getPrevInst())
                if (inst->getMnemonic() == Mnemonic_MOV)
                    if ((x0 = opandmap[inst->getOpnd(0)->getId()]) != -1 &&
                        (x1 = opandmap[inst->getOpnd(1)->getId()]) != -1 &&
                        x0 != x1 && !matrix.test(x0, x1))
                    {
                        Opndx& opndx0 = graph.at(x0),
                             & opndx1 = graph.at(x1);

                        if (opndx0.ridx  == opndx1.ridx && opndx0.avail == opndx1.avail)
                        {
                            coalesce (opandmap, matrix, x0, x1);
                            return true;
                        }
                    }
        }
    }

    return false;
}


//  Colalesce graph nodes (x0) and (x1) and the corresponding operands.
//  Node (x1) not to be used anymore, (x0) must be used unstead.
//  Note that (x1) remains in the graph
//
void RegAlloc3::coalesce (int* opandmap, BoolMatrix& matrix, int x0, int x1)
{
    DBGOUT("*coalescing (" << x0 << ") with (" << x1 << ")" << endl;)

    Opndx& opndx0 = graph.at(x0),
         & opndx1 = graph.at(x1);

    opndx1.spilled = true;

    Opnd* opnd0 = opndx0.opnd,
        * opnd1 = opndx1.opnd;

    opandmap[opnd1->getId()] = x0;

    Oproles::iterator it  = opndx1.oproles->begin(),
                      end = opndx1.oproles->end();
    for (; it != end; ++it)
        it->inst->replaceOpnd(opnd1, opnd0);

    opndx0.oproles->reserve(opndx0.oproles->size() + opndx1.oproles->size());
    opndx0.oproles->insert(opndx0.oproles->end(), opndx1.oproles->begin(), opndx1.oproles->end());
    opndx1.oproles->clear();

    for (int x = 0; x < graphsize; ++x)
        if (x != x1 && matrix.test(x, x1))
        {
            matrix.set(x, x0);
            matrix.clear(x, x1);
        }

    ++coalesceCount;
}


#ifndef _VAR0

struct sortRule1
{
    const RegAlloc3::Graph& graph;


    sortRule1 (const RegAlloc3::Graph& g)   :graph(g) {}


    bool operator () (int x1, int x2)
    {
        const RegAlloc3::Opndx& opndx1 = graph.at(x1),
                              & opndx2 = graph.at(x2);

#ifdef _VAR1_
        return opndx1.spillcost > opndx2.spillcost;
#endif
#ifdef _VAR2_
        return opndx1.spillcost < opndx2.spillcost;
#endif

    }
};

#endif


void RegAlloc3::pruneGraph ()
{
    DBGOUT(endl << "pruneGraph - start"<< endl;)

    size_t nbnodes = 0;
    for (int i = 0; i != graphsize; ++i)
        if (!graph.at(i).adjacents->empty())
            nbnodes++;

    StlVector<int> tmp(mm);

    nstack.reserve(nbnodes);
    while (nbnodes != 0)
    {
    //  Apply degree < R rule

#ifdef _VAR0_

        for (bool succ = false; !succ;)
        {
            succ = true;
            for (int i = 0; i != graphsize; ++i)
            {
                Opndx& opndx = graph.at(i);
                const int n = (int)opndx.adjacents->size();
                if (n != 0 && n < opndx.nbavails)
                {
                    nbnodes -= graph.disconnect(i);
                    nstack.push_back(i);
                    succ = false;

                    DBGOUT(" rule#1 (" << i << ")" << endl;)
                }
            }

        }

#else

        for (bool succ = false; !succ;)
        {
            succ = true;

            tmp.resize(0);

            for (int i = 0; i != graphsize; ++i)
            {
                Opndx& opndx = graph.at(i);
                const int n = (int)opndx.adjacents->size();
                if (n != 0 && n < opndx.nbavails)
                    tmp.push_back(i);
            }

            if (tmp.size() != 0)
            {
                DBGOUT("buck size:" << tmp.size() << endl;)
                if (tmp.size() > 1)
                    sort(tmp.begin(), tmp.end(), sortRule1(graph));

                for (StlVector<int>::iterator it = tmp.begin(); it != tmp.end(); ++it)
                {
                    nbnodes -= graph.disconnect(*it);
                    nstack.push_back(*it);

                    DBGOUT(" rule#1 (" << *it << ")  cost:" << graph.at(*it).spillcost << endl;)
                }

                succ = false;
            }
        }

#endif

    //  Apply degree >= R rule

        if (nbnodes != 0)
        {
            int x = -1, n;
            double cost = 0, w;

        //  Find some node to disconnect 
            for (int i = 0; i != graphsize; ++i)
            {
                Opndx& opndx = graph.at(i);
                if ((n = (int)opndx.adjacents->size()) != 0)
                {
                    w = (double)opndx.spillcost/(double)n;
                    DBGOUT("    (" << i << ") cost:" << w << endl;)
                    if (x == -1 || w < cost)
                        cost = w,
                        x    = i;
                }
            }

            assert(x != -1);
            if (x != -1)
            {
                nbnodes -= graph.disconnect(x);
                nstack.push_back(x);

                DBGOUT(" rule#2 (" << x << ") cost:" << cost << endl;)
            }
        }
    }

    DBGOUT("pruneGraph - stop"<< endl << graph;)
}


void RegAlloc3::assignRegs ()
{
    while (!nstack.empty())
    {
        int x = nstack.back();
        nstack.pop_back();

        Opndx& opndx = graph.at(x);
        graph.reconnect(x);
        opndx.spilled = !assignReg(x);
    }

    for (int x = 0; x != graphsize; ++x)
    {
        Opndx& opndx = graph.at(x);
        if (opndx.alloc == 0 && !opndx.spilled )
            assignReg(x);
    }
}


bool RegAlloc3::assignReg (int x)
{
    Opndx& opndx = graph.at(x);
    RegMask alloc = 0;

    for (Opndx::Indexes::iterator i = opndx.adjacents->begin(); i != opndx.adjacents->end(); ++i)
        alloc |= graph.at(*i).alloc;

    if ((alloc = opndx.avail & ~alloc) == 0)
    {
        ++count_spilled;
        DBGOUT("spilled (" << x << ")" << endl;)
        return false;
    }
    else
    {
        if (opndx.neighbs != 0)
        {
            RegMask neighbs = 0;
            for (Opndx::Indexes::iterator i = opndx.neighbs->begin(); i != opndx.neighbs->end(); ++i)
            {
                Opndx& neigbx = graph.at(*i);
                if (neigbx.ridx == opndx.ridx)
                    neighbs |= neigbx.alloc;
            }

            if ((neighbs & alloc) != 0 && neighbs != alloc)
            {
                DBGOUT("! alloc:" << std::hex << alloc << " * neighbs:"  << neighbs << " =" << (alloc & neighbs) << std::dec << endl);
                alloc &= neighbs;
            }
        }

        opndx.alloc = findHighest(alloc);
        opndx.opnd->assignRegName(getRegName((OpndKind)registers[opndx.ridx].getKind(), 
                                             opndx.opnd->getSize(), 
                                             (int)bitNumber(opndx.alloc)));

        ++count_assigned;
        DBGOUT("assigned (" << x << ") = <" << getRegNameString(opndx.opnd->getRegName()) << ">" << endl;)
        return true;
    }
}


//========================================================================================
// Internal debug helpers
//========================================================================================


#ifdef _DEBUG__REGALLOC3

static void onConstruct (const IRManager& irm)
{
    MethodDesc& md = irm.getMethodDesc();
    const char * methodName = md.getName();
    const char * methodTypeName = md.getParentType()->getName();
    dbgout << endl << "Constructed " << methodTypeName  << "." << methodName 
           << "(" << md.getSignatureString() << ")" << endl;

//  sos = strcmp(methodTypeName, "spec/benchmarks/_213_javac/BatchEnvironment") == 0 &&
//        strcmp(methodName,     "flushErrors") == 0;
}


static void onDestruct ()
{
    dbgout << endl << "Destructed" << endl;
}

#endif //#ifdef _DEBUG__REGALLOC3


//========================================================================================
//  Output formatters
//========================================================================================


#ifdef _DEBUG__REGALLOC3

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


static ostream& operator << (ostream& os, Constraint c)
{
    IRPrinter::printConstraint(os, c); 
    return os;
}


static ostream& operator << (ostream& os, const RegAlloc3::Registers& x)
{
    Sep s;;
    os << "{";
    for (RegAlloc3::Registers::const_iterator it = x.begin(); it != x.end(); ++it)
        os << s << *it;
    return os << "}";
}


static ostream& operator << (ostream& os, RegMasks x)
{
    return os << x.c;
}


static ostream& outRegMasks (ostream& os, RegAlloc3::RegMask* x, const RegAlloc3::Registers& registers)
{
    Sep s;;
    os << "{";
    for (size_t rk = 0; rk != registers.size(); ++rk)
    {
        RegAlloc3::RegMask msk = x[rk];

        for (size_t rx = 0; msk != 0; ++rx, msk >>= 1)
            if ((msk & 1) != 0)
            {
                RegName reg = getRegName((OpndKind)registers[rk].getKind(), registers[rk].getSize(), rx);
                os<< s << getRegNameString(reg);
            }
    }
    return os << "}";
}


static ostream& operator << (ostream& os, const RegAlloc3::Opndx& opndx)
{
    RegName reg;
    if ((reg = opndx.opnd->getRegName()) != RegName_Null)
        os << " <" << getRegNameString(reg) << ">";

    if (!opndx.adjacents->empty())
    {
        Sep s;
        os << " adjacents{";
        for (RegAlloc3::Opndx::Indexes::const_iterator i = opndx.adjacents->begin(); i != opndx.adjacents->end(); ++i)
            os << s << *i;
        os << "}";
    }

    if (!opndx.hiddens->empty())
    {
        Sep s;
        os << " hiddens{";
        for (RegAlloc3::Opndx::Indexes::const_iterator i = opndx.hiddens->begin(); i != opndx.hiddens->end(); ++i)
            os << s << *i;
        os << "}";
    }

    return os;
}


static ostream& operator << (ostream& os, const RegAlloc3::Graph& graph)
{
    int x = 0;
    for (RegAlloc3::Graph::const_iterator i = graph.begin(); i != graph.end(); ++i)
        os << x++ << ") " << *i << endl;

    return os;
}


#endif //#ifdef _DEBUG__REGALLOC3

} //namespace Ia32
} //namespace Jitrino
