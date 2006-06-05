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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.12.24.4 $
 *
 */

#ifndef _OPT_PASS_H_
#define _OPT_PASS_H_

#include <string>

namespace Jitrino {

class IRManager;
class MemoryManager;
class Timer;

//
// Jitrino is always invoked in one of the following modes.
//
enum CompilationMode {
    CM_NO_OPT,      // Perform no optimizations.  Used for class initializers and other cold code. 
    CM_STATIC,      // Perform aggressive static optimizations.  No profile information to be collected or used.
    CM_DPGO1,       // Perform 1st compile in DPGO mode.  Compile conservatively and prepare for profile gathering. 
    CM_DPGO2,       // Perform 2nd compile in DPGO mode.  Compile aggressively using profile information.    
    CM_NUM_MODES
};

#define DEFINE_OPTPASS(classname) \
class classname : public OptPass { \
public: \
    classname(); \
    const char* getName(); \
    const char* getTagName(); \
    const char* getTimerName(); \
protected: \
    void _run(IRManager& irm); \
};

#define DEFINE_OPTPASS_IMPL(classname, tagname, fullname) \
    classname::classname() { initialize(); } \
    const char* classname::getName() { return fullname; } \
    const char* classname::getTagName() { return #tagname; } \
    const char* classname::getTimerName() { return "opt::all::"#tagname; }

class OptPass {
public:
    //
    // The name of this optimization pass.  E.g., "Partial Redundancy Elimination"
    //
    virtual const char* getName() = 0;

    //
    // The short name of this pass.  Should be a single word.  E.g., "pre". 
    //
    virtual const char* getTagName() = 0;

    //
    // Get the compile-time timers.
    // 
    virtual const char* getTimerName() = 0;
    Timer*& getTimer();
    const char* getModeTimerName(CompilationMode mode);
    Timer*& getModeTimer(CompilationMode mode);

    //
    // Run the pass.
    //
    void run(IRManager& mm);

    //
    // Return true if in DPGO mode.  In this mode, do aggressive optimizations only when profile information is available.
    //
    static bool inDPGOMode(IRManager& irm);

    //
    // Get the Jitrino compilation mode
    //
    static CompilationMode getCompilationMode(IRManager& irm);
    static const char* getCompilationModeName(CompilationMode mode);

    //
    // Services to compute dominators, loops, ssa if necessary. 
    // These methods only recompute if a change is detected.
    //
    static void computeDominators(IRManager& irm);
    static void computeLoops(IRManager& irm);
    static void computeDominatorsAndLoops(IRManager& irm);
    static void fixupSsa(IRManager& irm);
    static void splitCriticalEdges(IRManager& irm);
    static bool isProfileConsistent(IRManager& irm);
    static void smoothProfile(IRManager& irm);

    static void printHIR(IRManager& irm);
    static void printDotFile(IRManager& irm, const char* suffix);

    static const char* indent(IRManager& irm);
protected:
	//
	// Virtual dtor - to provide a correct cleanup for derived classes and 
	// to avoid many compiler's warnings 
	//
	virtual ~OptPass() {};
    //
    // Callback to subclass to run optimization.
    //
    virtual void _run(IRManager& irm) = 0;

    //
    // Set options to print IR & dot files
    //
    virtual bool genIRBefore(IRManager& irm);
    virtual bool genIRAfter(IRManager& irm);
    virtual bool genDotFileBefore(IRManager& irm);
    virtual bool genDotFileAfter(IRManager& irm);

	void composeDotFileName(char * name, const char * suffix);
    void printHIR(IRManager& irm, bool condition, const char* when);
    void printDotFile(IRManager& irm, bool condition, const char* when);

    void initialize();

private:
    Timer* timer;
    ::std::string timerName;
    Timer* modeTimers[CM_NUM_MODES];
    ::std::string modeTimerNames[CM_NUM_MODES];

	unsigned id;
};

} //namespace Jitrino 

#endif //_OPT_PASS_H_
