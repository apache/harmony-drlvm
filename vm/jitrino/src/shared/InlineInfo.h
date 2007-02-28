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
 * @author Egor V. Pasko
 * @version $Revision: 1.6.18.3 $
 *
 */


#ifndef _INLINE_INFO_H_
#define _INLINE_INFO_H_

#include <assert.h>
#include <iostream>
#include "MemoryManager.h"
#include "Stl.h"
#include "Log.h"
#include "Type.h"
#include "CGSupport.h"

namespace Jitrino {

class Inst;

class InlineInfo {
    typedef ::std::pair<MethodDesc*, uint32> InlinePair;
    typedef StlList<InlinePair*>   InlinePairList;
public:

    InlineInfo(MemoryManager& mm) :
        memMgr(mm), inlineChain(NULL)
    {}

    bool isEmpty() const { return inlineChain == NULL || inlineChain->empty(); }

    void prependInlineChain(InlineInfo& ii)
    {
        if ( ii.inlineChain ) {
            init();
            InlinePairList::const_reverse_iterator it(ii.inlineChain->end());
            InlinePairList::const_reverse_iterator last(ii.inlineChain->begin());
            for (; it != last; ++it) {
                InlinePair* md = (*it);
                inlineChain->push_front(md);
            }
        }
    }

    void getInlineChainFrom(InlineInfo& ii) 
    {
        if ( ii.inlineChain ) {
            init();
            InlinePairList::const_iterator it = ii.inlineChain->begin();
            for (; it != ii.inlineChain->end(); ++it) {
                InlinePair* md = (*it);
                addLevelFast(md);
            }
        }else{
            inlineChain = NULL;
        }
    }

    //
    // adds a new level to the chain of inlined methods
    // convention is to add in 'parent-first' order
    //
    void addLevel(MethodDesc* md, uint32 bcOff)
    { 
        init();
        InlinePair* inlPair = new(memMgr) InlinePair(md, bcOff);
        addLevelFast(inlPair);
    }

    void prependLevel(MethodDesc* md, uint32 bcOff)
    {
        init();
        InlinePair* inlPair = new(memMgr) InlinePair(md, bcOff);
        inlineChain->push_front(inlPair);
    }

    void printLevels(std::ostream& os) const
    {
        if ( isEmpty() ) {
            return;
        }
        InlinePairList::const_iterator it = inlineChain->begin();
        for (; it != inlineChain->end(); ++it) {
            InlinePair* md = (*it);
            os << ((MethodDesc*)md->first)->getParentType()->getName() << "." 
                    << ((MethodDesc*)md->first)->getName() << " " << " bcOff " 
                    << md->second << " ";
        }
        os << ::std::endl;
    }

    uint32 countLevels() { return inlineChain ? (uint32)inlineChain->size() : 0; }

protected:
    friend class InlineInfoMap;

    bool isInitialized() const { return inlineChain != NULL; }

    void init() 
    { 
        if ( !isInitialized() ) {
            inlineChain = new (memMgr) InlinePairList(memMgr); 
        }
    }

    void addLevelFast(InlinePair* inlPair) { 
        inlineChain->push_back(inlPair); 
    }
private:
    MemoryManager&  memMgr;
    InlinePairList* inlineChain;
};

// 
// adds InlineInfo levels for all parents in parent-first order
//    no implementation for building a level here
//
class InlineInfoBuilder {
public:

    InlineInfoBuilder(InlineInfoBuilder* parentBuilder) : 
        parent(parentBuilder)
    {}
    virtual ~InlineInfoBuilder() {}

    void setParentBuilder(InlineInfoBuilder* builder) { parent = builder; }

    void buildInlineInfo(InlineInfo* ii, uint32 offset); // ii must be non-NULL
    uint32 buildInlineInfoForInst(Inst* inst, uint32 offset, MethodDesc* target_md = NULL);
    virtual uint32 getCurrentBcOffset() = 0;
    virtual MethodDesc* getCurrentMd() = 0;
protected:
    InlineInfoBuilder* parent;
private:
    virtual void addCurrentLevel(InlineInfo* ii, uint32 offset) = 0; // ii must be non-NULL
    virtual void setCurrentBcOffset(uint32 offSet) = 0;
};

} //namespace Jitrino 

#endif // _INLINE_INFO_H_

