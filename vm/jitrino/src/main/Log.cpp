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
 * @version $Revision: 1.23.20.4 $
 *
 */

#include <errno.h> // errno
#include <string.h> // strerror
#include <iostream>
#include <fstream>
#include "Log.h"
#include "methodtable.h"

namespace Jitrino {

// If NULL, log all methods.
const char* Log::methodNameToLog = NULL;

static bool singlefile = false;
static bool singlefile_old_log_dropped = false;
static bool fileclasstree = false;
static Method_Table *filter = NULL;


// forward declarations
static void mk_dir(const char *dirname);
static Log* get_tls_log();
static bool close_log_stream(::std::ostream **os_ptr);

Log::Settings::Settings() {
	outp = &::std::cout;
	created_outp = false;
	logDirName[0] = '\0';
	dotFileDirName[0] = '\0';
	logFileName[0] = '\0';
	className[0] = '\0';
	methodName[0] = '\0';
	methodSignature[0] = '\0';
	logFileCreationPending = false;
	truncateLogFileOnCreation = false;
	doLogging = true;
}

Log::Settings* Log::push() {
	Log::Settings *s = new Log::Settings();
	settingsStack.push_back(s);
	return s;
}

Log::Settings* Log::pop() {
	Log::Settings *res = top();
	assert(res != NULL);
	settingsStack.pop_back();
	return res;
}

Log::Settings* Log::top() {
	int stack_size = (int)settingsStack.size();
	return stack_size > 0 ? settingsStack[stack_size-1] : NULL;
}

void Log::pushSettings() {
	// UNUSED_VAR Settings *s = 
	cur()->push();
}

void Log::popSettings() {
	Log *tls_log = cur();
	Settings *s = tls_log->pop();

	// close popped log stream and dispose of settings
	close_log_stream(&(s->outp));
	delete s;

	// restore previous settings of the categories
	Category *tls_root_category = tls_log->cat_root();
	Log::Settings *s_ptr = tls_log->top();

	if (s_ptr != NULL) {
		Log::Settings &s = *s_ptr;
		tls_root_category->setLogging(s.doLogging);
		tls_root_category->set_out(s.outp);
	}
}

Log::Log() {
	root = NULL;
	fe = NULL;
	opt_sim = NULL;
	opt_vn = NULL;
	opt = NULL;
	opt_dc = NULL;
	opt_abcd = NULL;
	opt_inline = NULL;
	opt_loop = NULL;
	opt_gcm = NULL;
	opt_gvn = NULL;
	opt_td = NULL;
	opt_reassoc = NULL;
	opt_mem = NULL;
	opt_sync = NULL;
	opt_gc = NULL;
//	opt_prefetch = NULL;
	opt_lazyexc = NULL;
	cg = NULL;
	cg_cs = NULL;
	cg_cl = NULL;
	cg_ce = NULL;
	cg_sched = NULL;

	rt = new Category("rt");
	rt->setParentOverride(false);
	cg_gc = rt->createCategory("gc");
	patch = rt->createCategory("patch");
    ti = NULL;
    ti_bc = rt->createCategory("bc");

	typechecker = NULL;

	rootLogDirName[0] = '\0';
	rt_outp = NULL;
	methodCounter=0;
	nextStageId=0;
}

#define DELETE_CATEGORY(name) if (name != NULL) delete name; name = NULL

Log::~Log() {
	DELETE_CATEGORY(root);
	DELETE_CATEGORY(fe);
	DELETE_CATEGORY(opt_sim);
	DELETE_CATEGORY(opt_vn);
	DELETE_CATEGORY(opt);
	DELETE_CATEGORY(opt_dc);
	DELETE_CATEGORY(opt_abcd);
	DELETE_CATEGORY(opt_inline);
	DELETE_CATEGORY(opt_loop);
	DELETE_CATEGORY(opt_gcm);
	DELETE_CATEGORY(opt_gvn);
	DELETE_CATEGORY(opt_td);
	DELETE_CATEGORY(opt_reassoc);
	DELETE_CATEGORY(opt_mem);
	DELETE_CATEGORY(opt_sync);
	DELETE_CATEGORY(opt_gc);
//	DELETE_CATEGORY(opt_prefetch);
	DELETE_CATEGORY(opt_lazyexc);
	DELETE_CATEGORY(cg);
	DELETE_CATEGORY(cg_cs);
	DELETE_CATEGORY(cg_cl);
	DELETE_CATEGORY(cg_ce);
	DELETE_CATEGORY(cg_gc);
	DELETE_CATEGORY(cg_sched);
	DELETE_CATEGORY(rt);
	DELETE_CATEGORY(patch);
    DELETE_CATEGORY(typechecker);
    DELETE_CATEGORY(ti);
    DELETE_CATEGORY(ti_bc);

	close_log_stream(&rt_outp);
}

//----------

void
Log::initializeCategories()
{
	cur()->initializeCategories_();
}

void
Log::initializeCategories_()
{
    if (!root) {
        root = new Category("root");
        fe = root->createCategory("fe");
        opt = root->createCategory("opt");
        opt_sim = opt->createCategory("sim");
        opt_vn = opt->createCategory("vn");
        opt_dc = opt->createCategory("dc");
        opt_abcd = opt->createCategory("abcd");
        opt_inline = opt->createCategory("inline");
        opt_loop = opt->createCategory("loop");
        opt_gcm = opt->createCategory("gcm");
        opt_gvn = opt->createCategory("gvn");
        opt_reassoc = opt->createCategory("reassoc");
        opt_mem = opt->createCategory("mem");
        opt_sync = opt->createCategory("sync");
        opt_td = opt->createCategory("td");
        opt_gc = opt->createCategory("gc");
//        opt_prefetch = opt->createCategory("prefetch");
        opt_lazyexc = opt->createCategory("lazyexc");
        cg = root->createCategory("cg");
        cg_cs = cg->createCategory("cs");
        cg_cl = cg->createCategory("cl");
        cg_ce = cg->createCategory("ce");
        cg_gc = cg->createCategory("gc");
        cg_sched = cg->createCategory("sched");
        patch = root->createCategory("patch");
        typechecker = root->createCategory("typechecker");
        ti = root->createCategory("ti");
        ti_bc = ti->createCategory("bc");
    } else {
      root->reset(false);
      fe->reset(true);
      opt->reset(true);
      opt_sim->reset(true);
      opt_vn->reset(true);
      opt_dc->reset(true);
      opt_abcd->reset(true);
      opt_inline->reset(true);
      opt_reassoc->reset(true);
      opt_mem->reset(true);
      opt_sync->reset(true);
      opt_loop->reset(true);
      opt_gcm->reset(true);
      opt_gvn->reset(true);
      opt_td->reset(true);
      opt_gc->reset(true);
//      opt_prefetch->reset(true);
      opt_lazyexc->reset(true);
      cg->reset(true);
      cg_cs->reset(true);
      cg_cl->reset(true);
      cg_ce->reset(true);
      cg_gc->reset(true);
      cg_sched->reset(true);
      patch->reset(true);
      typechecker->reset(true);
      ti->reset(true);
      ti_bc->reset(true);
    }
}

//----------

void
Log::initializeThreshold(const char* catName, const char* level)
{
	cur()->initializeThreshold_(catName, level);
}

void
Log::initializeThreshold_(const char* catName, const char* level)
{
    Log::Settings *s_ptr = top();
    assert(s_ptr != NULL);
    Log::Settings &s = *s_ptr;

    if(catName != NULL) {
        if (strncmp(catName,"singlefile",10)==0) {
            singlefile = true;
            if (!s.created_outp) {
                s.created_outp=true;
                bool truncate = !singlefile_old_log_dropped;
                setLogDir_(getRootLogDirName_(), truncate);
                singlefile_old_log_dropped = true;
            }
			if( strcmp(level, "tree")==0 ) {
				fileclasstree = true;
			}
		} else if (strncmp(catName,"file",4)==0) {
            assert(!singlefile);
            if (!s.created_outp) {
				s.created_outp=true;
				setLogDir_(getRootLogDirName_(), false);
            }
			if( strcmp(level, "tree")==0 ) {
				fileclasstree = true;
			}
        } else if (strncmp(catName,"method",6)==0) {
			if (methodNameToLog == NULL) {
				static ::std::string methodName;
				methodName = level;
				methodNameToLog = methodName.c_str();
			}
        } else if (strncmp(catName,"filter",6)==0) {
                if (filter == NULL) {
                    filter = new Method_Table(level, "LOG_FILTER", false);
                }
        } else {
			Category* cat = strcmp("root", catName) ? root->getCategory(catName) : root;

            if (cat == NULL) {
				if (!strcmp("rt", catName)) {
					cat = rt;

					if (!rt->is_out_set()) {
						rt->set_out(getRtOutput_());
					}
				} else {
					::std::cerr << "***ERROR: invalid category name: " << catName << ::std::endl;
					exit(1);
				}
			}
			if(level != NULL && *level != '\0') {
				cat->setThreshold(level);
			} else {
				cat->setThreshold(LogAll);
			}
        }
    }
}

//----------

void 
Log::initializeThresholds(const char* values)
{
	cur()->initializeThresholds_(values);
}

void 
Log::initializeThresholds_(const char* values)
{
    if(values != NULL) {
        if(*values == '\0')
            initializeThreshold(values, NULL);
        while(*values != '\0') {
            ::std::string catName = ""; 
            ::std::string level = "";
            while((*values != '\0') && (*values != '=') && (*values != ',')) {
                catName += *values; values++;
            }
            if(*values == '=')
                values++;
            while((*values != '\0') && (*values != ',')) {
                level += *values; values++;
            }
            initializeThreshold(catName.c_str(), level.c_str());
            if(*values != '\0')
                values++;
        }
    }
}

::std::ostream&
Log::out_() {
	Log::Settings *s_ptr = top();

	if (s_ptr == NULL) {
		// this can happen if client code calls Log::out in run-time routines
		// (beyond the CompileMethod scope)
		// just return ::std::out in this case - client should have used rt
		// category stream for output and "file,rt=..." log option to catch it in a file
		return ::std::cout;
	}
	Log::Settings &s = *s_ptr;

	if (s.logFileCreationPending) {
		mk_dir(s.logDirName);
		openLogFile_(s.logFileName, s.truncateLogFileOnCreation);
		mk_dir(s.dotFileDirName);
	}
	assert(s.outp != NULL);
	return *s.outp;
}

//----------

bool
Log::setLogging(const char* methodName) {
    if (methodNameToLog == NULL) {
        return true;
    }
	Log *log = cur();
	Log::Settings *s_ptr = log->top();
	assert(s_ptr != NULL);
	Log::Settings &s = *s_ptr;
    s.doLogging = (strcmp(methodNameToLog, methodName) == 0);
    log->cat_root()->setLogging(s.doLogging);
    return s.doLogging;
}

//----------

bool
Log::setLogging(const char* className, const char* methodName, const char* methodSig) {
    if (methodNameToLog == NULL && filter == NULL) {
        return true;
	}
	Log *tls_log = cur();
	Category *tls_root_category = tls_log->cat_root();
	Log::Settings *s_ptr = tls_log->top();
	assert(s_ptr != NULL);
	Log::Settings &s = *s_ptr;

	if (filter != NULL) {
		bool filter_accepts = filter->accept_this_method(className, methodName, methodSig);

		// turn on/off the doLogging switch only if the filter is not
		// in method list generation mode when it accepts no methods
		if (!filter->is_in_list_generation_mode()) {
			// enable logging only for methods matching the filter (method table)
			s.doLogging = filter_accepts;
			tls_root_category->setLogging(s.doLogging);

			// ignore methodNameToLog-based filtering in presense of
			// method table-based filtering and return
			return s.doLogging;
		}
	}
	if (methodNameToLog == NULL) {
		return true;
	}
    uint32 len = (uint32) strlen(className);
    //
    //  Do logging if methodNameToLog is className.methodName or
    //  methodName.
    //
    bool fullMatch = (strncmp(methodNameToLog, className, len) == 0) && 
        (methodNameToLog[len] == '.') &&
        (strcmp(methodNameToLog+len+1, methodName) == 0);
    bool methodMatch = (strcmp(methodNameToLog,methodName) == 0);
    bool classMatch = (strcmp(methodNameToLog,className) == 0);
    s.doLogging = fullMatch || methodMatch || classMatch;
	tls_root_category->setLogging(s.doLogging);
    return s.doLogging;
}

//----------

void Log::setMethodToCompile(
	const char * clsname,
	const char * name,
	const char * signature,
	uint32 byteCodeSize)
{
	cur()->setMethodToCompile_(clsname, name, signature, byteCodeSize);
}

void Log::setMethodToCompile_(const char * clsname, const char * name, const char * signature, uint32 byteCodeSize)
{
	setLogging(clsname, name, signature);
	Log::Settings *s_ptr = top();
	assert(s_ptr != NULL);
	Log::Settings &s = *s_ptr;

	if (!s.created_outp) {
		return;
	}

	assert(clsname!=NULL && name!=NULL && signature!=NULL);

	strcpy(s.className, clsname);
	strcpy(s.methodName, name);
	strcpy(s.methodSignature, signature);

	++methodCounter;

	char dirname[1024];
	char logdir[1024];
	
	if( fileclasstree ) {
		char mdirname[130];
		snprintf(mdirname, 130, "%s%s", name, signature);
		if (strlen(mdirname)>128){
			snprintf(mdirname, 130, "_xxx_.%s%s", name, signature);
			mdirname[127]=0;
		}
		fixFileName(mdirname);
		snprintf(logdir, 1024, "%s/%s/%s", s.logDirName, s.className, mdirname);
		out_() << "__COMPILATION_START__:\t#" << methodCounter 
			<< "\tclassName=" << s.className 
			<< "\tmethodName=" << s.methodName 
			<< "\tmethodSignature=" << s.methodSignature
			<< "\tbyteCodeSize=" << byteCodeSize
			<< "\tlogDirName=" << logdir
			<< ::std::endl;
	} else {
		sprintf(dirname, "%06d.%s.%s%s", (int)methodCounter, clsname, name, signature);
		if (strlen(dirname)>128){
			sprintf(dirname, "%06d._xxx_.%s%s", (int)methodCounter, name, signature);
			dirname[127]=0;
		}
		fixFileName(dirname);
		sprintf(logdir, "%s/%s", s.logDirName, dirname);
		out_() << "__COMPILATION_START__:\t#" << methodCounter 
			<< "\tclassName=" << s.className 
			<< "\tmethodName=" << s.methodName 
			<< "\tmethodSignature=" << s.methodSignature
			<< "\tbyteCodeSize=" << byteCodeSize
			<< "\tlogDirName=" << dirname
			<< ::std::endl;
	}

	setLogDir_(logdir, true);
}

//----------

void Log::clearMethodToCompile(bool compilationResult, CompilationInterface * compilationInterface)
{
	cur()->clearMethodToCompile_(compilationResult, compilationInterface);
}

void Log::clearMethodToCompile_(bool compilationResult, CompilationInterface * compilationInterface)
{ 
	Log::Settings *s_ptr = top();
	assert(s_ptr != NULL);
	Log::Settings &s = *s_ptr;

	if (!s.created_outp) {
		return;
        }
	assert(s.className[0]!=0 && s.methodName[0]!=0 && s.methodSignature[0]!=0);
	setLogDir_(getRootLogDirName_(), false); 
    ::std::ostream& os=out_();
	os<<"__COMPILATION_END__:\t#"<<methodCounter<<"\tclassName="<<s.className<<"\tmethodName="<<s.methodName<<"\tmethodSignature="<<s.methodSignature;
	os<<"\tcompilationResult="<<(compilationResult?"SUCCESS":"FAILURE");
	if (compilationResult){
    	MethodDesc * md = compilationInterface->getMethodToCompile();

    	Byte * codeAddr=compilationInterface->getCodeBlockAddress(md, 0);
    	uint32 codeSize=compilationInterface->getCodeBlockSize(md, 0);
    	if (codeAddr!=NULL && codeSize>0){
			os<<"\tcodeRange=["<<(void*)codeAddr<<","<<(void*)(codeAddr+codeSize)<<"]";
			os<<"\tcodeSize="<<codeSize;
		}

    	Byte * infoAddr=compilationInterface->getInfoBlock(md);
    	uint32 infoSize=compilationInterface->getInfoBlockSize(md);
    	if (infoAddr!=NULL && infoSize>0){
			os<<"\tinfoRange=["<<(void*)infoAddr<<","<<(void*)(infoAddr+infoSize)<<"]";
			os<<"\tinfoSize="<<infoSize;
		}
	}
	os<<::std::endl;
}

//----------

void Log::setLogDir(const char * filename, bool truncate)
{
	cur()->setLogDir_(filename, truncate);
}

void Log::setLogDir_(const char * filename, bool truncate)
{
	Log::Settings *s_ptr = top();
	assert(s_ptr != NULL);
	Log::Settings &s = *s_ptr;

	if (!s.created_outp) {
		return;
	}
	s.logFileCreationPending = true;
	s.truncateLogFileOnCreation = truncate;

	if (filename==NULL) {
		filename="";
	}
	strcpy(s.logDirName, filename);
	strcpy(s.logFileName, s.logDirName);
    strcat(s.logFileName, "/jitrino.log");
	strcpy(s.dotFileDirName, s.logDirName);
    strcat(s.dotFileDirName, "/dotfiles");
}

::std::ostream* Log::getRtOutput_() {
	if (rt_outp == NULL) {
		char filename[1024];
		mk_dir(getRootLogDirName_());
		sprintf(filename, "%s/rt.log", getRootLogDirName_());
		rt_outp = new ::std::ofstream(filename, ::std::ios::app);

		if (!rt_outp || !*rt_outp) {
			::std::cerr << "***ERROR: unable to open " << filename << " for log output." << ::std::endl;
			::std::cerr << "    " << strerror(errno) << ::std::endl;
//VSH			exit(1);
		};
	}
	return rt_outp;
}

const char* Log::getRootLogDirName_() {
	if (rootLogDirName[0] == '\0') {
        if (singlefile) {
            sprintf(rootLogDirName, "log");
        } else {
            sprintf(rootLogDirName, "log.%x", logId);
        }
	}
	return rootLogDirName;
}

//----------

void Log::fixFileName(char * filename)
{
	if (filename==NULL)
		return;
    for (; *filename!=0; filename++) {
		if (!(isalpha(*filename)||isdigit(*filename)||*filename=='.'||*filename=='-'||*filename=='_'))
			*filename='_';
    }
}

//----------

void Log::openLogFile(const char * filename, bool truncate)
{
	cur()->openLogFile_(filename, truncate);
}

void Log::openLogFile_(const char * filename, bool truncate)
{
	Log::Settings *s_ptr = top();
	assert(s_ptr != NULL);
	Log::Settings &s = *s_ptr;

	if (close_log_stream(&s.outp)) {
		root->set_out(NULL);
	}
	if (!s.created_outp) {
		return;
	}
	s.outp = new ::std::ofstream(filename, truncate ? ::std::ios::trunc : ::std::ios::app);
	
	if (!*s.outp) {
		::std::cerr << "***ERROR: unable to open " << filename << " for log output." << ::std::endl;
		::std::cerr << "    " << strerror(errno) << ::std::endl;
//VSH		exit(1);
	};
	root->set_out(s.outp);
	s.logFileCreationPending = false;
}

//----------

uint32 Log::getNextStageId() {
	return cur()->getNextStageId_();
}

uint32 Log::getNextStageId_() {
	return nextStageId++;
}

//----------

const char * Log::getLogDirName()
{ 
	return cur()->getLogDirName_();
}

const char * Log::getLogDirName_()
{ 
	Log::Settings *s = top();
	assert(s != NULL);
	return s->logDirName; 
}

//----------

const char * Log::getDotFileDirName()
{ 
	return cur()->getDotFileDirName_();
}

const char * Log::getDotFileDirName_()
{ 
	Log::Settings *s = top();
	assert(s != NULL);
	return s->dotFileDirName; 
}

//----------

const char * Log::getLogFileName()
{ 
	return cur()->getLogFileName_();
}

const char * Log::getLogFileName_()
{ 
	Log::Settings *s = top();
	assert(s != NULL);
	return s->logFileName; 
}

//----------

const char * Log::getClassName()
{ 
	return cur()->getClassName_();
}

const char * Log::getClassName_()
{ 
	Log::Settings *s = top();
	assert(s != NULL);
	return s->className; 
}

//----------

const char * Log::getMethodName()
{ 
	return cur()->getMethodName_();
}

const char * Log::getMethodName_()
{ 
	Log::Settings *s = top();
	assert(s != NULL);
	return s->methodName; 
}

//----------

const char * Log::getMethodSignature()
{ 
	return cur()->getMethodSignature_();
}

const char * Log::getMethodSignature_()
{ 
	Log::Settings *s = top();
	assert(s != NULL);
	return s->methodSignature; 
}

//----------

void Log::printStageBegin(
	uint32 stageId,
	const char * stageGroup,
	const char * stageName,
	const char * stageTag)
{
	cur()->printStageBegin_(stageId, stageGroup, stageName, stageTag);
}

void Log::printStageBegin_(
	uint32 stageId,
	const char * stageGroup,
	const char * stageName,
	const char * stageTag)
{
	out_() << "========================================================================" << ::std::endl;
	out_() << "__STAGE_BEGIN__:\tstageId="<<stageId<<"\tstageGroup="<<stageGroup<<"\tstageName=" << stageName << "\tstageTag=" << stageTag << ::std::endl;
    out_() << "========================================================================" << ::std::endl << ::std::endl ;
}

//----------

void Log::printStageEnd(
	uint32 stageId,
	const char * stageGroup,
	const char * stageName,
	const char * stageTag)
{
	cur()->printStageEnd_(stageId, stageGroup, stageName, stageTag);
}

void Log::printStageEnd_(
	uint32 stageId,
	const char * stageGroup,
	const char * stageName,
	const char * stageTag)
{
	out_() << "========================================================================" << ::std::endl;
	out_() << "__STAGE_END__:\tstageId="<<stageId<<"\tstageGroup="<<stageGroup<<"\tstageName=" << stageName << "\tstageTag=" << stageTag << ::std::endl;
    out_() << "========================================================================" << ::std::endl << ::std::endl ;
}

//----------

void Log::printIRDumpBegin(
	uint32 stageId,
	const char * stageName,
	const char * subKind)
{
	cur()->printIRDumpBegin_(stageId, stageName, subKind);
}

void Log::printIRDumpBegin_(
	uint32 stageId,
	const char * stageName,
	const char * subKind)
{
	out_() << "========================================================================" << ::std::endl;
	out_() << "__IR_DUMP_BEGIN__:\tstageId="<<stageId<<"\tstageName=" << stageName << "\tsubKind=" << subKind << ::std::endl;
	out_() << "Printing IR " << stageName << " - " << subKind << ::std::endl;
	out_() << "========================================================================" << ::std::endl << ::std::endl ;
}

//----------

void Log::printIRDumpEnd(
	uint32 stageId,
	const char * stageName,
	const char * subKind)
{
	cur()->printIRDumpEnd_(stageId, stageName, subKind);
}

void Log::printIRDumpEnd_(
	uint32 stageId,
	const char * stageName,
	const char * subKind)
{
	out_() << "========================================================================" << ::std::endl;
	out_() << "__IR_DUMP_END__:\tstageId="<<stageId<<"\tstageName=" << stageName << "\tsubKind=" << subKind << ::std::endl;
	out_() << "========================================================================" << ::std::endl << ::std::endl ;
}

//----------

Log* Log::cur() {
	return get_tls_log();
}

/**
* Closes a non-null stream other than the standard ones.
* @return true if the stream has been closed, false otherwise
*/
static bool close_log_stream(::std::ostream **os_ptr) {
	assert(os_ptr != NULL);
	::std::ostream *os = *os_ptr;

	if (os != NULL && os != &::std::cout && os != &::std::cerr) {
		os->flush();
		delete os;
		*os_ptr=NULL;
		return true;
	} else {
		return false;
	}
}

/*---------------------------------------------------------------------
 * Platform-dependent stuff
 *---------------------------------------------------------------------
 */

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#include <direct.h>

uint32 tlsLogKey=TLS_OUT_OF_INDEXES;

static void mk_dir(const char *dirname) {
	if( strchr(dirname, '/') != NULL ) {
		char dir2make[1024]; dir2make[0] = 0;
		int tail = 1023;
		char *dir = _strdup(dirname);
		char *token = strtok( dir, "/");

		while( token != NULL && tail > 0 ) {
			/* While there are tokens in "string" */
			strncat(dir2make, token, tail);
			tail -= strlen(token);
			_mkdir(dir2make);
			strncat(dir2make, "/", tail--);

			/* Get next token: */
			token = strtok( NULL, "/");
		}
	} else {
		_mkdir(dirname);
	}
}

void Log::notifyThreadStart(void *data) {
	uint32 tls_index = *((uint32*)data);
	void *cur_value = TlsGetValue(tls_index);
	if( !(NULL == cur_value) )
		assert(0);
	int thread_id = GetCurrentThreadId();
	Log *log = new Log(thread_id);

	if (!TlsSetValue(tls_index, log)) {
		assert(0);
	}
}

void Log::notifyThreadFinish(void *data) {
	uint32 tls_index = *((uint32*)data);
	Log *log = (Log*)TlsGetValue(tls_index);
	assert(log != NULL);
	delete log;
	TlsSetValue(tls_index, NULL);
}

static Log* get_tls_log() {
	Log *log = (Log*)TlsGetValue(tlsLogKey);
	assert(log != NULL);
	return log;
}

#else

#ifndef PLATFORM_POSIX
#error Unsupported platform
#endif

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

pthread_key_t tlsLogKey;

static void mk_dir1(const char *dirname) {
	if (mkdir(dirname, 0777 /*rwxrwxrwx*/) != 0) {
        /* 'dirname' exists do nothing */
        struct stat dir_status;
        if ( access(dirname, F_OK) == 0 && 
             stat(dirname, &dir_status) == 0 &&
             S_ISDIR(dir_status.st_mode) ) {
            return;
        }else{
            ::std::cerr << "***ERROR: failed to create directory: " << dirname << ::std::endl;
            ::std::cerr << "    " << strerror(errno) << ::std::endl;
        }
    }
}

static void mk_dir(const char *dirname) {
	if( strchr(dirname, '/') != NULL ) {
		char dir2make[1024]; dir2make[0] = 0;
		int tail = 1023;
		char *dir = strdup(dirname);
		char *token = strtok( dir, "/");

		while( token != NULL && tail > 0 ) {
			/* While there are tokens in "string" */
			strncat(dir2make, token, tail);
			tail -= strlen(token);
			mk_dir1(dir2make);
			strncat(dir2make, "/", tail--);

			/* Get next token: */
			token = strtok( NULL, "/");
		}
	} else {
		mk_dir1(dirname);
	}
}

void Log::notifyThreadStart(void *data) {}
void Log::notifyThreadFinish(void *data) {}

static bool tls_key_created = false;

static void delete_log(void *log_) {
	Log *log = (Log*)log_;

	if (log != NULL) {
		delete log;
	}
}

static Log* get_tls_log() {
	if (!tls_key_created) {
		if (pthread_key_create(&tlsLogKey, delete_log) != 0) {
			::std::cerr << "***ERROR: failed to create TLS key" << ::std::endl;
			exit(1);
		}
		tls_key_created = true;
	}
	Log *log = (Log*)pthread_getspecific(tlsLogKey);

	if (log == NULL) {
		log = new Log(pthread_self());
		pthread_setspecific(tlsLogKey, log);
	}
	assert(log != NULL);
	return log;
}

#endif

} // namespace Jitrino

