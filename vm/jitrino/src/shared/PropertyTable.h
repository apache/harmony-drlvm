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
 * @version $Revision: 1.8.24.4 $
 *
 */

#ifndef _JITRINO_PARAMETER_TABLE_H_
#define _JITRINO_PARAMETER_TABLE_H_
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <list>
#include "HashTable.h"

namespace Jitrino {

template <class ELEM_TYPE>
class StringHashTable : public ConstHashTable<char, ELEM_TYPE> {
public:
  StringHashTable(MemoryManager& mm, uint32 size) : ConstHashTable<char,ELEM_TYPE>(mm,size) {}
protected:
  virtual bool keyEquals(const char* key1,const char* key2) const {
    return (strcmp(key1,key2)==0);
  }
  virtual uint32 getKeyHashCode(const char* key) const {
    // return some hash function;
    uint32 hash = 0;
    while (*key != '\0') {
      hash <<= 3;
      hash += *key;
      ++key;
    }
    return hash;
  }
};

class Method_Table;
class MethodDesc;

class JitrinoParameterTable {
private:
  typedef StringHashTable<char> TableType;
  typedef ConstHashTableIter<char,char> TableIterType;
  TableType table;
  ::std::list<char *> _createdStrings;
  const char *readingFile;
  MemoryManager &memManager;

  JitrinoParameterTable *next;
  Method_Table* mtable;
  uint32 tablesize;

  // insertfromcommandline==true if there was any insertion from command line
  static bool insertfromcommandline;
public:
  JitrinoParameterTable(MemoryManager& mm, uint32 size);
  ~JitrinoParameterTable();

  JitrinoParameterTable *insert(const char *propertyName, const char *value);
  JitrinoParameterTable *insertFromCommandLine(const char *commandLineParameter);
  JitrinoParameterTable *readFromFile(const char *fileName);

  const char *lookup(const char *propertyName) const;
  bool lookupBool(const char *propertyName, bool def) const;
  char lookupChar(const char *propertyName, char def) const;
  int lookupInt(const char *propertyName, int def) const;
  unsigned int lookupUint(const char *propertyName, unsigned int def) const;
  // only does first 20 chars;  uses static var so must be single-threaded!!
  static const char *stringToLower(const char *thestring);
  void print(::std::ostream &outs) const;

  JitrinoParameterTable *getTableForMethod(MethodDesc &md);
private:
  char *cacheString(const char *stringStart, const char *stringEnd);
  static char _tmp[21];
  void commandLineError(const char *badParam);
  JitrinoParameterTable *parseCommandLine(const char *commandLineParameter);
};

} //namespace Jitrino 

#endif // _JITRINO_PARAMETER_TABLE_H_
