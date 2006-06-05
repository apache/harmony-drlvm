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
 * @version $Revision: 1.32.8.3.4.4 $
 *
 */

#include <assert.h>
#include <iostream>

#include "open/types.h"
#include "optimizer.h"
#include "Inst.h"
#include "FlowGraph.h"
#include "irmanager.h"
#include "Dominator.h"
#include "Loop.h"

#include "ssa/SSA.h"

#include "Log.h"
#include "deadcodeeliminator.h"
#include "hashvaluenumberer.h"
#include "escapeanalyzer.h"
#include "globalopndanalyzer.h"
#include "simplifier.h"
#include "inliner.h"
#include "devirtualizer.h"
#include "PropertyTable.h"

#include "abcd/abcd.h"

#include "Jitrino.h"
#include "Profiler.h"
#include "codelowerer.h"
#include "Timer.h"
#include "globalcodemotion.h"
#include "tailduplicator.h"
#include "gcmanagedpointeranalyzer.h"
#include "memoryopt.h"
#include "aliasanalyzer.h"
#include "reassociate.h"
#include "syncopt.h"
#include "simplifytaus.h"
#include "pidgenerator.h"
#include "StaticProfiler.h"
#include "lazyexceptionopt.h"
#include "CompilationContext.h"

namespace Jitrino {

static const char* client_static_path = "ssa,devirt,inline,purge,simplify,uce,dce,dessa,lazyexc,ssa,memopt,simplify,uce,dce,lower,dessa,statprof";
static const char* client_inline_path = "ssa,devirt";

void readOptimizerFlagsFromCommandLine(CompilationContext* compilationContext)
{
    JitrinoParameterTable* params = compilationContext->getThisParameterTable();
    OptimizerFlags& optimizerFlags = *compilationContext->getOptimizerFlags();
	memset( &optimizerFlags, 0, sizeof optimizerFlags );
    optimizerFlags.noopt_path = params->lookup("opt::noopt_path");

    optimizerFlags.skip    = params->lookupBool("opt::skip", true);
    optimizerFlags.static_path = params->lookup("opt::static_path");
    optimizerFlags.inline_path = params->lookup("opt::inline_path");
    //
    // see TranslatorIntfc.cpp for additional info about the "client" option
    //
    if ( params->lookupBool("client", false) ) {
        optimizerFlags.skip = false;
        optimizerFlags.static_path = client_static_path;
        optimizerFlags.inline_path = client_inline_path;
    }
    optimizerFlags.dpgo1_path = params->lookup("opt::dpgo1_path");
    optimizerFlags.dpgo2_path = params->lookup("opt::dpgo2_path");
    
    optimizerFlags.meta_optimize = params->lookupBool("opt::meta_optimize", true);
    optimizerFlags.dump_paths = params->lookup("opt::dump_paths");

    optimizerFlags.skip_phase1 = params->lookupBool("opt::skip_phase1", false);
    optimizerFlags.fast_phase1 = optimizerFlags.skip_phase1 || params->lookupBool("opt::fast_phase1", false);
    optimizerFlags.dumpdot = params->lookupBool("opt::dumpdot", false);

    optimizerFlags.do_ssa = params->lookupBool("opt::do_ssa", false);
    optimizerFlags.build_loops= params->lookupBool("opt::build_loops", true);
    optimizerFlags.globals_span_loops = params->lookupBool("opt::globals_span_loops", true);
    optimizerFlags.do_abcd = params->lookupBool("opt::do_abcd", true);
    optimizerFlags.do_inline = params->lookupBool("opt::do_inline", true);
    optimizerFlags.inline_n = params->lookupUint("opt::inline_n", 0);

    optimizerFlags.do_guarded_devirtualization = params->lookupBool("opt::do_guarded_devirtualization", true);
    optimizerFlags.do_unguard = params->lookupBool("opt::do_unguard", true);
    optimizerFlags.do_peeling = params->lookupBool("opt::do_peeling", true);
    optimizerFlags.elim_cmp3 = params->lookupBool("opt::elim_cmp3", true);
    optimizerFlags.elim_checks = params->lookupBool("opt::elim_checks", true);
	optimizerFlags.do_lower = params->lookupBool("opt::do_lower", true);
    optimizerFlags.use_profile = params->lookupBool("opt::use_profile", false);
    optimizerFlags.do_latesimplify = params->lookupBool("opt::do_latesimplify", true);
    optimizerFlags.use_mulhi = params->lookupBool("opt::use_mulhi", false); 
    optimizerFlags.lower_divconst = params->lookupBool("opt::lower_divconst", true);
    optimizerFlags.do_gcm = params->lookupBool("opt::do_gcm", false);
    optimizerFlags.do_gvn = params->lookupBool("opt::do_gvn", false);
    optimizerFlags.do_tail_duplication = params->lookupBool("opt::do_tail_duplication", true);
    optimizerFlags.do_profile_tail_duplication = params->lookupBool("opt::do_profile_tail_duplication", true);
    optimizerFlags.do_early_profile_tail_duplication = params->lookupBool("opt::do_early_profile_tail_duplication", false);
    optimizerFlags.profile_unguarding_level = params->lookupUint("opt::profile_unguarding_level", 0);
    optimizerFlags.do_profile_redundancy_elimination = params->lookupBool("opt::do_profile_redundancy_elimination", true);
    optimizerFlags.do_prefetching = params->lookupBool("opt::do_prefetching", true);
    optimizerFlags.do_memopt = params->lookupBool("opt::do_memopt", true);
    optimizerFlags.profile_threshold = params->lookupUint("opt::profile_threshold", 5000);
    optimizerFlags.use_average_threshold = params->lookupBool("opt::use_average_threshold", false);
    optimizerFlags.use_minimum_threshold = params->lookupBool("opt::use_minimum_threshold", false);
    optimizerFlags.use_fixed_threshold = params->lookupBool("opt::use_fixed_threshold", false);
    optimizerFlags.prune_untaken_edges = params->lookupBool("opt::prune_untaken_edges", false);
    optimizerFlags.do_lazyexc = params->lookupBool("opt::do_lazyexc", true);

    optimizerFlags.ia32_code_gen = Jitrino::flags.codegen != Jitrino::CG_IPF;

    optimizerFlags.cse_final = params->lookupBool("opt::cse_final", true);
    optimizerFlags.no_simplify = params->lookupBool("opt::no_simplify", false);
    optimizerFlags.no_hvn = params->lookupBool("opt::no_hvn", false);
    optimizerFlags.brm_debug = params->lookupBool("opt::brm_debug", false);
    optimizerFlags.fixup_ssa = params->lookupBool("opt::fixup_ssa", true);
    optimizerFlags.number_dots = params->lookupBool("opt::number_dots", false);
    optimizerFlags.do_sxt = params->lookupBool("opt::do_sxt", true);
    optimizerFlags.do_reassoc = params->lookupBool("opt::do_reassoc", true);
    optimizerFlags.do_reassoc_depth = params->lookupBool("opt::do_reassoc_depth", true);
    optimizerFlags.do_reassoc_depth2 = params->lookupBool("opt::do_reassoc_depth2", true);
    optimizerFlags.do_prof_red2 = params->lookupBool("opt::do_prof_red2", false);
    optimizerFlags.dce2 = params->lookupBool("opt::dce2", true);
    optimizerFlags.do_redstore = params->lookupBool("opt::do_redstore", true);
    optimizerFlags.do_syncopt = params->lookupBool("opt::do_syncopt", true);
    optimizerFlags.keep_empty_nodes_after_syncopt = params->lookupBool("opt::keep_empty_nodes_after_syncopt", false);
    optimizerFlags.do_syncopt2 = params->lookupBool("opt::do_syncopt2", true);
    optimizerFlags.do_prof_red2_latesimplify = params->lookupBool("opt::do_prof_red2_latesimplify", false);
    optimizerFlags.gc_build_var_map = params->lookupBool("opt::gc::build_var_map", true);
    optimizerFlags.reduce_compref = params->lookupBool("opt::reduce_compref", false);
    optimizerFlags.split_ssa = params->lookupBool("opt::split_ssa", true);
    optimizerFlags.better_ssa_fixup 
        = params->lookupBool("opt::better_ssa_fixup", false);
    optimizerFlags.count_ssa_fixup 
        = params->lookupBool("opt::count_ssa_fixup", false);
    optimizerFlags.hvn_exceptions
        = params->lookupBool("opt::hvn_exceptions", true);
    optimizerFlags.hash_init_factor
        = params->lookupUint("opt::hash_init_factor", 1);
    optimizerFlags.hash_resize_factor
        = params->lookupUint("opt::hash_resize_factor", 2);
    optimizerFlags.hash_resize_to
        = params->lookupUint("opt::hash_resize_to", 3);
    optimizerFlags.hash_node_var_factor
        = params->lookupUint("opt::hash_node_var_factor", 1);
    optimizerFlags.hash_node_tmp_factor
        = params->lookupUint("opt::hash_node_tmp_factor", 2);
    optimizerFlags.hash_node_constant_factor
        = params->lookupUint("opt::hash_node_constant_factor", 1);
    optimizerFlags.sink_constants
        = params->lookupBool("opt::sink_constants", true);
    optimizerFlags.sink_constants1
        = params->lookupBool("opt::sink_constants1", false);
    optimizerFlags.gvn_exceptions
        = params->lookupBool("opt::gvn_exceptions", false);
    optimizerFlags.gvn_aggressive
        = params->lookupBool("opt::gvn_aggressive", false);
    optimizerFlags.do_reassoc_compref
        = params->lookupBool("opt::do_reassoc_compref", false);
    optimizerFlags.hvn_constants
        = params->lookupBool("opt::hvn_constants", true);
    optimizerFlags.simplify_taus
        = params->lookupBool("opt::simplify_taus", true);
    optimizerFlags.early_memopt
        = params->lookupBool("opt::early_memopt", true);
    optimizerFlags.early_memopt_prof
        = params->lookupBool("opt::early_memopt::prof", false);
    optimizerFlags.no_peel_inlined
        = params->lookupBool("opt::no_peel_inlined", false);
    optimizerFlags.hvn_inlined
        = params->lookupBool("opt::hvn_inlined", false);
    optimizerFlags.memopt_inlined
        = params->lookupBool("opt::memopt_inlined", false);

    optimizerFlags.type_check
        = params->lookupBool("opt::type_check", false);

    optimizerFlags.use_pattern_table2
        = params->lookupBool("opt::use_pattern_table2", false);
    optimizerFlags.use_fixup_vars
        = params->lookupBool("opt::use_fixup_vars", false);

    optimizerFlags.pass_profile_to_cg
        = params->lookupBool("opt::pass_profile_to_cg", true);

    Abcd::readDefaultFlagsFromCommandLine(params);
    GlobalCodeMotion::readDefaultFlagsFromCommandLine(params);
    MemoryOpt::readDefaultFlagsFromCommandLine(params);
    Reassociate::readDefaultFlagsFromCommandLine(params);
    SyncOpt::readDefaultFlagsFromCommandLine(params);
}

void showOptimizerFlagsFromCommandLine()
{
    Log::out() << "    opt::skip[={on|OFF}] = skip all optimization" << ::std::endl;
    Log::out() << "    opt::dumpdot[={on|OFF}] = dump dotfiles for loops, dominators, etc" << ::std::endl;
    Log::out() << "    opt::do_ssa[={on|OFF}] = do extra SSA optimizations" << ::std::endl;
    Log::out() << "    opt::globals_span_loops[={ON|off}] = ?" << ::std::endl;
    Log::out() << "    opt::do_abcd[={on|OFF}] = run ABCD pass" << ::std::endl;
    Log::out() << "    opt::do_inline[={ON|off}] = do IR level inlining" << ::std::endl;
    Log::out() << "    opt::do_guarded_devirtualization[={ON|off}] = convert some virtual calls into guarded direct calls" << ::std::endl;
    Log::out() << "    opt::do_peeling[={ON|off}] = do loop peeling / inversion" << ::std::endl;
    Log::out() << "    opt::elim_cmp3[={ON|off}] = eliminate cmp3 tests" << ::std::endl;
    Log::out() << "    opt::elim_checks[={ON|off}] = try to eliminate some checks using branch conditions" << ::std::endl;
	Log::out() << "    opt::do_lower[={ON|off}] = lower type checks" << ::std::endl;
    Log::out() << "    opt::do_lazyexc[={ON|off}] = do lazy exception throwing optimization" << ::std::endl;
    Log::out() << "    opt::use_profile[={on|OFF}] = use profile information to guide optimizations" << ::std::endl;
    Log::out() << "    opt::do_latesimplify[={ON|off}] = do late simplification pass to lower mulconst" << ::std::endl;
    Log::out() << "    opt::use_mulhi{ON|off}] = use MulHi opcode" << ::std::endl;
    Log::out() << "    opt::lower_divconst[={ON|off}] = lower div by constant to mul" << ::std::endl;
    Log::out() << "    opt::do_gcm[={on|OFF}] = run global code motion" << ::std::endl;
    Log::out() << "    opt::do_gvn[={on|OFF}] = run global value numbering" << ::std::endl;
    Log::out() << "    opt::do_tail_duplication[={ON|off}] = do tail duplication" << ::std::endl;
    Log::out() << "    opt::do_profile_tail_duplication[={ON|off}] = do profile guided tail duplication" << ::std::endl;
    Log::out() << "    opt::profile_unguarding_level[={0,1,2}] = do profile guided unguarding?" << ::std::endl;
    Log::out() << "    opt::do_profile_redundancy_elimination[={ON|off}] = do profile guided redundancy elimination" << ::std::endl;
    Log::out() << "    opt::do_prefetching[={on|OFF}] = do prefetching pass" << ::std::endl;
    Log::out() << "    opt::cse_final[={ON|off}] = do cse of final fields " << ::std::endl;
    Log::out() << "    opt::no_simplify[={on|OFF}] = suppress simplify " << ::std::endl;
    Log::out() << "    opt::no_hvn[={on|OFF}] = suppress hashvaluenumbering " << ::std::endl;
    Log::out() << "    opt::do_memopt[={ON|off}] = do memory optimization pass" << ::std::endl;
    Log::out() << "    opt::fixup_ssa[={on|OFF}] = fixup SSA form after code deletion" << ::std::endl;
    Log::out() << "    opt::number_dots[={on|OFF}] = use a counter in dot file names to show order" << ::std::endl;
    Log::out() << "    opt::do_sxt[={ON|off}] = do some sign extension elimination" << ::std::endl;
    Log::out() << "    opt::do_reassoc[={ON|off}] = do reassociation before code motion" << ::std::endl;
    Log::out() << "    opt::do_reassoc_depth[={ON|off}] = do depth-reducing reassociation before cgen" << ::std::endl;
    Log::out() << "    opt::do_reassoc_depth2[={ON|off}] = do depth-reducing reassoc before cgen in DPGO" << ::std::endl;
    Log::out() << "    opt::do_reassoc_compref[={on|OFF}] = reassoc reduced compref exprs" << ::std::endl;
    Log::out() << "    opt::do_prof_red2[={on|OFF}] = do redundancy elimination twice for dynopt";
    Log::out() << "    opt::do_prof_red2_latesimplify[={on|OFF}] = lower mults in dynopt pass 1";
    Log::out() << "    opt::dce2[={ON|off}] = use new version of DCE pass";
    Log::out() << "    opt::do_redstore[={ON|off}] = do redstore elim as a separate pass";
    Log::out() << "    opt::do_syncopt[={ON|off}] = do monitorenter/exit opts";
    Log::out() << "    opt::do_syncopt2[={ON|off}] = do extra monitorenter/exit opts in PGO pass";
    Log::out() << "    opt::gc::build_var_map[={ON|off}] = opt builds gc map for vars";
    Log::out() << "    opt::reduce_compref[={on|OFF}] = simplify reference un/compression";
    Log::out() << "    opt::split_ssa[={ON|off}] = rename nonoverlapping SSA var versions";
    Log::out() << "    opt::better_ssa_fixup[={on|OFF}] = defer ssa fixup until graph change";
    Log::out() << "    opt::count_ssa_fixup[={on|OFF}] = count invocations of SSA fixup";
    Log::out() << "    opt::hvn_exceptions[={ON|off}] = do value-numbering on exception paths" << ::std::endl;
    Log::out() << "    opt::hvn_constants[={ON|off}] = value-number constants from equality tests" << ::std::endl;
    Log::out() << "    opt::sink_constants[={ON|off}] = eliminate globals whose values are constant" << ::std::endl;
    Log::out() << "    opt::sink_constants1[={on|OFF}] = make sink_constants more aggressive" << ::std::endl;
    Log::out() << "    opt::gvn_exceptions[={on|OFF}] = apply gvn in exception code" << ::std::endl;
    Log::out() << "    opt::gvn_aggressive[={on|OFF}] = do more aggressive global value numbering" << ::std::endl;
    Log::out() << "    opt::simplify_taus[={ON|off}] = simplify tau expressions in optimizer" << ::std::endl;

    Log::out() << "    opt::early_memopt[={ON|off}] = do some early memory optimizations" << ::std::endl;
    Log::out() << "    opt::early_memopt::prof[={on|OFF}] = do early memory opts with opt::use_profile" << ::std::endl;
    Log::out() << "    opt::no_peel_inlined[={on|OFF}] = do not peel a method while inlining it" << ::std::endl;
    Log::out() << "    opt::hvn_inlined[={on|OFF}] = do some value numbering while inlining" << ::std::endl;
    Log::out() << "    opt::memopt_inlined[={on|OFF}] = do some memory opts while inlining" << ::std::endl;

    Log::out() << "    opt::type_check[={on|OFF}] = type check intermediate results" << ::std::endl;

    Log::out() << "    opt::use_pattern_table2[={on|OFF}] = use alternate pattern table" << ::std::endl;
    Log::out() << "    opt::use_fixup_vars[={on|OFF}] = use pass,fixupVars instead of dessa,pass,fixupvars" << ::std::endl;
    Log::out() << "    opt::pass_profile_to_cg[={ON|off}] = pass profile information to the code generator" << ::std::endl;

    Abcd::showFlagsFromCommandLine();
    GlobalCodeMotion::showFlagsFromCommandLine();
    MemoryOpt::showFlagsFromCommandLine();
    Reassociate::showFlagsFromCommandLine();
}



//
// Base optimizer consists of optimizations that do not require
// a self-contained flow graph.  In the case of inlining, we have flow graphs
// that refer to operands defined in the calling method's flowgraph.
// These optimizations can operate on inlined regions that
// refer to operands/instructions outside the region.
//
class BaseOptimizer {
public:
    BaseOptimizer(MemoryManager& memoryManager, IRManager& irManager0, bool suppressLog) :
        memoryManager(memoryManager), 
        irManager(irManager0),
        flowGraph(irManager0.getFlowGraph()),
        methodDesc(irManager0.getMethodDesc()),
        instFactory(irManager0.getInstFactory()),
        opndManager(irManager0.getOpndManager()),
        typeManager(irManager0.getTypeManager()),
        suppressLog(suppressLog),
        optimizerFlags(*irManager0.getCompilationContext()->getOptimizerFlags())
        {
			stageId=Log::getNextStageId();
            initMap();
        }

