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
 * @author Intel, Salikh Zakirov
 * @version $Revision: 1.1.2.1.4.3 $
 */  


// VM includes
#include "open/gc.h"
#include "open/vm.h"
#include "port_atomic.h"

// GC includes
#include "gc_cout.h"
#include "gc_globals.h"
#include "compressed_references.h"
#include "object_list.h"
#include "garbage_collector.h"

#ifdef PLATFORM_POSIX
#error "Characterize heap has not been tested on POSIX, Why is it even in the makefile?"
#endif

#ifdef __INTEL_COMPILER
/* float comparison is unreliable */
#pragma warning (disable:1572)
#endif


// This code is only checked out for debug on windows. 
#if (!defined(PLATFORM_POSIX) && defined(_DEBUG))

#include <iostream>
#include <fstream>
using namespace std;
//#include "win32_api.h"

#include "characterize_heap.h"
#define HEAPTRACE(message) TRACE2("gc.heaptrace", message)

const bool heapTracePrintTraceFlag = false;
const bool heapTraceDumpHeapFlag = false;   // controls dumping of heap
const bool heapTraceVerboseFlag = false;
const bool heapTraceDumpCharsFlag = false;

ostream* dumpStream = NULL;
ostream* statsStream = NULL;
ostream* charDumpStream = NULL;

const char* dumpFileName = "Heap_%d.dump";
const char* statsFileName = "HeapSlotCounts.txt";
const char* objectSizeHistogramsFileName = "ObjectSizeHistograms.txt";
const char* typeSizeHistogramsFileName = "TypeSizeHistograms.txt";
const char* typeCountHistogramsFileName = "TypeCountHistograms.txt";
const char* objectBitsHistogramsFileNameFormat = "%sBitSizesHistograms.txt";
const char* charHistogramFileName = "CharHistograms.txt";
const char* charDumpFileName = "HeapChars_%d.dump";



int dumpNumber = 0;
int dumpId     = 0;

struct HeapSlotTypeStats {
    char*   name;
    int     numBitsPerSlot;
    int     numBytes;
    int     numSlots;
    int     numLeadZeroBytes;   // not include zero-valued slots
    int     numCompressedBits;  // number of bits if you remove leading 0's and 1's
    int     numEntropyBytes;
#define NUM_BITS_HISTOGRAM_BINS 64
    int     numBitsHistogramBins[NUM_BITS_HISTOGRAM_BINS];
};

void
initStats(HeapSlotTypeStats* typeStats) {
    typeStats->numBytes = 0;
    typeStats->numSlots = 0;
    for (int i=0; i<NUM_BITS_HISTOGRAM_BINS; i++) {
        typeStats->numBitsHistogramBins[i] = 0;
    }
    typeStats->numCompressedBits = 0;
    typeStats->numLeadZeroBytes = 0;
    typeStats->numEntropyBytes = 0;
}

void
addStats(HeapSlotTypeStats* dst,HeapSlotTypeStats* src1,HeapSlotTypeStats* src2) {
    dst->numBytes = src1->numBytes + src2->numBytes;
    dst->numSlots = src1->numSlots + src2->numSlots;
    for (int i=0; i<NUM_BITS_HISTOGRAM_BINS; i++) {
        dst->numBitsHistogramBins[i] = src1->numBitsHistogramBins[i] + src2->numBitsHistogramBins[i];
    }
    dst->numCompressedBits = src1->numCompressedBits + src2->numCompressedBits;
    dst->numLeadZeroBytes = src1->numLeadZeroBytes + src2->numLeadZeroBytes;
    dst->numEntropyBytes = src1->numEntropyBytes + src2->numEntropyBytes;
}

void
histogramBin(uint32 value,int* bins,uint32 numBins) {
    if (value >= numBins)
        value = numBins-1;
    bins[value]++;
}

#define NUM_CHAR_BUCKETS 128
int charBuckets[NUM_CHAR_BUCKETS];

void
initCharBuckets() {
    for (int i=0; i<NUM_CHAR_BUCKETS; i++) {
        charBuckets[i] = 0;
    }
}

void
bucketChar(uint8 c) {
    if (c > NUM_CHAR_BUCKETS)
        return;
    charBuckets[c]++;
}

void
dumpCharHistogramHeaders() {
    ofstream co(charHistogramFileName,ios::out);
    for (int i=0; i<NUM_CHAR_BUCKETS; i++) {
        co << "CharValue" << i << ",";
    }
    co <<endl;
}

void
dumpCharHistogram() {
    ofstream co(charHistogramFileName,ios::app);
    for (int i=0; i<NUM_CHAR_BUCKETS; i++) {
        co << charBuckets[i] << ",";
    }
    co <<endl;
}


uint32
getNumBitsUnsigned(uint32 value) {
    uint32 numBits = 0;    // number of bits necessary to represent this unsigned number
    do {
        value = value >> 1;
        numBits++;
    } while (value != 0);
    return numBits;
}

uint32
getNumBitsSigned(int value) {
    uint32 numBits = 1;
    while (value != 0 && value != -1) {
        value = value >> 1;
        numBits++;
    }
    return numBits;
}

uint32
getNumBitsSigned64(int64 value) {
    uint32 numBits = 1;
    while (value != 0 && value != -1) {
        value = value >> 1;
        numBits++;
    }
    return numBits;
}

uint32
countBitsUnsigned(uint32 value,HeapSlotTypeStats* typeStats) {
    uint32 numBits = getNumBitsUnsigned(value);
    typeStats->numCompressedBits += numBits;
    histogramBin(numBits-1,&typeStats->numBitsHistogramBins[0],typeStats->numBitsPerSlot);
    HEAPTRACE(" numBits=" << (int)numBits);
    return numBits;
}

uint32
countBitsSigned(int value,HeapSlotTypeStats* typeStats) {
    uint32 numBits = getNumBitsSigned(value);
    typeStats->numCompressedBits += numBits;
    histogramBin(numBits-1,&typeStats->numBitsHistogramBins[0],typeStats->numBitsPerSlot);
    HEAPTRACE(" numBits=" << (int)numBits);
    return numBits;
}

uint32
countBitsSigned64(int64 value,HeapSlotTypeStats* typeStats) {
    uint32 numBits = getNumBitsSigned64(value);
    typeStats->numCompressedBits += numBits;
    histogramBin(numBits-1,&typeStats->numBitsHistogramBins[0],NUM_BITS_HISTOGRAM_BINS);
    HEAPTRACE(" numBits=" << (int)numBits);
    return numBits;
}

void
countTypeInt8(void* ptr,HeapSlotTypeStats* typeStats) {
    int value = *(uint8*)ptr;

    HEAPTRACE(" 0x" << hex << value << dec);

    typeStats->numSlots++;
    typeStats->numBytes++;

    countBitsUnsigned(value,typeStats);

    if (value == 0) {
        typeStats->numLeadZeroBytes++;
    } else {
        typeStats->numEntropyBytes++;
    }
}

