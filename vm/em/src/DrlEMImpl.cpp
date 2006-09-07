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
* @author Mikhail Y. Fursov
* @version $Revision: 1.1.2.2.4.3 $
*/

#include "DrlEMImpl.h"

#include "EBProfileCollector.h"

#include "jit_import.h"
#include "em_intf.h"
#include "open/vm.h"
#include "ini.h"
#include "cxxlog.h"

#include <assert.h>
#include <algorithm>
#include <sstream>
#include <fstream>


#define DEFAULT_JIT_DLL "jitrino"
#define DEFAULT_INTERPRETER_DLL "interpreter"
#define LOG_DOMAIN "em"

DrlEMImpl* DrlEMFactory::emInstance = NULL;

DrlEMImpl* DrlEMFactory::createAndInitEMInstance() {
    assert(emInstance == NULL);
    emInstance = new DrlEMImpl();
    if (!emInstance->init()) {
        delete emInstance;
        emInstance = NULL;
    }
        
    return emInstance;
}

DrlEMImpl* DrlEMFactory::getEMInstance() {
    assert(emInstance!=NULL);
    return emInstance;
}   

void DrlEMFactory::deinitEMInstance() {
    assert(emInstance!=NULL);
    emInstance->deinit();
    delete emInstance;
    emInstance = NULL;
}


static EM_PCTYPE get_pc_type(EM_Handle _this, PC_Handle pc) {
    assert(_this!=NULL);
    assert(pc!=NULL);
    assert(((ProfileCollector*)pc)->type == EM_PCTYPE_ENTRY_BACKEDGE);
    return EM_PCTYPE_ENTRY_BACKEDGE;
}

static PC_Handle get_pc(EM_Handle _this,  EM_PCTYPE profile_type,  JIT_Handle jh,  EM_JIT_PC_Role jit_role) {
    assert(_this!=NULL);
    assert(jh!=NULL);
    DrlEMImpl* em = (DrlEMImpl*) _this;
    return (PC_Handle)em->getProfileCollector(profile_type, jh, jit_role);
}


static Method_Profile_Handle 
get_method_profile(EM_Handle _this, PC_Handle pch, Method_Handle mh) {
    assert(_this!=NULL);
    assert(pch!=NULL);
    assert(mh!=NULL);
    ProfileCollector* pc  = (ProfileCollector*)pch;
    return (Method_Profile_Handle)pc->getMethodProfile(mh);
}



RStep::RStep(JIT_Handle _jit, const std::string& _jitName, RChain* _chain)
: jit(_jit), jitName(_jitName), catName(std::string(LOG_DOMAIN)+"."+_jitName), chain(_chain), loggingEnabled(false), enable_profiling(NULL) 
{}


//todo!! replace inlined strings with defines!!
DrlEMImpl::DrlEMImpl() : jh(NULL), _execute_method(NULL), interpreterMode(false) {
    nMethodsCompiled=0;
    nMethodsRecompiled=0;
    tick=0;
    hymutex_create(&recompilationLock, TM_MUTEX_NESTED);
#ifndef _EM64T_
    initProfileAccess();
#endif
}

DrlEMImpl::~DrlEMImpl() {
    deallocateResources();
    hymutex_destroy(recompilationLock);
}

void DrlEMImpl::initProfileAccess() {
    profileAccessInterface.get_pc_type = get_pc_type;
    profileAccessInterface.get_pc = get_pc;
    profileAccessInterface.get_method_profile =  get_method_profile;

    //EB profile
    profileAccessInterface.eb_profiler_create_profile = eb_profiler_create_profile;
    profileAccessInterface.eb_profiler_get_entry_counter_addr=eb_profiler_get_entry_counter_addr;
    profileAccessInterface.eb_profiler_get_backedge_counter_addr = eb_profiler_get_backedge_counter_addr;
    profileAccessInterface.eb_profiler_is_in_sync_mode= eb_profiler_is_in_sync_mode;
    profileAccessInterface.eb_profiler_get_entry_threshold = eb_profiler_get_entry_threshold;
    profileAccessInterface.eb_profiler_sync_mode_callback = (void (*)(Method_Profile_Handle))vm_create_helper_for_function((void *(*)(void *))eb_profiler_sync_mode_callback);
    profileAccessInterface.eb_profiler_get_backedge_threshold = eb_profiler_get_backedge_threshold;
}


