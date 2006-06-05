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
 * @version $Revision: 1.23.20.5 $
 *
 */


#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "Jitrino.h"
#include "Profiler.h"
#include "Inst.h"
#include "Stl.h"
#include "MemoryManager.h"

#if defined(_MSC_VER) && !defined (__ICL) && !defined (__GNUC__) /* Microsoft C Compiler ONLY */
#define SCANF_I64 "I64"
#define SPRINTF_I64 "I64"
#else
#define SCANF_I64 "ll"
#define SPRINTF_I64 "ll"
#endif

namespace Jitrino
    {

#define     DEFAULT_ITERATION_COUNT 1.0E5


ProfileControl profileCtrl;


// Generates the method string into the given buffer of size 'n'.
// Returns the string length if success.
// Returns 0 if the given buffer is too small.
uint32
genMethodString(MethodDesc& mdesc, char *methodStr, uint32 n)
{
    const char* className = mdesc.getParentType()->getName();
    const char* methodName = mdesc.getName();
    const char* methodSig = mdesc.getSignatureString();

    uint32 methodStrLen = (uint32) (strlen(className) + strlen(methodName)
                                    + strlen(methodSig) + 3);

    if (methodStrLen >= n)
        return 0;

    sprintf(methodStr, "%s::%s%s", className, methodName, methodSig);
    return methodStrLen;
}   // genMethodString


// Compute the checksum contribution of the given CFGEdge
uint32
getEdgeCheckSum(CFGEdge* edge)
{
    uint32  srcNodeId = edge->getSourceNode()->getId();
    uint32  dstNodeId = edge->getTargetNode()->getId();
    uint32  edgeId = edge->getId();
    uint32  checkSum = ((srcNodeId & 0x3FF) << 22) + ((dstNodeId & 0x3FF) << 12)
                        + (edgeId & 0xFFF);
    if (profileCtrl.debugCFGCheckSum())
        fprintf(stderr, "<checkSum> %u : %u -> %u\n", edgeId, srcNodeId, dstNodeId);
    return checkSum;
}   // getEdgeCheckSum


LoopProfile::LoopProfile(MemoryManager& mm, LoopNode *loop)
    : _mm(mm), _members(mm), _exits(mm)
{
    _iter_count = -1.0;
    _exit_prob = NULL;
    _loop = loop;
    _entry = loop->getHeader();
    addMember(_entry);
}   // LoopProfile::LoopProfile


void
LoopProfile::addMember(CFGNode *node)
{
    CFGEdge     *edge;
    CFGNode     *dst;

    assert(_loop->inLoop(node));
    _members.push_back(node->getId());

    // add any exit edges to 'exits'.
    const CFGEdgeDeque& oEdges = node->getOutEdges();
    CFGEdgeDeque::const_iterator eiter;
    for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
        edge = *eiter;
        dst = edge->getTargetNode();
        if ( !_loop->inLoop(dst) )
            _exits.push_back(edge);
        else if (dst == _entry)
            _back_edge = edge;
    }
}   // LoopProfile::addMember


void
LoopProfile::dump(FILE *file)
{
    uint32  i;
    CFGEdge *e;

    fprintf(file, "LOOP PROFILE: entry [%u] iter_count %g\n",
            _entry->getId(), _iter_count);
    fprintf(file, "\t back edge : %u -> %u\n",
        _back_edge->getSourceNode()->getId(), _back_edge->getTargetNode()->getId());
    fprintf(file, "\t members : ");
    for (i = 0; i < _members.size(); i++)
        fprintf(file, "%u ", _members[i]);
    fprintf(file, "\n\t exits : \n");
    for (i = 0; i < _exits.size(); i++) {
        e = _exits[i];
        fprintf(file, "\t\t%u -> %u [%g]\n", e->getSourceNode()->getId(),
            e->getTargetNode()->getId(), _exit_prob[i]);
    }
}   // LoopProfile::dump


EdgeProfile::EdgeProfile(MemoryManager& mm, FILE *file, char *str,
                         uint32 counterSize) : _mm(mm), _method(NULL)
{
    int     res;
    uint32  i;
    uint32  n = (uint32) strlen(str);

    _next = NULL;
    _methodStr = new(_mm) char[n+1];
    strcpy(_methodStr, str);
    _entryFreq = 0.0;
    _numEdges = _checkSum = 0;
    _blockInfo = NULL;
    _edgeProb = NULL;
    _hasCheckSum = false;
    _profileType = NoProfile;

    res = fscanf(file, "%u", &_checkSum);
    res &= fscanf(file, "%u", &_numEdges);
    if (counterSize == 4) {
        uint32  val32;
        res &= fscanf(file, "%u", &val32);
        if (res == 0) {
            _entryFreq = 0.0;
            _numEdges = _checkSum = 0;
            return;
        }
        _entryFreq = (double) val32;

        _numEdges--;    // exclude the entry block freq
        _edgeFreq = new(_mm) double[_numEdges];

        for ( i = 0; i < _numEdges; i++) {
            res = fscanf(file, "%u", &val32);
            if (res == 0 || res == EOF) {
                _entryFreq = 0.0;   // incomplete profile
                _numEdges = _checkSum = 0;
                break;
            }
            _edgeFreq[i] = (double) val32;
        }
    } else {        // counterSize == 8
        int64  val64;
        res &= fscanf(file, "%"SCANF_I64"d", &val64);
        if (res == 0) {
            _entryFreq = 0.0;
            _numEdges = _checkSum = 0;
            return;
        }

        _numEdges--;    // exclude the entry block freq
        _entryFreq = (double) val64;
        for ( i = 0; i < _numEdges; i++) {
            res = fscanf(file, "%"SCANF_I64"d", &val64);
            if (res == 0 || res == EOF) {
                _entryFreq = 0.0;   // incomplete profile
                _numEdges = _checkSum = 0;
                break;
            }
            _edgeFreq[i] = (double) val64;
        }
    }
    _hasCheckSum = true;
    _profileType = InstrumentedEdge;
}   // EdgeProfile::EdgeProfile(MemoryManager&, FILE*, char*, uint32)


// Build an edge profile buffer for the method specified by 'mdesc'.
EdgeProfile::EdgeProfile(MemoryManager& mm, MethodDesc& mdesc, bool smoothing)
    : _mm(mm), _method(&mdesc)
{
    uint32  i;
    char    methodStr[MaxMethodStringLength];

    _entryFreq = 0.0;
    _numEdges = _checkSum = 0;
    _edgeFreq = NULL;
    _next = NULL;
    _blockInfo = NULL;
    _edgeProb = NULL;
    _hasCheckSum = false;
    _profileType = NoProfile;

    _methodStr = NULL;
    if ((i = genMethodString(mdesc, methodStr, MaxMethodStringLength)) != 0) {
        _methodStr = new(_mm) char[i];
        strcpy(_methodStr, methodStr);
    }

    return;
}   