void
countTypeInt16(void* ptr,HeapSlotTypeStats* typeStats) {
    int value = *(uint16*)ptr;

    HEAPTRACE(" 0x" << hex << value << dec << "(" << (char)value << ")");

    typeStats->numSlots++;
    typeStats->numBytes += 2;

    countBitsSigned(value,typeStats);

    if (value == 0) {
        typeStats->numLeadZeroBytes += 2;
    } else if ((value & 0xFFFFFF00) == 0) {
        // XXX - should check for sign extension
        // upper byte is zero
        typeStats->numLeadZeroBytes++;
        typeStats->numEntropyBytes += 1;
    } else {
        typeStats->numEntropyBytes += 2;
    }
}

void
countTypeUInt16(void* ptr,HeapSlotTypeStats* typeStats) {
    int value = *(uint16*)ptr;

    HEAPTRACE(" 0x" << hex << value << dec << "(" << (char)value << ")");

    typeStats->numSlots++;
    typeStats->numBytes += 2;

    bucketChar(value);
    countBitsUnsigned(value,typeStats);

    if (value == 0) {
        typeStats->numLeadZeroBytes += 2;
    } else if ((value & 0xFFFFFF00) == 0) {
        // XXX - should check for sign extension
        // upper byte is zero
        typeStats->numLeadZeroBytes++;
        typeStats->numEntropyBytes += 1;
    } else {
        typeStats->numEntropyBytes += 2;
    }
}
// Perhaps an offset figure would be valuable object base - *slot.
void
countTypeInt32(void* ptr,HeapSlotTypeStats* typeStats) {
    int value = *(uint32*)ptr;

    HEAPTRACE(" " << value);

    typeStats->numSlots++;
    typeStats->numBytes += 4;

    countBitsSigned(value,typeStats);

    if (value == 0) {
        typeStats->numLeadZeroBytes += 4;
    } else if ((value & 0xFFFFFF00) == 0) {
        // XXX - should check for sign extension
        // upper 3 bytes are zero
        typeStats->numLeadZeroBytes += 3;
        typeStats->numEntropyBytes += 1;
    } else if ((value & 0xFFFF0000) == 0) {
        // upper 2 bytes are zero
        typeStats->numLeadZeroBytes += 2;
        typeStats->numEntropyBytes += 2;
    } else if ((value & 0xFF000000) == 0) {
        // upper 1 byte is zero
        typeStats->numLeadZeroBytes += 1;
        typeStats->numEntropyBytes += 3;
    } else {
        typeStats->numEntropyBytes += 4;
    }
}

void
countTypeUInt32(void* ptr, HeapSlotTypeStats* typeStats) {
    uint32 value = *(uint32*)ptr;

    HEAPTRACE(" " << value << " ");

    typeStats->numSlots++;
    typeStats->numBytes += 4;
    
    countBitsUnsigned(value,typeStats);

    if (value == 0) {
        typeStats->numLeadZeroBytes += 4;
    } else if ((value & 0xFFFFFF00) == 0) {
        // XXX - should check for sign extension
        // upper 3 bytes are zero
        typeStats->numLeadZeroBytes += 3;
        typeStats->numEntropyBytes += 1;
    } else if ((value & 0xFFFF0000) == 0) {
        // upper 2 bytes are zero
        typeStats->numLeadZeroBytes += 2;
        typeStats->numEntropyBytes += 2;
    } else if ((value & 0xFF000000) == 0) {
        // upper 1 byte is zero
        typeStats->numLeadZeroBytes += 1;
        typeStats->numEntropyBytes += 3;
    } else {
        typeStats->numEntropyBytes += 4;
    }
}

void
countTypeInt64(void* ptr,HeapSlotTypeStats* typeStats) {
    int64 value = *(uint64*)ptr;

    HEAPTRACE(" " << hex << value << dec);

    typeStats->numSlots++;
    typeStats->numBytes += 8;
    
    countBitsSigned64(value,typeStats);
    
    if (value == 0L) {
        typeStats->numLeadZeroBytes += 8;
    } else if ((value & 0xFFFFFFFFFFFFFF00L) == 0) {
        // upper 7 bytes are zero
        typeStats->numLeadZeroBytes += 7;
        typeStats->numEntropyBytes += 1;
    } else if ((value & 0xFFFFFFFFFFFF0000L) == 0) {
        // upper 6 bytes are zero
        typeStats->numLeadZeroBytes += 6;
        typeStats->numEntropyBytes += 2;
    } else if ((value & 0xFFFFFFFFFF000000L) == 0) {
        // upper 5 bytes are zero
        typeStats->numLeadZeroBytes += 5;
        typeStats->numEntropyBytes += 3;
    } else if ((value & 0xFFFFFFFF00000000L) == 0) {
        // upper 4 bytes are zero
        typeStats->numLeadZeroBytes += 4;
        typeStats->numEntropyBytes += 4;
    } else if ((value & 0xFFFFFF0000000000L) == 0) {
        // upper 3 bytes are zero
        typeStats->numLeadZeroBytes += 3;
        typeStats->numEntropyBytes += 5;
    } else if ((value & 0xFFFF000000000000L) == 0) {
        // upper 2 bytes are zero
        typeStats->numLeadZeroBytes += 2;
        typeStats->numEntropyBytes += 6;
    } else if ((value & 0xFF00000000000000L) == 0) {
        // upper 1 bytes is zero
        typeStats->numLeadZeroBytes += 1;
        typeStats->numEntropyBytes += 7;
    } else {
        typeStats->numEntropyBytes += 8;
    }
}

void
countTypeFloat(void* ptr,HeapSlotTypeStats* typeStats) {
    float value = *(float*)ptr;
    typeStats->numSlots++;
    typeStats->numBytes += 4;
    if (value == 0.0) {
        typeStats->numLeadZeroBytes += 4;
    } else {
        typeStats->numEntropyBytes += 4;
    }
    HEAPTRACE(" " << value);
}

void
countTypeDouble(void* ptr,HeapSlotTypeStats* typeStats) {
    double value = *(double*)ptr;
    typeStats->numSlots++;
    typeStats->numBytes += 8;
    if (value == 0.0) {
        typeStats->numLeadZeroBytes += 8;
    } else {
        typeStats->numEntropyBytes += 8;
    }
    HEAPTRACE(" " << value);
}



// Pointers are 32 bit quantities in some places so the first arg needs to be able
// to deal with 32 bit compressed refs
void
countTypeRef(void* ptr,HeapSlotTypeStats* refStats,HeapSlotTypeStats* relativeRefStats) {
    countTypeInt32(ptr, refStats);
    Partial_Reveal_Object* value = ((Slot) ptr).dereference();

    // now try relative addressing
    // This should be a 64  bit value
    int64 relValue = (value == NULL)?0:(char*)value - (char*)ptr;
    relValue = relValue >> 2;   // get rid of bottom zeroes
    countTypeInt32(&relValue,relativeRefStats);
}

