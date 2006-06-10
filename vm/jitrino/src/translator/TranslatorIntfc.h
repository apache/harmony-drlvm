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
 * @version $Revision: 1.13.16.4 $
 *
 */

#ifndef _TRANSLATOR_INTFC_H_
#define _TRANSLATOR_INTFC_H_

#include "open/types.h"
#include <string.h>
#include <assert.h>

namespace Jitrino {

class MethodDesc;
class IRBuilder;
class InstFactory;
class CompilationInterface;
class FlowGraph;
class Opnd;
class Inst;
class CFGNode;
class ExceptionInfo;
class IRManager;
class MemoryManager;
class JitrinoParameterTable;
class Method_Table;
class CompilationContext;

    // to select which byte code translator optimizations are done
struct TranslatorFlags {
        bool propValues       : 1;    // do value propagation
        bool inlineMethods    : 1;    // do method inlining
        bool guardedInlining  : 1;    // a step further for inlining
        bool genCharArrayCopy : 1;    // generate intrinsic calls to CharArrayCopy
        bool onlyBalancedSync : 1;    // treat all method synchronization as balanced
        bool ignoreSync       : 1;    // do not generate monitor enter/exit instructions
        bool syncAsEnterFence : 1;    // implement monitor enter as enter fence and
        bool newCatchHandling : 1;    // use fix for catch handler ordering problem
        bool magicMinMaxAbs   : 1;    // recognize, e.g., java/lang/Math.min/max/abs
        bool genMinMaxAbs     : 1;    // gen min/max/abs opcodes instead of using select
        bool genFMinMaxAbs    : 1;    // gen min/max/abs opcodes for floats
        bool optArrayInit     : 1;    // skip array initializers from optimizations
        Method_Table* inlineSkipTable; // do not inline these methods
        char *magicClass;             //  Name of the MagicClass. NULL if no class specified.
    };

class TranslatorIntfc {
public:

    virtual ~TranslatorIntfc() {}
    
    // all TranslatorIntfc::flags fields are initialized by readTranslatorCommandLineParams()
    static void readFlagsFromCommandLine(CompilationContext* cs, bool ia32Cg);
    static void showFlagsFromCommandLine();

    virtual void translateMethod(CompilationInterface&, MethodDesc&, IRBuilder&) = 0;
    
    // this is the regular routine to be used to generate IR for a method
    static void translateByteCodes(IRManager&);

    // for inlining
    static void translateByteCodesInline(IRManager& irManager,
                                         uint32 numArgs, Opnd** argOpnds, uint32 inlineDepth);

    // this routine is used for IR inlining and generating IR for a particular method
    // FG is obtained from irBuilder
    static void generateMethodIRForInlining(
                                            CompilationInterface& compilationInterface,
                                            MethodDesc& methodDesc,
                                            IRBuilder&        irBuilder,
                                            uint32            numActualArgs,
                                            Opnd**            actualArgs,
                                            Opnd**            returnOpnd,
                                            CFGNode**         returnNode,
                                            Inst*             inlineSite,
                                            uint32 inlineDepth);
};

class OpndStack {
public:
    OpndStack(MemoryManager& memManager,uint32 slots);
    bool isEmpty() {
        return tos == 0;
    }
    bool isFull() {
        return tos == maxSlots;
    }
    uint32 getNumElems() {
        return tos;
    }
    Opnd* top() {
        if (isEmpty()) {
            assert(0);
            return NULL;
        }
        return opnds[tos-1];
    }
    Opnd* pop() {
        if (isEmpty()) {
            assert(0);
            return NULL;
        }
        return opnds[--tos];
    }
    bool push(Opnd* opnd) {
        if (isFull()) {
            assert(0);
            return false;
        }
        opnds[tos++] = opnd;
        return true;
    }
    Opnd* getElem(uint32 i) {
        if (i >= tos) {
            assert(0);
            return NULL;
        }
        return opnds[i];
    }
    void makeEmpty() {
        tos=0;
    }
private:
    //
    // private fields
    //
    const uint32 maxSlots;
    uint32 tos;
    Opnd**    opnds;
};


//
// utility methods added to allow refactoring of Opnd.h
//
extern bool 
isNonNullOpnd(Opnd* opnd);

extern bool 
isExactTypeOpnd(Opnd* opnd);

extern bool
isStackOpndAliveOpnd(Opnd* opnd);

extern bool
isStackOpndSavedOpnd(Opnd* opnd);

extern void
setNonNullOpnd(Opnd* opnd,bool val);

extern void
setExactTypeOpnd(Opnd* opnd,bool val);

extern void
setStackOpndAliveOpnd(Opnd* opnd,bool val);

extern void
setStackOpndSavedOpnd(Opnd* opnd,bool val);

extern void 
IRinline(CompilationInterface&      compilationInterface,
         IRBuilder&                 irBuilder,
         InstFactory&               instFactory,
         FlowGraph*                 fg);

} //namespace Jitrino 

#endif // _TRANSLATOR_INTFC_H_