// Validate the profile source is consistent with the CFG.
// Return TRUE if the profile is OK; FALSE otherwise.
bool
EdgeProfile::isProfileValidate(
    FlowGraph&              flowGraph,
    StlVector<CFGNode*>&    po,
    bool                    warning_on)
{
    StlVector<CFGNode*>::reverse_iterator niter;
    CFGEdgeDeque::const_iterator eiter;
    const LoopTree& loopInfo = *(flowGraph.getIRManager()->getLoopTree());
    CFGNode  *node;

    if (_profileType == NoProfile) {    // no edge profile info
        if (warning_on)
            ::std::cerr << "WARNING: no profile for [" << _methodStr << "]" << ::std::endl;
        return false;
    } else if (_profileType == InstrumentedEdge) {
        if (_numEdges == 0) {
            if (warning_on)
                ::std::cerr << "WARNING: no profile for [" << _methodStr
                    << "] : profile has ZERO edge count" << ::std::endl;
            return false;
        }

        if (flowGraph.getMaxEdgeId() != _numEdges) {
            // the CFG is not consistent with what is assumed in the profile source
            if (warning_on)
                ::std::cerr << "WARNING: CFG mismatch in [" << _methodStr
                    << "] : profile edge count = " << (unsigned int) _numEdges
                    << ", use CFG edge count = " << (unsigned int) flowGraph.getMaxEdgeId()
                    << ::std::endl;
            return false;
        }
    }


    if (_hasCheckSum) {
        // Compute and verify CFG check sum.
        uint32  sum = 0;
        for(niter = po.rbegin(); niter != po.rend(); ++niter) {
            node = *niter;
            const CFGEdgeDeque& oEdges = (node)->getOutEdges();
            for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                sum += getEdgeCheckSum(*eiter);
            }

            // Assume that a loop header node has two predecessors.
            if ( loopInfo.isLoopHeader(node) && node->getInDegree() != 2) {
                ::std::cerr << "WARNING: [" << _methodStr << "] Loop header (N"
                    << (unsigned int)  node->getId() << ") has "
                    << (unsigned int) node->getInDegree() << "predecessors!"
                    << ::std::endl;
                return false;
            }
        }
        if (sum != _checkSum) {
            // the CFG is not consistent with that in the profile
            if (warning_on)
                ::std::cerr << "WARNING: CFG checksum mismatch [" << _methodStr << ::std::endl;
            return false;   //  skip profile annotation
        }
    }
    return true;
}   // EdgeProfile::isProfileValidate


// Set up the edge taken probability out of the source node for each outgoing
//  edges from instrumentation-based edge profile.
// It is assumed that a simple propagation phase to propagate the block
//  execution frequency have been performed and the CFG is already annotated
//  with the propagated profile.
// This function is expected to be invoked to sommth the profile when the
//  profile is annotated to the CFG exhinbits inconsistency.
void
EdgeProfile::setupEdgeProbFromInstrumentation(
     FlowGraph&     cfg,
     StlVector<CFGNode*>& po)
{
    StlVector<CFGNode*>::reverse_iterator niter;
    CFGEdgeDeque::const_iterator eiter;
    CFGNode     *srcNode;
    CFGEdge     *edge, *exception_edge;
    uint32      i, id, outDegree, exceptionEdgeCount;
    double      freqTotal, prob, efreq=0.0;
    uint32      edgeCount = cfg.getMaxEdgeId();

    _edgeProb = new(_mm) double[edgeCount];
    for ( i = 0; i < edgeCount; i++)
        _edgeProb[i] = -1.0;

    for(niter = po.rbegin(); niter != po.rend(); ++niter) {
        srcNode = *niter;
        const CFGEdgeDeque& oEdges = srcNode->getOutEdges();

        outDegree = (uint32) oEdges.size();
        if (outDegree == 0)
            continue;

        if (outDegree == 1) {
            edge = *(oEdges.begin());
            _edgeProb[edge->getId()] = 1.0;
            continue;
        }

        // outDegree >= 2

        // Compute the total frequency for all outgoing edges.
        exceptionEdgeCount = 0;
        exception_edge = NULL;
        freqTotal = 0.0;
        for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
            edge = *eiter;
            freqTotal += _edgeFreq[edge->getId()];
            if (edge->getEdgeType() == CFGEdge::Exception) {
                exceptionEdgeCount++;
                exception_edge = edge;
            }
        }
        assert(exceptionEdgeCount <= 1);

        // Now compute the edge taken probability for all outgoing edges
        //  from the srcNode.
        if (exceptionEdgeCount > 0) {
            if (srcNode->getFreq() > freqTotal) {
                efreq = srcNode->getFreq() - freqTotal;
                freqTotal = srcNode->getFreq();
            } else {
                efreq = 0.0;
            }
        }

        if (freqTotal > 0.0) {  // compute edge taken probability from profile
            for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                edge = *eiter;
                id = edge->getId();
                _edgeProb[id] = _edgeFreq[id] / freqTotal;
            }
            if (exception_edge != NULL) {
                _edgeProb[exception_edge->getId()] = efreq / freqTotal;
            }
        } else {    // estimate edge taken probability
            // Exception edges are set to probability of 0.0. Non-exception
            //  edges are assumed to be taken with equal probability.
            prob = 1.0 / ((double) (outDegree - exceptionEdgeCount));
            for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                edge = *eiter;
                id = edge->getId();
                _edgeProb[id] =
                    (edge->getEdgeType() == CFGEdge::Exception) ? 0.0 : prob;
            }
        }
    }
}   // EdgeProfile::setupEdgeProbFromInstrumentation