// raw heap word-sized heap slots
HeapSlotTypeStats rawWordStats          = {"RawWords",          32,0,0,0,0};
//
// object overheads
//
HeapSlotTypeStats vtablePtrStats        = {"VTablePtr",         32,0,0,0,0};
HeapSlotTypeStats vtableIdStats         = {"VTableId",          32,0,0,0,0};
HeapSlotTypeStats objInfoStats          = {"ObjInfo",           32,0,0,0,0};

//
// non-array object field
//
HeapSlotTypeStats byteFieldStats        = {"ByteFields",        8,0,0,0,0};
HeapSlotTypeStats boolFieldStats        = {"BoolFields",        8,0,0,0,0};
HeapSlotTypeStats charFieldStats        = {"CharFields",        16,0,0,0,0};
HeapSlotTypeStats shortFieldStats       = {"ShortFields",       16,0,0,0,0};
HeapSlotTypeStats intFieldStats         = {"IntFields",         32,0,0,0,0};
HeapSlotTypeStats longFieldStats        = {"LongFields",        64,0,0,0,0};
HeapSlotTypeStats floatFieldStats       = {"FloatFields",       32,0,0,0,0};
HeapSlotTypeStats doubleFieldStats      = {"DoubleFields",      64,0,0,0,0};
HeapSlotTypeStats refFieldStats         = {"RefFields",         32,0,0,0,0};
HeapSlotTypeStats relativeRefFieldStats = {"RelativeRefFields", 30,0,0,0,0};
//
// array object length & elements
//
HeapSlotTypeStats arrayLenStats         = {"ArrayLen",          32,0,0,0,0};
HeapSlotTypeStats byteElemStats         = {"ByteElems",         8,0,0,0,0};
HeapSlotTypeStats boolElemStats         = {"BoolElems",         8,0,0,0,0};
HeapSlotTypeStats charElemStats         = {"CharElems",         16,0,0,0,0};
HeapSlotTypeStats shortElemStats        = {"ShortElems",        16,0,0,0,0};
HeapSlotTypeStats intElemStats          = {"IntElems",          32,0,0,0,0};
HeapSlotTypeStats longElemStats         = {"LongElems",         64,0,0,0,0};
HeapSlotTypeStats floatElemStats        = {"FloatElems",        32,0,0,0,0};
HeapSlotTypeStats doubleElemStats       = {"DoubleElems",       64,0,0,0,0};
HeapSlotTypeStats refElemStats          = {"RefElems",          32,0,0,0,0};
HeapSlotTypeStats relativeRefElemStats  = {"RelativeRefElems",  30,0,0,0,0};
//
// Stats for slots of each type
//
HeapSlotTypeStats byteSlotStats        = {"ByteSlots",        8,0,0,0,0};
HeapSlotTypeStats boolSlotStats        = {"BoolSlots",        8,0,0,0,0};
HeapSlotTypeStats charSlotStats        = {"CharSlots",        16,0,0,0,0};
HeapSlotTypeStats shortSlotStats       = {"ShortSlots",       16,0,0,0,0};
HeapSlotTypeStats intSlotStats         = {"IntSlots",         32,0,0,0,0};
HeapSlotTypeStats longSlotStats        = {"LongSlots",        64,0,0,0,0};
HeapSlotTypeStats floatSlotStats       = {"FloatSlots",       32,0,0,0,0};
HeapSlotTypeStats doubleSlotStats      = {"DoubleSlots",      64,0,0,0,0};
HeapSlotTypeStats refSlotStats         = {"RefSlots",         32,0,0,0,0};
HeapSlotTypeStats relativeRefSlotStats = {"RelativeRefSlots", 30,0,0,0,0};


HeapSlotTypeStats* heapStatsArray[] = {

    &vtablePtrStats,        &objInfoStats,      &arrayLenStats,

    &byteFieldStats,        &byteElemStats,
    &boolFieldStats,        &boolElemStats,
    &charFieldStats,        &charElemStats,
    &shortFieldStats,       &shortElemStats,
    &intFieldStats,         &intElemStats,
    &longFieldStats,        &longElemStats,
    &floatFieldStats,       &floatElemStats,
    &doubleFieldStats,      &doubleElemStats,
    &refFieldStats,         &refElemStats,

    &byteSlotStats,
    &boolSlotStats,
    &charSlotStats,
    &shortSlotStats,
    &intSlotStats,
    &longSlotStats,
    &floatSlotStats,
    &doubleSlotStats,
    &refSlotStats,

    &relativeRefFieldStats, &relativeRefElemStats, &relativeRefSlotStats,
    &vtableIdStats,
    &rawWordStats,

    NULL
};

HeapSlotTypeStats* intSizeHistograms[] = {
    &charSlotStats,     &charFieldStats,        &charElemStats,
    &shortSlotStats,    &shortFieldStats,       &shortElemStats,
    &intSlotStats,      &intFieldStats,         &intElemStats,
    &arrayLenStats,
    &rawWordStats,
    NULL
};

HeapSlotTypeStats* refSizeHistograms[] = {
    &refSlotStats,          &refFieldStats,         &refElemStats,
    &relativeRefSlotStats,  &relativeRefFieldStats, &relativeRefElemStats,
    &vtablePtrStats,        &vtableIdStats,
    NULL
};

HeapSlotTypeStats* longSizeHistograms[] = {
    &byteSlotStats,     &byteFieldStats,    &byteElemStats,
    &longSlotStats,     &longFieldStats,    &longElemStats,
    NULL
};

HeapSlotTypeStats* floatSizeHistograms[] = {
    &floatSlotStats,    &floatFieldStats,   &floatElemStats,
    &doubleSlotStats,   &doubleFieldStats,  &doubleElemStats,
    NULL
};


void
dumpHeapSlotStats(ostream& co) {
    uint32 j;
    // dump the size in bytes
    for (j=0; heapStatsArray[j] != NULL; j++) {
        HeapSlotTypeStats* typeStats = heapStatsArray[j];
        co << typeStats->numBytes << ",";
    }
    // dump the lead zero information
    for (j=0; heapStatsArray[j] != NULL; j++) {
        HeapSlotTypeStats* typeStats = heapStatsArray[j];
        co << typeStats->numLeadZeroBytes << ",";
    }
    // dump the number of slots for each kind
    for (j=0; heapStatsArray[j] != NULL; j++) {
        HeapSlotTypeStats* typeStats = heapStatsArray[j];
        co << typeStats->numSlots << ",";
    }
    // dump the number of compressed bits
    for (j=0; heapStatsArray[j] != NULL; j++) {
        HeapSlotTypeStats* typeStats = heapStatsArray[j];
        // convert to bytes
        co << ((typeStats->numCompressedBits+7) >> 3) << ",";
    }

}