    // Dominators and loops
    void computeDominatorsAndLoops();

    // Printing utilities
    void printInstructions(bool condition, char* passName) {
        if(condition) {
            Log::out() << "Printing HIR " << passName << ::std::endl;
            OptPass::printHIR(irManager);
        }
    }

    void info2(const char* message) {
        if(!suppressLog)
            Log::cat_opt()->info2 << message << ::std::endl;
    }

    void run(const ::std::string& passName);
    void runPasses(const ::std::string& passName);

    void addPasses(const ::std::string& passName);
    void runCurrentPasses(const char* label);

    void metaOptimize(::std::string& passes);
    void metaOptimize();
    void findAndReplace(::std::string& str, const ::std::string& find, const ::std::string& replace);
    void trim(::std::string& path) { if(!path.empty() && path[0] == ',') path = path.substr(1); }

    ::std::string& getCurrentPasses();
    void clearCurrentPasses();

    const char* indent() { return OptPass::indent(irManager); }

	uint32 getStageId(){ return stageId; }
protected:
    static void initMap();
    static void registerPass(OptPass* pass);


    MemoryManager&  memoryManager;
    IRManager&      irManager;
    FlowGraph&      flowGraph;
    MethodDesc&     methodDesc;
    InstFactory&    instFactory;
    OpndManager&    opndManager;
    TypeManager&    typeManager;

