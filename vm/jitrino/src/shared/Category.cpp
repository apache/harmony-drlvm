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
 * @version $Revision: 1.13.24.4 $
 *
 */

#include "Category.h"

namespace Jitrino {

::std::ostream*	Category::_glob_outp = &::std::cout;

void
Category::initialize()
{
    debug.setCategory(this);
    debug2.setCategory(this);
    ir.setCategory(this);
    ir2.setCategory(this);
    info.setCategory(this);
    info2.setCategory(this);
    info3.setCategory(this);
    warn.setCategory(this);
    error.setCategory(this);
    fail.setCategory(this);
}

Category::Category(const ::std::string& _name, LogLevel threshold) :
	doLogging(true), name(_name), _outp(NULL),
	dynamicThreshold(threshold), parent(NULL), parentOverride(true)
{
    initialize();
}

Category*
Category::createCategory(const ::std::string& subname, LogLevel threshold)
{
    ::std::string full_subname = subname;

    if(name != "") {
            full_subname = name + "." + subname;
    }
    Category *res = new Category(full_subname, threshold);
    res->setParentOverride(true);
    res->parent = this;
    categoryMap[subname] = res;
    return res;
}

Category::~Category() {
	categoryMap.clear();
}

// Get a category.
Category*
Category::getCategory(const ::std::string& name)
{
	::std::string subcat_name = name;
	int ind = name.find('.');
	bool dot_found = ind != (int)::std::string::npos;

	if (dot_found) {
		assert(ind > 0);
		subcat_name = name.substr(0, ind); // not including the '.'
	}
	Category* subcat = NULL;
	CategoryMap::const_iterator i = categoryMap.find(subcat_name);

	if (i != categoryMap.end()) {
		subcat = categoryMap[subcat_name];
	}
	if (subcat == NULL) {
		return NULL;
	}
	if (dot_found) {
		::std::string tail = name.substr(ind + 1, name.length()-ind-1);
		subcat = subcat->getCategory(tail);
	}
	return subcat;
}

void Category::removeCategory(Category* child) {
	CategoryMap::const_iterator i;
	::std::string name = "";

	for (i = categoryMap.begin(); i != categoryMap.end(); i++) {
		if (i->second == child) {
			name = i->first;
			break;
		}
	}
	if (name != "") {
		categoryMap.erase(name);
	}
}

void Category::reset(bool parentOverrides) { 
	dynamicThreshold = LogDefaultThreshold;
	parentOverride = parentOverrides;
};

void Category::setThreshold(LogLevel level, bool check_parent_override) {
	dynamicThreshold = level;
}

// Set minimum logging threshold
void
Category::setThreshold(const ::std::string& str, bool check_parent_override)
{
    LogLevel level = LogNone;

    if(str == "all")
        level = LogAll;
    else if(str == "debug")
        level = LogDebug;
    else if(str == "debug2")
        level = LogDebug2;
    else if(str == "ir")
        level = LogIR;
    else if(str == "ir2")
        level = LogIR2;
    else if(str == "info")
        level = LogInfo;
    else if(str == "info2")
        level = LogInfo2;
    else if(str == "info3")
        level = LogInfo3;
    else if(str == "warn")
        level = LogWarn;
    else if(str == "error")
        level = LogError;
    else if(str == "fail")
        level = LogFail;
    else if(str == "none")
        level = LogNone;

    setThreshold(level, check_parent_override);
}

bool Category::doesParentOverride() const {
	return parentOverride;
}

void Category::setParentOverride(bool override) {
	parentOverride = override;
}

::std::ostream& Category::out() {
	return _outp == NULL ? global_out() : *_outp;
}

/**
 * Turn on/off logging for this category and recursively for all
 * child categories.
 */
void Category::setLogging(bool on, bool check_parent_override) {
	if (on == true) {
		doLogging = on;
	}
	for (CategoryMap::iterator i = categoryMap.begin(); i != categoryMap.end(); i++) {
		Category *child = (*i).second;

		if (!check_parent_override || child->doesParentOverride()) {
			child->setLogging(on, check_parent_override);
		}
	}
	doLogging = on;
}

/**
 * Set new output for this category and recursively for all
 * child categories.
 */
void Category::set_out(::std::ostream *new_outp, bool check_parent_override) {
	for (CategoryMap::iterator i = categoryMap.begin(); i != categoryMap.end(); i++) {
		Category *child = (*i).second;

		if (!check_parent_override || child->doesParentOverride()) {
			child->set_out(new_outp);
		}
	}
	_outp = new_outp;
}

bool Category::is_out_set() {
	return _outp != NULL;
}

::std::ostream& Category::global_out() {
	assert(_glob_outp != NULL);
	return *_glob_outp;
}

void Category::set_global_out(::std::ostream *new_outp) {
	_glob_outp = new_outp;
}

void Category::dump() {
	::std::cerr << "Category " << name.c_str() << " : " << (int) dynamicThreshold << ::std::endl;
}

bool Category::isLogDynamicEnabled(LogLevel level) {
	bool level_ok = level >= dynamicThreshold;
	bool parent_ok = parent != NULL && parent->isLogDynamicEnabled(level);

	return doLogging && (level_ok || (parentOverride && parent_ok));
}

bool Category::isLogEnabled(LogLevel level) {
	bool res = (level >= LogStaticThreshold) && isLogDynamicEnabled(level);
	return res;
}

Category* Category::getRoot() {
	return (parent == NULL ? this : parent->getRoot());
}

void Category::logf(LogLevel level, const char* format, ...) {
	if(isLogEnabled(level)) {
		va_list va;
		va_start(va, format);
		outf(format, va);
		va_end(va);
	}
}

// Format output C style
void
Category::outf(const char* format, ...)
{
    const int SIZE = 1000;
    char buffer[SIZE];
    assert((int)strlen(format) < SIZE);
    va_list va;
    va_start(va, format);
    vsprintf(buffer, format, va);
    va_end(va);
    out() << buffer;   
}

} //namespace Jitrino 