void
dumpHeapSlotStatHeaders(ostream& co) {
    uint32 j;
    // dump the size in bytes
    for (j=0; heapStatsArray[j] != NULL; j++) {
        HeapSlotTypeStats* typeStats = heapStatsArray[j];
        const char* name = typeStats->name;
        co << "numBytesOf" << name << ",";
    }
    // dump the lead zero information
    for (j=0; heapStatsArray[j] != NULL; j++) {
        HeapSlotTypeStats* typeStats = heapStatsArray[j];
        const char* name = typeStats->name;
        co << "numBytesOfLeadZeroesIn" << name << ",";
    }
    // dump the number of slots for each kind
    for (j=0; heapStatsArray[j] != NULL; j++) {
        HeapSlotTypeStats* typeStats = heapStatsArray[j];
        const char* name = typeStats->name;
        co << "num" << name << ",";
    }
    for (j=0; heapStatsArray[j] != NULL; j++) {
        HeapSlotTypeStats* typeStats = heapStatsArray[j];
        const char* name = typeStats->name;
        co << "num" << name << "CompressedBytes,";
    }
}


void
dumpHeapSlotStatHistogramHeader(ostream& co,HeapSlotTypeStats* typeStats) {
    int numBins = typeStats->numBitsPerSlot;
    if (numBins > NUM_BITS_HISTOGRAM_BINS)
        numBins = NUM_BITS_HISTOGRAM_BINS;

    for (int i=0; i<numBins; i++) {
        if (i != 0)
            co << ",";
        co << typeStats->name << (i+1);
    }
}

void
dumpHeapSlotStatHistogram(ostream& co,HeapSlotTypeStats* typeStats) {
    int numBins = typeStats->numBitsPerSlot;
    if (numBins > NUM_BITS_HISTOGRAM_BINS)
        numBins = NUM_BITS_HISTOGRAM_BINS;

    for (int i=0; i<numBins; i++) {
        if (i != 0)
            co << ",";
        co << typeStats->numBitsHistogramBins[i];
    }
}

void
dumpHeapSlotStatHistogramHeaders(char* name,HeapSlotTypeStats** typeStatsArray) {
    char filename[100];
    sprintf(filename,objectBitsHistogramsFileNameFormat,name);
    ofstream co(filename,ios::out);
    for (int j=0; typeStatsArray[j] != NULL; j++) {
        if (j != 0)
            co << ",";
        dumpHeapSlotStatHistogramHeader(co,typeStatsArray[j]);
    }
    co << endl;
}
void
dumpHeapSlotStatHistograms(char* name,HeapSlotTypeStats** typeStatsArray) {
    char filename[100];
    sprintf(filename,objectBitsHistogramsFileNameFormat,name);
    ofstream co(filename,ios::app);
    for (int j=0; typeStatsArray[j] != NULL; j++) {
        if (j != 0)
            co << ",";
        dumpHeapSlotStatHistogram(co,typeStatsArray[j]);
    }
    co << endl;
}

void
dumpHeapSlotStatHistogramHeaders() {
    dumpHeapSlotStatHistogramHeaders("Int",intSizeHistograms);
    dumpHeapSlotStatHistogramHeaders("Ref",refSizeHistograms);
    dumpHeapSlotStatHistogramHeaders("Long",longSizeHistograms);
    dumpHeapSlotStatHistogramHeaders("Float",floatSizeHistograms);
}

void
dumpHeapSlotStatHistograms() {
    dumpHeapSlotStatHistograms("Int",intSizeHistograms);
    dumpHeapSlotStatHistograms("Ref",refSizeHistograms);
    dumpHeapSlotStatHistograms("Long",longSizeHistograms);
    dumpHeapSlotStatHistograms("Float",floatSizeHistograms);
}

// number of objects
int numObjects      = 0;
int numArrays       = 0;
int numNonArrays    = 0;
int numArrayBytes   = 0;
int numNonArrayBytes= 0;
int numObjectFieldPaddingBytes = 0;
int numArrayPaddingBytes = 0;
// raw bytes
int numTotalBytes   = 0;
//
// raw zero bytes
//
int numZeroBytes    = 0; // number of bytes that are zero
int numNonZeroBytes = 0;
int numCompressedBytes = 0;
int numCompressedOverheadBytes = 0;

struct stat_s {char* name; int* value;} heapTraceStats[] = {
    {"dumpId", &dumpId},
    // number of objects
    {"numObjects",&numObjects},
    {"numArrays",&numArrays},
    {"numNonArrays",&numNonArrays},
    {"numArrayBytes", &numArrayBytes},
    {"numNonArrayBytes",&numNonArrayBytes},
    // raw bytes
    {"numTotalBytes", &numTotalBytes},
    // raw zero bytes
    {"numNonZeroBytes",&numNonZeroBytes},
    {"numZeroBytes",&numZeroBytes},
    {"numCompressedBytes", &numCompressedBytes},
    {"numCompressedOverheadBytes", &numCompressedOverheadBytes},
    {"numArrayPaddingBytes", &numArrayPaddingBytes},
    {"numObjectFieldPaddingBytes", &numObjectFieldPaddingBytes},
    {NULL,0}
};

void
dumpHeapTraceStats(ostream& co) {
    for (uint32 i=0; heapTraceStats[i].name != NULL; i++) {
        co << *heapTraceStats[i].value << ",";
    }
}

void
dumpHeapTraceHeaders(ostream& co) {
    // print out column headers
    for (uint32 i=0; heapTraceStats[i].name != NULL; i++) 
        co << heapTraceStats[i].name << ",";
}

#define NUM_OBJECT_SIZE_BUCKETS 32
#define BUCKET_SHIFT_AMOUNT 3

//
// histogram of sizes for all objects
//
int numObjectsPerBucket[NUM_OBJECT_SIZE_BUCKETS];
int totalBytesPerBucket[NUM_OBJECT_SIZE_BUCKETS];
//
// histogram of sizes for arrays
//
int numArraysPerBucket[NUM_OBJECT_SIZE_BUCKETS];
int numArrayBytesPerBucket[NUM_OBJECT_SIZE_BUCKETS];
//
// histogram of sizes for nonarray objects
//
int numNonarraysPerBucket[NUM_OBJECT_SIZE_BUCKETS];
int numNonarrayBytesPerBucket[NUM_OBJECT_SIZE_BUCKETS];

void
initObjectSizeBuckets() {
    for (int32 i=0; i<NUM_OBJECT_SIZE_BUCKETS; i++) {
        numObjectsPerBucket[i] = totalBytesPerBucket[i] = 0;
        numArraysPerBucket[i] = numArrayBytesPerBucket[i] = 0;
        numNonarraysPerBucket[i] = numNonarrayBytesPerBucket[i] = 0;
    }
}

void
bucketObject(int objectSize,int numPerBucket[],int bytesPerBucket[]) {
    int bucketIndex = objectSize >> BUCKET_SHIFT_AMOUNT;
    if (bucketIndex >= NUM_OBJECT_SIZE_BUCKETS)
        bucketIndex = NUM_OBJECT_SIZE_BUCKETS-1;
    numPerBucket[bucketIndex]++;
    bytesPerBucket[bucketIndex] += objectSize;
}

