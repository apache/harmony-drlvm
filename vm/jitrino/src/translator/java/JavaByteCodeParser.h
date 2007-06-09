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
 * @version $Revision: 1.9.16.1.4.4 $
 *
 */

#ifndef _JAVABYTECODEPARSER_H_
#define _JAVABYTECODEPARSER_H_

#include <stdio.h>
#include <iostream>
#include "ByteCodeParser.h"
#include "BitSet.h"
#include "Queue.h"
#include "HashTable.h"

namespace Jitrino {


//
// big-endian ordering
//
int16    si16(const uint8* bcp);
uint16   su16(const uint8* bcp);
int32    si32(const uint8* bcp);
uint64   si64(const uint8* bcp);

class JavaSwitchTargetsIter;
class JavaLookupSwitchTargetsIter;

class JavaByteCodeParserCallback : public ByteCodeParserCallback {
public:
    JavaByteCodeParserCallback() : isLinearPass(true)  {
        currentOffset = 0;
        nextOffset = 0; 
        linearPassDone = false;
        visited = NULL;
        bytecodevisited = NULL;
        prepassVisited = NULL;
        labelStack = NULL;
        noNeedToParse = false;
    }
    JavaByteCodeParserCallback(MemoryManager& memManager,uint32 byteCodeLength) 
        : isLinearPass(false) 
    {
        currentOffset = 0;
        nextOffset = 0; 
        linearPassDone = false;
        visited = new (memManager) BitSet(memManager,byteCodeLength);
        bytecodevisited = new (memManager) BitSet(memManager,byteCodeLength);
        prepassVisited = NULL;
        labelStack = new (memManager) Queue<uint8>(memManager);
        noNeedToParse = false;
    }
    bool parseByteCode(const uint8* byteCodes,uint32 byteCodeOffset);
    BitSet* getVisited()  { return visited; }
protected:
    // the current byte codes offset
    uint32           currentOffset;
    const bool       isLinearPass;
    bool             linearPassDone;
    BitSet*          visited;
    BitSet*          bytecodevisited;
    BitSet*          prepassVisited;
    Queue<uint8>*    labelStack;

    // for example when some exception type can not be resolved
    bool noNeedToParse;

