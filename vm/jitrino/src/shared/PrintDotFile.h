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
 * @version $Revision: 1.10.24.4 $
 *
 */

//
// interface to print dot files, you should subclass this class and
// override the method printDotBody()
//
#ifndef _PRINTDOTFILE_
#define _PRINTDOTFILE_
//
// interface to print dot files, you should subclass this class and
// implement the printBody() method
//
#include <stdlib.h>
#include <fstream>
#include <string.h>

namespace Jitrino {

class MethodDesc;

class PrintDotFile {
public:
    PrintDotFile(): os(NULL), count(0) {}
    //
    // dump out to a filename composed of the method descriptor and a suffix
    //
    virtual void printDotFile(MethodDesc& mh, const char *suffix);

    virtual void printDotHeader(MethodDesc& mh);
    virtual void printDotEnd   ();
    virtual void printDotBody  ();
protected:
	virtual ~PrintDotFile() {};
	::std::ostream* os;
    int count;
private:
    void createFileName(MethodDesc& mh, char* filename,const char *suffix);
};
} //namespace Jitrino 

#endif