void
dumpObjectSizeHistogramHeader(ostream& co,const char* prefix) {
    uint32 size = 0;
    uint32 increment = 1 << BUCKET_SHIFT_AMOUNT;
    for (int32 i=0; i<NUM_OBJECT_SIZE_BUCKETS; i++) {
        co << "num" << prefix << (int)size;
        if (i == NUM_OBJECT_SIZE_BUCKETS-1)
            co << "+";
        co << ",";
        size += increment;
    }
    size = 0;
    for (i=0; i<NUM_OBJECT_SIZE_BUCKETS; i++) {
        co << "bytes" << prefix << (int)size;
        if (i == NUM_OBJECT_SIZE_BUCKETS-1)
            co << "+";
        co << ",";
        size += increment;
    }
}

void
dumpObjectSizeHistogramHeaders() {
    ofstream co(objectSizeHistogramsFileName,ios::out);
    dumpObjectSizeHistogramHeader(co,"Object");
    dumpObjectSizeHistogramHeader(co,"Array");
    dumpObjectSizeHistogramHeader(co,"Nonarray");
    co << endl;
}

void
dumpObjectSizeHistograms() {
    ofstream co(objectSizeHistogramsFileName,ios::app);
    for (int32 i=0; i<NUM_OBJECT_SIZE_BUCKETS; i++) {
        co << numObjectsPerBucket[i] << ",";
    }
    for (i=0; i<NUM_OBJECT_SIZE_BUCKETS; i++) {
        co << totalBytesPerBucket[i] << ",";
    }
    for (i=0; i<NUM_OBJECT_SIZE_BUCKETS; i++) {
        co << numArraysPerBucket[i] << ",";
    }
    for (i=0; i<NUM_OBJECT_SIZE_BUCKETS; i++) {
        co << numArrayBytesPerBucket[i] << ",";
    }
    for (i=0; i<NUM_OBJECT_SIZE_BUCKETS; i++) {
        co << numNonarraysPerBucket[i] << ",";
    }
    for (i=0; i<NUM_OBJECT_SIZE_BUCKETS; i++) {
        co << numNonarrayBytesPerBucket[i] << ",";
    }
    co << endl;
}
//Forward reference.
void init_type_counts ();
void begin_type_counts (); 

void heapTraceInit() {
    HEAPTRACE("XXX - heapTraceInit");
    // initialize the stats file by writing out the column headers
    ofstream co(statsFileName,ios::out);
    dumpHeapTraceHeaders(co);
    dumpHeapSlotStatHeaders(co);
    co << endl;
    // initialize the object bits histograms file
    dumpHeapSlotStatHistogramHeaders();
    // initialize the histograms file by writing out the column headers
    dumpObjectSizeHistogramHeaders();

    dumpCharHistogramHeaders();
    init_type_counts ();
    
    HEAPTRACE("XXX - heapTraceInit done");
}

void dump_type_counts(bool on_exit); // forward reference.

void dumpTrace (bool on_exit)
{

    HEAPTRACE("XXX - heapTraceEnd: writing stats for dump " << dumpId);
    //
    // total up the stats
    //
    addStats(&byteSlotStats,&byteFieldStats,&byteElemStats);
    addStats(&boolSlotStats,&boolFieldStats,&boolElemStats);
    addStats(&shortSlotStats,&shortFieldStats,&shortElemStats);
    addStats(&charSlotStats,&charFieldStats,&charElemStats);
    addStats(&intSlotStats,&intFieldStats,&intElemStats);
    addStats(&longSlotStats,&longFieldStats,&longElemStats);
    addStats(&floatSlotStats,&floatFieldStats,&floatElemStats);
    addStats(&doubleSlotStats,&doubleFieldStats,&doubleElemStats);
    addStats(&refSlotStats,&refFieldStats,&refElemStats);
    addStats(&relativeRefSlotStats,&relativeRefFieldStats,&relativeRefElemStats);

    //
    // total compressed bits
    //
    numCompressedBytes = byteSlotStats.numCompressedBits + boolSlotStats.numCompressedBits +
                        shortSlotStats.numCompressedBits + charSlotStats.numCompressedBits +
                        intSlotStats.numCompressedBits + longSlotStats.numCompressedBits +
                        floatSlotStats.numCompressedBits + doubleSlotStats.numCompressedBits +
                        relativeRefSlotStats.numCompressedBits +
                        arrayLenStats.numCompressedBits + objInfoStats.numCompressedBits +
                        vtableIdStats.numCompressedBits;

    numCompressedBytes = (numCompressedBytes + 7) >> 3;
    numCompressedOverheadBytes = (byteSlotStats.numSlots * 3) +
                                 (shortSlotStats.numSlots * 4) +
                                 (charSlotStats.numSlots * 4) +
                                 (intSlotStats.numSlots * 5) +
                                 (longSlotStats.numSlots * 6) +
                                 (relativeRefSlotStats.numSlots * 5) +
                                 (vtableIdStats.numSlots * 5);

    numCompressedOverheadBytes = (numCompressedOverheadBytes + 7) >> 3;

    // dump the counts
    ofstream co(statsFileName,ios::app);
    dumpHeapTraceStats(co);
    dumpHeapSlotStats(co);
    co << endl;

    // dump the bit sizes histograms
    dumpHeapSlotStatHistograms();

    // dump the histograms
    dumpObjectSizeHistograms();

    // dump the character histograms
    dumpCharHistogram();

    // close any open streams
    if (dumpStream != NULL)
        delete dumpStream;
    dumpStream = NULL;
    if (charDumpStream != NULL)
        delete charDumpStream;
    charDumpStream = NULL;
    dump_type_counts (on_exit);
}

void heapTraceBegin(bool before_gc) {
    //
    // A - We need to characterize at the end of every GC to get a characterization of live objects.
    // B - We need to characterize at the start of every GC to get a characterizatin of live + dead objects.
    // You want a characterization of allocated objects just subract A from B 
    //
    // If we are before the GC then what we have in the characterization tables are the 
    // types of objects live at the end of the last GC *plus* all the objects that have
    // been allocated since the last GC.
    // We dump these out,
    // If one wants the characterization of the objects allocated since the last
    // collection then on can take the difference bet
    // We then clear the counts but basically ignore the data collected between before_gc being true
    // and a call to before_gc being false. If it is false we clear the counts 
    // 
    // 
    // Now in heapTraceEnd with before_gc true we will dump out the
    // live object characteristics.
    // If before_gc is false then we clear the counts here and recharacterize the heap.
    // heapTraceEnd will dump this characterization out. This means that 
    // Each GC will cause three entries to be dumped.
    assert(before_gc);
    if (before_gc) {
        dumpTrace (false);
    }

    initObjectSizeBuckets();
    initCharBuckets();
    // initialize counts to zero
    for (uint32 i=0; heapTraceStats[i].name != NULL; i++) {
        *heapTraceStats[i].value = 0;
    }
    for (uint32 j=0; heapStatsArray[j] != NULL; j++) {
        initStats(heapStatsArray[j]);
    }
    dumpId = dumpNumber++;
    if (heapTraceDumpHeapFlag == true) {
        char fileName[100];
        sprintf(fileName,dumpFileName,dumpId);
        dumpStream = new ofstream(fileName,ios::out);
    }
    if (heapTraceDumpCharsFlag == true) {
        char fileName[100];
        sprintf(fileName,charDumpFileName,dumpId);
        charDumpStream = new ofstream(fileName,ios::out);
    }
    
    begin_type_counts ();
}