// Set up the edge taken probability array _edgeProb from the existing CFG
//  profile information.
void
EdgeProfile::setupEdgeProbFromCFG(
     FlowGraph&     cfg,
     StlVector<CFGNode*>& po)
{
    CFGEdgeDeque::const_iterator eiter;
    StlVector<CFGNode*>::reverse_iterator niter;
    CFGNode     *node;
    uint32      i, count, edgeCount;
    double      p, prob;

    // Set the entry node frequency.
    node = *po.rbegin();
    assert(node->getInEdges().size() == 0);
    _entryFreq = node->getFreq();

    // Set the edge probability array.
    edgeCount = cfg.getMaxEdgeId();
    _edgeProb = new(_mm) double[edgeCount];
    for ( i = 0; i < edgeCount; i++)
        _edgeProb[i] = -1.0;

    for(niter = po.rbegin(); niter != po.rend(); ++niter) {
        CFGNode *node = *niter;
        const CFGEdgeDeque& oEdges = node->getOutEdges();

        prob = 0.0;
        count = 0;      // count non-exception edges
        for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
            CFGEdge *edge = *eiter;
            p = edge->getEdgeProb();
            _edgeProb[edge->getId()] = p;
            prob += p;
            if ( edge->getEdgeType() != CFGEdge::Exception )
                count++;
        }

        // Fix the taken probability from node if it is corrupted.
        if (prob == 0.0) {
            // Evenly distribute the probability among non-exception edges.
            if (count != 0) {
                p = 1.0 / count;
                for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                    CFGEdge *edge = *eiter;
                    if ( edge->getEdgeType() != CFGEdge::Exception )
                        _edgeProb[edge->getId()] = p;
                }
            } else {
                p = 1.0 / ((double) oEdges.size());
                for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                    CFGEdge *edge = *eiter;
                    _edgeProb[edge->getId()] = p;
                }
            }
        } else if (prob != 1.0) {
            // Scale the taken probability so that the taken probability of all
            //  outgoing edges add up to 1,0.
            for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                CFGEdge *edge = *eiter;
                i = edge->getId();
                _edgeProb[i] = _edgeProb[i] / prob;
            }
        }
    }
}   // EdgeProfile::setupEdgeProbFromCFG



// Setup Block Info array.
// Also construct the LoopProfile structure if a node belongs to a loop.
//  The LoopProfile structure contains several loop-related information
//  such as loop entry, exits, members, probability of each exit, and
//  the estimated loop iteration count. The exit probability and the
//  estimated loop iteration count will be filled in later by
//  computeLoopIterCountExitProb().
void
EdgeProfile::setupBlockInfo(FlowGraph& cfg, StlVector<CFGNode*>& po)
{
    LoopTree    *loopInfo = cfg.getIRManager()->getLoopTree();
    StlVector<CFGNode*>::reverse_iterator niter;
    CFGNode     *node, *header;
    LoopProfile *loopPf;
    LoopNode    *loopNode;
    uint32      nodeCount = cfg.getMaxNodeId();
    uint32      i, id;

    _blockInfo = new(_mm) BlockProfile[nodeCount];
    for ( i = 0; i < nodeCount; i++) {
        _blockInfo[i].count = -1.0;
        _blockInfo[i].block = NULL;
        _blockInfo[i].loop = NULL;
    }

    for(niter = po.rbegin(); niter != po.rend(); ++niter) {
        node = *niter;
        id = node->getId();
        _blockInfo[id].block = node;
        if ( loopInfo->isLoopHeader(node) ) {
            loopNode = loopInfo->getLoopNode(node);
            assert(loopNode && loopNode->getHeader() == node);
            _blockInfo[id].loop = new(_mm) LoopProfile(_mm, loopNode);
        } else if (loopInfo->hasContainingLoopHeader(node)) {
            header = loopInfo->getContainingLoopHeader(node);
            assert(header);
            loopPf = _blockInfo[header->getId()].loop;
            if (loopPf) {
                loopPf->addMember(node);
                _blockInfo[id].loop = loopPf;
            } else {
                // The header node of a loop should be traversed
                // before its other members.
                fprintf(stderr, "No loop header found!!\n");
                assert(0);
            }
        }
    }
}   // EdgeProfile::setupBlockInfo
// Compute the estimated loop iteration count and the probability of each
//  loop exit (normalized so that they sum is equal to 1.0) and record
//  them in the LoopProfile structure of the respective loop.
void
EdgeProfile::computeLoopIterCountExitProb(
    LoopProfile *loop_profile,
    double      *edge_prob
    )
{
    double  node_prob;  // probability of reaching a node from the loop entry
    double  eprob;
    uint32  node_id, i, j, n, eid;
    CFGNode *node, *dstNode;
    CFGEdgeDeque::const_iterator eiter;
    CFGEdge* edge;
    LoopProfile *loop_pfl;
    bool    dbg_dump = false;   // profileCtrl.matchSpecifiedMethod(_methodStr);



    // Visit each node in the loop in topological order and compute the
    //  reaching probability from the loop entry node for each node and
    //  its outgoing edges.
    // Note: the members are arranged in topological order.
    for (i = 0; i < loop_profile->_members.size(); i++) {
        node_id = loop_profile->_members[i];
        node = _blockInfo[node_id].block;

        // Compute the reaching probability to the node from loop entry.
        if (i == 0) {    // this is the loop entry node
            node_prob = 1.0;    // entry node freq starts with 1.0
        } else {
            // Non-entry node has incoming edges from inside the loop only.
            // The node probability is the sum of all its incoming edges.
            const CFGEdgeDeque& iEdges = node->getInEdges();
            node_prob = 0.0;
            for(eiter = iEdges.begin(); eiter != iEdges.end(); ++eiter) {
                edge = *eiter;
                node_prob += edge_prob[edge->getId()];
            }
        }

        // Compute the probability of each outgoing edge of this node.
        const CFGEdgeDeque& oEdges = node->getOutEdges();
        for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
            edge = *eiter;
            n = edge->getId();
            edge_prob[n] = node_prob * _edgeProb[n];

            if (dbg_dump) {
                fprintf(stderr, "\t++++ %u -> %u [%g]\n", edge->getSourceNode()->getId(),
                   edge->getTargetNode()->getId(), edge_prob[n]);
            }

            // If the dstNode of an outgoing edge is the header of a loop L
            //  nested in the current loop, propagate probability to all exits
            //  of loop L.
            eprob = edge_prob[n];
            dstNode = edge->getTargetNode();
            if (loop_profile->_loop->inLoop(dstNode)) {
                loop_pfl = _blockInfo[dstNode->getId()].loop;
                if (loop_pfl  != loop_profile) {
                    for (j = 0; j < loop_pfl->_exits.size(); j++) {
                        eid = loop_pfl->_exits[j]->getId();
                        edge_prob[eid] = loop_pfl->_exit_prob[j] * eprob;

                        if (dbg_dump) {
                            fprintf(stderr, "\t++++** %u -> %u [%g]\n",
                                    loop_pfl->_exits[j]->getSourceNode()->getId(),
                                    loop_pfl->_exits[j]->getTargetNode()->getId(),
                                    edge_prob[eid]);
                        }
                    }
                }
            }
        }
    }

    // Compute the estimated loop iteration count as
    //      1.0/(1.0 - back_edge_prob).
    assert(loop_profile->_back_edge);
    n = loop_profile->_back_edge->getId();

    if (dbg_dump) {
        fprintf(stderr, "\t++++ back edge [%g]\n", edge_prob[n]);
    }

    eprob = edge_prob[n];       // back edge probability
    if (eprob == 1.0) {         // an infinite loop
        // give a big number instead of infinite so that the profile
        // inside the loop can still be derived and differentiated.
        loop_profile->_iter_count = DEFAULT_ITERATION_COUNT;
    } else {
        loop_profile->_iter_count = 1.0 / (1.0 - eprob);
    }

    // Setup the probability array for loop exit edges. The sum of all
    //  exits are normalized to probability of 1.0.
    eprob = 1.0 - eprob;        // sum of all exit edges probability
    loop_profile->allocExitProbArray();
    for (i = 0; i < loop_profile->_exits.size(); i++) {
        n = loop_profile->_exits[i]->getId();
        if (eprob <= 0.0)
            loop_profile->_exit_prob[i] = 0.0;
        else
            loop_profile->_exit_prob[i] = edge_prob[n] / eprob;
    }
}   // EdgeProfile::computeLoopIterCountExitProb


