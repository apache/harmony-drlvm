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
 * @author Intel, George A. Timoshenko
 * @version $Revision: 1.19.8.2.4.4 $
 *
 */

#ifndef _JAVABYTECODETRANSLATOR_H_
#define _JAVABYTECODETRANSLATOR_H_

#include "JavaTranslator.h"
#include "IRBuilder.h"
#include "JavaByteCodeParser.h"
#include "JavaLabelPrepass.h"
#include "JavaFlowGraphBuilder.h"

#include "VMInterface.h"

namespace Jitrino {

class Opnd;

class JavaByteCodeTranslator : public JavaByteCodeParserCallback {
public:
    virtual ~JavaByteCodeTranslator() {
    }

    // version for non-inlined methods
    JavaByteCodeTranslator(CompilationInterface& compilationInterface,
                    MemoryManager&,
                    IRBuilder&,
                    ByteCodeParser&,
                    MethodDesc& methodToCompile,
                    TypeManager& typeManager,
                    JavaFlowGraphBuilder& cfg);
    
    

    void offset(uint32 offset);
    void offset_done(uint32 offset);
    void checkStack();
    // called before parsing starts
    virtual void parseInit() {}
    // called after parsing ends, but not if an error occurs
    virtual void parseDone();
    // called when an error occurs during the byte code parsing
    void parseError();
    Opnd* getResultOpnd() {    // for inlined methods only
        return resultOpnd;
    }
    // generates the argument loading code
    void genArgLoads();
    // generated argument loading for inlined methods
    void genArgLoads(uint32 numActualArgs,Opnd** actualArgs);