    // called before each byte code to indicate the next byte code's offset
    virtual void offset(uint32 offset) = 0;
    virtual void offset_done(uint32 offset) = 0;
    virtual void nop() = 0;
    virtual void aconst_null() = 0;
    virtual void iconst(int32) = 0;
    virtual void lconst(int64) = 0;
    virtual void fconst(float) = 0;
    virtual void dconst(double) = 0;
    virtual void bipush(int8) = 0;
    virtual void sipush(int16) = 0;
    virtual void ldc(uint32) = 0;
    virtual void ldc2(uint32) = 0;
    virtual void iload(uint16 varIndex) = 0;
    virtual void lload(uint16 varIndex) = 0;
    virtual void fload(uint16 varIndex) = 0;
    virtual void dload(uint16 varIndex) = 0;
    virtual void aload(uint16 varIndex) = 0;
    virtual void iaload() = 0;
    virtual void laload() = 0;
    virtual void faload() = 0;
    virtual void daload() = 0;
    virtual void aaload() = 0;
    virtual void baload() = 0;
    virtual void caload() = 0;
    virtual void saload() = 0;
    virtual void istore(uint16 varIndex,uint32 off) = 0;
    virtual void lstore(uint16 varIndex,uint32 off) = 0;
    virtual void fstore(uint16 varIndex,uint32 off) = 0;
    virtual void dstore(uint16 varIndex,uint32 off) = 0;
    virtual void astore(uint16 varIndex,uint32 off) = 0;
    virtual void iastore() = 0;
    virtual void lastore() = 0;
    virtual void fastore() = 0;
    virtual void dastore() = 0;
    virtual void aastore() = 0;
    virtual void bastore() = 0;
    virtual void castore() = 0;
    virtual void sastore() = 0;
    virtual void pop() = 0;
    virtual void pop2() = 0;
    virtual void dup() = 0;
    virtual void dup_x1() = 0;
    virtual void dup_x2() = 0;
    virtual void dup2() = 0;
    virtual void dup2_x1() = 0;
    virtual void dup2_x2() = 0;
    virtual void swap() = 0;
    virtual void iadd() = 0;
    virtual void ladd() = 0;
    virtual void fadd() = 0;
    virtual void dadd() = 0;
    virtual void isub() = 0;
    virtual void lsub() = 0;
    virtual void fsub() = 0;
    virtual void dsub() = 0;
    virtual void imul() = 0;
    virtual void lmul() = 0;
    virtual void fmul() = 0;
    virtual void dmul() = 0;
    virtual void idiv() = 0;
    virtual void ldiv() = 0;
    virtual void fdiv() = 0;
    virtual void ddiv() = 0;
    virtual void irem() = 0;
    virtual void lrem() = 0;
    virtual void frem() = 0;
    virtual void drem() = 0;
    virtual void ineg() = 0;
    virtual void lneg() = 0;
    virtual void fneg() = 0;
    virtual void dneg() = 0;
    virtual void ishl() = 0;
    virtual void lshl() = 0;
    virtual void ishr() = 0;
    virtual void lshr() = 0;
    virtual void iushr() = 0;
    virtual void lushr() = 0;
    virtual void iand() = 0;
    virtual void land() = 0;
    virtual void ior() = 0;
    virtual void lor() = 0;
    virtual void ixor() = 0;
    virtual void lxor() = 0;
    virtual void iinc(uint16 varIndex,int32 amount) = 0;
    virtual void i2l() = 0;
    virtual void i2f() = 0;
    virtual void i2d() = 0;
    virtual void l2i() = 0;
    virtual void l2f() = 0;
    virtual void l2d() = 0;
    virtual void f2i() = 0;
    virtual void f2l() = 0;
    virtual void f2d() = 0;
    virtual void d2i() = 0;
    virtual void d2l() = 0;
    virtual void d2f() = 0;
    virtual void i2b() = 0;
    virtual void i2c() = 0;
    virtual void i2s() = 0;
    virtual void lcmp() = 0;
    virtual void fcmpl() = 0;
    virtual void fcmpg() = 0;
    virtual void dcmpl() = 0;
    virtual void dcmpg() = 0;
    virtual void ifeq(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void ifne(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void iflt(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void ifge(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void ifgt(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void ifle(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void if_icmpeq(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void if_icmpne(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void if_icmplt(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void if_icmpge(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void if_icmpgt(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void if_icmple(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void if_acmpeq(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void if_acmpne(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void goto_(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void jsr(uint32 offset, uint32 nextOffset) = 0;
    virtual void ret(uint16 varIndex) = 0;
    virtual void tableswitch(JavaSwitchTargetsIter*) = 0;
    virtual void lookupswitch(JavaLookupSwitchTargetsIter*) = 0;
    virtual void ireturn(uint32 off) = 0;
    virtual void lreturn(uint32 off) = 0;
    virtual void freturn(uint32 off) = 0;
    virtual void dreturn(uint32 off) = 0;
    virtual void areturn(uint32 off) = 0;
    virtual void return_(uint32 off) = 0;
    virtual void getstatic(uint32 constPoolIndex) = 0;
    virtual void putstatic(uint32 constPoolIndex) = 0;
    virtual void getfield(uint32 constPoolIndex) = 0;
    virtual void putfield(uint32 constPoolIndex) = 0;
    virtual void invokevirtual(uint32 constPoolIndex) = 0;
    virtual void invokespecial(uint32 constPoolIndex) = 0;
    virtual void invokestatic(uint32 constPoolIndex) = 0;
    virtual void invokeinterface(uint32 constPoolIndex,uint32 count) = 0;
    virtual void new_(uint32 constPoolIndex) = 0;
    virtual void newarray(uint8 type) = 0;
    virtual void anewarray(uint32 constPoolIndex) = 0;
    virtual void arraylength() = 0;
    virtual void athrow() = 0;
    virtual void checkcast(uint32 constPoolIndex) = 0;
    virtual int  instanceof(const uint8* bcp, uint32 constPoolIndex, uint32 off) = 0;
    virtual void monitorenter() = 0;
    virtual void monitorexit() = 0;
    virtual void multianewarray(uint32 constPoolIndex,uint8 dimensions) = 0;
    virtual void ifnull(uint32 targetOffset,uint32 nextOffset) = 0;
    virtual void ifnonnull(uint32 targetOffset,uint32 nextOffset) = 0;

    virtual bool skipParsing() {return noNeedToParse;}

};


class JavaSwitchTargetsIter {
public:
    JavaSwitchTargetsIter(const uint8* bcp,uint32 off) {
        // skip over padding
        switchOffset = off;
        uint32 offset = ((off+4)&~3)-off;
        // read in the default target
        defaultTarget = readU4Be(bcp+offset);
        lowValue      = readU4Be(bcp+offset+4);
        highValue     = readU4Be(bcp+offset+8);
        numTargets    = highValue - lowValue + 1;
        nextTarget    = bcp+offset+12;
        length = ((uint32)(nextTarget-bcp)) + (numTargets * 4);
        nextByteCode = bcp + length;
    }
    bool    hasNext() {
        return (nextTarget < nextByteCode);
    }
    uint32    getNextTarget() {
        if (hasNext() == false)
            return 0;
        int32 target = readU4Be(nextTarget);
        nextTarget += 4;
        return target+switchOffset;
    }
    uint32         getNumTargets()      {return numTargets;}
    uint32         getLength()          {return length;}
    uint32         getDefaultTarget()   {return defaultTarget+switchOffset;}
    uint32         getHighValue()       {return highValue;}
    uint32         getLowValue()        {return lowValue;}
private:
    const uint8*   nextByteCode;
    const uint8*   nextTarget;
    uint32         switchOffset;
    uint32         numTargets;
    uint32         length;
     int32         defaultTarget;
    uint32         highValue;
    uint32         lowValue;
};

class JavaLookupSwitchTargetsIter {
public:
    JavaLookupSwitchTargetsIter(const uint8* bcp,uint32 off) {
        // skip over padding
        lookupOffset = off;
        uint32 offset = ((off+4)&~3)-off;
        // read in the default target
        defaultTarget = readU4Be(bcp+offset);
        numTargets    = readU4Be(bcp+offset+4);
        nextTarget = bcp+offset+8;
        length = ((uint32)(nextTarget-bcp)) + (numTargets * 8);
        nextByteCode = bcp + length;
    }
    bool      hasNext() {
        return (nextTarget < nextByteCode);
    }
    uint32    getNextTarget(uint32* key) {
        if (hasNext() == false)
            return 0;
        *key = readU4Be(nextTarget);
        int32 target = readU4Be(nextTarget+4);
        nextTarget += 8;
        return target+lookupOffset;
    }
    uint32    getNumTargets()        {return numTargets;}
    uint32    getLength()            {return length;}
    uint32    getDefaultTarget()    {return defaultTarget+lookupOffset;}
private:
    const uint8*    nextByteCode;
    const uint8*    nextTarget;
    uint32          lookupOffset;
    uint32          numTargets;
    uint32          length;
     int32          defaultTarget;
};


} //namespace Jitrino 

#endif // _JAVABYTECODEPARSER_H_