void DrlEMImpl::deallocateResources() {
    tbsClients.clear();

    for (RChains::const_iterator cit = chains.begin(), cend = chains.end(); cit!=cend; ++cit) {
        RChain* chain = *cit;
        for (RSteps::const_iterator sit = chain->steps.begin(), send = chain->steps.end(); sit!=send; ++sit) {
            RStep* step = *sit;
            //todo: handle jit instance -> unload or deinit
            delete step;
        }
        delete chain;
    }
    chains.clear();

    for (ProfileCollectors::iterator it = collectors.begin(), end = collectors.end(); it!=end; ++it) {
        ProfileCollector* pc = *it;
        delete pc;
    }    
    collectors.clear();
}

//_____________________________________________________________________
// Reading and parsing configuration


std::string buildDefaultLibPath(const std::string& dll_name) {
    std::string library_path = vm_get_property_value("vm.boot.library.path");
#ifdef PLATFORM_NT
    std::string fullPath = dll_name + ".dll";
    
    if (!library_path.empty()) { 
    	fullPath = library_path + "\\" + fullPath;
    }
    
#else
//  $$$ GMJ
//
//    std::string fullPath = library_path + "/lib" + dll_name + ".so";

	std::string fullPath = "lib" + dll_name + ".so";
	
    if (!library_path.empty()) {
  		fullPath = library_path + "/" + fullPath;
    }
    
#endif
    return fullPath;
}

static std::string getParam(const std::string& config, const std::string& name) {
    std::istringstream is(config);
    std::string propPrefix = name+"=";
    size_t prefixLen = propPrefix.length();
    std::string line;
    while (std::getline(is, line)) {
        if (line.length() > prefixLen && std::equal(propPrefix.begin(),propPrefix.end(), line.begin())) {
            return line.substr(prefixLen);
        }
    }
    return "";
}


typedef std::vector<std::string> StringList;
static StringList getParamAsList(const std::string& config, const std::string& name, char listSeparator, bool notEmpty) {
    std::string value = getParam(config, name);
    StringList res;
    std::string token;
    for (std::string::const_iterator it = value.begin(), end = value.end(); it!=end; ++it) {
        char c = *it;
        if (c == listSeparator) {
            if (token.empty() && notEmpty) {
                continue;
            }
            res.push_back(token);
            token.clear();
        } else {
            token.push_back(c);
        }
    }
    if (!token.empty()) { //last value
        res.push_back(token);
    }
    return res;
}

static StringList getAllParamsAsList(const std::string& config, const std::string& name) {
    StringList res;
    std::istringstream is(config);
    std::string propPrefix = name+"=";
    size_t prefixLen = propPrefix.length();
    std::string line;
    while (std::getline(is, line)) {
        if (line.length() > prefixLen && std::equal(propPrefix.begin(),propPrefix.end(), line.begin())) {
            res.push_back(line.substr(prefixLen));
        }
    }
    return res;
}

static std::string single2Chain(const string& pathToDll, const std::string& jitName) {
    return "chains=chain1\nchain1.jits="+jitName+"\n"+jitName+".file="+pathToDll+"\n";
}