    void nop();
    void aconst_null();
    void iconst(int32 val);
    void lconst(int64 val);
    void fconst(float val);
    void dconst(double val);
    void bipush(int8 val);
    void sipush(int16 val);
    void ldc(uint32 constPoolIndex);
    void ldc2(uint32 constPoolIndex);
    void iload(uint16 varIndex);
    void lload(uint16 varIndex);
    void fload(uint16 varIndex);
    void dload(uint16 varIndex);
    void aload(uint16 varIndex);
    void iaload();
    void laload();
    void faload();
    void daload();
    void aaload();
    void baload();
    void caload();
    void saload();
    void istore(uint16 varIndex,uint32 off);
    void lstore(uint16 varIndex,uint32 off);
    void fstore(uint16 varIndex,uint32 off);
    void dstore(uint16 varIndex,uint32 off);
    void astore(uint16 varIndex,uint32 off);
    void iastore();
    void lastore();
    void fastore();
    void dastore();
    void aastore();
    void bastore();
    void castore();
    void sastore();
    void pop();
    void pop2();
    void dup();
    void dup_x1();
    void dup_x2();
    void dup2();
    void dup2_x1();
    void dup2_x2();
    void swap();
    void iadd();
    void ladd();
    void fadd();
    void dadd();
    void isub();
    void lsub();
    void fsub();
    void dsub();
    void imul();
    void lmul();
    void fmul();
    void dmul();
    void idiv();
    void ldiv();
    void fdiv();
    void ddiv();
    void irem();
    void lrem();
    void frem();
    void drem();
    void ineg();
    void lneg();
    void fneg();
    void dneg();
    void ishl();
    void lshl();
    void ishr();
    void lshr();
    void iushr();
    void lushr();
    void iand();
    void land();
    void ior();
    void lor();
    void ixor();
    void lxor();
    void iinc(uint16 varIndex,int32 amount);
    void i2l();
    void i2f();
    void i2d();
    void l2i();
    void l2f();
    void l2d();
    void f2i();
    void f2l();
    void f2d();
    void d2i();
    void d2l();
    void d2f();
    void i2b();
    void i2c();
    void i2s();
    void lcmp();
    void fcmpl();
    void fcmpg();
    void dcmpl();
    void dcmpg();
    void ifeq(uint32 targetOffset,uint32 nextOffset);
    void ifne(uint32 targetOffset,uint32 nextOffset);
    void iflt(uint32 targetOffset,uint32 nextOffset);
    void ifge(uint32 targetOffset,uint32 nextOffset);
    void ifgt(uint32 targetOffset,uint32 nextOffset);
    void ifle(uint32 targetOffset,uint32 nextOffset);
    void if_icmpeq(uint32 targetOffset,uint32 nextOffset);
    void if_icmpne(uint32 targetOffset,uint32 nextOffset);
    void if_icmplt(uint32 targetOffset,uint32 nextOffset);
    void if_icmpge(uint32 targetOffset,uint32 nextOffset);
    void if_icmpgt(uint32 targetOffset,uint32 nextOffset);
    void if_icmple(uint32 targetOffset,uint32 nextOffset);
    void if_acmpeq(uint32 targetOffset,uint32 nextOffset);
    void if_acmpne(uint32 targetOffset,uint32 nextOffset);
    void goto_(uint32 targetOffset,uint32 nextOffset);
    void jsr(uint32 offset, uint32 nextOffset);
    void ret(uint16 varIndex);
    void tableswitch(JavaSwitchTargetsIter*);
    void lookupswitch(JavaLookupSwitchTargetsIter*);
    void ireturn(uint32 off);
    void lreturn(uint32 off);
    void freturn(uint32 off);
    void dreturn(uint32 off);
    void areturn(uint32 off);
    void return_(uint32 off);
    void getstatic(uint32 constPoolIndex);
    void putstatic(uint32 constPoolIndex);
    void getfield(uint32 constPoolIndex);
    void putfield(uint32 constPoolIndex);
    void invokevirtual(uint32 constPoolIndex);
    void invokespecial(uint32 constPoolIndex);
    void invokestatic(uint32 constPoolIndex);
    void invokeinterface(uint32 constPoolIndex,uint32 count);
    void new_(uint32 constPoolIndex);
    void newarray(uint8 type);
    void anewarray(uint32 constPoolIndex);
    void arraylength();
    void athrow();
    void checkcast(uint32 constPoolIndex);
    int  instanceof(const uint8* bcp, uint32 constPoolIndex, uint32 off) ;
    void monitorenter();
    void monitorexit();
    void multianewarray(uint32 constPoolIndex,uint8 dimensions);
    void ifnull(uint32 targetOffset,uint32 nextOffset);
    void ifnonnull(uint32 targetOffset,uint32 nextOffset);
private:

    typedef StlMap<uint32, Inst*> OffsetToInstMap;

    class JavaInlineInfoBuilder : public InlineInfoBuilder {
    public:
        JavaInlineInfoBuilder(InlineInfoBuilder* parent, 
                MethodDesc& thisMethodDesc, uint32 byteCodeOffset) :
            InlineInfoBuilder(parent), methodDesc(thisMethodDesc), 
                bcOffset(byteCodeOffset)
        {}

    virtual uint32 getCurrentBcOffset() { return bcOffset; }; 
    virtual MethodDesc* getCurrentMd() { return &methodDesc; }; 

    private:
        virtual void addCurrentLevel(InlineInfo* ii, uint32 currOffset)
        {
            ii->addLevel(&methodDesc, currOffset);
        }
        virtual void setCurrentBcOffset(uint32 offSet) { bcOffset = offSet; };


        MethodDesc& methodDesc;
        uint32 bcOffset;
    };