void 
heapTraceEnd(bool before_gc) {
    assert (!before_gc);
    // At the end of a GC dump out the trace - this is a characterization of all live objects.
    if (!before_gc) {
        dumpTrace(true);
    }
}

// Called after *all* the heap tracing is done for this run. This will
// output some type names collected common to all the heap traces.

void 
heapTraceFinalize() {
    void type_counts_finalize (); // Forward reference.
    type_counts_finalize ();
}

char *getVectorDataStart(Partial_Reveal_Object *vector) {
    char *result = (char *)vector + 
        vector_first_element_offset_vtable_handle((VTable_Handle) vector->vt()); 
    return result;
}

uint32
traceByteElems(Partial_Reveal_Object *vector) {
    int32 length = vector_get_length(vector);
    uint8 *body = (uint8 *)getVectorDataStart(vector); // The data in the vector, used as an array of uint8.
    
    for (int32 i=0; i<length; i++) {
        countTypeInt8(&body[i],&byteElemStats);
    }
    return length;
}

uint32
traceBoolElems(Partial_Reveal_Object* vector) {
    int32 length = vector_get_length(vector);
    uint8 *body  = (uint8 *)getVectorDataStart(vector);; // The data in the vector, used as an array of uint8.

    for (int32 i=0; i<length; i++) {
        countTypeInt8(&body[i],&boolElemStats);
    }
    return length;
}

uint32
traceCharElems(Partial_Reveal_Object* vector) {
    int32 length = vector_get_length(vector);
    uint16 *body  = (uint16 *)getVectorDataStart(vector);; // The data in the vector, used as an array of uint8.

    for (int32 i=0; i<length; i++) {
        countTypeUInt16(&body[i],&charElemStats);
    }
    return length*2;
}

uint32
traceShortElems(Partial_Reveal_Object* vector) {
    int32 length = vector_get_length(vector);
    uint16 *body  = (uint16 *)getVectorDataStart(vector);; // The data in the vector, used as an array of uint8.

    for (int32 i=0; i<length; i++) {
        countTypeInt16(&body[i],&shortElemStats);
    }
    return length*2;
}

uint32
traceIntElems(Partial_Reveal_Object* vector) {
    int32 length = vector_get_length(vector);
    uint32 *body  = (uint32 *)getVectorDataStart(vector);; // The data in the vector, used as an array of uint8.

    for (int32 i=0; i<length; i++) {
        countTypeInt32(&body[i],&intElemStats);
    }
    return length*4;
}

uint32
traceLongElems(Partial_Reveal_Object* vector) {
    int32 length = vector_get_length(vector);
    int64 *body  = (int64 *)getVectorDataStart(vector);; // The data in the vector, used as an array of uint8.

    for (int32 i=0; i<length; i++) {
        countTypeInt64(&body[i],&longElemStats);
    }
    return length*8;
}

uint32
traceFloatElems(Partial_Reveal_Object* vector) {
    int32 length = vector_get_length(vector);
    float *body  = (float *)getVectorDataStart(vector);; // The data in the vector, used as an array of uint8.

    for (int32 i=0; i<length; i++) {
        countTypeFloat(&body[i],&floatElemStats);
    }
    return length*4;
}

uint32
traceDoubleElems(Partial_Reveal_Object* vector) {
    int32 length = vector_get_length(vector);
    double *body  = (double *)getVectorDataStart(vector);; // The data in the vector, used as an array of uint8.

    for (int32 i=0; i<length; i++) {
        countTypeDouble(&body[i],&doubleElemStats);
    }
    return length*8;
}

// This needs to deal with both 32 and 64 bit pointers
uint32
traceRefElems(Partial_Reveal_Object* vector) {
    int32 length = vector_get_length(vector);
    uint32 *body  = (uint32 *)getVectorDataStart(vector);; // The data in the vector, used as an array of uint8.
    for (int32 i=0; i<length; i++) {
        countTypeRef(&body[i],&refElemStats, &relativeRefElemStats);
    }
    return length*4;
}

uint32
traceArrayElems(Partial_Reveal_Object* vector) {
    // get type of elements
    Class_Handle elem_class = class_get_array_element_class(vector->vt()->get_gcvt()->gc_clss);
    VM_Data_Type elemJavaType = class_get_primitive_type_of_class(elem_class);
    int32 length = vector_get_length(vector);
    // Dump the chars
    if (heapTraceDumpCharsFlag == true && 
        elemJavaType == JAVA_TYPE_CHAR) {
        char* bytes = (char*)getVectorDataStart(vector);

        int sizeInBytes = length * 2;
        for (int i=0; i<sizeInBytes; i++) {
            if (bytes[i] == 0)
                continue;
            charDumpStream->put(bytes[i]);
        }
        charDumpStream->put((char)0);
    }
    HEAPTRACE("\n\tlen=" << length
        << "\n\telems: ");
    countTypeUInt32(&length, &arrayLenStats); // Count the length as a uint32
    switch (elemJavaType) {
        case JAVA_TYPE_BYTE:
            return traceByteElems(vector);
        case JAVA_TYPE_BOOLEAN:
            return traceBoolElems(vector);
        case JAVA_TYPE_CHAR:
            return traceCharElems(vector);
        case JAVA_TYPE_SHORT:
            return traceShortElems(vector);
        case JAVA_TYPE_INT:
            return traceIntElems(vector);
        case JAVA_TYPE_LONG:
            return traceLongElems(vector);
        case JAVA_TYPE_FLOAT:
            return traceFloatElems(vector);
        case JAVA_TYPE_DOUBLE:
            return traceDoubleElems(vector);
        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
            return traceRefElems(vector);
        default: 
            ABORT("Unexpected java type");
    }
    return 0;
}