// Loop profile is computed and setup in depth-first order. That is,
//  from inner-most out.
void
EdgeProfile::setupLoopProfile(
    LoopNode    *loopNode,
    double      *work
    )
{
    CFGNode     *node;
    uint32      n, id;
    LoopProfile *loopProfile;

    for ( n = 1; loopNode; loopNode = loopNode->getSiblings(), n++) {
        // process inner loops first
        setupLoopProfile(loopNode->getChild(), work);

        // process this loop.
        node = loopNode->getHeader();
        if (node == NULL)
            return;

        id = node->getId();
        loopProfile = _blockInfo[id].loop;
        assert(loopProfile != NULL && loopProfile->_loop == loopNode);
        if (loopProfile) {
            computeLoopIterCountExitProb(loopProfile, work);
        }
    }
}   // EdgeProfile::setupLoopProfile


// Derive profile from the edge taken probability and the method entry
//  frequency.
void
EdgeProfile::deriveProfile(FlowGraph& cfg, StlVector<CFGNode*>& po)
{
    LoopTree    *loopInfo = cfg.getIRManager()->getLoopTree();
    CFGNode *node, *srcNode;
    CFGEdge *edge;
    CFGEdgeDeque::const_iterator eiter;
    StlVector<CFGNode*>::reverse_iterator niter;
    uint32  nid, snid, eid;
    double  freq;
    bool    dbg_dump = false;

    if (dbg_dump) fprintf(stderr, "deriveProfile> %s\n", _methodStr);
    node = *po.rbegin();
    _blockInfo[node->getId()].count = _entryFreq;
    for(niter = po.rbegin(); niter != po.rend(); ++niter) {
        node = *niter;
        nid = node->getId();

        const CFGEdgeDeque& iEdges = node->getInEdges();
        if (iEdges.size() == 0) {
            if (_blockInfo[nid].count < 0.0)
                fprintf(stderr, "!?!?!? [%s] unreachable node N%u\n", _methodStr, nid);
            continue;
        }

        // Current node freq is the sum of freq from all incoming edges.
        // If the current node is a loop header (or entry), its freq is
        //  the freq from the loop pre-header multiplied by the estimated
        //  loop iteration count.
        if (dbg_dump) fprintf(stderr, "deriveProfile> N%u = (", nid);

        freq = 0.0;
        if (loopInfo->isLoopHeader(node)) {
            for(eiter = iEdges.begin(); eiter != iEdges.end(); ++eiter) {
                edge = *eiter;
                srcNode = edge->getSourceNode();
                snid = srcNode->getId();
                if (_blockInfo[nid].loop != _blockInfo[snid].loop) {
                    // count the freq from loop pre-header only
                    eid = edge->getId();

                    if (_blockInfo[snid].count > 0.0)
                        freq += _blockInfo[snid].count * _edgeProb[eid];

                    if (dbg_dump) {
                        fprintf(stderr, "N%u->N%u[%g*%g] ",
                                snid, edge->getTargetNode()->getId(),
                                _blockInfo[snid].count, _edgeProb[eid]);
                    }
                }
            }
            freq = freq * _blockInfo[nid].loop->_iter_count;
            if (dbg_dump) fprintf(stderr, ") * %g", _blockInfo[nid].loop->_iter_count);
        } else {
            for(eiter = iEdges.begin(); eiter != iEdges.end(); ++eiter) {
                edge = *eiter;
                srcNode = edge->getSourceNode();
                snid = srcNode->getId();
                eid = edge->getId();

                if (_blockInfo[snid].count > 0.0)
                    freq += _blockInfo[snid].count * _edgeProb[eid];

                if (dbg_dump) {
                    fprintf(stderr, "N%u->N%u[%g*%g] ",
                            snid, edge->getTargetNode()->getId(),
                            _blockInfo[snid].count, _edgeProb[eid]);
                }
            }
            if (dbg_dump) fprintf(stderr, ")");
        }
        _blockInfo[nid].count = freq;
        if (dbg_dump) {
            fprintf(stderr, " = %g\n", freq);
            fflush(stderr);
        }
    }

    setProfile(cfg, po);
}   // EdgeProfile::deriveProfile


// Copy the profile derived from smoothing into the flowgraph.
void
EdgeProfile::setProfile(FlowGraph& cfg, StlVector<CFGNode*>& po)
{
    CFGNode *node;
    CFGEdge *edge;
    CFGEdgeDeque::const_iterator eiter;
    StlVector<CFGNode*>::reverse_iterator niter;
    uint32  nid, eid;
    double  v;

    for(niter = po.rbegin(); niter != po.rend(); ++niter) {
        node = *niter;
        nid = node->getId();
        v = node->getFreq();        // for debug
        node->setFreq(_blockInfo[nid].count);
        _blockInfo[nid].count = v;  // for debug

        const CFGEdgeDeque& oEdges = node->getOutEdges();
        for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
            edge = *eiter;
            eid = edge->getId();
            v = edge->getEdgeProb();    // for debug
            edge->setEdgeProb(_edgeProb[eid]);
            _edgeProb[eid] = v;         // for debug
        }
    }
}   // EdgeProfile::setProfile