static std::string dpgo2Chain(const string& pathToDll, bool async) {
    std::string res = std::string("chains=chain1,chain2\n")
        +"chain1.jits=JET_CLINIT\n"
        +"chain2.jits=JET_DPGO,OPT\n"

        +"chain1.filter=+::<clinit>\n"
        +"chain1.filter=-\n"

        +"JET_CLINIT.file="+pathToDll+"\n"
        +"JET_DPGO.file="+pathToDll+"\n"
        +"OPT.file="+pathToDll+"\n"

        +"JET_DPGO.genProfile=JET_DPGO_PROFILE\n"
        +"JET_DPGO_PROFILE.profilerType=ENTRY_BACKEDGE_PROFILER\n"
        +"OPT.useProfile=JET_DPGO_PROFILE\n"

        +"JET_DPGO_PROFILE.mode="+(async ? "ASYNC" : "SYNC")+"\n"
        +"JET_DPGO_PROFILE.entryThreshold=10000\n"
        +"JET_DPGO_PROFILE.backedgeThreshold=100000\n"
        +"JET_DPGO_PROFILE.tbsTimeout=5\n"
        +"JET_DPGO_PROFILE.tbsInitialTimeout=0\n";

    return res;
}

static std::string readEMConfiguration() {
    std::string prop = vm_get_property_value("em.properties");
    std::string config;
    bool isJet=false, isOpt = false;
    if (prop.empty()) {
#ifdef _IPF_
        config = single2Chain(buildDefaultLibPath(std::string(DEFAULT_JIT_DLL)), std::string("JIT"));
#else
        config = dpgo2Chain(buildDefaultLibPath(std::string(DEFAULT_JIT_DLL)), false);
//        config = single2Chain(buildDefaultLibPath(std::string(DEFAULT_JIT_DLL)), std::string("JIT"));
#endif
    } else if (startsWith(prop, "single:") 
        || (isOpt = startsWith(prop, "opt:") || prop == "opt") 
        || (isJet = startsWith(prop, "jet:") || prop == "jet")) {
        
        if (isJet) {
            prop = std::string("single:")+ "JET:" + (prop.length() > 4 ? prop.substr(4) : std::string(""));
        } else  if (isOpt) {
            prop = std::string("single:")+ "OPT:" + (prop.length() > 4 ? prop.substr(4) : std::string(""));
        }
        std::string jitName, fileName;
        size_t jitNameStartIdx = strlen("single:");
        size_t jitNameEndIdx = prop.find(':', jitNameStartIdx);
        if (jitNameEndIdx!=std::string::npos) {
            jitName = prop.substr(jitNameStartIdx, jitNameEndIdx - jitNameStartIdx);
            fileName = prop.substr(jitNameEndIdx+1);
        } 
        if (fileName.empty()) {
            fileName = buildDefaultLibPath(std::string(DEFAULT_JIT_DLL));
        }
        config = single2Chain(fileName, jitName);
    } else if (startsWith(prop, "dpgo:")   || prop == "dpgo" 
        || startsWith(prop, "dpgo_sync:")  || prop == "dpgo_sync" 
        || startsWith(prop, "dpgo_async:") || prop == "dpgo_async" )
    {
        bool async = startsWith(prop, "dpgo_async");
        std::string jitPath;
        size_t colonPos = prop.find(':');
        if (colonPos!= std::string::npos && prop.length() > colonPos) {
            jitPath = prop.substr(colonPos+1);
        } else {
            jitPath = buildDefaultLibPath(DEFAULT_JIT_DLL);
        }
        config = dpgo2Chain(jitPath, async);
    } else {
        std::ifstream configFile;
        configFile.open(prop.c_str(), std::ios::in);
        bool rc = false;
        if (configFile.is_open()) {
            std::string line;
            while (getline(configFile, line)) {
                config+=line+"\n";
            }
            rc = !config.empty();
        } 
        if (!rc) {
            std::string errMsg = "EM: Can't read configuration from '" + prop + "'";
            ECHO(errMsg.c_str());
        }
    }
    return config;
}

//_______________________________________________________________________
// EM initialization methods