    bool            suppressLog;
    std::string     passList;
    OptimizerFlags  optimizerFlags;

    typedef ::std::string StdString;
    typedef StlHashMap<StdString, OptPass*> OptMap;
    static MemoryManager mapmm;
    static OptMap optMap;

    struct PatternMapping {
        const char* pattern;
        const char* replacement;
    };
    
	uint32				stageId;

	static PatternMapping patternTable[];
    static PatternMapping patternTable2[];
};

MemoryManager BaseOptimizer::mapmm(1024, "OptimizerMapMemoryManager");

BaseOptimizer::OptMap BaseOptimizer::optMap(BaseOptimizer::mapmm);

DEFINE_OPTPASS(InlinePass)
DEFINE_OPTPASS_IMPL(InlinePass, inline, "Method Inlining")

void
BaseOptimizer::initMap() {
    if(optMap.empty()) {
        registerPass(new (mapmm) ABCDPass());                       // abcd
        registerPass(new (mapmm) CodeLoweringPass());               // lower
        registerPass(new (mapmm) DeadCodeEliminationPass());        // dce
        registerPass(new (mapmm) UnreachableCodeEliminationPass()); // uce
        registerPass(new (mapmm) PurgeEmptyNodesPass());            // purge
        registerPass(new (mapmm) EscapeAnalysisPass());             // escape
        registerPass(new (mapmm) GCManagedPointerAnalysisPass());   // gcmap
        registerPass(new (mapmm) GlobalOperandAnalysisPass());      // markglobals
        registerPass(new (mapmm) GlobalCodeMotionPass());           // gcm
        registerPass(new (mapmm) GlobalValueNumberingPass());       // gvn
        registerPass(new (mapmm) HashValueNumberingPass());         // hvn
        registerPass(new (mapmm) LoopPeelingPass());                // peel
        registerPass(new (mapmm) MemoryValueNumberingPass());       // memopt        
        registerPass(new (mapmm) ReassociationPass());              // reassoc
        registerPass(new (mapmm) DepthReassociationPass());         // reassocdepth
        registerPass(new (mapmm) LateDepthReassociationPass());     // latereassocdepth
        registerPass(new (mapmm) SimplificationPass());             // simplify
        registerPass(new (mapmm) LateSimplificationPass());         // latesimplify
        registerPass(new (mapmm) TauSimplificationPass());          // tausimp
        registerPass(new (mapmm) SyncOptPass());                    // syncopt
        registerPass(new (mapmm) RedundantBranchMergingPass());     // taildup
        registerPass(new (mapmm) HotPathSplittingPass());           // hotpath
        registerPass(new (mapmm) GuardedDevirtualizationPass());    // devirt
        registerPass(new (mapmm) GuardRemovalPass());               // unguard
        registerPass(new (mapmm) InlinePass());                     // inline
        registerPass(new (mapmm) PersistentInstIdGenerationPass()); // pidgen
        registerPass(new (mapmm) SSAPass());                        // ssa
        registerPass(new (mapmm) DeSSAPass());                      // dessa
        registerPass(new (mapmm) SplitSSAPass());                   // splitssa
        registerPass(new (mapmm) FixupVarsPass());                  // fixupvars
        registerPass(new (mapmm) StaticProfilerPass());             // statprof
        registerPass(new (mapmm) LazyExceptionOptPass());           // lazyexc
    }
}

void
BaseOptimizer::registerPass(OptPass* pass) {
	if (Log::cat_opt()->isInfo2Enabled()){
    	Log::cat_opt()->info2 << "Registering [" << pass->getTagName() << "] " << pass->getName() << ::std::endl; 
    }
    optMap[pass->getTagName()] = pass;
}

void
BaseOptimizer::run(const ::std::string& passName) {
    OptMap::iterator iter;
    if((iter = optMap.find(passName)) == optMap.end()) {
        ::std::cerr << "Cannot find optimization [" << passName << "]" << ::std::endl;
        assert(0);
    }else{
        iter->second->run(irManager);
    }
}

void
BaseOptimizer::runPasses(const ::std::string& passName) {
    size_t len = passName.length();
    size_t pos1 = 0;
    size_t pos2 = 0;
    while(pos2 < len && !irManager.getAbort()) {
        pos2 = passName.find(",", pos1);
        run(passName.substr(pos1, (pos2-pos1)));
        pos1 = pos2+1;
    } 
}

void
BaseOptimizer::addPasses(const ::std::string& passes) {
    if(passList.empty())
        passList = passes;
    else
        passList += "," + passes;
}

void
BaseOptimizer::runCurrentPasses(const char* label) {
    metaOptimize();
    Log::cat_opt()->info2 << indent() << "Opt: Run optimization path: " << passList.c_str() << ::std::endl;
    runPasses(passList);
    passList = "";
}

BaseOptimizer::PatternMapping
BaseOptimizer::patternTable[] = {
        { ",dessa,ssa,",                                                    ","      },
        { ",ssa,dessa,",                                                    ","      },
        { ",simplify,uce,dce,hvn,uce,dce,simplify,uce,dce,hvn,uce,dce,",    ",simplify,uce,dce,hvn,uce,dce,"    },
        { ",simplify,uce,dce,simplify,uce,dce,",                            ",simplify,uce,dce,"    },
        { ",uce,dce,uce,dce,",                                              ",uce,dce,",  },
        { NULL,                                                             NULL, }, 
    };
BaseOptimizer::PatternMapping
BaseOptimizer::patternTable2[] = {
        { ",dessa,ssa,",                                    ","      },
        { ",ssa,dessa,",                                    ","      },
        { ",simplify,uce,dce,hvn,uce,dce,simplify,uce,dce,hvn,uce,dce,",    ",simplify,uce,dce,hvn,uce,dce,"    },
        { ",simplify,uce,dce,simplify,uce,dce,",                    ",simplify,uce,dce,"    },
        { ",uce,dce,uce,dce,",                                      ",uce,dce,",  },
        { ",uce,dce,simplify,uce,dce,",                             ",simplify,uce,dce,",  },
        { ",uce,dce,dessa,syncopt,uce,dce,",                        ",uce,dce,dessa,syncopt,",  },
        { NULL,                                               NULL, }, 
    };

void
BaseOptimizer::metaOptimize(::std::string& passes) {
    OptimizerFlags& optimizerFlags = *irManager.getCompilationContext()->getOptimizerFlags();
    passes = std::string(",") + passes + ",";

    // Suppress hash value numbering (cse) if directed
    if(optimizerFlags.no_hvn)
        findAndReplace(passes, ",hvn,", ",");

    // Suppress simplification if directed
    if(optimizerFlags.no_simplify) {
        findAndReplace(passes, ",simplify,uce,dce,", ",");
        findAndReplace(passes, ",latesimplify,uce,dce,", ",");
        findAndReplace(passes, ",simplify,", ",");
        findAndReplace(passes, ",latesimplify,", ",");
    }
    
    if(optimizerFlags.meta_optimize) {
        // Replace the optimization patterns in the pattern table above with the provided replacements
        if (optimizerFlags.use_pattern_table2) {
            for(uint32 i = 0; patternTable2[i].pattern != NULL; ++i) {
                assert(patternTable2[i].replacement);
                findAndReplace(passes, patternTable2[i].pattern, patternTable2[i].replacement);
            }
        } else {
            for(uint32 i = 0; patternTable[i].pattern != NULL; ++i) {
                assert(patternTable[i].replacement);
                findAndReplace(passes, patternTable[i].pattern, patternTable[i].replacement);
            }
        }
    }

    passes = passes.substr(1, passes.length() - 2);
}

void
BaseOptimizer::metaOptimize() {
    metaOptimize(passList);
}

void
BaseOptimizer::findAndReplace(::std::string& str, const ::std::string& find, const ::std::string& replace) {
    for(size_t i = str.find(find); i != ::std::string::npos; i = str.find(find))
        str.replace(i, find.length(), replace);
    
}

::std::string&
BaseOptimizer::getCurrentPasses() {
    return passList;
}

void
BaseOptimizer::clearCurrentPasses() {
    passList = "";
}

void
BaseOptimizer::computeDominatorsAndLoops() {
    OptPass::computeDominatorsAndLoops(irManager);
}

class GlobalOptimizer : public BaseOptimizer {
public:
    GlobalOptimizer(MemoryManager& memoryManager, IRManager& irManager, bool suppressLog=false) :
        BaseOptimizer(memoryManager, irManager, suppressLog) {}


