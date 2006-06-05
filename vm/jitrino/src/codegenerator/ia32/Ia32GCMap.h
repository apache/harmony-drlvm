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
 * @author Intel, Mikhail Y. Fursov
 * @version $Revision: 1.10.14.2.4.4 $
 */

#ifndef _IA32_GC_MAP_H_
#define _IA32_GC_MAP_H_

#include "Stl.h"
#include "MemoryManager.h"
#include "Ia32IRManager.h"
#include "Ia32StackInfo.h"
#include "Ia32BCMap.h"
#include "DrlVMInterface.h"
namespace Jitrino
{
namespace Ia32 {
#ifdef _DEBUG
#define GC_MAP_DUMP_ENABLED
#endif

    class GCSafePointsInfo;
    class GCSafePoint;
    class GCSafePointOpnd;
    class StackInfo;

    class GCMap {
        typedef StlVector<GCSafePoint*> GCSafePoints;
    public:
        GCMap(MemoryManager& mm);

        void registerInsts(IRManager& irm);

        uint32 getByteSize() const ;
        static uint32 readByteSize(const Byte* input);
        void write(Byte*);
        const GCSafePointsInfo* getGCSafePointsInfo() const {return offsetsInfo;}
        
        static const uint32* findGCSafePointStart(const uint32* image, uint32 ip);
        static void checkObject(DrlVMTypeManager& tm, const void* p);

    private:
        void processBasicBlock(IRManager& irm, const BasicBlock* block);
        void registerGCSafePoint(IRManager& irm, const LiveSet& ls, Inst* inst);
        void registerHardwareExceptionPoint(Inst* inst);
        bool isHardwareExceptionPoint(const Inst* inst) const;

        
        
        MemoryManager& mm;
        GCSafePoints gcSafePoints;
        GCSafePointsInfo* offsetsInfo;

    };

    class GCSafePoint {
        friend class GCMap;
        typedef StlVector<GCSafePointOpnd*> GCOpnds;
    public:
        GCSafePoint(MemoryManager& mm, uint32 _ip):gcOpnds(mm), ip(_ip) {
#ifdef _DEBUG
            instId = 0;
            hardwareExceptionPoint = false;
#endif
        }
        GCSafePoint(MemoryManager& mm, const uint32* image);

        uint32 getUint32Size() const;
        void write(uint32* image) const;
        uint32 getNumOpnds() const {return gcOpnds.size();}
        static uint32 getIP(const uint32* image);

        void enumerate(GCInterface* gcInterface, const JitFrameContext* c, const StackInfo& stackInfo) const;
    
    private:
        //return address in memory where opnd value is saved
        uint32 getOpndSaveAddr(const JitFrameContext* ctx, const StackInfo& sInfo,const GCSafePointOpnd* gcOpnd) const;
        GCOpnds gcOpnds;
        uint32 ip;
#ifdef _DEBUG
        uint32 instId;
        bool hardwareExceptionPoint;
    public: 
        bool isHardwareExceptionPoint() const {return hardwareExceptionPoint;}
#endif 
    };

    class GCSafePointOpnd {
        friend class GCSafePoint;
        static const uint32 OBJ_MASK  = 0x1;
        static const uint32 REG_MASK  = 0x2;

#ifdef _DEBUG
        // flags + val + mptrOffset + firstId
        static const uint32 IMAGE_SIZE_UINT32 = 4; //do not use sizeof due to the potential struct layout problems
#else 
        // flags + val + mptrOffset 
        static const uint32 IMAGE_SIZE_UINT32 = 3;
#endif 

    public:
        
        GCSafePointOpnd(bool isObject, bool isOnRegister, int32 _val, int32 _mptrOffset) : val(_val), mptrOffset(_mptrOffset) {
            flags = isObject ? OBJ_MASK : 0;
            flags = flags | (isOnRegister ? REG_MASK: 0);
#ifdef _DEBUG
            firstId = 0;
#endif
        }
        