// Validate the profile information.
bool
EdgeProfile::debugSmoothedProfile(FlowGraph& cfg, StlVector<CFGNode*>& po)
{
    // Check with the profile without smoothing.
    CFGNode *node;
    CFGEdge *edge;
    CFGEdgeDeque::const_iterator eiter;
    StlVector<CFGNode*>::reverse_iterator niter;
    uint32  nid, eid;
    double  diff, rel_diff;
    double  uerr = PROFILE_ERROR_ALLOWED;
    double  lerr = -PROFILE_ERROR_ALLOWED;
    bool    pass = true;


    for(niter = po.rbegin(); niter != po.rend(); ++niter) {
        node = *niter;
        nid = node->getId();

        diff = node->getFreq() - _blockInfo[nid].count;
        rel_diff = node->getFreq() == 0.0 ? diff : diff / node->getFreq();
        if ( rel_diff > uerr || rel_diff < lerr) {
            pass = false;
            ::std::cerr << "??? N" << (unsigned int) nid << " : freq " << node->getFreq()
                << " old_freq " << _blockInfo[nid].count << " diff " << diff
                << " rel_diff " << rel_diff << ::std::endl;
        }

        if (node->getFreq() == 0.0)
            continue;

        const CFGEdgeDeque& oEdges = node->getOutEdges();
        for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
            edge = *eiter;
            eid = edge->getId();
            diff = edge->getEdgeProb() - _edgeProb[eid];
            if ( diff > uerr || diff < lerr) {
                pass = false;
                ::std::cerr << "??? E" <<  (unsigned int) eid << "[" <<  (unsigned int) nid;
                if (node->isDispatchNode())
                    ::std::cerr << "D";
                ::std::cerr << " -> " << (unsigned int) edge->getTargetNode()->getId();
                if (edge->getTargetNode()->isDispatchNode())
                    ::std::cerr << "D";
                ::std::cerr << "] : prob " << edge->getEdgeProb() << " old_prob " << _edgeProb[eid]
                    << " diff " << diff << ::std::endl;
            }
        }
    }
    if (!pass)
        ::std::cerr << "??? [" << _methodStr << "] ========== profile validation FAILED" << ::std::endl;
    return pass;
}   // EdgeProfile::debugSmoothedProfile


// Return true if the profile is consistent. Otherwise, return false.
bool
EdgeProfile::printInconsistentProfile(FlowGraph& cfg)
{
    const CFGNodeDeque& nodes = cfg.getNodes();
    CFGEdgeDeque::const_iterator eiter;
    CFGNodeDeque::const_iterator niter;
    CFGNode     *node, *srcNode;
    CFGEdge     *edge;
    double      f, diff;
    bool        consistent = true;

    ::std::cerr << "printInconsistentProfile [" << _methodStr << "]" << ::std::endl;

    for( niter = nodes.begin(); niter != nodes.end(); ++niter) {
        node = *niter;

        // Check whether the sum of freq from incoming edges equal the node freq.
        const CFGEdgeDeque& iEdges = node->getInEdges();
        if (iEdges.size() > 0) {
            f = 0.0;
            for(eiter = iEdges.begin(); eiter != iEdges.end(); ++eiter) {
                edge = *eiter;
                srcNode = edge->getSourceNode();
                f += srcNode->getFreq() * edge->getEdgeProb();
            }

            diff = (f == 0.0 ?  node->getFreq() : (f - node->getFreq())/f);
            if (diff > PROFILE_ERROR_ALLOWED || diff < -PROFILE_ERROR_ALLOWED) {
                consistent = false;
                unsigned int nid = node->getId();
                ::std::cerr << "==> N" << nid << " : Incoming freq (" << f
                    << ") != Node freq (" << node->getFreq() << ")" << ::std::endl;
                ::std::cerr << "====> N" << nid << " : freq " << node->getFreq()
                    << "old_freq " << _blockInfo[nid].count << ::std::endl;
            }
        }


        // Check whether the sum of outgoing edge probability is 1.0.
        const CFGEdgeDeque& oEdges = node->getOutEdges();
        if (oEdges.size() > 0) {
            f = 0.0;
            for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                edge = *eiter;
                f += edge->getEdgeProb();
            }

            diff = (f > 1.0) ? (f - 1.0) : (1.0 - f);
            if (diff > PROB_ERROR_ALLOWED) {
                consistent = false;
                unsigned int nid = node->getId();
                ::std::cerr << "==> N" << nid << " -- Outgoing prob (" << f
                    << ") != 1.0" << ::std::endl;
                for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                    edge = *eiter;
                    ::std::cerr << "====> [E" << (unsigned int) edge->getId() << " N" << nid
                        << "->N" << (unsigned int) edge->getTargetNode()->getId()
                        << "] : " << "prob " << edge->getEdgeProb()
                        << " old_prob " << _edgeProb[edge->getId()] << ::std::endl;
                }
            }
        }
    }
    return consistent;
}   // EdgeProfile::printInconsistentProfile


