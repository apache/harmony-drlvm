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
 * @version $Revision: 1.15.16.4 $
 *
 */

#ifndef _OPTIMIZER_H_
#define _OPTIMIZER_H_

#include "TranslatorIntfc.h"

namespace Jitrino {

class CompilationInterface;
class CompilationContext;
class IRManager;

bool optimize(IRManager& irManager); // returns success or failure

class JitrinoParameterTable;

void readOptimizerFlagsFromCommandLine(CompilationContext* compilationContext);
void showOptimizerFlagsFromCommandLine();

struct OptimizerFlags {
    const char* noopt_path;
    const char* static_path;
    const char* dpgo1_path;
    const char* dpgo2_path;
    
    const char* inline_path;
    bool meta_optimize;
    const char* dump_paths;

    bool skip;
    bool fast_phase1;
    bool skip_phase1;
    bool dumpdot;
    bool build_loops;
    bool do_ssa;
    bool globals_span_loops;
    bool do_abcd;
    bool do_inline;
    uint32 inline_n;
    bool do_guarded_devirtualization;
    bool do_unguard;
    bool do_peeling;
    bool elim_cmp3;
    bool elim_checks;
    bool do_lower;
    bool use_profile;
    bool do_latesimplify;
    bool use_mulhi;
    bool lower_divconst;
    bool do_gcm;
    bool do_gvn;
    bool ia32_code_gen;
    bool cse_final;
    bool no_simplify;
    bool no_hvn;
    bool do_tail_duplication;
    bool do_profile_tail_duplication;
    bool do_early_profile_tail_duplication;
    bool prune_untaken_edges;
    uint32 profile_unguarding_level;
    uint32 profile_threshold;
    bool check_profile_threshold;
    bool use_average_threshold;
    bool use_minimum_threshold;
    bool use_fixed_threshold;
    bool do_profile_redundancy_elimination;
    bool do_prefetching;
    bool do_memopt;
    bool brm_debug;
    bool fixup_ssa;
    bool number_dots;
    bool do_sxt;
    bool do_reassoc;
    bool do_reassoc_depth;
    bool do_reassoc_depth2;
    bool do_prof_red2;
    bool dce2;
    bool do_redstore;
    bool do_syncopt;
    bool keep_empty_nodes_after_syncopt;
    bool do_syncopt2;
    bool do_prof_red2_latesimplify;
    bool gc_build_var_map;
    bool reduce_compref;
    bool split_ssa;
    bool better_ssa_fixup;
    bool count_ssa_fixup;
    bool hvn_exceptions;
    bool sink_constants;
    bool sink_constants1;
    bool gvn_exceptions;
    bool gvn_aggressive;
    bool do_reassoc_compref;
    uint32 hash_init_factor;
    uint32 hash_resize_factor;
    uint32 hash_resize_to;
    uint32 hash_node_var_factor;
    uint32 hash_node_tmp_factor;
    uint32 hash_node_constant_factor;
    bool hvn_constants;
    bool simplify_taus;
    bool early_memopt;
    bool early_memopt_prof;
    bool no_peel_inlined;
    bool hvn_inlined;
    bool memopt_inlined;
    bool type_check;
    bool use_pattern_table2;
    bool use_fixup_vars;
    bool pass_profile_to_cg;
    bool do_lazyexc;
};

} //namespace Jitrino 

#endif // _OPTIMIZER_H_