bool DrlEMImpl::init() {
    interpreterMode = vm_get_boolean_property_value_with_default("vm.use_interpreter");
    if (interpreterMode) {
        apr_dso_handle_t* libHandle;
        std::string interpreterLib = buildDefaultLibPath(DEFAULT_INTERPRETER_DLL); 
        jh = vm_load_jit(interpreterLib.c_str(), &libHandle);

        if (jh == NULL) {
            ECHO(("EM: Can't load EE library:" + interpreterLib).c_str());
            return false;
        }
        apr_dso_handle_sym_t fn = NULL;
        apr_dso_sym(&fn, libHandle, "JIT_execute_method");
        _execute_method = (void(*)(JIT_Handle,jmethodID, jvalue*, jvalue*)) fn;
        if (_execute_method==NULL) {
            ECHO(("EM: Not a EE shared library: '" + std::string(interpreterLib) + "'").c_str());
            return false;
        }
        RStep step(jh, "interpreter", NULL);
        return initJIT(interpreterLib, libHandle, step);
    }
    //normal mode with recompilation chains..
    _execute_method = JIT_execute_method_default;
	std::string config = readEMConfiguration();
    if (!config.empty()) {
        buildChains(config);
    }
    return !chains.empty();
}


static bool enable_profiling_stub(JIT_Handle jit, PC_Handle pc, EM_JIT_PC_Role role) {
    return false;
}

bool DrlEMImpl::initJIT(const std::string& libName, apr_dso_handle_t* libHandle, RStep& step) {
    apr_dso_handle_sym_t fn = NULL;
    if (apr_dso_sym(&fn, libHandle, "JIT_init") != APR_SUCCESS) {
        ECHO(("EM: Not a JIT shared lib: '" + libName + "'").c_str());
        return false;
    }
    void (*_init)(JIT_Handle, const char*) = (void (*)(JIT_Handle, const char*)) fn;
    _init(step.jit, step.jitName.c_str());

    bool pcEnabled = false;
    if (apr_dso_sym(&fn, libHandle, "JIT_set_profile_access_interface") == APR_SUCCESS) {
        pcEnabled =  true;
        void (*_setPAInterface)(JIT_Handle, EM_Handle, struct EM_ProfileAccessInterface*) = (void (*)(JIT_Handle, EM_Handle, struct EM_ProfileAccessInterface*))fn;
        _setPAInterface(step.jit, (EM_Handle)this, &profileAccessInterface);
    } 

    if (pcEnabled && apr_dso_sym(&fn, libHandle, "JIT_enable_profiling") == APR_SUCCESS) {
        step.enable_profiling = (bool(*)(JIT_Handle, PC_Handle, EM_JIT_PC_Role))fn;
    } else {
        step.enable_profiling = enable_profiling_stub;
    }

    return true;
}

void DrlEMImpl::buildChains(std::string& config) {
    bool loggingEnabled =  is_info_enabled(LOG_DOMAIN);
    StringList chainNames = getParamAsList(config, "chains", ',', true);
    if (chainNames.empty()) {
        ECHO("EM: No 'chains' property found in configuration");
        return;
    }
    bool failed = false;
    for (StringList::const_iterator chainIt = chainNames.begin(), chainEnd = chainNames.end(); chainIt!=chainEnd; ++chainIt) {
        std::string chainName = *chainIt;
        if (std::count(chainNames.begin(), chainNames.end(), chainName)!=1) {
            failed = true;
            break;
        }
        RChain* chain = new RChain();
        chains.push_back(chain);
        StringList jitsInChain= getParamAsList(config, chainName + ".jits", ',', true);
        for (StringList::const_iterator jitIt = jitsInChain.begin(), jitEnd = jitsInChain.end(); jitIt!=jitEnd; ++jitIt) {
            std::string jitName= *jitIt;
            std::string libName = getParam(config, jitName+".file");
            if (libName.empty()) {
                ECHO(("EM: No JIT library specified for JIT :'"  + jitName+ "'").c_str());
                failed = true;
                break;
            }
            apr_dso_handle_t* libHandle;
            JIT_Handle jh = vm_load_jit(libName.c_str(), &libHandle); //todo: do not load the same dll twice!!!
            if (jh == NULL) {
                ECHO(("EM: JIT library loading error:'"  + libName + "'").c_str());
                failed = true;
                break;
            }
            RStep* step = new RStep(jh, jitName, chain);
            step->loggingEnabled = loggingEnabled || is_info_enabled(step->catName.c_str());
            chain->steps.push_back(step);

            if (!initJIT(libName, libHandle, *step)) {
                failed = true;
                break;
            }
        }
        failed = failed || chain->steps.empty();
        if (!failed) { 
            // reading chain filters
            StringList filters = getAllParamsAsList(config, chainName+".filter");
            for (StringList::const_iterator filterIt = filters.begin(), filterEnd = filters.end(); filterIt!=filterEnd; ++filterIt) {
                const std::string& filter = *filterIt;
                bool res = chain->addMethodFilter(filter);
                if (!res) {
                    ECHO(("EM: Invalid filter :'"  + filter+ "'").c_str());
                }
            }
        }
        if (failed) {
            break;
        }
    }
    if (!failed) { //initialize profile collectors
        for (RChains::const_iterator it = chains.begin(), end = chains.end(); it!=end; ++it) {
            RChain* chain = *it;
            failed = !initProfileCollectors(chain, config);
            if (failed) {
                break;
            }
        }
    }
    if (failed) {
        deallocateResources();
    }
}