void
EdgeProfile::generateInstrumentedProfile(
    FlowGraph&              flowGraph,
    StlVector<CFGNode*>&    poNodes,
    bool                    warning_on)
{
    LoopTree    *loopInfo = flowGraph.getIRManager()->getLoopTree();
    uint32  edgeCount = flowGraph.getMaxEdgeId();
#ifdef _DEBUG
    CFGNode* entryNode = flowGraph.getEntry();
#endif
    CFGNode* node;
    uint32  i, n;
    double   srcNodeFreq, f, g;
    CFGEdgeDeque::const_iterator eiter;
    CFGEdge* edge;
    StlVector<CFGNode*>::reverse_iterator niter;

    // Allocate and initialize the working copy of edge freq for profile
    //  processing.
    double   *edgeFreq = new(_mm) double[edgeCount];
    for (i = 0; i < edgeCount; i++) {
        edgeFreq[i] = _edgeFreq[i];
    }

    // Set entry node frequency.
    node = *poNodes.rbegin();
    assert(node == entryNode);
    node->setFreq(_entryFreq);

    bool done = false;
    while(!done) {
        done = true;
        for(niter = poNodes.rbegin(); niter != poNodes.rend(); ++niter) {
            CFGNode* dstNode;
            CFGNode* srcNode = *niter;

            // Compute the node execution freq.
            const CFGEdgeDeque& iEdges = srcNode->getInEdges();
            if (iEdges.size() > 0) {
                srcNodeFreq = 0.0;
                for(eiter = iEdges.begin(); eiter != iEdges.end(); ++eiter) {
                    edge = *eiter;
                    srcNodeFreq += edgeFreq[edge->getId()];
                }
                if(srcNodeFreq > srcNode->getFreq()) {
                    // Update with new edge information
                    done = false;
                    srcNode->setFreq(srcNodeFreq);
                } else {
                    // Already computed for this node.
                    assert(srcNodeFreq == srcNode->getFreq());
                }
            } else
                srcNodeFreq = srcNode->getFreq();

            // Compute the edge taken probability for each outgoing edge.
            const CFGEdgeDeque& oEdges = srcNode->getOutEdges();
            if (oEdges.size() == 0)
                continue;

            if (oEdges.size() == 1) {
                edge = *(oEdges.begin());
                edgeFreq[edge->getId()] = srcNode->getFreq();
                edge->setEdgeProb(1.0);
                continue;
            }

            // We have more than one outgoing edges from srcNode.
            if (srcNodeFreq == 0.0) {
                g = 1.0 / ((double) oEdges.size());
                for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                    edge = *eiter;
                    edge->setEdgeProb( g );
                }
                continue;
            }

            f = srcNodeFreq;
            n = 0;
            for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                edge = *eiter;
                dstNode = edge->getTargetNode();
                if (dstNode->isBlockNode()) {
                    g = edgeFreq[edge->getId()];
                    edge->setEdgeProb( g/srcNodeFreq );
                    f -= g;
                } else {
                    n++;    // count edges that are not instrumented
                    edge->setEdgeProb(0.0);     // reset edge prob to 0.0
                }
            }

            if (f != 0.0) {
                if ( n > 0 && f > 0.0 ) {
                    // Here is the freq for the edges that are not instrumented.
                    // Revisit the outgoing edges and fix up their probability.
                    g = f / ((double) n);    // the freq for non-instrumented edges
                    for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                        edge = *eiter;
                        dstNode = edge->getTargetNode();
                        if ( !dstNode->isBlockNode() ) {
                            edgeFreq[edge->getId()] = g;
                            edge->setEdgeProb( g/srcNodeFreq );
                        }
                    }
                }
            }
        }
    }

    bool profile_inconsistent = !flowGraph.isProfileConsistent(_methodStr, false);
    if (profile_inconsistent && profileCtrl.getDebugCtrl() > 0) {
        // Derive edge profile from edge probability using profile smoothing.
        setupEdgeProbFromInstrumentation(flowGraph, poNodes);
        setupBlockInfo(flowGraph, poNodes);
        setupLoopProfile((LoopNode *) loopInfo->getRoot(), edgeFreq);
        deriveProfile(flowGraph, poNodes);

        if (profileCtrl.getDebugCtrl() != 0
            && flowGraph.isProfileConsistent(_methodStr, warning_on) == false) {
            ::std::cerr << "!!!!!!! Profile INCONSISTENT after smoothing (generateInstrumentedProfile)" << ::std::endl;
            printCFGDot(flowGraph, "smoothing error", "sma_err");
            debugSmoothedProfile(flowGraph, poNodes);
        }
    }
}   // EdgeProfile::generateInstrumentedProfile


void
EdgeProfile::annotateCFGWithProfile(FlowGraph& flowGraph, bool online)
{
    bool    warning_on = ((profileCtrl.getDebugCtrl() != 0) || !online);
    StlVector<CFGNode*> poNodes(_mm);
    flowGraph.getNodesPostOrder(poNodes);

    if ( !isProfileValidate(flowGraph, poNodes, warning_on) ) {
        if (warning_on)
            ::std::cerr << "WARNING! annotateCFGWithProfile: isProfileValidate returns FALSE on ["
                << _methodStr << "]" << ::std::endl;
        return;
    }

    if (_profileType == InstrumentedEdge) {
        generateInstrumentedProfile(flowGraph, poNodes, warning_on);
    } else {
        assert(0);
        return;
    }
    flowGraph.setEdgeProfile(true);
}   // EdgeProfile::annotateCFGWithProfile



// Smooth the profile in the flowGraph by deriving the profile from the edge
//  taken probability and the method entry frequency that already exist in
//  the flowGraph.
void
EdgeProfile::smoothProfile(FlowGraph& flowGraph)
{
    bool        warning_on = ((profileCtrl.getDebugCtrl() != 0));
    LoopTree    *loopInfo = flowGraph.getIRManager()->getLoopTree();
    uint32      edgeCount = flowGraph.getMaxEdgeId();
    double      *edgeFreq = new(_mm) double[edgeCount];

    StlVector<CFGNode*> poNodes(_mm);
    flowGraph.getNodesPostOrder(poNodes);

    setupEdgeProbFromCFG(flowGraph, poNodes);
    setupBlockInfo(flowGraph, poNodes);
    setupLoopProfile((LoopNode *) loopInfo->getRoot(), edgeFreq);
    deriveProfile(flowGraph, poNodes);

    if (profileCtrl.getDebugCtrl() != 0
        && flowGraph.isProfileConsistent(_methodStr, warning_on) == false) {
        ::std::cerr << "!!!!!!! Profile INCONSISTENT after smoothing (smoothProfile)" << ::std::endl;
        printCFGDot(flowGraph, "smoothing profile error", "sm_prof_err");
        printInconsistentProfile(flowGraph);
    }
}   // EdgeProfile::smoothProfile


void
EdgeProfile::printCFG(FILE *file, FlowGraph& cfg, char *str)
{
    CFGEdgeDeque::const_iterator eiter;
    CFGNodeDeque::const_iterator niter;
    CFGNode     *dstNode, *srcNode;
    CFGEdge     *edge;

    fprintf(file, "START CFG dump %s =========== %s\n", _methodStr, str);

    const CFGNodeDeque& nodes = cfg.getNodes();
    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        srcNode = *niter;
        fprintf(file, "N%d (Freq: %f)\n", (int)srcNode->getId(), srcNode->getFreq());

        const CFGEdgeDeque& oEdges = srcNode->getOutEdges();
        if (oEdges.size() > 0) {
            for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                edge = *eiter;
                dstNode = edge->getTargetNode();
                fprintf(file, "To: N%d ( E%d %f [%f] )\n", (int)dstNode->getId(),
                        (int)edge->getId(), edge->getEdgeProb(),
                        (_edgeFreq != NULL ? _edgeFreq[edge->getId()] : -1.0));
            }
        }
    }
    fprintf(file, "END  CFG dump %s ===========\n", _methodStr);
}   // EdgeProfile::printCFG


