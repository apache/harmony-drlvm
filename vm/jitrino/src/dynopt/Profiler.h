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
 * @version $Revision: 1.15.24.5 $
 *
 */


#include <stdlib.h>
#include "irmanager.h"
#include "VMInterface.h"
#include "FlowGraph.h"
#include "Loop.h"


#define     MaxMethodStringLength   2048
#define     PROFILE_ERROR_ALLOWED   0.001
#define     PROB_ERROR_ALLOWED      0.00001

namespace Jitrino
	{
	
//
// Utility functions
//
extern uint32   genMethodString(MethodDesc& mdesc, char *buf, uint32 n);

//
//  EdgeProfile -- structure for holding edge profile information for a method.
//      
// 

class LoopProfile {
public:
    LoopProfile(MemoryManager& mm, LoopNode *loop);

    void addMember(CFGNode *node);
    void allocExitProbArray() { _exit_prob = new(_mm) double[_exits.size()]; }
    void dump(FILE *file);
    
    MemoryManager&      _mm;
    StlVector<uint32>   _members;   // members of the loop (block id) in PostOrder
    StlVector<CFGEdge*> _exits;     // loop exit edges 
    LoopNode            *_loop;     // pointer to the loop node
    CFGNode             *_entry;    // the loop entry (or header) block
    CFGEdge             *_back_edge;    // loop back edge
    double              *_exit_prob;    // loop exit edge probability
    double              _iter_count;    // estimated loop iteration count
};

struct BlockProfile {
    double      count;
    CFGNode     *block;      
    LoopProfile *loop;      // pointer to LoopProfile if this is a loop header
};  // class LoopProfile

class EdgeProfile {
public:
    EdgeProfile(MemoryManager& mm, FILE *file, char *str, uint32 counterSize);
    EdgeProfile(MemoryManager& mm, MethodDesc& mdesc, bool smoothing = false);
    ~EdgeProfile() {
    }

    const char*   getMethodStr() { return _methodStr; }
    uint32  getCheckSum() { return _checkSum; }
    uint32  getNumEdges() { return _numEdges; }
    double  getEntryFreq() { return _entryFreq; }
    double  getEdgeFreq(uint32 i) { return _edgeFreq[i]; }
    EdgeProfile*    next() { return _next; }
    void    setNext(EdgeProfile* n) { _next = n; }

    void    annotateCFGWithProfile(FlowGraph& flowGraph, bool online);
    void    smoothProfile(FlowGraph& flowGraph);

private:
    enum    EdgeProfileType {
        NoProfile,
        InstrumentedEdge,
    
    };

    bool    isProfileValidate(FlowGraph& flowGraph, StlVector<CFGNode*>& po, 
                                bool warning_on);
    void    generateInstrumentedProfile(FlowGraph& flowGraph, StlVector<CFGNode*>& po, 
                                        bool warning_on);
    
    void    setupEdgeProbFromInstrumentation(FlowGraph& cfg, StlVector<CFGNode*>& po);
    
	
    void    setupEdgeProbFromCFG(FlowGraph& cfg, StlVector<CFGNode*>& po);
    

    void    setupBlockInfo(FlowGraph& cfg, StlVector<CFGNode*>& po);
    void    setupLoopProfile(LoopNode *loopNode, double *work);
    
    void    computeLoopIterCountExitProb(LoopProfile *loop_profile, double *edge_prob);   
    void    deriveProfile(FlowGraph& flowGraph, StlVector<CFGNode*>& po); 
    void    setProfile(FlowGraph& flowGraph, StlVector<CFGNode*>& po);

    bool    debugSmoothedProfile(FlowGraph& flowGraph, StlVector<CFGNode*>& po);
    bool    printInconsistentProfile(FlowGraph& flowGraph);
    void    printCFG(FILE *file, FlowGraph& cfg, char *str);
    void    printCFGDot(FlowGraph& cfg, char *str, char *suffix, bool node_label=true);

    MemoryManager&  _mm;
    MethodDesc*     _method;    // NULL if the EdgeProfile is built from a file
    EdgeProfileType _profileType;
    bool            _hasCheckSum;
    char*           _methodStr; 
    uint32          _checkSum;
    uint32          _numEdges;
    double          _entryFreq;
    double*         _edgeFreq;
    EdgeProfile*    _next;

    // work areas
    BlockProfile    *_blockInfo;
    double          *_edgeProb;
};  // class EdgeProfile


//
// Profile configuration and control.
//

class ProfileControl {
public:
    ProfileControl() : config(0), instrumentGen(0), profUse(0),
                        profileFile(NULL), methodStr(NULL), 
                        edgeProfiles(NULL), lastEPPtr(NULL)
    { 
        counterSize = 4;
    }

    ~ProfileControl() {

        if (profileFile) 
            free(profileFile);

    }

    void    init(const char *str);
    void    deInit();

    uint32  getCounterSize() { return counterSize; }
    unsigned getInstrumentGen() {  return instrumentGen; }
    unsigned getInstrumentGen(MethodDesc& mdesc) {
        if ( instrumentSpecifiedMethod() ) {
            char str[MaxMethodStringLength];
            const char* methodName = mdesc.getName();
            const char* className = mdesc.getParentType()->getName();
            sprintf(str, "%s::%s", className, methodName);
            if ( strcmp(methodStr, str) )
                return 0;
        }
        return instrumentGen; 
    }
    unsigned useProf() { return profUse; }

    bool    doMethodInstrument() { return ((instrumentGen & 1) != 0); }
    bool    doEdgeInstrument() { return ((instrumentGen & 2) != 0); }
    bool    useProfileFile() { return (profileFile != NULL); }
    char*   getProfileFileName() { return profileFile; }

    bool    printNameToFile() { return ((config & (1 << 1)) != 0); }
    bool    instrumentSpecifiedMethod() { return ((config & (1 << 2)) != 0); }
    bool    instrumentForDispatchFreq() { return ((config & (1 << 3)) != 0); }
    bool    debugCFGCheckSum() { return ((config & (1 << 4)) != 0); }
    bool    useOptimizedCG() { return ((config & (1 << 5)) != 0); }
    bool    matchSpecifiedMethod(char *name) { 
        return (methodStr != NULL && (strstr(name, methodStr) != NULL));
    }

    unsigned long   getDebugCtrl() { return debugCtrl; }

    EdgeProfile*    readProfileFromFile(MethodDesc& mdesc);

private:
    void    setProfileConfig(char *lhs, unsigned lhs_len, char *rhs,
                             unsigned rhs_len);

    //
    // config -- configuration flags
    //           bit 0: synchronized instrumentation
    //           bit 1: print instrumented method name to file
    //           bit 2: only instrument for method specified in 'method='
    //           bit 3: instrument to count dispatch edge frequency
    //
    //           bit 4: debug CFG checkSum
    //           bit 5: use CG optimize path for 1st compilation
    unsigned long   config;

    // debug -- debug control flags
    //           bit 0: dump debug info
    unsigned long   debugCtrl;
    
    // instrumentGen:   0 -- no edge instrumentation
    //                  1 -- instrument for method invocation count
    //                  2 -- instrument for edge profile
    //                  3 -- instrument for both method and edge profile
    unsigned    instrumentGen;

    

    // profUse: 0 -- no profile feedback, 
    //          1 -- use instrumentation-based method profile
    //          2 -- use instrumentation-based edge profile
    //          3 -- use instrumentation-based method and edge profile
    
    unsigned    profUse;

    // profileFile: NULL -- no profile file; non-NULL -- string pointer to the
    //              file name for the profile file.
    char        *profileFile;

    // method: only instrument the specified method if config[2] is set.
    char        *methodStr;

    // a linked list of edge profiles, one for each method
    EdgeProfile *edgeProfiles;

    //  Pointer to the last visited edge profile entry in edgeProfiles
    EdgeProfile *lastEPPtr;       

    // size of counter in byte
    uint32  counterSize;
};  // class ProfileControl

extern ProfileControl profileCtrl;


//
// Code Profiler
//

class CodeProfiler {
public:
    CodeProfiler() : numCounter(0), checkSum(0), counterArrayBase(NULL) {
        counterSize = profileCtrl.getCounterSize();
    }

    void    writeProfile(FILE *file, const char *className, const char *methodName, const char *methodSig);

    void    getOfflineProfile(IRManager& irm);
    void    getOnlineProfile(IRManager& irm);
    void    *getCounterBaseAddr() { return counterArrayBase; }
    void    *getCounterAddr(uint32 n) { 
        if (counterSize == 4)
            return (void *) &(((uint32 *) counterArrayBase)[n]);
        else
            return (void *) &(((uint64 *) counterArrayBase)[n]);
    }
    uint32  getCounterSize() { return counterSize; }

private:
    uint32  numCounter;
    uint32  counterSize;        // in Bytes
    uint32  checkSum;
    void    *counterArrayBase;  
};  // class CodeProfiler
}