    //
    // helper methods for generating code
    //
    Opnd**  popArgs(uint32 numArgs);
    // for invoke emulation if resolution fails
    void    pseudoInvoke(const char* mdesc);
    void    genCallWithResolve(JavaByteCodes bc, unsigned cpIndex);
    void    invalid();    // called when invalid IR is encountered
    void    genLdVar(uint32 varIndex,JavaLabelPrepass::JavaVarType javaType);
    void    genStVar(uint32 varIndex,JavaLabelPrepass::JavaVarType javaType);
    void    genTypeStVar(uint16 varIndex);
    void    genReturn(JavaLabelPrepass::JavaVarType javaType,uint32 off);
    void    genReturn(uint32 off);
    void    genAdd(Type* dstType);
    void    genSub(Type* dstType);
    void    genMul(Type* dstType);
    void    genDiv(Type* dstType);
    void    genRem(Type* dstType);
    void    genNeg(Type* dstType);
    void    genFPAdd(Type* dstType);
    void    genFPSub(Type* dstType);
    void    genFPMul(Type* dstType);
    void    genFPDiv(Type* dstType);
    void    genFPRem(Type* dstType);
    void    genAnd(Type* dstType);
    void    genOr(Type* dstType);
    void    genXor(Type* dstType);
    void    genArrayLoad(Type* type);
    void    genTypeArrayLoad();
    void    genArrayStore(Type* type);
    void    genTypeArrayStore();
    void    genShl(Type* type, ShiftMaskModifier mod);
    void    genShr(Type* type, SignedModifier mod1, ShiftMaskModifier mod2);
    void    genIf1(ComparisonModifier,int32 targetOffset,int32 nextOffset);
    void    genIf1Commute(ComparisonModifier,int32 targetOffset,int32 nextOffset);
    void    genIf2(ComparisonModifier,int32 targetOffset,int32 nextOffset);
    void    genIf2Commute(ComparisonModifier mod,int32 targetOffset,int32 nextOffset);
    void    genIfNull(ComparisonModifier,int32 targetOffset,int32 nextOffset);
    void    genThreeWayCmp(Type::Tag cmpType,ComparisonModifier src1ToSrc2); // Src2toSrc1 must be same
    //
    // LinkingException throw
    // 
    void linkingException(uint32 constPoolIndex, uint32 operation);
    //
    // helper methods for inlining, call and return sequences
    //
    bool    needsReturnLabel(uint32 off);
    void    genInvokeStatic(MethodDesc * methodDesc,uint32 numArgs,Opnd ** srcOpnds,Type * returnType);
    bool    genVMMagic(const char* mname, uint32 numArgs,Opnd ** srcOpnds,Type * returnType);
    bool    genVMHelper(const char* mname, uint32 numArgs,Opnd ** srcOpnds,Type * returnType);
    
    bool    methodIsArraycopy(MethodDesc * methodDesc);
    bool    arraycopyOptimizable(MethodDesc * methodDesc, uint32 numArgs, Opnd ** srcOpnds);

    bool    genCharArrayCopy(MethodDesc * methodDesc,uint32 numArgs,Opnd ** srcOpnds, Type * returnType);
    bool    genArrayCopyRepMove(MethodDesc * methodDesc,uint32 numArgs,Opnd ** srcOpnds);
    bool    genArrayCopy(MethodDesc * methodDesc,uint32 numArgs,Opnd ** srcOpnds);
    bool    genMinMax(MethodDesc * methodDesc,uint32 numArgs,Opnd ** srcOpnds, Type * returnType);
    void    newFallthroughBlock();

    
    // 
    // initialization
    //
    void    initJsrEntryMap();
    void    initArgs();
    void    initLocalVars();

    //
    // labels and control flow
    //
    uint32            labelId(uint32 offset);
    LabelInst* getLabel(uint32 labelId) {
        assert(labelId < numLabels);
        LabelInst *label = labels[labelId];
        return label;
    }
    void       setLabel(uint32 labelId, LabelInst *label) {
        labels[labelId] = label;
    }
    LabelInst* getNextLabel() {
        return labels[nextLabel];
    }
    uint32    getNextLabelId() {
        assert(nextLabel < numLabels);
        return nextLabel++;
    }
    //
    // operand stack manipulation
    //
    Opnd*            topOpnd();
    Opnd*            popOpnd();
    Opnd*            popOpndStVar();
    void             pushOpnd(Opnd* opnd);
    //
    // field, method, and type resolution
    //
    Type*            getFieldType(FieldDesc*, uint32 constPoolIndex);
    const char*      methodSignatureString(uint32 cpIndex);
    //
    //
    // locals access
    //
    Opnd*            getVarOpndLdVar(JavaLabelPrepass::JavaVarType javaType,uint32 index);
    VarOpnd*         getVarOpndStVar(JavaLabelPrepass::JavaVarType javaType,uint32 index,Opnd *opnd);
    bool             needsReturnLabel();
    //
    //  synchronization
    //
    void             genMethodMonitorEnter();
    void             genMethodMonitorExit();
    
