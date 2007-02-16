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
#include "ver_subroutine.h"

static vf_Result
MarkTopcode(vf_ContextHandle context, vf_NodeHandle nh) {
    return VER_OK;
} // MarkTopcode

static vf_Result
InlineMarkedSubNodes(vf_ContextHandle context) {
    return VER_OK;
} // InlineMarkedSubNodes

vf_Result
vf_inline_subroutines(vf_ContextHandle context) {

    vf_Result r = MarkTopcode(context, context->m_graph->GetFirstNode());
    if (VER_OK != r) {
        return r;
    }

    r = InlineMarkedSubNodes(context);
    return r;
} // InlineSubroutines