        bool isObject() const {return (flags & OBJ_MASK)!=0;}
        bool isMPtr() const  {return !isObject();}
        
        bool isOnRegister() const { return (flags & REG_MASK)!=0;}
        bool isOnStack() const {return !isOnRegister();}
        
        RegName getRegName() const { assert(isOnRegister()); return RegName(val);}
        int32 getDistFromInstESP() const { assert(isOnStack()); return val;}

        int32 getMPtrOffset() const {return mptrOffset;}

#ifdef _DEBUG
        uint32 firstId;
#endif

    private:
        GCSafePointOpnd(uint32 _flags, int32 _val, int32 _mptrOffset) : flags(_flags), val(_val), mptrOffset(_mptrOffset) {}

        //first bit is location, second is type
        uint32 flags;        
        //opnd placement ->Register or offset
        int32 val; 
        int32 mptrOffset;
    };


	BEGIN_DECLARE_IRTRANSFORMER(GCMapCreator, "gcmap", "GC map creation")
		IRTRANSFORMER_CONSTRUCTOR(GCMapCreator)
		void runImpl();
		uint32 getNeedInfo()const{ return NeedInfo_LivenessInfo;}
#ifdef  GC_MAP_DUMP_ENABLED
        uint32 getSideEffects() {return Log::cat_cg()->isIREnabled();}
        bool isIRDumpEnabled(){ return true;}
#else 
        uint32 getSideEffects() {return 0;}
        bool isIRDumpEnabled(){ return false;}
#endif
        
	END_DECLARE_IRTRANSFORMER(GCMapCreator) 

	BEGIN_DECLARE_IRTRANSFORMER(InfoBlockWriter, "info", "Creation of method info block")
		IRTRANSFORMER_CONSTRUCTOR(InfoBlockWriter)
		void runImpl()
		{ 
            StackInfo * stackInfo = (StackInfo*)irManager.getInfo("stackInfo");
            assert(stackInfo != NULL);
            GCMap * gcMap = (GCMap*)irManager.getInfo("gcMap");
            assert(gcMap != NULL);
            BcMap *bcMap = (BcMap*)irManager.getInfo("bcMap");
            assert(bcMap != NULL);
			InlineInfoMap * inlineInfo = (InlineInfoMap*)irManager.getInfo("inlineInfo");
			assert(inlineInfo !=NULL);
            
            CompilationInterface& compIntf = irManager.getCompilationInterface();

			if ( !inlineInfo->isEmpty() ) {
				inlineInfo->write(compIntf.allocateJITDataBlock(inlineInfo->computeSize(), 8));
			}

            uint32 stackInfoSize = stackInfo->getByteSize();
            uint32 gcInfoSize = gcMap->getByteSize();
            uint32 bcMapSize = bcMap->getByteSize(); // we should write at least the size of map  in the info block
            assert(bcMapSize >= 4);                              // so  bcMapSize should be more than 4 for ia32
            
			Byte* infoBlock = compIntf.allocateInfoBlock(stackInfoSize + gcInfoSize + bcMapSize);
            stackInfo->write(infoBlock);
            gcMap->write(infoBlock+stackInfoSize);

            if (compIntf.isBCMapInfoRequired()) {
                 bcMap->write(infoBlock + stackInfoSize + gcInfoSize);
            } else {
                 // if no bc info write size equal to zerro
                 // this will make possible handle errors in case
                 bcMap->writeZerroSize(infoBlock + stackInfoSize + gcInfoSize);
            } 
		}
		uint32 getNeedInfo()const{ return 0; }
		uint32 getSideEffects()const{ return 0; }
		bool isIRDumpEnabled(){ return false; }
	END_DECLARE_IRTRANSFORMER(InfoBlockWriter) 

}} //namespace

#endif /* _IA32_GC_MAP_H_ */