uint32
traceField(Field_Handle field, Partial_Reveal_Object* p_object) {
    char* fieldBytes = (char*)p_object + field_get_offset(field);
    Java_Type javaType = type_info_get_type (field_get_type_info_of_field_value (field));
    HEAPTRACE("\t" << field_get_name(field) << "@" << field_get_offset(field)
         << ":" << (char)javaType
         << "=");
    uint32 fieldSize = 0;
    switch (javaType) {
        case JAVA_TYPE_BYTE:
            countTypeInt8(fieldBytes,&byteFieldStats);
            fieldSize = 1;
            break;
        case JAVA_TYPE_BOOLEAN:
            countTypeInt8(fieldBytes,&boolFieldStats);
            fieldSize = 1;
            break;
        case JAVA_TYPE_CHAR:
            countTypeUInt16(fieldBytes,&charFieldStats);
            fieldSize = 2;
            break;
        case JAVA_TYPE_SHORT:
            countTypeInt16(fieldBytes,&shortFieldStats);
            fieldSize = 2;
            break;
        case JAVA_TYPE_INT:
            if (0)
            {
                uint32 value = *(uint32*)fieldBytes;
                uint32 numBits = getNumBitsSigned(value);
                POINTER_SIZE_INT value_as_addr = (POINTER_SIZE_INT) value;
                if (numBits > 28) {
                    HEAPTRACE("&&&&&& ------- large int: "
                         << class_get_name(field_get_class_of_field_value(field)) << "."
                         << field_get_name(field)
                         << " value=" << (int)value << "("<< (void*)value_as_addr << ")"
                         << " numBits=" << (int)numBits);
                }
            }
            countTypeInt32(fieldBytes,&intFieldStats);
            fieldSize = 4;
            break;
        case JAVA_TYPE_LONG:
            if (0)
            {
                int64 value = *(int64*)fieldBytes;
                uint32 numBits = getNumBitsSigned64(value);
                if (numBits > 39) {
                    HEAPTRACE("&&&&&& ------- large long: "
                         << class_get_name(field_get_class_of_field_value(field)) << "."
                         << field_get_name(field)
                         << " value=" << (int)value
                         << " numBits=" << (int)numBits);
                }
            }
            countTypeInt64(fieldBytes,&longFieldStats);
            fieldSize = 8;
            break;
        case JAVA_TYPE_FLOAT:
            countTypeFloat(fieldBytes,&floatFieldStats);
            fieldSize = 4;
            break;
        case JAVA_TYPE_DOUBLE:
            countTypeDouble(fieldBytes,&doubleFieldStats);
            fieldSize = 8;
            break;
        case JAVA_TYPE_CLASS:
        case JAVA_TYPE_ARRAY:
            countTypeRef(fieldBytes,&refFieldStats,&relativeRefFieldStats);
            fieldSize = 4;
            break;
        default: ABORT("Unexpected java type");
    }
    return fieldSize;
}

// Returns the unboxed size.
uint32 
traceObjectFields(Class_Handle clss, Partial_Reveal_Object* p_object) {
    uint32 totalFieldSize = 0;
    if (clss == NULL) {
        return totalFieldSize;
    }
    int num_of_fields = class_num_instance_fields_recursive(clss);
    for (uint32 i=0; i<class_num_instance_fields_recursive(clss); i++) {
        Field_Handle field = class_get_instance_field_recursive(clss, i);
        // we ignore static fields
        totalFieldSize += traceField(field, p_object);
    }
    return totalFieldSize;
}

// Use this as a large grain lock so only one object is processed by the characterize_object routine.
LONG characterize_lock = 0;

void 
heapTraceObject(Partial_Reveal_Object* p_object, int size) {
    if (!characterize_heap) return;
    Class_Handle objectClass = p_object->vt()->get_gcvt()->gc_clss;
    //
    // skip over java/lang/Class
    //
    if (strcmp(class_get_name(objectClass), "java/lang/Class") == 0) {
        return;
    }

    assert (sizeof(LONG) == 4);
    while (apr_atomic_cas32((volatile uint32 *)&characterize_lock, 1, 0)) {
        while (characterize_lock) {
        }
    }
        
    unsigned int sizeInBytes = size;
    unsigned int sizeInWords = sizeInBytes >> 2;

    HEAPTRACE("XXX - object@" << (void*)p_object
             << ":" << class_get_name(objectClass) 
             << "[size=" << sizeInBytes << "]" << endl << "\t");
    bucketObject(sizeInBytes,numObjectsPerBucket,totalBytesPerBucket);
    // update counts
    numObjects++;
    numTotalBytes += sizeInBytes;
    uint32* words = (uint32*)p_object;
    for (uint32 i=0; i<sizeInWords; i++) {
        uint8* byte = (uint8*)words;
        for (uint32 j=0; j < 4; j++) {
            if (*byte == 0)
                numZeroBytes++;
            else 
                numNonZeroBytes++;
            byte++;
        }
        countTypeInt32(words,&rawWordStats);
        words++;
    }

    HEAPTRACE("\n\tvtable@0=");
    // The vt is the first 32 bits of the object here. 
    countTypeInt32(&p_object,&vtablePtrStats);
    HEAPTRACE("\n\tclassId@0=");

    countTypeUInt32(p_object->vt()->gcvt_address(), &vtableIdStats);
    HEAPTRACE("\n\tobjInfo@4=");
    countTypeUInt32(p_object->obj_info_addr(),&objInfoStats);
    uint32 objectDataSize = sizeof(Partial_Reveal_Object);
    if (class_is_array (objectClass)) {
        // array type
        numArrays++;
        numArrayBytes += sizeInBytes;
        objectDataSize += traceArrayElems(p_object);
        numArrayPaddingBytes += (sizeInBytes - objectDataSize);
        bucketObject(sizeInBytes,numArraysPerBucket,numArrayBytesPerBucket);
    } else {
        // non-array type
        numNonArrays++;
        numNonArrayBytes += sizeInBytes;
        objectDataSize += traceObjectFields(objectClass,p_object);
        numObjectFieldPaddingBytes += (sizeInBytes - objectDataSize);
        bucketObject(sizeInBytes,numNonarraysPerBucket,numNonarrayBytesPerBucket);
    }
    assert(sizeInBytes >= objectDataSize);

    HEAPTRACE("\n");
    // Dump the raw bytes
    if (heapTraceDumpHeapFlag == true) {
        char* bytes = (char*)p_object;
        for (uint32 i=0; i<sizeInBytes; i++) {
            dumpStream->put(bytes[i]);
        }
    }
    void characterize_types (Partial_Reveal_VTable *vt, int size); // forward ref
    characterize_types (p_object->vt(), sizeInBytes);

    assert (sizeof(LONG) == 4);
    if (!(apr_atomic_cas32((volatile uint32 *)&characterize_lock, 0, 1))) {
        DIE("characterize lock went bad in characterize heap.");
    }
}


static LONG xchar_array_size = 0;
static LONG xchar_array_count = 0;
static LONG xend_of_gc_char_array_count = 0;
static LONG xobject_count = 0;
static int thousand_count = 1;

void characterize_types (Partial_Reveal_VTable *vt, int size); // forward ref. 

void
heapTraceAllocation(Partial_Reveal_Object* p_object, int size) 
{

    assert (sizeof(LONG) == 4);
    while (apr_atomic_cas32((volatile uint32 *)&characterize_lock, 1, 0)) {
        while (characterize_lock) {
        }
    }
        
    // size of arrays can't be determined since the length field will be zero here so size must be passed in.....
    assert(size);
    assert(p_object->vt());
    characterize_types(p_object->vt(), size);
    
    if (apr_atomic_cas32((volatile uint32 *)&characterize_lock, 0, 1) == 0) {
        ABORT("Unexpected lock value");
    }
}


