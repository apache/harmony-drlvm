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
 * @version $Revision: 1.16.16.2.4.4 $
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "PropertyTable.h"
#include "methodtable.h"
#include "PlatformDependant.h"

namespace Jitrino {

bool JitrinoParameterTable::insertfromcommandline = false;

void
JitrinoParameterTable::commandLineError(const char *badParam)
{
  if (readingFile) {
    ::std::cerr << "JitrinoParameterTable: Bad parameter found in file " << readingFile << ":\n";
  } else {
    ::std::cerr << "JitrinoParameterTable: Bad parameter on command line:\n";
  }
  ::std::cerr << "    " << badParam << "\n";
  exit(1);
}

JitrinoParameterTable::JitrinoParameterTable(MemoryManager &mm, uint32 size) : 
	table(mm, size), _createdStrings(), readingFile(0), 
	memManager(mm), next(0), mtable(0), tablesize(size)
{
}

JitrinoParameterTable::~JitrinoParameterTable()
{
  ::std::list<char *>::iterator a = _createdStrings.begin();
  ::std::list<char *>::iterator b = _createdStrings.end();
  while (a != b) {
    ++a;
  }
}

JitrinoParameterTable *
JitrinoParameterTable::insert(const char *propertyName, const char *value)
{
  // insertfromcommandline==true if there was any insertion from command line
  insertfromcommandline = true;

#ifdef PARAM_DEBUG
  ::std::cerr << "Entering param " << propertyName << " -> " << value << "\n";
#endif
  if (strncmp(propertyName,"METHODS",8)==0) {
      Method_Table *newmtable = new (memManager) Method_Table(value, "Jitrino", true);
      JitrinoParameterTable *newpt =
          new (memManager) JitrinoParameterTable(memManager, tablesize);
      newpt->next = this;
      newpt->mtable = newmtable;
      newpt->readingFile = readingFile; readingFile = 0;
      // add entries from this to newpt
      TableIterType titer(&table);
      const char *key, *elem;
      while (titer.getNextElem(key, elem)) {
          newpt->table.insert(key, elem);
      }
      return newpt;
  } else if (strncmp(propertyName,"LOG",4)==0) {
      const char *oldval = table.lookup(propertyName);
      if (oldval) {
          uint32 size1 = (uint32) strlen(oldval);
          uint32 size2 = (uint32) strlen(value);
          uint32 size = (uint32) (size1 + size2 + 1);
          char *tmp1= new (memManager) char[size+1];
          strcpy(tmp1,oldval);
          tmp1[size1] = ',';
          strcpy(tmp1+size1+1,value);
          _createdStrings.push_back(tmp1);
          value = tmp1;
      }
  }
  table.insert(propertyName, value);
  {
#ifdef PARAM_DEBUG
     const char *newvalue = lookup(propertyName);
     if (newvalue) {
       ::std::cerr << "New value is " << newvalue << "\n";
     } else {
       ::std::cerr << " New value is NULL!! \n";
     }
#endif
  }
  return this;
}

// must have stringStart <= stringEnd.
// if stringStart is 0, then stringEnd must be 0 also.
// string goes from stringStart to stringEnd-1
char *
JitrinoParameterTable::cacheString(const char *stringStart, const char *stringEnd)
{
  uint32 size = (uint32) (stringEnd-stringStart);
  char *tmp1= new (memManager) char[size+1];
  strncpy(tmp1,stringStart,size);
  tmp1[size]='\0';
  _createdStrings.push_back(tmp1);
  return tmp1;
}

JitrinoParameterTable *
JitrinoParameterTable::insertFromCommandLine(const char *commandLineParameter)
{
  const char *was_file = lookup("fromfile");
  JitrinoParameterTable *t = this;
  t = t->parseCommandLine(commandLineParameter);
  const char *now_file = t->lookup("fromfile");
  if (was_file != now_file) {
      return t->readFromFile(now_file);
  }
  return t;
}

JitrinoParameterTable *
JitrinoParameterTable::parseCommandLine(const char *commandLineParameter)
{
    const char *p3 = commandLineParameter;
    JitrinoParameterTable *t = this;
    if (p3 != 0) {
        if (*p3 == '#') return t; // Ignore lines beginning with the '#' character.
        bool done=(*p3 == '\0');
        while (!done) {
            const char *propStart = p3;
            const char *propEnd = 0;
            const char *valueStart = 0;
            const char *valueEnd = 0;
            bool quoted = false;
            bool wasQuoted = false;

            do {
                switch (*p3) {
                case '=':
                    if (!quoted) {
                        propEnd = p3; valueStart = p3+1;
                    }
                    ++p3; break;
                case ',':
                    if (quoted) {
                        ++p3;
                        break;
                    }
                // otherwise fall through:
                case '\0':
                    done = true;
                    if (valueStart) {
                        valueEnd = p3;
                    } else {
                        propEnd = p3;
                    }
                    break;
                case '"':
                    if (quoted) {
                        quoted = false;
                        wasQuoted = true;
                    } else {
                        if (wasQuoted) {
                            t->commandLineError(commandLineParameter);
                        }
                        quoted = true;
                    }
                    ++p3;
                    break;
                default:
                    ++p3;
                    break;
                }
            } while (!done);
            if ( valueStart == 0 ) {
                // 
                // no value means "on"
                //
                t->insert(cacheString(propStart,propEnd), "on");
            }else{
                if (wasQuoted) {
                    if ( (*valueStart != '"') || (*(valueEnd-1) != '"') ) {
                        t->commandLineError(commandLineParameter);
                    }
                    ++valueStart;
                    --valueEnd;
                }
                t = t->insert(cacheString(propStart,propEnd), cacheString(valueStart,valueEnd));
            }
            done = (*p3 == '\0');
            ++p3;
        }
    }
    return t;
}

JitrinoParameterTable *
JitrinoParameterTable::readFromFile(const char *fileName)
{
  ::std::ifstream source(fileName);
  if (!source) {
    ::std::cerr << "unable to open " << fileName << " for reading.\n";
    exit(1);
  }
  char inbuf[4096];
  inbuf[0] = '\0';
  source >> inbuf;
  JitrinoParameterTable *t = this;
  t->readingFile = fileName;
  while (inbuf[0] != '\0') {
      t = t->insertFromCommandLine(inbuf);
      inbuf[0] = '\0';
      source >> inbuf;
      if(source.eof())
          break;
  }
  t->readingFile = 0;
  return t;
}

const char *
JitrinoParameterTable::lookup(const char *propertyName) const
{
  if ( !insertfromcommandline ) return NULL;
  const char *a = table.lookup(propertyName);
  return a;
}

bool JitrinoParameterTable::lookupBool(const char *propertyName, bool def) const
{
  if ( !insertfromcommandline ) return def;
  const char *s = lookup(propertyName);
  if (s != 0) {
    if ((strnicmp(s,"yes",10)==0) ||
        (strnicmp(s,"true",10)==0) ||
        (strnicmp(s,"on",10)==0) ||
        (strnicmp(s,"t",10)==0) ||
        (strnicmp(s,"y",1)==0)) {
        return true;
    }
  } else {
      return def;
  }
  return false;
}

char JitrinoParameterTable::lookupChar(const char *propertyName, char def) const
{
  if ( !insertfromcommandline ) return def;
  const char *s = lookup(propertyName);
  if (s == 0) {
      return def;
  } else {
      return *s;
  };
}

int JitrinoParameterTable::lookupInt(const char *propertyName, int def) const
{
  if ( !insertfromcommandline ) return def;
  const char *s = lookup(propertyName);
  if (s == 0) {
      return def;
  }
  int i=0;
  int j = sscanf(s, "%d", &i);
  if (j == 1) {
    return i;
  } else {
    return -123456789;
  }
}

unsigned int JitrinoParameterTable::lookupUint(const char *propertyName,
                                               unsigned int def) const
{
  if ( !insertfromcommandline ) return def;
  const char *s = lookup(propertyName);
  if (s == 0) {
      return def;
  }
  unsigned int i=0;
  unsigned int j = sscanf(s, "%u", &i);
  if (j == 1) {
    return i;
  } else {
    return def;
  }
}

void JitrinoParameterTable::print(::std::ostream &outs) const
{
  outs << "printing JitrinoParameterTable\n";

  outs << "Created Strings: ";
  ::std::list<char *>::const_iterator a = _createdStrings.begin();
  ::std::list<char *>::const_iterator b = _createdStrings.end();
  while (a != b) {
    char *av = *a;
    outs << "\"" << av << "\" ";
    ++a;
  }
  outs << "\n";
  outs << "The table:\n";
  TableIterType iter(&(((JitrinoParameterTable *)this)->table));
  const char *key, *value;
  if (iter.getNextElem(key, value)) {
    outs << "{ " << key << "=" << "\"" << value << "\"";
#ifdef PARAM_DEBUG
    ::std::cerr << "saw binding of key ";
    ::std::cerr << key;
    ::std::cerr << "\n";
#endif
    while (iter.getNextElem(key, value)) {
      outs << ",\n  " << key << "=" << "\"" << value << "\"";
#ifdef PARAM_DEBUG
      ::std::cerr << "saw binding of key ";
      ::std::cerr << key;
      ::std::cerr << "\n";
#endif
    }
    outs << "\n}\n";
  } else {
    outs << "EMPTY\n";
  }
}

JitrinoParameterTable *
JitrinoParameterTable::getTableForMethod(MethodDesc &md)
{
    JitrinoParameterTable *t = this;
    while (t && t->mtable && !t->mtable->accept_this_method(md)) {
        t = t->next;
    }
    assert(t);
    return t;
}

} //namespace Jitrino