void
EdgeProfile::printCFGDot(FlowGraph& cfg, char *str, char *suffix, bool node_label)
{
    CFGEdgeDeque::const_iterator eiter;
    CFGNodeDeque::const_iterator niter;
    CFGNode     *dstNode, *srcNode;
    CFGEdge     *edge;
    char        name[1024];
    uint32      i, j;
    FILE        *file;

    strcpy(name, "dotfiles/");
    for ( i = (uint32) strlen(name), j = 0;
          _methodStr[j] != '(' && _methodStr[j] != '\0';
          i++, j++) {
        if (_methodStr[j] == '/' || _methodStr[j] == '<' || _methodStr[j] == '>')
            name[i] = '_';
        else if (_methodStr[j] == ':')
            name[i] = '.';
        else
            name[i] = _methodStr[j];
    }
    name[i++] = '.';
    strcpy(&name[i], suffix);
    i = (uint32) strlen(name);
    strcpy(&name[i], ".dot");

    file = fopen(name, "w");
    if (file == NULL) {
        return;
    }

    fprintf(file, "digraph dotgraph {\n");
    fprintf(file, "center=TRUE;\n");
    fprintf(file, "ranksep=\".25\";\n");
    fprintf(file, "nodesep=\".20\";\n");
    fprintf(file, "fontpath=\"c:\\winnt\\fonts\";\n");
    fprintf(file, "ratio=auto;\n");
    fprintf(file, "page=\"8.5,11\";\n");
    fprintf(file, "node [shape=record,fontname=\"Courier\",fontsize=11];\n");
    fprintf(file, "label=\"[%s] %s\"\n", _methodStr, str);

    const CFGNodeDeque& nodes = cfg.getNodes();


    for(niter = nodes.begin(); niter != nodes.end(); ++niter) {
        srcNode = *niter;
        const CFGEdgeDeque& oEdges = srcNode->getOutEdges();

        if (node_label) {
            // print node
            if (srcNode->isDispatchNode()) {
                fprintf(file, "N%u [shape=diamond,color=blue,label=\"N%u (%g)",
                        srcNode->getId(), srcNode->getId(), srcNode->getFreq());
            } else {
                fprintf(file, "N%u [label=\"N%u (%g)",
                        srcNode->getId(), srcNode->getId(), srcNode->getFreq());
            }

            // add node label
            for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
                edge = *eiter;
                dstNode = edge->getTargetNode();
                fprintf(file, "\\nTo: N%u (%g) (src: %g)", dstNode->getId(),
                        edge->getEdgeProb(),
                        (_edgeFreq != NULL ? _edgeFreq[edge->getId()] : -1.0));
            }
            fprintf(file, "\"];\n");
        } else {
            if (srcNode->isDispatchNode()) {
                fprintf(file, "N%u [shape=diamond,color=blue];\n", srcNode->getId());
            }
        }

        // print graph edges
        for(eiter = oEdges.begin(); eiter != oEdges.end(); ++eiter) {
            edge = *eiter;
            dstNode = edge->getTargetNode();
            fprintf(file, "N%d -> N%d", (int)srcNode->getId(), (int)dstNode->getId());
            if (dstNode->isDispatchNode())
                fprintf(file, " [style=dotted,color=blue]");
            fprintf(file, ";\n");
        }
    }
    fprintf(file, "}\n");
    fclose(file);
}   // EdgeProfile::printCFGDot


// Parse the PROF command line parameters.
void
ProfileControl::init(const char *str)
{
    char        *p, *lhs, *rhs;
    unsigned    lhs_len, rhs_len;

    // default configurations
    // * bit 3: add instrumentation on exception branches
    config = 0x8;

    // Parse the command line parameters for PROF.

    if (str == NULL)
        return;

    p = (char *) (*str == '"' ? str + 1 : str);
    while ( *p != '\0' && *p != '"' ) {
        lhs = p;
        rhs = NULL;
        lhs_len = rhs_len = 0;
        while (*p != '=' && *p != ',' && *p != '\0' && *p != '"') {
            p++;
            lhs_len++;
        }
        if (*p == '=') {
            p++;
            rhs = p;
            while (*p != ',' && *p != '\0' && *p != '"') {
                p++;
                rhs_len++;
            }
        }
        if (*p == ',')
            p++;

        setProfileConfig(lhs, lhs_len, rhs, rhs_len);
    }
    if (debugCtrl)
        fprintf(stderr, "iGen %u  profUse %u config %lu debug %lu File %s\n",
            instrumentGen, profUse, config, debugCtrl, profileFile);
}   // ProfileControl::init


// Set profile configuration for one of the following options.
// 'file' -- generate into or use from the specified file
// 'config' -- configuration value
// 'debug' -- debug flag
// 'method' -- method name that specifies the only method to be instrumented
void
ProfileControl::setProfileConfig(
    char        *lhs,
    unsigned    lhs_len,
    char        *rhs,
    unsigned    rhs_len)
{
    char            c, lhs_str[10], rhs_str[128];
    unsigned        i, len;
    unsigned long   n;

    if (lhs_len == 0 || rhs_len == 0)
        return;

    len = (lhs_len > 6) ? 9 : lhs_len;
    for (i = 0; i < len; i++)
        lhs_str[i] = lhs[i];
    lhs_str[i] = '\0';

    if (lhs_len > 6) {
        fprintf(stderr, "Unsupported option '%s' skipped!\n", lhs_str);
        return;
    }

    switch (lhs_str[0]) {
    case 'g':   // gen
        if (strcmp(lhs_str, "gen") != 0) {
            fprintf(stderr, "Unsupported option '%s' skipped!\n", lhs_str);
            return;
        }

        for (i = 0; i < rhs_len; i++) {
            c = rhs[i];
            switch (c) {
            case 'm':
                instrumentGen |= 0x1;
                break;

            case 'e':
                instrumentGen |= 0x2;
                break;


            default:
                fprintf(stderr, "Unsupported 'gen' option '%c' skipped!\n", c);
            }
        }
        break;

    case 'u':   // use
        if (strcmp(lhs_str, "use") != 0) {
            fprintf(stderr, "Unsupported option '%s' skipped!\n", lhs_str);
            return;
        }

        for (i = 0; i < rhs_len; i++) {
            c = rhs[i];
            switch (c) {
            case 'm':
                profUse |= 0x1;
                break;

            case 'e':
                profUse |= 0x2;
                break;

            case 'p':
                profUse |= 0x4;
                break;

            default:
                fprintf(stderr, "Unsupported 'use' option '%c' skipped!\n", c);
            }
        }
        break;

    case 'f':   // file
        if (strcmp(lhs_str, "file") != 0) {
            fprintf(stderr, "Unsupported option '%s' skipped!\n", lhs_str);
            return;
        }
        profileFile = (char *) malloc(rhs_len+1);
        for (i = 0; i < rhs_len; i++)
            profileFile[i] = rhs[i];
        profileFile[i] = '\0';
        break;

    case 'c':   // config
        if (strcmp(lhs_str, "config") != 0) {
            fprintf(stderr, "Unsupported option '%s' skipped!\n", lhs_str);
            return;
        }

        len = (rhs_len > 127) ? 127 : rhs_len;
        for (i = 0; i < len; i++)
            rhs_str[i] = rhs[i];
        rhs_str[i] = '\0';
        if ( rhs_len > 127 ) {
            fprintf(stderr, "Unsupported 'config' option '%s' skipped!\n",
                rhs_str);
            return;
        }

        n = strtoul(rhs_str, NULL, 0);
        if (n == ULONG_MAX) {
            fprintf(stderr, "Unsupported 'config' option '%s' skipped!\n",
                rhs_str);
            return;
        }
        config = n;
        break;

    case 'd':   // debug control
        if (strcmp(lhs_str, "debug") != 0) {
            fprintf(stderr, "Unsupported option '%s' skipped!\n", lhs_str);
            return;
        }

        len = (rhs_len > 127) ? 127 : rhs_len;
        for (i = 0; i < len; i++)
            rhs_str[i] = rhs[i];
        rhs_str[i] = '\0';
        if ( rhs_len > 127 ) {
            fprintf(stderr, "Unsupported 'debug' option '%s' skipped!\n",
                rhs_str);
            return;
        }

        n = strtoul(rhs_str, NULL, 0);
        if (n == ULONG_MAX) {
            fprintf(stderr, "Unsupported 'debug' option '%s' skipped!\n",
                rhs_str);
            return;
        }
        debugCtrl = n;
        break;

    case 'm':   // method
        if (strcmp(lhs_str, "method") != 0) {
            fprintf(stderr, "Unsupported option '%s' skipped!\n", lhs_str);
            return;
        }
        methodStr = (char *) malloc(rhs_len+1);
        for (i = 0; i < rhs_len; i++)
            methodStr[i] = rhs[i];
        methodStr[i] = '\0';
        break;

    default:
        fprintf(stderr, "Unsupported option '%s' skipped!\n", lhs_str);
        break;
    }
}   // ProfileControl::setProfileConfig