//
// This code dumps out the type and thier frequencies.
// Each line holds the counts comma deliminated and ready for Excel.
// Since we do not know all the types that might exist when the first line is dumped out we
// need to delay dumping out the type names until the final line. 
// The implementation therefore requires a call to end_type_counts to dump the names of the types.
//  
//
// If we want to characterize the allocation part of this code we need to do it with 
//
// -Dcharacterize_heap=on -inline_allocation:off
//
// This allows the GC code to get a crack at the allocations so we can characterize them.
//


// If we are counting types then we need to initialize this from inside verify_init.
Count_Hash_Table *type_counts = 0;
// The total size of the type (in bytes)
Count_Hash_Table *type_size_counts = 0;
// The order that the types are to be dumped out in.
VTable_List *type_order = 0; 
// Call this before the first verification in order to make sure all of the demographic data is initialized.
// This is called at each GC. At the first GC the Count_Hash_Tables are created and from then on the counts
// are cleared but the type names are maintained.
void init_type_counts () 
{
    if (!type_counts) {
        type_counts = new Count_Hash_Table();
        type_size_counts = new Count_Hash_Table();
        type_order = new VTable_List();
        
        ofstream cosize(typeSizeHistogramsFileName,ios::out);
        ofstream cocount(typeCountHistogramsFileName,ios::out);
        
        cocount << "dump_id, Total Count, Type names at end of file... " << endl;
        cosize << "dump_id, Total Size (bytes), Type names at end of file..." << endl;
    } 

    type_counts->zero_counts();
    type_size_counts->zero_counts();
    TRACE("initing type counts.");
}

void begin_type_counts () 
{
    assert (type_counts);

    type_counts->zero_counts();
    type_size_counts->zero_counts();
    TRACE("initing type counts.");
}

void type_counts_finalize ()
{
    // this routine dumps out the types in a comma delimited fashion.
    ofstream cosize(typeSizeHistogramsFileName,ios::app);
    ofstream cocount(typeCountHistogramsFileName,ios::app);
    Partial_Reveal_VTable *the_vt;
    cocount << "dump_id, Total Count, ";
    cosize << "dump_id, Total Size (bytes), ";
    type_order->rewind();
    while (the_vt = (Partial_Reveal_VTable *)type_order->next()) {
        POINTER_SIZE_INT the_size = type_size_counts->get_val(the_vt);
        POINTER_SIZE_INT the_count = type_counts->get_val(the_vt);
        cocount << the_vt->get_gcvt()->gc_class_name << ", ";
        cosize << the_vt->get_gcvt()->gc_class_name << ", ";
    }
}

void verify_dump_char_sizes (bool on_exit)
{
}

// Count the types as you are walking the heap.
void characterize_types (Partial_Reveal_VTable *vt, int size)
{
    if (!type_counts->is_present((void *)vt)) {
        TRACE("The object_type for counts is " << vt->get_gcvt()->gc_class_name);
        type_counts->add_entry((void *)vt);
        // type_order keeps track of the order that types are dumped out in.
        type_order->add_entry(vt);
    }
    type_counts->inc_val((void *)vt, 1);

    if (!type_size_counts->is_present((void *)vt)) {
        TRACE("The object_type for size is " << vt->get_gcvt()->gc_class_name);
        type_size_counts->add_entry((void *)vt);
    }

    type_size_counts->inc_val((void *)vt, size);

    // Put the char arrays into the correct bucket for the size histogram.
    if (strcmp(vt->get_gcvt()->gc_class_name, "[C") == 0) {      
        assert (size != 0);
        assert (sizeof(LONG) == 4);
        assert (sizeof(int) == 4);

        apr_atomic_add32((volatile uint32 *)&xchar_array_size, (uint32)size);
        
        int char_count = apr_atomic_inc32((volatile uint32 *)&xchar_array_count);
    } 
}

//
// When we enter the GC dump out the counts and the sizes then set them to zero
// On exit dump out counts and sizes.
// The entry numbers are what is live in the heap at the last GC + what has been allocated since.
// The exit numbers is what is currently live.
//
int type_counts_id = 0;

// This is called when you want the verify routines to print out any final information.
void dump_type_counts (bool on_exit)
{
    TRACE("dumping type counts");

    ofstream cosize(typeSizeHistogramsFileName,ios::app);
    ofstream cocount(typeCountHistogramsFileName,ios::app);

    char *on_exit_str = on_exit?"Live " : "All  ";

    TRACE(on_exit_str << "char count = "<< xchar_array_count 
        << ", char size = " << xchar_array_size);

    if (on_exit) {
        xend_of_gc_char_array_count = xchar_array_count;    
    }
    
    if (!on_exit) {
        xchar_array_count = 0;
        xchar_array_size = 0;
    }

    cocount << on_exit_str << (int)type_counts_id << ",";
    cosize << on_exit_str << (int)type_counts_id << ",";
    
    POINTER_SIZE_INT total_count = 0;
    POINTER_SIZE_INT total_bytes = 0;

    type_order->rewind();

    Partial_Reveal_VTable *the_vt;    

    // Calculate the totals to go in the first two columns
    while (the_vt = (Partial_Reveal_VTable *)type_order->next()) {
        POINTER_SIZE_INT the_size = type_size_counts->get_val(the_vt);
        POINTER_SIZE_INT the_count = type_counts->get_val(the_vt);
        total_count += the_count;
        total_bytes += the_size;
    }
    
    cocount << (int)total_count << ",";
    cosize << (int)total_bytes << ",";
    
    type_order->rewind();
    while (the_vt = (Partial_Reveal_VTable *)type_order->next()) {
        POINTER_SIZE_INT the_size = type_size_counts->get_val(the_vt);
        POINTER_SIZE_INT the_count = type_counts->get_val(the_vt);
        cocount << (int)the_count << ",";
        cosize << (int)the_size << ",";
    }
    
    cocount << endl;
    cosize << endl;

    verify_dump_char_sizes(on_exit);
    type_counts_id++;
}


// Does this object belong to an object that survived the last GC?
bool is_long_lived (Partial_Reveal_Object *p_obj)
{
    if (p_global_gc->obj_belongs_in_single_object_blocks(p_obj)) {
        // Actually we don't know but we will return true here for now.
        return true;
        
    }
    block_info *info = GC_BLOCK_INFO(p_obj);
    if (info->block_has_been_swept) {
        if (info->in_nursery_p) {
            if (info->num_free_areas_in_block == 0) {
                // There were no free areas to allocate in. 
                return true;
            }
            unsigned int i = 0;
            free_area *area = &(info->block_free_areas[i]);
            while (area) {
                if ( ((POINTER_SIZE_INT)p_obj >= (POINTER_SIZE_INT)area->area_base) && ((POINTER_SIZE_INT)p_obj < (POINTER_SIZE_INT)area->area_ceiling) ) {
                    return false;
                }
                i++;
                area = &(info->block_free_areas[i]);
            }
        }
    }
    return true;
}

#endif // #ifndef PLATFORM_POSIX