    // Build optimization path
    void buildStaticAggressivePath(CompilationMode mode, ::std::string& path);
    void buildStaticFastPath(::std::string& path);
    void buildProfileGuidedPath(CompilationMode mode, ::std::string& path);
    void buildInlineOptPath(::std::string& path);
    void buildDPGOInlineOptPath(::std::string& path);

    void buildNoOptPath(::std::string& path);
    void buildStaticPath(::std::string& path);
    void buildDPGO1Path(::std::string& path);
    void buildDPGO2Path(::std::string& path);    

    // Privatization passes
    void doPrivatization(::std::string& path);

    // Redundancy elimination passes
    void doRedundancyElimination(::std::string& path);

    // IR lowering
    void doSimplifyTaus(::std::string& path);
    void doIRLowering(::std::string& path);
    void doSyncOpt(::std::string& path);

    
    void doTailDuplication(::std::string& path);
    void doProfileTailDuplication(::std::string& path);

    void doLazyExceptionOpt(::std::string& path);

    // PGO passes
    void generateProfile();
    void annotateProfile();
    void setHeatThreshold();

    // Mode paths
    void doNoOptPath();
    void doStaticPath();
    void doDPGO1Path(bool genProfile=true);
    void doDPGO2Path();
    

    // Dot file printing utilities
    void printDotFile(bool condition, char* subKind) {
        if(condition) {
            char dotName[30]="opt.";
            sprintf(dotName+4, "%d", (int)stageId);
            strcat(dotName, "."); 
            strcat(dotName, subKind);
            OptPass::printDotFile(irManager, dotName);
        }
    }
    void printDominatorTree(bool condition) {
        if(condition) {
			char dotName[30]="opt."; 
			sprintf(dotName+4, "%d", (int)stageId);
            strcat(dotName, "domtree");
            irManager.getDominatorTree()->printDotFile(methodDesc, "domtree");
        }
    }
    void printLoopTree(bool condition) {
        if(condition) {
			char dotName[30]="opt."; 
			sprintf(dotName+4, "%d", (int)stageId); 
			strcat(dotName, "looptree");
            irManager.getLoopTree()->printDotFile(methodDesc, "looptree");
        }
    }