void
ProfileControl::deInit()
{
}    
 


EdgeProfile*
ProfileControl::readProfileFromFile(MethodDesc& mdesc) {
    char        methodStr[MaxMethodStringLength];
    EdgeProfile *epf, *lastEpf;

    if ( useProfileFile() == false )
        return NULL;

    if (genMethodString(mdesc, methodStr, MaxMethodStringLength) == 0)
        return NULL;    // won't handle method whose name exceeds length limit

    if (profileCtrl.debugCFGCheckSum())
        fprintf(stderr, "<checkSum> %s\n", methodStr);

    if ( edgeProfiles == NULL ) {
        // This is the first time we want to look up the profile from the
        //  profile file.
        // Read in profile from a file and cache them in edgeProfiles
        //  (a list of EdgeProfile structures).
        int     res;
        char    mStr[MaxMethodStringLength];

        FILE *profFile = fopen(profileFile, "r");
        if ( profFile == NULL ) {
            fprintf(stderr, "Cannot open profile file: %s!!\n",
                profileFile);
            return NULL;
        }

        res = fscanf(profFile, "%u", &counterSize);
        if (res == EOF || res == 0)
            return NULL;
        assert(counterSize == 4 || counterSize == 8);

        MemoryManager&  gmm = Jitrino::getGlobalMM();
        lastEpf = NULL;
        while ( fscanf(profFile, "%s", mStr) != EOF ) {
            epf = new(gmm) EdgeProfile(gmm, profFile, mStr, counterSize);
            if (epf->getEntryFreq() <= 0.0) {
                // no profile info for this method
            } else if (edgeProfiles == NULL) {
                edgeProfiles = lastEpf = epf;
            } else {
                lastEpf->setNext(epf);
                lastEpf = epf;
            }
        }
        fclose(profFile);
    }

    // Now look up the edge profile list for the given method.
    epf = NULL;
    if (lastEPPtr != NULL) {
        epf = lastEPPtr->next();
    }

    if (epf && strcmp(methodStr, epf->getMethodStr()) == 0)
        return epf;

    for (epf = edgeProfiles; epf; epf = epf->next())
        if (strcmp(methodStr, epf->getMethodStr()) == 0)
            return epf;
    return NULL;
}   // ProfileControl::readProfileFromFile




// Write the current profile data collected for each method into the file
//  specified in the profileControl.
void
CodeProfiler::writeProfile(
    FILE *file,
    const char *className,
    const char *methodName,
    const char *methodSig)
{
    fprintf(file, "%s::%s%s\n", className, methodName, methodSig);
    fprintf(file, "%u\n", checkSum);
    fprintf(file, "%u\n", numCounter);
    if (counterSize == 4) {
        uint32 *counters4B = (uint32 *) counterArrayBase;
        for (uint32 i = 0; i < numCounter; i++)
            fprintf(file, "%u\n", counters4B[i]);
    } else if (counterSize == 8) {
        uint64 *counters8B = (uint64 *) counterArrayBase;
        for (uint32 i = 0; i < numCounter; i++)
            fprintf(file, "%"SPRINTF_I64"u\n", counters8B[i]);
    } else
        assert(0);
    fflush(file);
}   // CodeProfiler::writeProfile



void
CodeProfiler::getOfflineProfile(IRManager& irm)
{
    unsigned    useProfFlags = profileCtrl.useProf();

    if ( useProfFlags & 0x2 ) {     // use edge profile
        MethodDesc& mdesc = irm.getMethodDesc();
        EdgeProfile* edgeProfile = profileCtrl.readProfileFromFile(mdesc);
        if ( edgeProfile )
            edgeProfile->annotateCFGWithProfile(irm.getFlowGraph(), false);
    }
}   // CodeProfiler::getOfflineProfile



void
CodeProfiler::getOnlineProfile(IRManager& irm)
{
    FlowGraph& flowGraph = irm.getFlowGraph();

    // The memory manager for buffers in the EdgeProfile.
    MemoryManager   mm(flowGraph.getMaxEdgeId() * counterSize * 16,
                        "CodeProfiler::getOnlineProfile");

    // Create the edge profile structure for the compiled method in 'irm'.
    EdgeProfile     edgeProfile(mm, irm.getMethodDesc(), false);

    // Annotate the CFG with edge profile.
    edgeProfile.annotateCFGWithProfile(flowGraph, true);
}   // CodeProfiler::getOnlineProfile
}
