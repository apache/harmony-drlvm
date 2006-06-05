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
 * @version $Revision: 1.12.24.4 $
 *
 */

#ifndef _CATEGORY_H_
#define _CATEGORY_H_

#include <assert.h>
#include <iostream>
#include <stdarg.h>

//#include "Stl.h"
#include <string>
#include <map>

namespace Jitrino {

// Logging levels
enum LogLevel {
    LogAll     =   0,
    LogDebug2  =   5,
    LogDebug   =  10,
    LogIR2     =  12,
    LogIR      =  15,
    LogInfo3   =  17,
    LogInfo2   =  18,
    LogInfo    =  20,
    LogWarn    =  30,
    LogError   =  40,
    LogFail    =  50,
    LogNone    =  60,
};

// Set the static threshold.  Any operation below this threshold should be removed.
#ifdef _DEBUG 
// Log everything
const LogLevel
LogStaticThreshold = LogAll;
#else
// Log nothing
#ifndef PRUNED_BINARY
const LogLevel 
LogStaticThreshold = LogAll; // change back to LogNone for maximal speed
#else
const LogLevel 
LogStaticThreshold = LogNone;
#endif
#endif

const LogLevel
LogDefaultThreshold = LogNone;


/**
 * A stream-like class for logging output.
 **/
template<LogLevel level>
class CategoryStream {
    friend class Category;
public:
    template <class T>
    CategoryStream<level>& operator<<(const T& value) {
        // Should be compile-time NOP if first test fails.
        if(level >= LogStaticThreshold) {
            if(cat->isLogDynamicEnabled(level)) {
                cat->out() << value;
            }
        }
        return *this;
    }

    CategoryStream& operator << (::std::ostream& (*f)(::std::ostream&))
    {
        // Should be compile-time NOP if first test fails.
        if(level >= LogStaticThreshold) {
            if(cat->isLogDynamicEnabled(level)) {
                f(cat->out());
            }
        }
        return *this;
    }

private:
    CategoryStream() {}
    CategoryStream(const CategoryStream& os);
    void operator=(const CategoryStream& os);
    
    void setCategory(Category* c) { cat = c; }
    Category* cat;
};

#define DEFINE_LEVEL(level, name) \
    /* Is logging enabled for this level.*/ \
    bool is##name##Enabled() { return isLogEnabled(Log##name); } \
    \
    /* C++ style output guarded by this level.*/ \
    CategoryStream<Log##name> level; 

/**
 * A log4j style logging facility.
 */
class Category {
private:
    // GCC doesn't understand empty namespace in template
    typedef ::std::string StdString;
    typedef ::std::map<StdString, Category*> CategoryMap;
public:
    Category(const ::std::string& name, LogLevel threshold = LogDefaultThreshold);
    Category(Category *parent, const ::std::string& name, LogLevel threshold = LogDefaultThreshold);
    ~Category();

    // Get the root category.
    Category* getRoot();

    // Get a child category with given name.
    Category* getCategory(const ::std::string& name);

    // Create a new child category.
    Category* createCategory(const ::std::string& name, LogLevel threshold = LogNone);

    // Remove child category
    void removeCategory(Category* child);

    // Reset to default values
    void reset(bool parentOverrides);

    // Set minimum logging threshold.
    void setThreshold(LogLevel level, bool check_parent_override = true);

    void setThreshold(const ::std::string& str, bool check_parent_override = true);

    // Does the parent category's threshold override this category's threshold.
    bool doesParentOverride() const;

    void setParentOverride(bool override);

    DEFINE_LEVEL(debug, Debug)
    DEFINE_LEVEL(debug2, Debug2)
    DEFINE_LEVEL(ir, IR)
    DEFINE_LEVEL(ir2, IR2)
    DEFINE_LEVEL(info, Info)
    DEFINE_LEVEL(info2, Info2)
    DEFINE_LEVEL(info3, Info3)
    DEFINE_LEVEL(warn, Warn)
    DEFINE_LEVEL(error, Error)
    DEFINE_LEVEL(fail, Fail)

    ::std::ostream& out();

    void setLogging(bool on, bool check_parent_override = true);
    void set_out(::std::ostream *new_outp, bool check_parent_override = true);
    bool is_out_set();
    static ::std::ostream& global_out();
    static void set_global_out(::std::ostream *new_outp);
    void dump();

    bool isLogDynamicEnabled(LogLevel level);
    bool isLogEnabled(LogLevel level);

private:
    Category(const Category& cat) { assert(0); }
    void operator=(const Category& cat) { assert(0); }

    void initialize();

    void logf(LogLevel level, const char* format, ...);

    void outf(const char* format, ...);

    static ::std::ostream* _glob_outp;

    // Per instance data

    /** On/off switch */
    bool doLogging;

    /** category name */
    ::std::string name;

    /** category output stream */
    ::std::ostream* _outp;

    /** map of child categories */
    CategoryMap categoryMap;

    /** current logging level for this category */
    LogLevel dynamicThreshold;

    /** parent category */
    Category* parent;

    /** whether parent settings (such as logging level) override this category's ones */
    bool parentOverride;

};

} //namespace Jitrino 

#endif // _CATEGORY_H_
