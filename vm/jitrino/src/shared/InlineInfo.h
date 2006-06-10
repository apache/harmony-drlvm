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

namespace Jitrino {

class InlineInfo {
    typedef StlList<MethodDesc*>   MethodDescList;
public:

    InlineInfo(MemoryManager& mm) :
        memMgr(mm), inlineChain(NULL)
    {}

    bool isEmpty() const { return inlineChain == NULL || inlineChain->empty(); }

    void prependInlineChain(InlineInfo& ii)
    {
        if ( ii.inlineChain ) {
            init();
            MethodDescList::const_reverse_iterator it(ii.inlineChain->end());
            MethodDescList::const_reverse_iterator last(ii.inlineChain->begin());
            for (; it != last; ++it) {
                MethodDesc* md = (*it);
                inlineChain->push_front(md);
            }
        }
    }

    void getInlineChainFrom(InlineInfo& ii) 
    {
        if ( ii.inlineChain ) {
            init();
            MethodDescList::const_iterator it = ii.inlineChain->begin();
            for (; it != ii.inlineChain->end(); ++it) {
                MethodDesc* md = (*it);
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
    void addLevel(MethodDesc* md) 
    { 
        init();
        addLevelFast(md);
    }

    void prependLevel(MethodDesc* md)
    {
        init();
        inlineChain->push_front(md);
    }

    void printLevels(CategoryStream<LogIR>& os) const
    {
        if ( isEmpty() ) {
            return;
        }
        MethodDescList::const_iterator it = inlineChain->begin();
        for (; it != inlineChain->end(); ++it) {
            MethodDesc* md = (*it);
            os << md->getParentType()->getName() << "." << md->getName() << " ";
        }
        os << ::std::endl;
    }

    uint32 countLevels() { return inlineChain ? inlineChain->size() : 0; }

protected:
    friend class InlineInfoMap;

    bool isInitialized() const { return inlineChain != NULL; }

    void init() 
    { 
        if ( !isInitialized() ) {
            inlineChain = new (memMgr) MethodDescList(memMgr); 
        }
    }

    void addLevelFast(MethodDesc* md) { inlineChain->push_back(md); }
private:
    MemoryManager&  memMgr;
    MethodDescList* inlineChain;
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

    void buildInlineInfo(InlineInfo* ii) // ii must be non-NULL
    {
        if ( parent ) {
             parent->buildInlineInfo(ii);
        }
        addCurrentLevel(ii);
    }
    void buildInlineInfoForInst(Inst* inst, MethodDesc* target_md = NULL);
private:

    virtual void addCurrentLevel(InlineInfo* ii) = 0; // ii must be non-NULL
    InlineInfoBuilder* parent;
};

} //namespace Jitrino 

#endif // _INLINE_INFO_H_