void DrlEMImpl::deinit() {
}

//______________________________________________________________________________
// EM runtime
void DrlEMImpl::executeMethod(jmethodID meth, jvalue  *return_value, jvalue *args) {
    //do not choose JIT here, this method will call-back from vm with compileMethod request.
    _execute_method(0, meth, return_value, args);
}

JIT_Result DrlEMImpl::compileMethod(Method_Handle mh) {
    //initial method compilation. Select chain to use.

    nMethodsCompiled++;
    //these vars used for logging
    const char* methodName = NULL;
    const char* className = NULL;
    const char* signature = NULL;
    size_t n = nMethodsCompiled;

    assert(!chains.empty());
    for (RChains::const_iterator it = chains.begin(), end = chains.end(); it!=end; ++it) {
        RChain* chain = *it;
        if (chain->acceptMethod(mh, nMethodsCompiled)) {
            assert(!chain->steps.empty());
            RStep* step = chain->steps[0];

            if (step->loggingEnabled) {
                methodName = method_get_name(mh);
                Class_Handle ch = method_get_class(mh);
                className = class_get_name(ch);
                signature = method_get_descriptor(mh);
                std::ostringstream msg;
                msg <<"EM: compile start:["<<step->jitName.c_str()<<" n="<<n<<"] "
                    <<className<<"::"<<methodName<<signature;
                INFO2(step->catName.c_str(), msg.str().c_str());
            }

            JIT_Result res = vm_compile_method(step->jit, mh);

            if (step->loggingEnabled) {
                std::ostringstream msg;
                msg << "EM: compile done:["<<step->jitName.c_str()<<" n="<<n<<": "
                    <<(res ==JIT_SUCCESS ? "OK" : "FAILED")<<"] "<<className<<"::"<<methodName<<signature;
                INFO2(step->catName.c_str(), msg.str().c_str());
            }


            if (res == JIT_SUCCESS) {
                return JIT_SUCCESS;
            }
        }
    }
    return JIT_FAILURE;
}



//______________________________________________________________________________
// Profile collectors initialization and recompilation

static uint32 toNum(const std::string& numStr, bool *rc ) {
    if (isNum(numStr)) {
        *rc = true;
        return atoi(numStr.c_str());
    }
    *rc = false;
    return 0;
}