    void dumpPathsToFile();
};

void
GlobalOptimizer::doSimplifyTaus(::std::string& path) {
    if (optimizerFlags.simplify_taus) {
        path += ",tausimp";
    }
}

void
GlobalOptimizer::doIRLowering(::std::string& path) {
    if (optimizerFlags.do_lower) {
        path += ",lower";
    }
}

void
GlobalOptimizer::doSyncOpt(::std::string& path) {
    if (optimizerFlags.do_syncopt) {
        if (optimizerFlags.use_fixup_vars) {
            path += ",uce,dce,syncopt,uce,dce,fixupvars,uce,dce";
        } else {
            path += ",dessa,syncopt,uce,dce,ssa";
        }
    }
}
void
GlobalOptimizer::doTailDuplication(::std::string& path) {
    if(optimizerFlags.do_tail_duplication) {
        path += ",dessa,taildup,ssa,simplify,uce,dce,hvn,uce,dce";
    }
}

void
GlobalOptimizer::doProfileTailDuplication(::std::string& path) {
    if(optimizerFlags.do_profile_tail_duplication || optimizerFlags.do_early_profile_tail_duplication) {
        path += ",dessa,hotpath,ssa,simplify,uce,dce,hvn,uce,dce";
    }
}

void
GlobalOptimizer::doLazyExceptionOpt(::std::string& path) {
    //  disable for IPF until CG support is implemented 
    if (optimizerFlags.do_lazyexc && optimizerFlags.ia32_code_gen) {
        path += ",lazyexc";
    }
}

void
InlinePass::_run(IRManager& irm) {
    computeDominatorsAndLoops(irm);

    // Set up Inliner
    bool connectEarly = irm.getParameterTable().lookupBool("opt::inline::connect_early", true);
    Inliner inliner(irm.getNestedMemoryManager(), irm, inDPGOMode(irm) && getCompilationMode(irm) != CM_DPGO1);
    InlineNode *rootRegionNode = (InlineNode*) inliner.getInlineTree().getRoot();
    inliner.inlineAndProcessRegion(rootRegionNode, irm.getDominatorTree(), irm.getLoopTree());

    // Inline calls
    InlineNode *regionNode = inliner.getNextRegionToInline();
    while(regionNode != NULL) {
        assert(regionNode != rootRegionNode);
        IRManager &regionManager = regionNode->getIRManager();
        GlobalOptimizer regionOptimizer(regionManager.getNestedMemoryManager(), regionManager, 
            false);

        // If in DPGO mode, redo DPGO1 optimizations on inlined regions.
        CompilationMode mode = getCompilationMode(irm);
        

        // Connect region arguments to top-level flowgraph
        if(connectEarly)
            inliner.connectRegion(regionNode);

        // Optimize inlined region before splicing
        {
            PhaseInvertedTimer modeTimer(getModeTimer(mode));
            PhaseInvertedTimer timer(getTimer());            

            regionOptimizer.addPasses(irm.getInlineOptPath());
            regionOptimizer.runCurrentPasses("Inline Passes");
        }

        // Splice into flow graph and find next region.
        regionOptimizer.computeDominatorsAndLoops();
        if(!connectEarly)
            inliner.connectRegion(regionNode);
        inliner.inlineAndProcessRegion(regionNode, regionManager.getDominatorTree(), regionManager.getLoopTree());
        regionNode = inliner.getNextRegionToInline();
    }
    OptimizerFlags& optimizerFlags = *irm.getCompilationContext()->getOptimizerFlags();
    // Print the results to logging / dot file
    if(optimizerFlags.dumpdot)
        inliner.getInlineTree().printDotFile(irm.getMethodDesc(), "inlinetree");
    if(Log::cat_opt_inline()->isInfo2Enabled()) {
            Log::cat_opt_inline()->info2 << indent(irm) << "Opt: Inline Tree" << ::std::endl;
            inliner.getInlineTree().printIndentedTree(Log::out(), "  ");
    }
    Log::cat_opt_inline()->info << "Inline Checksum == " << (int) inliner.getInlineTree().computeCheckSum() << ::std::endl;
}


void
GlobalOptimizer::buildInlineOptPath(::std::string& path) {
    if (optimizerFlags.do_peeling && !optimizerFlags.use_profile
        && !optimizerFlags.no_peel_inlined) {
        
        if (optimizerFlags.memopt_inlined || optimizerFlags.hvn_inlined) {
            path += ",ssa,simplify,uce,dce,hvn,uce,dce";
            if (optimizerFlags.memopt_inlined) {
                path += ",memopt,simplify,uce,dce,hvn,uce,dce";
            }
            path += ",dessa";
        }
        
        path += ",peel";
    }
        
    path += ",ssa";
    
    if ((optimizerFlags.memopt_inlined || optimizerFlags.hvn_inlined) 
        && optimizerFlags.no_peel_inlined) {
        path += ",ssa,simplify,uce,dce,hvn,uce,dce";
        if (optimizerFlags.memopt_inlined) {
            path += ",memopt,simplify,uce,dce,hvn,uce,dce";
        }
        path += ",dessa";
    }

    path += ",simplify,uce,dce";
    
    if(optimizerFlags.do_guarded_devirtualization) {
        path += ",devirt";
    }

    trim(path);
}


void
GlobalOptimizer::buildDPGOInlineOptPath(::std::string& path) {
    if(optimizerFlags.do_early_profile_tail_duplication) {
        if(optimizerFlags.do_tail_duplication)
            path += ",dessa,taildup,ssa,simplify,uce,dce,hvn,uce,dce";
        if(optimizerFlags.do_profile_tail_duplication)
            path += ",dessa,hotpath,ssa,simplify,uce,dce,hvn,uce,dce";
    }
    
    path += ",ssa,simplify,uce,dce";
    
    if(optimizerFlags.do_unguard) {
        path += ",unguard,simplify,uce,dce";
    }

    trim(path);
}

void
GlobalOptimizer::doPrivatization(::std::string& path) {
    path += ",escape";

    if (optimizerFlags.do_memopt) {
        path += ",memopt";
    }

    if (optimizerFlags.do_reassoc) {
        path += ",reassoc,uce,dce,hvn,uce,dce";
    }
}

void
GlobalOptimizer::doRedundancyElimination(::std::string& path) {
    bool needCleanup = false;

    if (optimizerFlags.do_memopt) {
        path += ",uce,dce";
        path += ",memopt";
    }

    if (optimizerFlags.do_reassoc) {
        path += ",reassoc,uce,dce,hvn,uce,dce";
    }

    if (optimizerFlags.do_abcd) {
        // after abcd, cleanup removed conversions and their predecessors
        path += ",abcd,uce,dce";

        needCleanup = true;
    }
    
    if (optimizerFlags.do_gcm) {
        if (optimizerFlags.do_gvn) {
            path += ",gvn";
        }

        path += ",gcm";
        needCleanup = true;
    }

    if (needCleanup) {
        // try some cleanup
        path += ",simplify,uce,dce,hvn,uce,dce";
    }
}

void
GlobalOptimizer::buildStaticFastPath(::std::string& path) {
    if(!optimizerFlags.skip_phase1) {
        path += ",ssa,simplify,uce,dce";
        if(optimizerFlags.do_guarded_devirtualization)
            path += ",devirt,uce,dce";
        if(!optimizerFlags.no_hvn)
            path += ",hvn,uce,dce";
        path += ",dessa";
    }
    path += ",pidgen";
}

void
GlobalOptimizer::buildStaticAggressivePath(CompilationMode mode, ::std::string& path) {
    assert(mode == CM_STATIC || mode == CM_DPGO1);
    path += ",ssa,simplify,uce,dce,hvn,uce,dce";
	if (optimizerFlags.early_memopt && optimizerFlags.do_memopt) {
        path += ",memopt,simplify,uce,dce,hvn,uce,dce";
    }
    path += ",dessa";

    buildInlineOptPath(path);
    if(optimizerFlags.do_inline && mode == CM_STATIC)
        path += ",inline";
    path += ",simplify,uce,dce";
    
    path += ",simplify,uce,dce,hvn,uce,dce";
    doSimplifyTaus(path);
    doLazyExceptionOpt(path);

    doPrivatization(path);


    doIRLowering(path);
    doSyncOpt(path);

    if (optimizerFlags.use_profile 
        && optimizerFlags.do_profile_redundancy_elimination) {

        if (optimizerFlags.do_prof_red2) {
            doRedundancyElimination(path);
            
        }
        if (optimizerFlags.do_prof_red2_latesimplify) {
            path += ",latesimplify,uce,dce,hvn,uce,dce";
        }
    } else { // not in a dynopt run, always do redundancy elim
        doRedundancyElimination(path);
        
        if (optimizerFlags.do_latesimplify) {
            path += ",latesimplify,uce,dce,hvn,uce,dce";
        }
    } 

    if (optimizerFlags.do_reassoc_depth) {
        path += ",reassocdepth,uce,dce,hvn,uce,dce";
    }

    doTailDuplication(path);

    path += ",purge";
    if(optimizerFlags.split_ssa)
        path += ",splitssa";
#ifdef _IPF_
    path += ",gcmap";
#endif

    path += ",dessa";
    path += ",pidgen";
    path += ",statprof";
}


void
GlobalOptimizer::buildProfileGuidedPath(CompilationMode mode, ::std::string& path) {    
    buildDPGOInlineOptPath(path);
    if(optimizerFlags.do_inline)
        path += ",inline";

    path += ",simplify,uce,dce,hvn,uce,dce";
    doSimplifyTaus(path);

    if(optimizerFlags.do_profile_tail_duplication) {
        doTailDuplication(path);
        doProfileTailDuplication(path);
    } else {
        doTailDuplication(path);
    }

    if(optimizerFlags.fast_phase1) {
        doPrivatization(path);
        doIRLowering(path);
    }
    if (optimizerFlags.do_syncopt2) {
        doSyncOpt(path);
    }
    if (optimizerFlags.do_peeling && optimizerFlags.use_profile) {
        path += ",dessa,peel,ssa";
    }        

    path += ",simplify,uce,dce,hvn,uce,dce";

    if(optimizerFlags.do_profile_redundancy_elimination) { 
        doRedundancyElimination(path);

        if (optimizerFlags.do_latesimplify) {
            path += ",latesimplify,uce,dce,hvn,uce,dce";
        }
    }

    if (optimizerFlags.do_reassoc_depth2) {
        path += ",reassocdepth,uce,dce,hvn,uce,dce";
    }

    path += ",purge";
    if(optimizerFlags.split_ssa)
        path += ",splitssa";
#ifdef _IPF_
    path += ",gcmap";
#endif
    path += ",dessa";
}

void
GlobalOptimizer::generateProfile() {
    //
    // Profile generation.
    // Dynamic profiling has higher priority than off-line profiling.
    //
    computeDominatorsAndLoops();
    printDotFile(optimizerFlags.dumpdot, "beforepgen");

    assert(0);
    
    printDotFile(optimizerFlags.dumpdot, "afterpgen");
}

void
GlobalOptimizer::annotateProfile() {
    //
    // Profile feedback.
    // Dynamic profiling has higher priority than off-line profiling.
    //
    computeDominatorsAndLoops();
    CompilationInterface& compileIntf = irManager.getCompilationInterface();
    if ( compileIntf.isDynamicProfiling() ) {
        // For dynamic profiling controlled by the Profile Manager.
        CodeProfiler codeProfiler;
        codeProfiler.getOnlineProfile(irManager);
    } else if ( profileCtrl.useProf() ) {
        // For offline profile generation and feedback as controlled by 
        //  -jitrino PROF="...".
        CodeProfiler codeProfiler;
        codeProfiler.getOfflineProfile(irManager);
    }

    if (flowGraph.hasEdgeProfile()) {
		if (Log::cat_opt()->isInfo2Enabled()){
	        Log::cat_opt()->info2 << "Jitrino: " << methodDesc.getParentType()->getName()
                        << "::" << methodDesc.getName() << "... HAS dynamic profile" << ::std::endl;
        }
        if (profileCtrl.getDebugCtrl() > 0) {
            ::std::cerr << "Jitrino: " << methodDesc.getParentType()->getName()
                 << "::" << methodDesc.getName() << "... HAS dynamic profile" << ::std::endl;
            printInstructions(profileCtrl.getDebugCtrl() > 1, "IR after annotation");
        }
    } else {
		if (Log::cat_opt()->isInfo2Enabled()){
        	Log::cat_opt()->info2 << "Jitrino: " << methodDesc.getParentType()->getName()
                       << "::" << methodDesc.getName() << "... HAS NO dynamic profile" << ::std::endl;
        }
        if (profileCtrl.getDebugCtrl() > 0) {
            ::std::cerr << "Jitrino: " << methodDesc.getParentType()->getName()
                 << "::" << methodDesc.getName() << "... HAS NO dynamic profile" << ::std::endl;
            printInstructions(profileCtrl.getDebugCtrl() > 1, "IR after annotation");
        }
    }
}

void
GlobalOptimizer::setHeatThreshold() {
    double profile_threshold = optimizerFlags.profile_threshold;
    if(optimizerFlags.use_average_threshold) {
        // Keep a running average of method counts.
        static uint32 count = 0;
        static double total = 0.0;
        count++;
        double methodFreq = flowGraph.getEntry()->getFreq();
        assert(methodFreq > 0);
        total += methodFreq;
        irManager.setHeatThreshold(total / count);
    } else if(optimizerFlags.use_minimum_threshold) {
        double methodFreq = flowGraph.getEntry()->getFreq();
        if(methodFreq < profile_threshold)
            methodFreq = profile_threshold;
        irManager.setHeatThreshold(methodFreq);
    } else if(optimizerFlags.use_fixed_threshold) {
        irManager.setHeatThreshold(profile_threshold);
    } else {
        // Use the method entry
        irManager.setHeatThreshold(flowGraph.getEntry()->getFreq());
    }
    Log::cat_opt()->info2 << "Heat threshold = " << irManager.getHeatThreshold() << ::std::endl; 
}

void
GlobalOptimizer::buildNoOptPath(::std::string& path) {
    doIRLowering(path);
    doLazyExceptionOpt(path);
    if (!irManager.getMethodDesc().isClassInitializer()) {
        path+=",statprof";
    }
    trim(path);
}

void
GlobalOptimizer::buildStaticPath(::std::string& path) {
    buildStaticAggressivePath(CM_STATIC, path);
    trim(path);
}

void
GlobalOptimizer::buildDPGO1Path(::std::string& path) {
    if(!optimizerFlags.fast_phase1)
        buildStaticAggressivePath(CM_DPGO1, path);
    else
        buildStaticFastPath(path);
    trim(path);
}

void
GlobalOptimizer::buildDPGO2Path(::std::string& path) {
    buildProfileGuidedPath(CM_DPGO2, path);
    trim(path);
}




void
GlobalOptimizer::doNoOptPath() {
    Log::cat_opt()->info2 << indent() << "Opt: Entering No Optimization Path" << ::std::endl;
    ::std::string path;
    if(optimizerFlags.noopt_path)
        path = optimizerFlags.noopt_path;
    else
        buildNoOptPath(path);
    addPasses(path);
    runCurrentPasses("Passes");
}

void
GlobalOptimizer::doStaticPath() {
    Log::cat_opt()->info2 << indent() << "Opt: Entering Static Optimization Path" << ::std::endl;
    ::std::string inlinePath;
    if(optimizerFlags.inline_path)
        inlinePath = optimizerFlags.inline_path;
    else
        buildInlineOptPath(inlinePath);

    irManager.setInlineOptPath(inlinePath.c_str());

    ::std::string path;
    if(optimizerFlags.static_path)
        path = optimizerFlags.static_path;
    else
        buildStaticPath(path);
    addPasses(path);
    runCurrentPasses("Passes");
}

void
GlobalOptimizer::doDPGO1Path(bool genProfile) {
    Log::cat_opt()->info2 << indent() << "Opt: Entering Profile Collection (DPGO1) Path" << ::std::endl;
    ::std::string path;
    if(optimizerFlags.dpgo1_path)
        path = optimizerFlags.dpgo1_path;
    else
        buildDPGO1Path(path);
    addPasses(path);
    runCurrentPasses("Passes");

    if(genProfile)
        generateProfile();
}

void
GlobalOptimizer::doDPGO2Path() {
    //
    // Repeat DPGO1 without generating profile
    //
    doDPGO1Path(false);
    annotateProfile();
    setHeatThreshold();

    //
    // Do profile guided optimization
    //
    Log::cat_opt()->info2 << indent() << "Opt: Entering Profile Guided Optimization (DPGO2) Path" << ::std::endl;
    double methodCount = Inliner::getProfileMethodCount(irManager.getCompilationInterface(), methodDesc); 
	if((!optimizerFlags.check_profile_threshold || 
        methodCount >= optimizerFlags.profile_threshold) && flowGraph.hasEdgeProfile()) {
        Log::cat_opt()->info << "Jitrino: reoptimize " << methodDesc.getParentType()->getName()
            << "::" << methodDesc.getName();
        Log::cat_opt()->info2 << "... methodCount = " << methodCount;
        Log::cat_opt()->info << ::std::endl;
                
        ::std::string inlinePath;
        if(optimizerFlags.inline_path)
            inlinePath = optimizerFlags.inline_path;
        else
            buildDPGOInlineOptPath(inlinePath);
        irManager.setInlineOptPath(inlinePath.c_str());

        ::std::string path;
        if(optimizerFlags.dpgo2_path)
            path = optimizerFlags.dpgo2_path;
        else
            buildDPGO2Path(path);
        addPasses(path);
        runCurrentPasses("Passes");
    }   
    else {
        Log::cat_opt()->info << "Jitrino: do not reoptimize " << methodDesc.getParentType()->getName()
            << "::" << methodDesc.getName() << "... methodCount = " << methodCount << ::std::endl;
    }
}



void
GlobalOptimizer::dumpPathsToFile() {
    static bool dumped = false;
    if(!dumped && optimizerFlags.dump_paths != NULL) {
        ::std::ofstream out(optimizerFlags.dump_paths);
        if (!out) {
            ::std::cerr << "Unable to open " << optimizerFlags.dump_paths << " for path output.\n";
            exit(1);
        };

        ::std::string path;

        out << "#\n# NOOPT PATH\n#\n";
        out << "opt::noopt_path=\"lower\"\n";

        if(!OptPass::inDPGOMode(irManager)) {
            out << "#\n# STATIC PATH\n#\n";
            buildStaticPath(path);
            metaOptimize(path);
            out << "opt::static_path=\"" << path.c_str() << "\"\n";
            path = "";

            out << "#\n# INLINE PATH\n#\n";
            buildInlineOptPath(path);
            metaOptimize(path);
            out << "opt::inline_path=\"" << path.c_str() << "\"\n";
            path = "";        
        } else {
            out << "#\n# DPGO1 PATH\n#\n";
            buildDPGO1Path(path);
            metaOptimize(path);
            out << "opt::dpgo1_path=\"" << path.c_str() << "\"\n";
            path = "";

            out << "#\n# DPGO2 PATH\n#\n";
            buildDPGO2Path(path);
            metaOptimize(path);
            out << "opt::dpgo2_path=\"" << path.c_str() << "\"\n";
            path = "";
            out << "#\n# INLINE PATH\n#\n";
            buildDPGOInlineOptPath(path);
            metaOptimize(path);
            out << "opt::inline_path=\"" << path.c_str() << "\"\n";
            path = "";
        }
        dumped = true;
    }
}


//
// entry point to the optimizer
//
bool
optimize(IRManager& irManager) {
    static Timer* timer = NULL;
    PhaseTimer phaseTimer(timer, "opt::all");            
    CompilationInterface& compileIntf = irManager.getCompilationInterface();
    MethodDesc&     methodDesc  = irManager.getMethodDesc();
    OptimizerFlags& optimizerFlags = *irManager.getCompilationContext()->getOptimizerFlags();

    optimizerFlags.use_profile = OptPass::inDPGOMode(irManager);
    if (optimizerFlags.use_profile) {
        optimizerFlags.early_memopt = optimizerFlags.early_memopt_prof;
    }
    optimizerFlags.check_profile_threshold = irManager.getParameterTable().lookupBool("opt::check_profile_threshold", !compileIntf.isDynamicProfiling());

    MemoryManager& memManager = irManager.getMemoryManager();
    GlobalOptimizer optimizer(memManager, irManager);

	if (Log::cat_opt()->isInfo2Enabled()){
	    Log::cat_opt()->info2 << "Jitrino: optimize " << methodDesc.getParentType()->getName()
    	    << "::" << methodDesc.getName() << "..." << ::std::endl;
    }

	uint32 stageId=optimizer.getStageId();

	if (Log::cat_opt()->isIREnabled()){
		Log::printStageBegin(stageId, "HLO", "High Level Optimizations", "opt");

		Log::printIRDumpBegin(stageId, "High Level Optimizations", "before");
		optimizer.printInstructions(Log::cat_opt()->isIREnabled(), "before optimization");
		Log::printIRDumpEnd(stageId, "High Level Optimizations", "before");

		optimizer.printDotFile(optimizerFlags.dumpdot, "before");
		optimizer.dumpPathsToFile();
	}


    CompilationMode mode = OptPass::getCompilationMode(irManager);

    switch(mode) {
    case CM_NO_OPT: 
        {
            static Timer* timer = NULL;
            PhaseTimer phaseTimer(timer, "opt::noopt");
            optimizer.doNoOptPath();
        }
        break;
    case CM_STATIC:
        {
            static Timer* timer = NULL;
            PhaseTimer phaseTimer(timer, "opt::static");
            optimizer.doStaticPath();
        }
        break;
    case CM_DPGO1:
        {
            static Timer* timer = NULL;
            PhaseTimer phaseTimer(timer, "opt::dpgo1");
            optimizer.doDPGO1Path();
        }
        break;
    case CM_DPGO2:
        {
            static Timer* timer = NULL;
            PhaseTimer phaseTimer(timer, "opt::dpgo2");
            optimizer.doDPGO2Path();
        }
        break;    
    default:
        assert(0);
    }

    //
    // Prepare for code generation.
    //
    optimizer.computeDominatorsAndLoops();
    optimizer.run("markglobals");

	if (Log::cat_opt()->isIREnabled()){
		optimizer.printDominatorTree(optimizerFlags.dumpdot);
		optimizer.printLoopTree(optimizerFlags.dumpdot);

		Log::printIRDumpBegin(stageId, "High Level Optimizations", "after");
		optimizer.printInstructions(Log::cat_opt()->isIREnabled(), "IR after Optimization");
		Log::printIRDumpEnd(stageId, "High Level Optimizations", "after");

		optimizer.printDotFile(optimizerFlags.dumpdot, "after");

		Log::printStageEnd(stageId, "HLO", "High Level Optimizations", "opt");
	}

    assert(irManager.getAbort() == false);
    return true;
}


} //namespace Jitrino 
