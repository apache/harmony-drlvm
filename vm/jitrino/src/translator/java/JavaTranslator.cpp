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
 * @version $Revision: 1.18.8.1.4.4 $
 *
 */

#include <assert.h>

#include "open/types.h"
#include "MemoryManager.h"
#include "VMInterface.h"
#include "JavaTranslator.h"
#include "ByteCodeParser.h"
#include "JavaByteCodeTranslator.h"
#include "MemoryEstimates.h"
#include "Log.h"

namespace Jitrino {

void
JavaTranslator::translateMethod(CompilationInterface& compilationInterface,
                                MethodDesc& methodDesc,
                                IRBuilder& irBuilder) {
    uint32 byteCodeSize = methodDesc.getByteCodeSize();
    const unsigned char* byteCodes = methodDesc.getByteCodes();
    MemoryManager    
        translatorMemManager(byteCodeSize*ESTIMATED_TRANSLATOR_MEMORY_PER_BYTECODE,
                             "JavaTranslator::translateMethod.translatorMemManager");

    JavaFlowGraphBuilder cfgBuilder(irBuilder.getInstFactory().getMemManager(),irBuilder);

    ByteCodeParser parser((const uint8*)byteCodes,byteCodeSize);
    // generate code
    JavaByteCodeTranslator translator(compilationInterface,
                              translatorMemManager,
                              irBuilder,
                              parser,
                              methodDesc, 
                              irBuilder.getTypeManager(),
                              cfgBuilder);
							  // isInlined
    parser.parse(&translator);
    cfgBuilder.build();
}


//
// version for translation-level inlining
//
Opnd*
JavaCompileMethodInline(CompilationInterface& compilationInterface,
                        MemoryManager& translatorMemManager,
                        MethodDesc& methodDesc,
                        IRBuilder&        irBuilder,
                        uint32            numActualArgs,
                        Opnd**            actualArgs,
                        JavaFlowGraphBuilder&  cfgBuilder, 
                        uint32 inlineDepth,
                        InlineInfoBuilder* parentInlineInfoBuilder,
                        JsrEntryInstToRetInstMap* parentJsrEntryMap)
{
    uint32 byteCodeSize = methodDesc.getByteCodeSize();
    const unsigned char* byteCodes = methodDesc.getByteCodes();


    ByteCodeParser parser((const uint8*)byteCodes,byteCodeSize);
    // generate code
    JavaByteCodeTranslator translator(compilationInterface,
                              translatorMemManager,
                              irBuilder,
                              parser,
                              methodDesc,
                              irBuilder.getTypeManager(),
                              cfgBuilder,
                              numActualArgs,actualArgs,NULL,NULL,
                              (ExceptionInfo*)irBuilder.getCurrentLabel()->getState(),
                              inlineDepth,false /* startNewBlock */,
                              parentInlineInfoBuilder,
                              parentJsrEntryMap);  // isInlined=true for this c-tor
    parser.parse(&translator);
    return translator.getResultOpnd();
}

// version for IR inlining
void
JavaTranslateMethodForIRInlining(
                            CompilationInterface& compilationInterface,
                            MethodDesc& methodDesc,
                            IRBuilder&        irBuilder,
                            uint32            numActualArgs,
                            Opnd**            actualArgs,
                            Opnd**            returnOpnd,
                            CFGNode**         returnNode,
                            Inst*             inlineSite,
                            uint32 inlineDepth) {
    uint32 byteCodeSize = methodDesc.getByteCodeSize();
    const unsigned char* byteCodes = methodDesc.getByteCodes();
    MemoryManager
        translatorMemManager(byteCodeSize*ESTIMATED_TRANSLATOR_MEMORY_PER_BYTECODE,
                             "JavaTranslator::translateMethod.translatorMemManager");


    JavaFlowGraphBuilder cfgBuilder(irBuilder.getInstFactory().getMemManager(),
                                    irBuilder);

    ByteCodeParser parser((const uint8*)byteCodes,byteCodeSize);
    // generate code
    ExceptionInfo *exceptionInfo = NULL; 
    JavaByteCodeTranslator translator(compilationInterface,
                              translatorMemManager,
                              irBuilder,
                              parser,
                              methodDesc,
                              irBuilder.getTypeManager(),
                              cfgBuilder,
                              numActualArgs,actualArgs,
                              returnOpnd,returnNode,
                              exceptionInfo,
                              inlineDepth,true /* startNewBlock */,
                              NULL /* parentInlineInfoBuilder */,
                              NULL /* parentJsrEntryMap */);  // isInlined=true for this c-tor

    translator.setNoInlineInfoBuilder();

    parser.parse(&translator);
    cfgBuilder.build();
    if(Log::cat_opt()->isDebugEnabled()) {
        FlowGraph &ojo = irBuilder.getFlowGraph();
        Log::out() << "INLINED\n";
        ojo.printInsts(Log::out(),methodDesc);
    }
}


} //namespace Jitrino 