ProfileCollector* DrlEMImpl::createProfileCollector(const std::string& profilerName, const std::string& config, RStep* step)  {
    ProfileCollector* pc = getProfileCollector(profilerName);
    if (pc != NULL){
        return NULL;
    }    
    std::string profilerType = getParam(config, profilerName+".profilerType");
    if (profilerType!="ENTRY_BACKEDGE_PROFILER") {
        ECHO("EM: Unsupported profiler type");
        return NULL;
    }
    EBProfileCollector::EB_ProfilerMode ebMode = EBProfileCollector::EB_PCMODE_SYNC;
    std::string mode = getParam(config, profilerName+".mode");
    if (mode == "ASYNC") {
        ebMode = EBProfileCollector::EB_PCMODE_ASYNC;
    }  else if (mode!="SYNC") {
        ECHO("EM: unsupported profiler mode");
        return NULL;
    }
    
    bool ok = false;
    uint32 eThreshold = toNum(getParam(config, profilerName+".entryThreshold"), &ok);//todo: default values..
        if (!ok) {
        ECHO("EM: illegal 'entryThreshold' value");
        return NULL;
        }
            uint32 bThreshold = toNum(getParam(config, profilerName+".backedgeThreshold"), &ok);
    if (!ok) {
        ECHO("EM: illegal 'backedgeThreshold' value");
        return NULL;
    }
    uint32 tbsTimeout = 0, tbsInitialTimeout = 0;
    if (ebMode == EBProfileCollector::EB_PCMODE_ASYNC) {
        tbsTimeout= toNum(getParam(config, profilerName+".tbsTimeout"), &ok);
        if (!ok) {
            ECHO("EM: illegal 'tbsTimeout' value");
            return NULL;
            }
        tbsInitialTimeout= toNum(getParam(config, profilerName+".tbsInitialTimeout"), &ok);
        if (!ok) {
            ECHO("EM: illegal 'tbsInitialTimeout' value");
            return NULL;
        }
    }
    pc = new EBProfileCollector(this, profilerName, step->jit, ebMode, eThreshold, bThreshold, tbsInitialTimeout, tbsTimeout);
    return pc;
}

ProfileCollector* DrlEMImpl::getProfileCollector(const std::string& name) const {
    for (ProfileCollectors::const_iterator it = collectors.begin(), end = collectors.end(); it!=end; ++it) {
        ProfileCollector* c = *it;
        if (c->name == name) {
            return c;
        }
    }
    return NULL;
}

bool DrlEMImpl::initProfileCollectors(RChain* chain, const std::string& config) {
    bool failed = false;
    for (RSteps::const_iterator it = chain->steps.begin(), end = chain->steps.end(); it!=end; ++it) {
        RStep* step = *it;
        std::string profilerName = getParam(config, step->jitName + ".genProfile");
        if (!profilerName.empty()) {
            ProfileCollector* pc = createProfileCollector(profilerName, config, step);
            if (pc == NULL) {
                ECHO(("EM: profile configuration failed: "+ profilerName).c_str());
                failed = true;
                break;
            }
            bool genOk = step->enable_profiling(step->jit, (PC_Handle)pc, EM_JIT_PROFILE_ROLE_GEN);
            if (genOk) {
                collectors.push_back(pc);
                TbsEMClient* tbsClient = pc->getTbsEmClient();
                if (tbsClient!=NULL) {
                    assert(tbsClient->getTimeout() != 0 && tbsClient->getTimeout()!=0);
                    tbsClient->setNextTick(tbsClient->getInitialTimeout());
                    tbsClients.push_back(tbsClient);
                }
            } else {
                ECHO(("EM: profile generation is not supported: " + profilerName).c_str());
                delete pc;
                failed = true;
                break;
            }

        }
        profilerName = getParam(config, step->jitName+ ".useProfile");
        if (!profilerName.empty()) {
            ProfileCollector* pc = getProfileCollector(profilerName);
            bool invalidChain = true;
            if (pc!=NULL) {
                for(RSteps::const_iterator it2=chain->steps.begin(); it2 <it; ++it2) {
                    RStep* prevStep = *it2;
                    if (prevStep->jit == pc->genJit) {
                        invalidChain = false;
                        break;
                    }
                }
            }
            bool useOk = !invalidChain && (pc!=NULL && step->enable_profiling(step->jit, (PC_Handle)pc, EM_JIT_PROFILE_ROLE_USE));
            if (useOk) {
                pc->addUseJit(step->jit);
            } else {
                if (pc == NULL) {
                    ECHO(("EM: profile not found: " + profilerName).c_str());
                } else if (invalidChain) {
                    ECHO(("EM: illegal use of profile: " + profilerName).c_str());
                } else {
                    ECHO(("EM: profile usage is not supported: " + profilerName).c_str());
                }
            }
        }
    }
    if (!failed && !tbsClients.empty()) {
        //timer.start(this);
    }
    return !failed;
}

