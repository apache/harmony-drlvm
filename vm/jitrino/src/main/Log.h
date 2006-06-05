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
 * @author Intel, George A. Timoshenko
 * @version $Revision: 1.18.20.4 $
 *
 */

#ifndef _LOG_H_
#define _LOG_H_

#include <vector>
#include "Category.h"
#include "VMInterface.h"

namespace Jitrino {

#ifdef PLATFORM_POSIX
#include <pthread.h>
	extern pthread_key_t tlsLogKey;
#else
#define snprintf _snprintf
	extern uint32 tlsLogKey;
#endif

class Log {
public:
	Log();
	Log(uint32 id) : logId(id) {new (this) Log();}
	~Log();

    static void initializeCategories();
    static void initializeThreshold(const char* category, const char* level);
    static void initializeThresholds(const char* values);

    static void pushSettings();
    static void popSettings();

    static void setMethodToCompile(const char * clsname, const char * name, const char * signature, uint32 byteCodeSize);
    static void fixFileName(char * filename);

    static void clearMethodToCompile(bool compilationResult, CompilationInterface * compilationInterface);

    static const char * getLogDirName();
    static const char * getDotFileDirName();
    static const char * getLogFileName();

    static const char * getClassName();
    static const char * getMethodName();
    static const char * getMethodSignature();

    static void printStageBegin(uint32 stageId, const char * stageGroup, const char * stageName, const char * stageTag);
    static void printStageEnd(uint32 stageId, const char * stageGroup, const char * stageName, const char * stageTag);
    static void printIRDumpBegin(uint32 stageId, const char * stageName, const char * subKind);
    static void printIRDumpEnd(uint32 stageId, const char * stageName, const char * subKind);

    static void setLogDir(const char * filename, bool truncate);
    static void openLogFile(const char * filename, bool truncate);

    static uint32 getNextStageId();

#define DEFINE_CATEGORY(cat_name) \
	Category *cat_name; \
	static Category* cat_##cat_name() { \
		Category *res = cur()->cat_name; \
		assert(res != NULL); \
		return res; \
	}

    DEFINE_CATEGORY(root);      // Everything
    DEFINE_CATEGORY(fe);        // CLI and Java front ends
    DEFINE_CATEGORY(opt_sim);   // Optimizer - simplifier
    DEFINE_CATEGORY(opt_vn);    // Optimizer - value numbering
    DEFINE_CATEGORY(opt);       // Optimizer
    DEFINE_CATEGORY(opt_dc);    // Optimizer - dead code elimination
    DEFINE_CATEGORY(opt_abcd);  // Optimizer - array bounds check elimination
    DEFINE_CATEGORY(opt_inline);// Optimizer - inliner
    DEFINE_CATEGORY(opt_loop);  // Optimizer - loop peeling / normalization
    DEFINE_CATEGORY(opt_gcm);   // Optimizer - global code motion
    DEFINE_CATEGORY(opt_gvn);   // Optimizer - global value numbering
    DEFINE_CATEGORY(opt_td);    // Optimizer - tail duplicator
    DEFINE_CATEGORY(opt_reassoc);  // Optimizer - re-association
    DEFINE_CATEGORY(opt_mem);      // Optimizer - memory
    DEFINE_CATEGORY(opt_sync);     // Optimizer - sync
	DEFINE_CATEGORY(opt_gc);	   // Optimizer - GC enumeration support
//    DEFINE_CATEGORY(opt_prefetch); // Optimizer - data pre-fetching
    DEFINE_CATEGORY(opt_lazyexc);     // Optimizer - lazy exception opt
    DEFINE_CATEGORY(cg);        // Code generator
    DEFINE_CATEGORY(cg_cs);     // Code generator - code selection
    DEFINE_CATEGORY(cg_cl);     // Code generator - code lowering (IA32)
    DEFINE_CATEGORY(cg_ce);     // Code generator - code emission
    DEFINE_CATEGORY(cg_gc);     // Code generator - garbage collection support
    DEFINE_CATEGORY(cg_sched);  // Code generator - scheduler
    DEFINE_CATEGORY(rt);        // Runtime (stack unwinding / GC enumeration)
    DEFINE_CATEGORY(patch);     // Patching code during the runtime
    DEFINE_CATEGORY(typechecker); // The HIR type checker
    DEFINE_CATEGORY(ti); // The JVTI support category
    DEFINE_CATEGORY(ti_bc); // The JVTI byte code mapping category

    ::std::ostream *rt_outp;

    ::std::ostream &out_();
    static ::std::ostream &out() { return cur()->out_(); };

    static bool setLogging(const char* methodName);
    static bool setLogging(const char* className, const char* methodName, const char* methodSig=NULL);

    static void notifyThreadStart(void *data);
    static void notifyThreadFinish(void *data);

private:
    static Log* cur();

    void initializeCategories_();
    void initializeThreshold_(const char* category, const char* level);
    void initializeThresholds_(const char* values);

    void setMethodToCompile_(const char * clsname, const char * name, const char * signature, uint32 byteCodeSize);
    void clearMethodToCompile_(bool compilationResult, CompilationInterface * compilationInterface);

    ::std::ostream* getRtOutput_();
    const char * getRootLogDirName_();
    const char * getLogDirName_();
    const char * getDotFileDirName_();
    const char * getLogFileName_();

    const char * getClassName_();
    const char * getMethodName_();
    const char * getMethodSignature_();

    void printStageBegin_(uint32 stageId, const char * stageGroup, const char * stageName, const char * stageTag);
    void printStageEnd_(uint32 stageId, const char * stageGroup, const char * stageName, const char * stageTag);
    void printIRDumpBegin_(uint32 stageId, const char * stageName, const char * subKind);
    void printIRDumpEnd_(uint32 stageId, const char * stageName, const char * subKind);

    void setLogDir_(const char * filename, bool truncate);
    void openLogFile_(const char * filename, bool truncate);

    uint32 getNextStageId_();

private:
    struct Settings {
        Settings();

        char className[1024];
        char methodName[1024];
        char methodSignature[1024];

        char logDirName[1024];
        char dotFileDirName[1024];
        char logFileName[1024];

        bool logFileCreationPending;
        bool truncateLogFileOnCreation;

        ::std::ostream *outp;
        bool created_outp;

        bool doLogging;
    };

    Settings* push();
    Settings* pop();
    Settings* top();

    static const char* methodNameToLog;

    char rootLogDirName[1024];

    uint32 methodCounter;
    uint32 nextStageId;

	uint32 logId;

    ::std::vector<Settings*> settingsStack;
};

} // namespace Jitrino
#endif // _LOG_H_

