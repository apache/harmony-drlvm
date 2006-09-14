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
 * @version $Revision: 1.12.8.1.4.4 $
 *
 */

#ifndef _JAVATRANSLATOR_H_
#define _JAVATRANSLATOR_H_

#include "TranslatorIntfc.h"
#include "Stl.h"

namespace Jitrino {
class Opnd;
class JavaFlowGraphBuilder;
class Inst;
class InlineInfoBuilder;

typedef StlMultiMap<Inst*, Inst*> JsrEntryInstToRetInstMap;
typedef std::pair<JsrEntryInstToRetInstMap::const_iterator, 
        JsrEntryInstToRetInstMap::const_iterator> JsrEntryCIterRange;

//
// version for translation-level inlining
//
extern Opnd* JavaCompileMethodInline(
                                CompilationInterface& compilationInterface,
                                MemoryManager& translatorMemManager,
                                MethodDesc& methodDesc,
                                IRBuilder& irBuilder,
                                uint32 numActualArgs,
                                Opnd** actualArgs,
                                JavaFlowGraphBuilder& cfgBuilder, uint32 inlineDepth,
                                InlineInfoBuilder* parentInlineInfoBuilder,
                                JsrEntryInstToRetInstMap* parentJsrEntryMap);


class JavaTranslator {
public:
    
    // translates into the IRBuilder object's flow graph.
    static void translateMethod(CompilationInterface& ci, MethodDesc& methodDesc, IRBuilder& irBuilder);
};

} //namespace Jitrino 

#endif // _JAVATRANSLATOR_H_
