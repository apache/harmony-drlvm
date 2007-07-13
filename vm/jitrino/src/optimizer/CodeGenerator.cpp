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
 * @author Intel, Pavel A. Ozhdikhin
 * @version $Revision: 1.27.8.1.4.4 $
 *
 */

#include "CodeSelectors.h"

namespace Jitrino {


class HIR2LIRSelectorSessionAction: public SessionAction {
public:
    virtual void run ();
};
static ActionFactory<HIR2LIRSelectorSessionAction> _hir2lir("hir2lir");

//
// code generator entry point
//
void HIR2LIRSelectorSessionAction::run() {

#if defined(_IPF_)
#else
    CompilationContext* cc = getCompilationContext();
    IRManager& irManager = *cc->getHIRManager();
    CompilationInterface* ci = cc->getVMCompilationInterface();
    MethodDesc* methodDesc  = ci->getMethodToCompile();
    OpndManager& opndManager = irManager.getOpndManager();
    const OptimizerFlags& optFlags = irManager.getOptimizerFlags();
    VarOpnd* varOpnds   = opndManager.getVarOpnds();
    MemoryManager& mm  = cc->getCompilationLevelMemoryManager();

    MethodCodeSelector* mcs = new (mm) _MethodCodeSelector(irManager,methodDesc,varOpnds,&irManager.getFlowGraph(),
        opndManager, optFlags.sink_constants, optFlags.sink_constants1);

    Ia32::CodeGenerator cg;
    cg.genCode(this, *mcs);
#endif
}



InlineInfoMap::Entry* InlineInfoMap::newEntry(Entry* parent, Method_Handle mh, uint16 bcOffset) {
    Entry* e = new (memManager) Entry(parent, bcOffset, mh);
    return e;
}

void InlineInfoMap::registerEntry(Entry* e, uint32 nativeOffs) {
    entryByOffset[nativeOffs] = e;
    for (Entry* current = e; current!=NULL; current = current->parentEntry) {
        if (std::find(entries.begin(), entries.end(), current)!=entries.end()) {
            assert(current!=e); //possible if inlined method has multiple entries, skip this method marker.
        } else {
            entries.push_back(current);
        }
    }
}


static uint32 getIndexSize(size_t nEntriesInIndex) {
    return (uint32)(2 * nEntriesInIndex * sizeof(uint32) + sizeof(uint32)); //zero ending list of [nativeOffset, entryOffsetInImage] pairs
}

uint32
InlineInfoMap::getImageSize() const {
    if (isEmpty()) {
        return sizeof(uint32);
    }
    return getIndexSize(entryByOffset.size())   //index size
          + entries.size() * sizeof(Entry); //all entries size;
}

void
InlineInfoMap::write(InlineInfoPtr image)
{
    if (isEmpty()) {
        *(uint32*)image=0;
        return;
    }

    //write all entries first;
    Entry*  entriesInImage = (Entry*)((char*)image + getIndexSize(entryByOffset.size()));
    Entry*  entriesPtr = entriesInImage; 
    for (StlVector<Entry*>::iterator it = entries.begin(), end = entries.end(); it != end; it++) {
        Entry* e = *it;
        *entriesPtr = *e;
        entriesPtr++;
    }
    assert(((char*)entriesPtr) == ((char*)image) + getImageSize());

    //now update parentEntry reference to written entries
    for (uint32 i=0; i < entries.size(); i++) {
        Entry* imageChild = entriesInImage + i;
        Entry* compileTimeParent = imageChild->parentEntry;
        if (compileTimeParent!=NULL) {
            size_t parentIdx = std::find(entries.begin(), entries.end(), compileTimeParent) - entries.begin();
            assert(parentIdx<entries.size());
            Entry* imageParent = entriesInImage + parentIdx;
            imageChild->parentEntry = imageParent;
        }
    }

    //now write index header
    uint32* header = (uint32*)image;
    for (StlMap<uint32, Entry*>::iterator it = entryByOffset.begin(), end = entryByOffset.end(); it!=end; it++) {
        uint32 nativeOffset = it->first;
        Entry* compileTimeEntry = it->second;
        size_t entryIdx = std::find(entries.begin(), entries.end(), compileTimeEntry) - entries.begin();
        assert(entryIdx<entries.size());
        Entry* imageEntry = entriesInImage + entryIdx;
        *header = nativeOffset;
        header++;
        *header = (char*)imageEntry - (char*)image;
        header++;
    }
    *header = 0;
    header++;
    assert((char*)header == (char*)entriesInImage);
}


const InlineInfoMap::Entry* InlineInfoMap::getEntryWithMaxDepth(InlineInfoPtr ptr, uint32 nativeOffs) {
    uint32* header = (uint32*)ptr;
    while (*header!=0) {
        uint32 nativeOffset = *header;
        header++;
        uint32 entryOffset = *header;
        header++;
        if (nativeOffset == nativeOffs) {
            Entry* e = (Entry*)((char*)ptr + entryOffset);
            return e;
        }
    }
    return NULL;
}

const InlineInfoMap::Entry* InlineInfoMap::getEntry(InlineInfoPtr ptr, uint32 nativeOffs, uint32 inlineDepth) {
    const Entry* e = getEntryWithMaxDepth(ptr, nativeOffs);
    while (e!=NULL) {
        if (e->getInlineDepth() == inlineDepth) {
            return e;
        }
        e = e->parentEntry;
    }
    return NULL;
}


} //namespace Jitrino 