    void             applyMaskToTop(int32 mask);

    // Method checks if following bytecodes are array initializers for newly created array.
    // If they are then substitute array initializers with jit helper array copy instruction.
    // Returns the length of bytecodes converted by this routine.
    uint32 checkForArrayInitializer(Opnd* arrayOpnd, const uint8* byteCodes, uint32 offset, const uint32 byteCodeLength);
    // Obtain the next numeric value from the bytecode in array initialization sequence
    // Returns number of bytes read from the byteCodes array.
    uint32 getNumericValue(const uint8* byteCodes, uint32 offset, const uint32 byteCodeLength, uint64& value);

    //
    // private fields
    //
    MemoryManager&             memManager;

    CompilationInterface&    compilationInterface;
    //
    // references to other compiler objects
    //
    MethodDesc&              methodToCompile;
    ByteCodeParser&          parser;
    TypeManager&             typeManager;
    IRBuilder&               irBuilder;
    TranslatorFlags   translationFlags;
    JavaFlowGraphBuilder&   cfgBuilder;
    //
    // byte code parsing state
    //
    OpndStack           opndStack;          // operand stack
    Opnd**              locVarPropValues;   // value propagation for variables
    Type**              varState;           //  variable state (in terms of type)
    bool                lastInstructionWasABranch;    // self explanatory
    bool                moreThanOneReturn;  // true if there is more than one return
    bool                jumpToTheEnd;       // insert an extra jump to the exit node on return
    //
    // used for IR inlining
    //
    Opnd**              returnOpnd;         // if non-null, used this operand
    Node **          returnNode;         // returns the node where the return is located
    //
    // method state
    //
    ExceptionInfo*      inliningExceptionInfo; // instruction where inlining begins
    Node*            inliningNodeBegin;  // used by inlining of synchronized methods
    uint32              numLabels;          // number of labels in this method
    uint32              numVars;            // number of variables in this method
    uint32              numStackVars;       // number of non-empty stack locations in this method
    uint32              numArgs;            // number of arguments in this method
    Type**              argTypes;           // types for each argument
    Opnd**              args;               // argument opnds
    Opnd*               resultOpnd;         // used for inlining only
    Type*               retType;            // return type of method
    uint32              nextLabel;
    LabelInst**         labels;
    Type*               javaTypeMap[JavaLabelPrepass::NumJavaVarTypes];
    JavaLabelPrepass    prepass;
    StateInfo           *stateInfo;
    uint32              firstVarRef;
    uint32              numberVarRef;
    // Synchronization
    Opnd*               lockAddr;
    Opnd*               oldLockValue;
    JavaInlineInfoBuilder  thisLevelBuilder;
    
    //
    // mapping: 
    //   [ subroutine entry stvar inst ] -> [ ret inst ]
    //   taken from parent if translating inlined method
    // this mapping should be provided but not used since
    //   inlining translators update parent maps
    //
    JsrEntryInstToRetInstMap* jsrEntryMap;

    //
    // mapping to bytecode offsets:
    // * 'ret' instructions
    // * subroutine entries (=='jsr' targets)
    //
    OffsetToInstMap retOffsets, jsrEntryOffsets;
};

} //namespace Jitrino 

#endif //  _JAVABYTECODETRANSLATOR_H_
