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
 * @version $Revision: 1.9.24.4 $
 *
 */

#ifndef _JITRINO_METHOD_TABLE_H
#define _JITRINO_METHOD_TABLE_H

namespace Jitrino {

class MethodDesc;

class Method_Table
{
public:
    Method_Table(const char *default_envvar, const char *envvarname, bool accept_by_default);
    ~Method_Table() {}
    bool accept_this_method(MethodDesc &md);
    bool accept_this_method(const char* classname, const char *methodname, const char *signature);
    bool is_in_list_generation_mode();
    enum Decision
    {
        mt_rejected,
        mt_undecided,
        mt_accepted
    };
    struct method_record
    {
        char *class_name;
        char *method_name;
        char *signature;
        Decision decision;
    };

    
private:
    struct method_record *_method_table;
    int _mrec_size, _mrec_capacity;
    struct method_record *_decision_table;
    int _dtable_size, _dtable_capacity;
    Decision _default_decision;
    bool _accept_all;
    bool _dump_to_file;
    char *_method_file;
    bool _accept_by_default;

    void init(const char *default_envvar, const char *envvarname);
    void make_filename(char *str, int len);
    bool read_method_table();
};

} //namespace Jitrino 

#endif //_JITRINO_METHOD_TABLE_H
