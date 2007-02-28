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
/**
 * @author Intel, Nikolay A. Sidelnikov
 * @version $Revision: 1.8.14.2.4.4 $
 */

#include "Ia32IRManager.h"
#include "Ia32RuntimeInterface.h"
#include "Ia32StackInfo.h"
#include "Ia32GCMap.h"
#include "Ia32BCMap.h"
#include "CGSupport.h"

namespace Jitrino
{
namespace Ia32{

void RuntimeInterface::unwindStack(MethodDesc* methodDesc, JitFrameContext* context, bool isFirst) {
    StackInfo stackInfo;
#ifdef _EM64T_
    stackInfo.read(methodDesc, *context->p_rip, isFirst);
#else
    stackInfo.read(methodDesc, *context->p_eip, isFirst);
#endif
    stackInfo.unwind(methodDesc, context, isFirst);
}

void* RuntimeInterface::getAddressOfThis(MethodDesc * methodDesc, const JitFrameContext* context, bool isFirst) {
    assert(!methodDesc->isStatic());
    if (!methodDesc->isSynchronized() &&  !methodDesc->isMethodClassIsLikelyExceptionType()) {
        static const uint64 default_this=0;
        return (void*)&default_this;
    }
    assert(context);
    StackInfo stackInfo;
#ifdef _EM64T_
    stackInfo.read(methodDesc, *context->p_rip, isFirst);
    assert(isFirst || (POINTER_SIZE_INT)context->p_rip+8 == context->rsp);
    return (void *)(context->rsp + stackInfo.getStackDepth() + (int)stackInfo.getOffsetOfThis());
#else
    stackInfo.read(methodDesc, *context->p_eip, isFirst);
    assert(isFirst || (uint32)context->p_eip+4 == context->esp);
    return (void *)(context->esp + stackInfo.getStackDepth() + stackInfo.getOffsetOfThis());
#endif
}

void  RuntimeInterface::fixHandlerContext(MethodDesc* methodDesc, JitFrameContext* context, bool isFirst)
{
    StackInfo stackInfo;
#ifdef _EM64T_
    stackInfo.read(methodDesc, *context->p_rip, isFirst);
#else
    stackInfo.read(methodDesc, *context->p_eip, isFirst);
#endif
    stackInfo.fixHandlerContext(context);
}

bool RuntimeInterface::getBcLocationForNative(MethodDesc* method, uint64 native_pc, uint16 *bc_pc)
{
    StackInfo stackInfo;

    Byte* infoBlock = method->getInfoBlock();
    POINTER_SIZE_INT stackInfoSize = stackInfo.readByteSize(infoBlock);
    POINTER_SIZE_INT gcMapSize = GCMap::readByteSize(infoBlock + stackInfoSize);

    const char* methName;

    uint64 bcOffset = BcMap::get_bc_location_for_native(native_pc, infoBlock + stackInfoSize + gcMapSize);
    if (bcOffset != ILLEGAL_VALUE) {
        *bc_pc = (uint16)bcOffset;
        return true;
    } else if (Log::isLogEnabled(LogStream::RT)) {
        methName = method->getName();
        Log::log(LogStream::RT) << "Native code for method: " << methName << " BC = " << bc_pc 
                << " not found " << std::endl;
    }
    return false;
}
bool RuntimeInterface::getNativeLocationForBc(MethodDesc* method, uint16 bc_pc, uint64 *native_pc) {
    StackInfo stackInfo;

    Byte* infoBlock = method->getInfoBlock();
    POINTER_SIZE_INT stackInfoSize = stackInfo.readByteSize(infoBlock);
    POINTER_SIZE_INT gcMapSize = GCMap::readByteSize(infoBlock + stackInfoSize);

    const char* methName;

    uint64 ncAddr = BcMap::get_native_location_for_bc(bc_pc, infoBlock + stackInfoSize + gcMapSize);
    if (ncAddr != ILLEGAL_VALUE) {
        *native_pc =  ncAddr;
        return true;
    } else if (Log::isLogEnabled(LogStream::RT)) {
        methName = method->getName();
        Log::log(LogStream::RT) << "Native code for method: " << methName << " BC = " << bc_pc 
                << " not found " << std::endl;
    }
    return false;
}

uint32  RuntimeInterface::getInlineDepth(InlineInfoPtr ptr, uint32 offset) {
    return InlineInfoMap::get_inline_depth(ptr, offset);
}

Method_Handle   RuntimeInterface::getInlinedMethod(InlineInfoPtr ptr, uint32 offset, uint32 inline_depth) {
    return InlineInfoMap::get_inlined_method(ptr, offset, inline_depth);
}

uint16 RuntimeInterface::getInlinedBc(InlineInfoPtr ptr, uint32 offset, uint32 inline_depth) {
    return InlineInfoMap::get_inlined_bc(ptr, offset, inline_depth);
}


}}; //namespace Ia32

