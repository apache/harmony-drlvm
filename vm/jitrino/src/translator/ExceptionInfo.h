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

#ifndef _EXCEPTIONINFO_H_
#define _EXCEPTIONINFO_H_

namespace Jitrino {

class LabelInst;
class CatchBlock;
class Type;

class ExceptionInfo {
public:
    virtual ~ExceptionInfo() {}

    uint32  getId()            {return id;}
    uint32  getBeginOffset(){return beginOffset;}
    uint32  getEndOffset()    {return endOffset;}
    void    setEndOffset(uint32 offset)    { endOffset = offset; }
    bool    equals(uint32 begin,uint32 end) {
        return (begin == beginOffset && end == endOffset);
    }
    ExceptionInfo*  getNextExceptionInfoAtOffset() {return nextExceptionAtOffset;}
    void            setNextExceptionInfoAtOffset(ExceptionInfo* n) {nextExceptionAtOffset = n;}
    virtual bool isCatchBlock()        {return false;}
    virtual bool isCatchHandler()    {return false;}

    void setLabelInst(LabelInst *lab) { label = lab; }
    LabelInst *getLabelInst()         { return label; }
protected:
    ExceptionInfo(uint32 _id,
                  uint32 _beginOffset,
                  uint32 _endOffset) 
    : id(_id), beginOffset(_beginOffset), endOffset(_endOffset),
      nextExceptionAtOffset(NULL), label(NULL)
    {}
private:
    uint32 id;
    uint32 beginOffset;
    uint32 endOffset;
    ExceptionInfo*    nextExceptionAtOffset;
    LabelInst* label;
};

class CatchHandler : public ExceptionInfo {
public:
    CatchHandler(uint32 id,
                 uint32 beginOffset,
                 uint32 endOffset,
                 Type* excType) 
                 : ExceptionInfo(id, beginOffset, endOffset), 
                 exceptionType(excType), nextHandler(NULL), order(0) {}
    virtual ~CatchHandler() {}

    Type*          getExceptionType()              {return exceptionType;}
    uint32         getExceptionOrder()             {return order;        }
    CatchHandler*  getNextHandler()                {return nextHandler;  }
    void           setNextHandler(CatchHandler* n) {nextHandler=n;       }
    void           setOrder(uint32 ord)            {order = ord;         }
    bool           isCatchHandler()                {return true;         }
private:
    Type*          exceptionType;
    CatchHandler*  nextHandler;
    uint32         order;
};

class CatchBlock : public ExceptionInfo {
public:
    CatchBlock(uint32 id,
               uint32 beginOffset,
               uint32 endOffset,
               uint32 exceptionIndex) 
    : ExceptionInfo(id,beginOffset,endOffset), handlers(NULL), excTableIndex(exceptionIndex) {}
    virtual ~CatchBlock() {}

    bool isCatchBlock()                {return true;}
    uint32 getExcTableIndex() { return excTableIndex; }
    void addHandler(CatchHandler* handler) {
        uint32 order = 0;
        if (handlers == NULL) {
            handlers = handler;
        } else {
            order++;
            CatchHandler *h = handlers;
            for ( ;
                 h->getNextHandler() != NULL;
                 h = h->getNextHandler())
                order++;
            h->setNextHandler(handler);
        }
        handler->setOrder(order);

    }
    bool hasOffset(uint32 offset)
    {
        return (getBeginOffset() <= offset) && (offset < getEndOffset());
    }
    bool offsetSplits(uint32 offset)
    {
        return (getBeginOffset() < offset) && (offset + 1 < getEndOffset());
    }
    CatchHandler*    getHandlers()    {return handlers;}
private:
    CatchHandler* handlers;
    uint32 excTableIndex;
};

} //namespace Jitrino 

#endif // _EXCEPTIONINFO_H_