void DrlEMImpl::methodProfileIsReady(MethodProfile* mp) {
    
    hymutex_lock(recompilationLock);
    if (methodsInRecompile.find((Method_Profile_Handle)mp)!=methodsInRecompile.end()) {
        //method is already recompiling by another thread or by this thread(recursion)
        hymutex_unlock(recompilationLock);
        return;
    }
    methodsInRecompile.insert((Method_Profile_Handle)mp);
    nMethodsRecompiled++;
    hymutex_unlock(recompilationLock);

    const char* methodName = NULL;
    const char* className = NULL;
    const char* signature = NULL;
    size_t n = nMethodsRecompiled;

    JIT_Handle jit = mp->pc->genJit;
    for (RChains::const_iterator it = chains.begin(), end = chains.end(); it!=end; ++it) {
        RChain* chain = *it;
        for (RSteps::const_iterator sit = chain->steps.begin(), send = chain->steps.end(); sit!=send; ++sit) {
            RStep* step = *sit;
            if (step->jit == jit) {
                ++sit;
                RStep* nextStep = sit!=send ? *sit: NULL;
                if (nextStep != NULL) {

                    if (nextStep->loggingEnabled) {
                        methodName = method_get_name(mp->mh);
                        Class_Handle ch = method_get_class(mp->mh);
                        className = class_get_name(ch);
                        signature = method_get_descriptor(mp->mh);
                        std::ostringstream msg;
                        msg <<"EM: recompile start:["<<nextStep->jitName.c_str()<<" n="<<n<<"] "
                            <<className<<"::"<<methodName<<signature;
                        INFO2(nextStep->catName.c_str(), msg.str().c_str());
                    } 

                    vm_compile_method(nextStep->jit, mp->mh);

                    if (nextStep->loggingEnabled) {
                        std::ostringstream msg;
                        msg << "EM: recompile done:["<<nextStep->jitName.c_str()<<" n="<<n<<"] "
                            <<className<<"::"<<methodName<<signature;
                        INFO2(nextStep->catName.c_str(), msg.str().c_str());
                    }

                }
            }
        }
    }
    hymutex_lock(recompilationLock);
    methodsInRecompile.erase((Method_Profile_Handle)mp);
    hymutex_unlock(recompilationLock);
}

ProfileCollector* DrlEMImpl::getProfileCollector(EM_PCTYPE type, JIT_Handle jh, EM_JIT_PC_Role jitRole) const {
    for (ProfileCollectors::const_iterator it = collectors.begin(), end = collectors.end(); it!=end; ++it) {
        ProfileCollector* pc = *it;
        if (pc->type == type) {
            bool matched = false;
            if (jitRole == EM_JIT_PROFILE_ROLE_GEN) {
                matched =  pc->genJit == jh;
            } else {
                matched =  std::find(pc->useJits.begin(), pc->useJits.end(), jh)!=pc->useJits.end();
            }
            if (matched) {
                return pc;
            }
        }
    }
    return NULL;
}

bool DrlEMImpl::needTbsThreadSupport() const {
    return !tbsClients.empty();
}

void DrlEMImpl::tbsTimeout() {
    for (TbsClients::const_iterator it = tbsClients.begin(), end = tbsClients.end(); it!=end; ++it) {
        TbsEMClient* c = *it;
        if (c->getNextTick() == tick) {
//            printf("tick!\n");
            c->onTimeout();
            c->setNextTick(tick + c->getTimeout());
        }
    }
    tick++;
}


int DrlEMImpl::getTbsTimeout() const {
	return 100;
}              

 
