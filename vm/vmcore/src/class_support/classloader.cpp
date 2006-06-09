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
 * @author Pavel Pervov
 * @version $Revision: 1.1.2.11.2.1.2.6 $
 */  

#define LOG_DOMAIN util::CLASS_LOGGER
#include "cxxlog.h"
#include "vm_log.h"

#include <sstream>

#include "classloader.h"
#include "object_layout.h"
#include "String_Pool.h"
#include "open/vm.h"
#include "exceptions.h"
#include "properties.h"
#include "vm_strings.h"
#include "Verifier_stub.h"
#include "nogc.h"
#include "bytereader.h"
#include "Package.h"
#include "jvmti_internal.h"
#include "ini.h"
#include "open/gc.h"
#include "open/thread.h"
#include "open/vm_util.h"
#include "suspend_checker.h"
#include "verifier.h"

#include "port_filepath.h"
#include <apr_file_io.h>
#include <apr_file_info.h>

#include "jarfile_util.h"
#include "jni_utils.h"

//
// private static variable containing the id of the next class
// access to this needs to be thread safe; also, this will have to 
// change if we want to reuse class ids
//
// an id of 0 is reserved to mean null; therefore, ids start from 1
//
// ppervov: FIXME: usage of this variable is not currently thread safe
unsigned class_next_id = 1;

unsigned ClassLoader::m_capacity = 0;
unsigned ClassLoader::m_unloadedBytes = 0;
unsigned ClassLoader::m_nextEntry = 0;
ClassLoader** ClassLoader::m_table = NULL;
Lock_Manager ClassLoader::m_tableLock;

void mark_classloader(ClassLoader* cl)
{
    if(cl->GetLoader() && cl->NotMarked()) {
        TRACE2("classloader.unloading.markloader", "  Marking loader "
            << cl << " (" << cl->GetLoader() << " : "
            << ((VTable*)(*(unsigned**)(cl->GetLoader())))->clss->name->bytes << ")");
        cl->Mark();
    }
}


/*VMEXPORT*/ jthrowable class_get_error(ClassLoaderHandle clh, const char* name)
{
    assert(clh);
    assert(name);
    return clh->GetClassError(name);
}


void class_set_error_cause(Class *clss, jthrowable exn) {
    tmn_suspend_disable();
    clss->m_lock->_lock();
    clss->state = ST_Error;
    clss->p_error = exn->object;
    clss->m_lock->_unlock();
    tmn_suspend_enable();
}

jthrowable class_get_error_cause(Class *clss) {
    assert(clss->p_error);
    return (jthrowable) &clss->p_error;
}


bool ClassLoader::Initialize( ManagedObject* loader )
{
    m_loader = loader;
    if(!(m_package_table = new Package_Table())){
        return false;
    }
    m_loadedClasses = new ClassTable();
    if(!m_loadedClasses) return false;
    m_loadingClasses = new LoadingClasses();
    if(!m_loadingClasses) return false;
    m_reportedClasses = new ReportedClasses();
    if(!m_reportedClasses) return false;
    m_failedClasses = new FailedClasses();
    if(!m_failedClasses) return false;
    m_javaTypes = new JavaTypes();
    if(!m_javaTypes) return false;
    return true;
}

ClassLoader::~ClassLoader() 
{
    apr_pool_destroy(pool);

    ManagedObject** ppc;
    ReportedClasses* RepClasses = GetReportedClasses();
    ReportedClasses::iterator itc;
    for (itc = RepClasses->begin(); itc != RepClasses->end(); itc++)
    {
        ppc = &itc->second;
        assert(*ppc);
        Class* c = jclass_to_struct_Class((jclass)ppc);
        ClassClearInternals(c);
    }
    if (GetLoadedClasses())
        delete GetLoadedClasses();
    if (GetFailedClasses())
        delete GetFailedClasses();
    if (GetLoadingClasses())
        delete GetLoadingClasses();
    if (GetReportedClasses())
        delete GetReportedClasses();
    if (GetVerifyData())
        vf_release_verify_data(GetVerifyData());
    if (GetJavaTypes())
    {
        JavaTypes::iterator itjt;
        for (itjt = GetJavaTypes()->begin(); itjt != GetJavaTypes()->end(); itjt++)
        {
            TypeDesc* td = itjt->second;
            delete td;
        }
        delete GetJavaTypes();
    }
}

bool ClassLoader::LoadingClass::CreateWaitingEvent(const String* className) 
{
    if(m_loadWaitEvent == 0)
        m_loadWaitEvent = vm_create_event(NULL, TRUE, FALSE, NULL);
    if(m_loadWaitEvent == 0) {
        DIE2("classloader", "Event creation failed for class " << className->bytes
            << ", which is loaded concurrently");
        return false;
    }
    return true;
}

void ClassLoader::LoadingClass::EnqueueInitiator(VM_thread* new_definer, ClassLoader* cl, const String* clsname) 
{
    if(!IsInitiator(new_definer)) {
        if(!IsInitiator(NULL)) { // no initiator, i.e. exitings
            AddWaitingThread(m_initiatingThread, cl, clsname);
            UpdateInitiator(new_definer);
        }
        RemoveWaitingThread(new_definer, cl, clsname);
    }
}

void ClassLoader::LoadingClass::ChangeDefinerAndInitiator(VM_thread* new_definer, ClassLoader* cl, const String* clsname) 
{
    TRACE2("classloader.collisions", cl << " " << new_definer << " CDAI " << m_initiatingThread << " " << clsname->bytes);
    assert(m_defineOwner == NULL);
    m_defineOwner = new_definer;
    EnqueueInitiator(new_definer, cl, clsname);
}

bool ClassLoader::LoadingClass::AlreadyWaiting(VM_thread* thread) 
{
    for(WaitingThread* wt = m_waitingThreads; wt; wt = wt->m_next) {
        if(wt->m_waitingThread == thread) {
            return true;
        }
    }
    return false;
}

void ClassLoader::LoadingClass::AddWaitingThread(VM_thread* thread, ClassLoader* cl, const String* clsname) 
{
    TRACE2("classloader.collisions", cl << " AWT " << thread << " " << clsname->bytes);
    if(!m_threadsPool)
        apr_pool_create(&m_threadsPool, 0);

    WaitingThread* wt =
        (WaitingThread*)apr_palloc(m_threadsPool, sizeof(WaitingThread));
    wt->m_waitingThread = thread;
    wt->m_next = m_waitingThreads;
    m_waitingThreads = wt;
}

void ClassLoader::LoadingClass::RemoveWaitingThread(VM_thread* thread, ClassLoader* cl, const String* clsname) 
{
    TRACE2("classloader.collisions", cl << " RWT " << thread << " " << clsname->bytes);
    // find in waiting threads
    WaitingThread* prev;
    for(WaitingThread* wt = prev = m_waitingThreads; wt; prev = wt, wt = wt->m_next) {
        if(wt->m_waitingThread == thread) {
            // need to remove this element
            if(prev == wt) {
                m_waitingThreads = m_waitingThreads->m_next;
                break;
            }
            prev->m_next = wt->m_next;
            break;
        }
    }
}

Class* ClassLoader::NewClass(const Global_Env* env, const String* name)
{
    Class *clss = NULL;
    TRACE2("classloader.newclass", "allocating Class for \"" << name->bytes << "\"\n" );

    if(env->InBootstrap() && name == env->JavaLangClass_String) {
        assert(env->JavaLangClass_Class == NULL);
    }

    clss = (Class *) Alloc(sizeof(Class));
    // ppervov: FIXME: should check that class is successfully allocated
    assert(clss);

    return InitClassFields(env, clss, name);
}

ManagedObject** ClassLoader::RegisterClassInstance(const String* className, ManagedObject* instance) 
{
    TRACE2("reported:newclass", "DIRECT: inserting class \"" << className->bytes
        << "\" with key " << className << " and object " << instance);
    return m_reportedClasses->Insert(className, instance);
}

Class* ClassLoader::DefineClass(Global_Env* env, const char* class_name,
                                uint8* bytecode, unsigned offset, unsigned length,
                                const String** res_name)
{
    const String *className;

    TRACE2("classloader.defineclass", "Defining class " << class_name << " with loader " << this);
    if(class_name) {
        className = env->string_pool.lookup(class_name);
    } else {
        className = class_extract_name(env, bytecode, offset, length);
        if(className == NULL) {
            jthrowable exn = exn_create("java/lang/LinkageError",
                "DefineClass was called without class name and "
                "class name could not be extracted from provided class data");
            exn_raise_only(exn);
            return NULL;
        }
    }
    if(res_name) {
        *res_name = className;
    }

    Class* clss;
    if((clss = WaitDefinition(env, className)) != NULL || exn_raised())
        return clss;

    m_lock._lock();
    if(m_failedClasses->Lookup(className)) {
        m_lock._unlock();
        return NULL;
    }
    m_lock._unlock();

    uint8 *redef_buf = NULL;
    int redef_buflen = 0;
    jvmti_send_class_file_load_hook_event(env, this, class_name,
        length, bytecode + offset,
        &redef_buflen, &redef_buf);
    if(NULL != redef_buf)
    {
        bytecode = redef_buf;
        offset = 0;
        length = redef_buflen;
    }

    clss = NewClass(env, className);
    if(!clss) {
        return NULL;
    }
    // Create a Class File Stream object
    ByteReader cfs(bytecode, offset, length);
    unsigned super_class_cp_index;
    assert(clss->state == ST_Start);
    if(!class_parse(env, clss, &super_class_cp_index, cfs)) {
        if (NULL != redef_buf)
            _deallocate(redef_buf);
        FailedLoadingClass(className);
        return NULL;
    }
    if (NULL != redef_buf)
        _deallocate(redef_buf);

    // XXX: should be removed
    // Special case: ClassLoader.defineClass() call with null classname value.
    // Calling AllocateAndReportInstance() after class_parse() which has
    // determined real class name.
    // Note: in ordinary case AddToReported() is called from
    // NewClass()->InitClassFields().
    if( className == NULL ) {
        clss = AllocateAndReportInstance(env, clss);
        if (clss == NULL) {
            FailedLoadingClass(className);
            return NULL;
        }
    }
    // XXX

    //clss->class_loader = this;
    if(!FinishLoadingClass(env, clss, &super_class_cp_index)) {
        FailedLoadingClass(className);
        return NULL;
    }

    clss->package = ProvidePackage(env, className, NULL);

    InsertClass(clss);
    SuccessLoadingClass(className);

    //bool doNotNotifyBaseClasses = // false if class is either j/l/Object, j/io/Serializable, or j/l/Class
    //    (clss->name != env->JavaLangObject_String)
    //    && (env->java_io_Serializable_Class != NULL && clss->name != env->java_io_Serializable_Class->name)
    //    && (clss->name != env->JavaLangClass_String);
    //if( this != env->bootstrap_class_loader || doNotNotifyBaseClasses )
    if(this != env->bootstrap_class_loader || !env->InBootstrap())
    {
        jvmti_send_class_load_event(env, clss);
    }

    return clss;
}

 Package* ClassLoader::ProvidePackage(Global_Env* env, const String *class_name, 
                                     const char *jar) {
     const char* clss = class_name->bytes;
     const char* sep = strrchr(clss, '/');
     const String* package_name;
     if (!sep) {
         // this must be the default package...
         package_name = env->string_pool.lookup("");
     } else {
         package_name = env->string_pool.lookup(clss, 
            static_cast<unsigned>(sep - clss));
     }
     Lock();
     Package *package = m_package_table->lookup(package_name);
     if (package == NULL) {
         // create a new package
         void* p = apr_palloc(pool, sizeof(Package));
         const char* jar_url = jar ? 
             apr_pstrcat(pool, "jar:file:", jar, "!/", NULL) : NULL;
         package = new (p) Package(package_name, jar_url);
         m_package_table->Insert(package);
     }
     Unlock();
 
     return package;
 }

Class* ClassLoader::LoadVerifyAndPrepareClass(Global_Env* env, const String* name)
{
    assert(tmn_is_suspend_enabled());

    Class* clss = LoadClass(env, name);
    if(!clss) return NULL;

    if(!class_verify(env, clss)) {
        return NULL;
    }

    if(!class_prepare(env, clss)) {
        return NULL;
    }

    return clss;
}


void ClassLoader::AddFailedClass(const String *className, const jthrowable exn) {
    assert(exn != NULL);
    tmn_suspend_disable();
    m_lock._lock();

    FailedClass fc;
    fc.m_name = className;
    fc.m_exception = ((ObjectHandle)exn)->object;
    m_failedClasses->Insert(className, fc);

    m_lock._unlock();
    tmn_suspend_enable();
}

void ClassLoader::ReportFailedClass(Class* klass, const jthrowable exn)
{
    if (exn == NULL) {
        assert(exn_raised());
        return; // OOME
    }
    AddFailedClass(klass->name, exn);
    class_set_error_cause(klass, exn);
}

void ClassLoader::ReportFailedClass(Class* klass, const char* exnclass, std::stringstream& exnmsg)
{
    jthrowable exn = exn_create(exnclass, exnmsg.str().c_str());

    // ppervov: FIXME: should throw OOME
    AddFailedClass(klass->name, exn);
    class_set_error_cause(klass, exn);
}



void ClassLoader::ReportFailedClass(const char* klass, const char* exnclass, std::stringstream& exnmsg)
{
    const String* klassName = VM_Global_State::loader_env->string_pool.lookup(klass);

    jthrowable exn = exn_create(exnclass, exnmsg.str().c_str());
    // ppervov: FIXME: should throw OOME
    AddFailedClass(klassName, exn);
}

bool ClassLoader::FinishLoadingClass(Global_Env* env, Class* clss, unsigned* super_class_cp_index)
{
    clss->state = ST_LoadingAncestors;

    String* superName = clss->super_name;

    if( superName == NULL ) {
        if(env->InBootstrap() || clss->name != env->JavaLangClass_String) {
            // This class better be java.lang.Object
            if( clss->name != env->JavaLangObject_String ) {
                // ClassFormatError
                std::stringstream ss;
                ss << clss->name->bytes << ": class does not have superclass but the class is not java.lang.Object";
                REPORT_FAILED_CLASS_CLASS(this, clss, "java/lang/ClassFormatError", ss.str().c_str());
                return false;
            }
        }
    } else {
        // Load super class
        Class* superClass;
        clss->super_class = NULL;
        superClass = LoadClass(env, superName);

        if(superClass == NULL) {
            if(!GetClassError(clss->name->bytes)) {
                // Don't report failed classes more than one time
                REPORT_FAILED_CLASS_CLASS_EXN(this, clss, GetClassError(superName->bytes));
            }
            return false;
        }
        if(class_is_interface(superClass) || class_is_final(superClass)) {
            REPORT_FAILED_CLASS_CLASS(this, clss, "java/lang/IncompatibleClassChangeError", clss->name->bytes);
            return false;
        }
        if(!class_verify(env, superClass)) return false;
        if(!class_prepare(env, superClass)) return false;

        // super class was successfully loaded
        clss->super_class = superClass;
        if( super_class_cp_index && *super_class_cp_index ) {
            cp_resolve_to_class( clss->const_pool, *super_class_cp_index, superClass );
        }

        // if it's an interface, its superclass must be java/lang/Object
        if(class_is_interface(clss)) {
            if((env->JavaLangObject_Class != NULL) && (superClass != env->JavaLangObject_Class)) {
                std::stringstream ss;
                ss << clss->name->bytes << ": interface superclass is not java.lang.Object";
                REPORT_FAILED_CLASS_CLASS(this, clss, "java/lang/ClassFormatError", ss.str().c_str());
                return false;
            }
        }

        // 2003-06-18. Update the cha_first_child and cha_next_sibling fields.
        clss->cha_first_child = NULL;
        if (clss->super_class != NULL)
        {
            clss->cha_next_sibling = clss->super_class->cha_first_child;
            clss->super_class->cha_first_child = clss;
        }
        // Notify interested JITs that the superclass has been extended.
        do_jit_extended_class_callbacks( superClass, /*new_subclass*/ clss );
    }

    //
    // load in super interfaces
    //
    for( unsigned i = 0; i < clss->n_superinterfaces; i++ ) {
        String *intfc_name = clss->superinterfaces[i].name;
        Class* intfc = LoadVerifyAndPrepareClass(env, intfc_name);
        if( intfc == NULL ) {
            if(!GetClassError(clss->name->bytes)) {
                REPORT_FAILED_CLASS_CLASS_EXN(this, clss, GetClassError(intfc_name->bytes));
            }
            return false;
        }
        if(!class_is_interface(intfc)) {
            REPORT_FAILED_CLASS_CLASS(this, clss, "java/lang/IncompatibleClassChangeError",
                clss->name->bytes << ": " << intfc->name->bytes << " is not an interface");
            return false;
        }

        // superinterface was successfully loaded
        clss->superinterfaces[i].clss = intfc;
    }
    // class, superclass, and superinterfaces successfully loaded

    clss->state = ST_Loaded;

    return true;
}

void ClassLoader::RemoveLoadingClass(const String* className, LoadingClass* loading) 
{
    assert(loading);
    loading->EnqueueInitiator(get_thread_ptr(), this, className);
    loading->UpdateInitiator(NULL);
    if(!loading->HasWaitingThreads()) {
        TRACE2("classloader.collisions", this << " " << get_thread_ptr() << " R " << className->bytes);
        m_loadingClasses->Remove(className);
    }
}

void ClassLoader::SuccessLoadingClass(const String* className) 
{
    LMAutoUnlock aulock( &m_lock );
    LoadingClass* lc = m_loadingClasses->Lookup(className);
    assert(lc);
    lc->SignalLoading();
    RemoveLoadingClass(className, lc);
}

ClassLoader* ClassLoader::FindByObject(ManagedObject* loader)
{
    LMAutoUnlock aulock( &(ClassLoader::m_tableLock) );
    ClassLoader* cl;
    for(unsigned i = 0; i < m_nextEntry; i++)
    {
        cl = m_table[i];
        if( loader == cl->m_loader ) return cl;
    }
    return NULL;
}


ClassLoader* ClassLoader::LookupLoader( ManagedObject* loader )
{
    if( !loader ) return NULL;
    ClassLoader *cl = FindByObject( loader );
    if( cl )
        return cl;
    else
        return AddClassLoader( loader );
}


void ClassLoader::UnloadClassLoader( ManagedObject* loader )
{
    LMAutoUnlock aulock( &(ClassLoader::m_tableLock) );
    unsigned i;
    for(i = 0; i < m_nextEntry; i++)
    {
        ClassLoader* cl = m_table[i];
        if( loader == cl->m_loader ) break;
    }
    if (i == m_nextEntry) return;
    ClassLoader* cl = m_table[i];
    --m_nextEntry;
    for (; i < m_nextEntry; i++)
        m_table[i] = m_table[i+1];
    delete cl; // !!! Must use MM here
}


void ClassLoader::gc_enumerate()
{
    TRACE2("enumeration", "enumerating classes");
    LMAutoUnlock aulock( &(ClassLoader::m_tableLock) );
    FailedClasses::iterator fci;
    // should enumerate errors in bootstrap
    for(fci = VM_Global_State::loader_env->bootstrap_class_loader->m_failedClasses->begin();
        fci != VM_Global_State::loader_env->bootstrap_class_loader->m_failedClasses->end(); fci++)
    {
        vm_enumerate_root_reference((void**)(&(fci->second.m_exception)), FALSE);
    }

    for(unsigned int i = 0; i < m_nextEntry; i++) {
        if(m_table[i]->m_loader != NULL) {
            vm_enumerate_root_reference((void**)(&(m_table[i]->m_loader)), FALSE);
            // should enumerate errors for classes
            for(fci = m_table[i]->m_failedClasses->begin();
                fci != m_table[i]->m_failedClasses->end(); fci++)
            {
                vm_enumerate_root_reference((void**)(&(fci->second.m_exception)), FALSE);
            }
        }
    }
}


void ClassLoader::ClearMarkBits()
{
    TRACE2("classloader.unloading.clear", "Clearing mark bits");
    LMAutoUnlock aulock( &(ClassLoader::m_tableLock) );
    ClassTable::iterator cti;
    unsigned i;
    for(i = 0; i < m_nextEntry; i++) {
        if(m_table[i]->m_unloading) {
            TRACE2("classloader.unloading.debug", "  Skipping \"unloaded\" classloader "
                << m_table[i] << " (" << m_table[i]->m_loader << " : "
                << ((VTable*)(*(unsigned**)(m_table[i]->m_loader)))->clss->name->bytes << ")");
            continue;
        }
        TRACE2("classloader.unloading.debug", "  Clearing mark bits in classloader "
            << m_table[i] << " (" << m_table[i]->m_loader << " : "
            << ((VTable*)(*(unsigned**)(m_table[i]->m_loader)))->clss->name->bytes << ") and its classes");
        // clear mark bits in loader and classes
        m_table[i]->m_markBit = 0;
        for(cti = m_table[i]->m_loadedClasses->begin();
            cti != m_table[i]->m_loadedClasses->end(); cti++)
        {
            if(cti->second->class_loader == m_table[i]) {
                cti->second->m_markBit = 0;
             }
         }
     }
    TRACE2("classloader.unloading.clear", "Finished clearing mark bits");
    TRACE2("classloader.unloading.marking", "Starting mark loaders");
}


void ClassLoader::StartUnloading()
{
    TRACE2("classloader.unloading.marking", "Finished marking loaders");
    TRACE2("classloader.unloading.do", "Start checking loaders ready to be unloaded");
    LMAutoUnlock aulock( &(ClassLoader::m_tableLock) );
    unsigned i;
    for(i = 0; i < m_nextEntry; i++) {
        if(m_table[i]->m_unloading) {
            TRACE2("classloader.unloading.debug", "  Skipping \"unloaded\" classloader "
                << m_table[i] << " (" << m_table[i]->m_loader << " : "
                << ((VTable*)(*(unsigned**)(m_table[i]->m_loader)))->clss->name->bytes << ")");
            continue;
        }
        TRACE2("classloader.unloading.debug", "  Scanning loader "
            << m_table[i] << " (" << m_table[i]->m_loader << " : "
            << ((VTable*)(*(unsigned**)(m_table[i]->m_loader)))->clss->name->bytes << ")");
        if(!m_table[i]->m_markBit) {
            TRACE2("classloader.unloading.stats", "  (!) Ready to unload classloader "
                << m_table[i] << " (" << m_table[i]->m_loader << " : "
                << ((VTable*)(*(unsigned**)(m_table[i]->m_loader)))->clss->name->bytes << ")");
            TRACE2("classloader.unloading.stats", "  (!) This will free "
                << m_table[i]->GetFullSize() << " bytes in C heap");
            m_table[i]->m_unloading = true;
            m_unloadedBytes += m_table[i]->GetFullSize();
        }
    }
    TRACE2("classloader.unloading.do", "Finished checking loaders");
}


void ClassLoader::PrintUnloadingStats()
{
    unsigned i;
    TRACE2("classloader.unloading.stats", "----------------------------------------------");
    TRACE2("classloader.unloading.stats", "Class unloading statistics:");
    for(i = 0; i < m_nextEntry; i++) {
        if(m_table[i]->m_unloading) {
            TRACE2("classloader.unloading.stats", "  Class loader "
                << m_table[i] << " (" << m_table[i]->m_loader << " : "
                << ((VTable*)(*(unsigned**)(m_table[i]->m_loader)))->clss->name->bytes
                << ") contains " << m_table[i]->GetFullSize() << " bytes in C heap");
        }
    }
    TRACE2("classloader.unloading.stats", "A total of "
        << m_unloadedBytes << " bytes would be freed in C heap for this scenario");
    TRACE2("classloader.unloading.stats", "----------------------------------------------");
}


void vm_classloader_iterate_objects(void *iterator) {

    // skip the object iteration if it is not needed
    // (logging is not enabled and 
    // class unloading is not yet implemented).
    if (!is_info_enabled("class_unload")) return;

    Managed_Object_Handle obj;
    int nobjects = 0;
    while((obj = gc_get_next_live_object(iterator))) {
        nobjects++;
    }
    INFO2("class_unload", "classloader_iterate_objects " << nobjects << " iterated");
}


ClassLoader* ClassLoader::AddClassLoader( ManagedObject* loader )
{
    LMAutoUnlock aulock( &(ClassLoader::m_tableLock) );
    ClassLoader* cl = new UserDefinedClassLoader();
    TRACE2("classloader.unloading.add", "Adding class loader "
        << cl << " (" << loader << " : "
        << ((VTable*)(*(unsigned**)(loader)))->clss->name->bytes << ")");
    cl->Initialize( loader );
    if( m_capacity <= m_nextEntry )
        ReallocateTable( m_capacity?(2*m_capacity):32 );
    m_table[m_nextEntry++] = cl;
    return cl;
}


void ClassLoader::ReallocateTable( unsigned int new_capacity )
{
    LMAutoUnlock aulock( &(ClassLoader::m_tableLock) );
    ClassLoader** new_table =
        (ClassLoader**) STD_MALLOC( new_capacity*sizeof(ClassLoader*) );
    if( new_table == NULL ) return;
    memcpy( new_table, m_table, m_nextEntry*sizeof(ClassLoader*) );
    assert(m_nextEntry <= m_capacity);
    assert(m_nextEntry < new_capacity);
    if( m_table ) {
        STD_FREE( m_table );
    }
    m_table = new_table;
    m_capacity = new_capacity;
}


Class* ClassLoader::StartLoadingClass(Global_Env* UNREF env, const String* className)
{
    Class** pklass = NULL;
    Class* klass = NULL;

    // try to find if class is already loaded
    VM_thread* cur_thread = get_thread_ptr();
    while(true)
    {
        LMAutoUnlock aulock(&m_lock);
        FailedClass* failed = m_failedClasses->Lookup(className);
        if(failed) return NULL;
        pklass = m_loadedClasses->Lookup(className);
        if(pklass)
        {
            klass = *pklass;
            // class has already been loaded
            if(klass->state == ST_LoadingAncestors) {
                // there is a circularity in the class hierarchy
                aulock.ForceUnlock();
                REPORT_FAILED_CLASS_CLASS(this, klass, "java/lang/ClassCircularityError", klass->name->bytes);
                return NULL;
            }
            return klass;
        }
        LoadingClass* loading = m_loadingClasses->Lookup(className);
        if(loading)
        {
            if(loading->IsInitiator(cur_thread) || loading->IsDefiner(cur_thread)) {
                // check if one thread can load one class recursively
                // at first sight, this is a circularity error condition
                aulock.ForceUnlock();
                REPORT_FAILED_CLASS_NAME(this, className->bytes,
                    "java/lang/ClassCircularityError", className->bytes);
                return NULL;
            }
            TRACE2("classloader.collisions",
                this << " Collision while loading class " << className->bytes);
            if(m_loader == NULL) {
                // avoid preliminary removal of LoadingClass
                loading->AddWaitingThread(cur_thread, this, className);
                // lazy wait event creation
                if(!loading->CreateWaitingEvent(className)) {
                    // should never get here
                    DIE("Event creation failure was not reported: should never get here!");
                    return NULL;
                }
                aulock.ForceUnlock();
                // only wait for class loading if we are in bootstrap class loader
                loading->WaitLoading();
                continue;
            } else {
                // if we are in a user class loader
                // register as a loading thread
                if(!loading->AlreadyWaiting(cur_thread)) {
                    loading->AddWaitingThread(cur_thread, this, className);
                } else {
                    // it is recursive resolution
                    assert(0 && "Recursive resolution happened!");
                }

                return NULL;
            }
        }
        LoadingClass lc;
#ifdef _DEBUG
        lc.m_name = (String*)className;
#endif
        lc.SetInitiator(cur_thread);
        TRACE2("classloader.collisions", this << " " << cur_thread << " I " << className->bytes);
        m_loadingClasses->Insert(className, lc);
        return NULL;
    }
}


void ClassLoader::FailedLoadingClass(const String* className)
{
    LMAutoUnlock aulock( &m_lock );

    LoadingClass* lc = m_loadingClasses->Lookup(className);
    assert(lc);
    lc->SignalLoading();
    RemoveLoadingClass(className, lc);
    if(m_reportedClasses->Lookup(className)) {
        m_reportedClasses->Remove(className);
    }
}


unsigned ClassLoader::GetFullSize() {
    if(m_fullSize)
        return m_fullSize;
    m_fullSize = sizeof(ClassLoader);
    ClassTable::iterator cti;
    for(cti = m_loadedClasses->begin();
        cti != m_loadedClasses->end(); cti++)
    {
        if(cti->second->class_loader == this) {
            m_fullSize += class_calculate_size(cti->second);
        }
    }
    return m_fullSize;
}


Class* ClassLoader::WaitDefinition(Global_Env* env, const String* className)
{
    VM_thread* cur_thread = get_thread_ptr();
    LoadingClass* loading;
    Class** pclss;
    Class* clss = NULL;
    m_lock._lock();
    do
    {
        if(m_failedClasses->Lookup(className)) {
            m_lock._unlock();
            return NULL;
        }
        if((pclss = m_loadedClasses->Lookup(className)) != NULL) {
            m_lock._unlock();
            return *pclss;
        }
        loading = m_loadingClasses->Lookup(className);
        if(loading) {
            TRACE2("classloader.collisions", this << " " << cur_thread << " DC " << className->bytes);
            if(!loading->HasDefiner()) {
                break;
            } else {
                // defining thread should not get here
                assert(!loading->IsDefiner(cur_thread));
                assert(loading->AlreadyWaiting(cur_thread));
                // lazy wait event creation
                if(!loading->CreateWaitingEvent(className)) {
                    // should never get here
                    DIE("Event creation failure was not reported: should never get here!");
                    return NULL;
                }
                m_lock._unlock();
                TRACE2("classloader.collisions", this << " " << cur_thread << " WAITING " << className->bytes);
                // wait class loading
                loading->WaitLoading();
                m_lock._lock();
                // check if it is not direct defineClass call which happened to
                // compete in defining this class with some other thread
            }
        }

        // if we do not have class with this name already loaded
        if((clss = StartLoadingClass(env, className)) != NULL) {
            if(loading && loading->AlreadyWaiting(cur_thread)) {
                loading->RemoveWaitingThread(cur_thread, this, className);
                if(!loading->HasWaitingThreads()){
                    TRACE2("classloader.collisions", this << " " << cur_thread << " R " << className->bytes);
                    m_loadingClasses->Remove(className);
                }
                m_lock._unlock();
                TRACE2("classloader.collisions", this << " " << cur_thread << " ret " << className->bytes);
                return clss;
            }
            // now we will execute Java code, when creating exception
            // so, unlock class loader lock
            m_lock._unlock();
            // otherwise, we have a class, which was already successfully
            // created earlier, but someone has called ClassLoader.defineClass
            // second time for this class. Spec requires us to throw LinkageError.
            // We only have to report current error condition and do not need
            // to record error state in the class...
            std::stringstream ss;
            ss << "class " << clss->name->bytes << " is defined second time";
            jthrowable exn = exn_create("java/lang/LinkageError", ss.str().c_str());
            exn_raise_only(exn);
            return NULL;
        }

        loading = m_loadingClasses->Lookup(className);
    } while(0);

    assert(loading != NULL);
    // mark ourselves as defining thread
    loading->ChangeDefinerAndInitiator(cur_thread, this, className);

    // unlock class loader and start defining class
    m_lock._unlock();

    return clss;
}


Class* ClassLoader::SetupAsArray(Global_Env* env, const String* classNameString)
{
    // check java/lang/Object is loaded
    assert(env->JavaLangObject_Class != NULL);
    assert(classNameString);
    const char* className = classNameString->bytes;
    // make sure it is array name
    assert(className && className[0] == '[');

    // count number of dimentions in requested array
    unsigned n_dimensions = 1;
    while(className[n_dimensions] == '[') n_dimensions++;

    bool isArrayOfPrimitives = false;
    const char* baseType = &className[n_dimensions];
    Class* baseClass;
    switch(*baseType)
    {
    case 'B':
    case 'C':
    case 'D':
    case 'F':
    case 'I':
    case 'J':
    case 'S':
    case 'Z':
        // for primitive type these must be no more symbols
        // in class name
        if ('\0' != *(baseType + 1))
        {
            REPORT_FAILED_CLASS_NAME(this, classNameString->bytes, "java/lang/NoClassDefFoundError", classNameString->bytes);
            return NULL;
        }
        baseClass = (Class*)class_get_class_of_primitive_type((VM_Data_Type)*baseType);
        isArrayOfPrimitives = true;
        break;
    case 'L':
        {
            unsigned nameLen;
            baseType++;
            for(nameLen = 0; baseType[nameLen] != ';' &&
                             baseType[nameLen] != 0; nameLen++);
            if(!baseType[nameLen]) {
                REPORT_FAILED_CLASS_NAME(this, className, "java/lang/NoClassDefFoundError", className);
                return NULL;
            }
            baseClass = LoadVerifyAndPrepareClass(env, env->string_pool.lookup(baseType, nameLen));
            if(baseClass == NULL) {
                FailedLoadingClass(classNameString);
                return NULL;
            }
            isArrayOfPrimitives = false;
        }
        break;

    default:
        REPORT_FAILED_CLASS_NAME(this, className,
            "java/lang/NoClassDefFoundError", className);
        return NULL;
    }
    ClassLoader* baseLoader = baseClass->class_loader;
    Class* elementClass = baseClass;
    if(n_dimensions > 1) {
        elementClass = baseLoader->LoadVerifyAndPrepareClass(env, env->string_pool.lookup(&className[1]));
        if(elementClass == NULL) {
            FailedLoadingClass(classNameString);
            return NULL;
        }
    }

    Class* klass;
    if(baseLoader != this) {
        // if base class was loaded with different loader
        // allow base loader to load the array
        klass = baseLoader->LoadVerifyAndPrepareClass(env, classNameString);
        if(klass) {
            SuccessLoadingClass(classNameString);
        } else {
            FailedLoadingClass(classNameString);
        }
        return klass;
    } else {
        // we should wait here for creating class
        if((klass = WaitDefinition(env, classNameString)) != NULL)
            return klass;

        m_lock._lock();
        if(m_failedClasses->Lookup(classNameString)) {
            m_lock._unlock();
            return NULL;
        }
        m_lock._unlock();

        // create class
        klass = NewClass(env, classNameString);
        if (!klass) {
            FailedLoadingClass(classNameString);
            return NULL;
        }

        // setup array-related fields
        klass->is_array = 1;
        klass->n_dimensions = (unsigned char)n_dimensions;
        if(n_dimensions == 1) {
            klass->is_array_of_primitives = isArrayOfPrimitives;
        } else {
            klass->is_array_of_primitives = false;
        }
        klass->array_element_class = elementClass;
        klass->array_base_class = baseClass;
        klass->is_verified = 2;

        assert(elementClass);
        klass->array_element_type_desc = type_desc_create_from_class(elementClass);

        // insert Java field, required by spec - 'length'
        klass->n_fields = 1;
        klass->fields = new Field[1];
        klass->fields[0].set(klass, env->Length_String, env->IntDescriptor_String, ACC_PUBLIC|ACC_FINAL);
        klass->fields[0].set_field_type_desc(
            type_desc_create_from_java_descriptor("I", NULL));

        klass->super_name = env->JavaLangObject_String;

        // set array access flags the same as its base class
        klass->access_flags = (ACC_FINAL | ACC_ABSTRACT);
        if(isArrayOfPrimitives) {
            klass->access_flags |= ACC_PUBLIC;
        } else {
            klass->access_flags = (uint16)(klass->access_flags | (baseClass->access_flags
                & (ACC_PUBLIC | ACC_PRIVATE | ACC_PROTECTED) ));
        }
        klass->package = elementClass->package;

        // array classes implement two interfaces: Cloneable and Serializable
        klass->superinterfaces = (Class_Superinterface*) STD_MALLOC(2 * sizeof(Class_Superinterface));
        klass->superinterfaces[0].name = env->Clonable_String;
        klass->superinterfaces[1].name = env->Serializable_String;
        klass->n_superinterfaces = 2;
    }

    if(!FinishLoadingClass(env, klass, NULL)) {
        FailedLoadingClass(classNameString);
        return NULL;
    }

    InsertClass(klass);
    SuccessLoadingClass(classNameString);

    return klass;
} // ClassLoader::SetupAsArray

/** Adds Class* pointer to m_reportedClasses HashTable. 
*   clss->name must not be NULL.
*/
Class* ClassLoader::AllocateAndReportInstance(const Global_Env* env, Class* clss)
{
    const String* name = clss->name;
    assert(name);

    if(env->InBootstrap()) {
        assert((clss->name == env->JavaLangObject_String)
             || (strcmp(clss->name->bytes, "java/io/Serializable") == 0)
             || (clss->name == env->JavaLangClass_String));
        clss->class_handle = NULL;
    } else {
        Class* root_class = env->JavaLangClass_Class;
        assert(root_class != NULL);

        tmn_suspend_disable(); // -----------------vvv
        ManagedObject* new_java_lang_Class =
            (ManagedObject*)gc_alloc(root_class->instance_data_size,
                root_class->allocation_handle, vm_get_gc_thread_local());
        if(new_java_lang_Class == NULL)
        {
            tmn_suspend_enable();
            // couldn't allocate java.lang.Class instance for this class
            // ppervov: TODO: throw OutOfMemoryError
            exn_raise_only(
                VM_Global_State::loader_env->java_lang_OutOfMemoryError);
            return NULL;
        }
        // add newly created java_lang_Class to reportable collection
        LMAutoUnlock aulock(&m_lock);
        clss->class_handle = m_reportedClasses->Insert(name, new_java_lang_Class);
        aulock.ForceUnlock();
        TRACE("NewClass inserting class \"" << name->bytes
            << "\" with key " << name << " and object " << new_java_lang_Class);
        assert(new_java_lang_Class == *(clss->class_handle));
#ifdef VM_STATS
        root_class->num_allocations++;
        root_class->num_bytes_allocated += root_class->instance_data_size;
#endif //VM_STATS

        assert(env->vm_class_offset);
        set_struct_Class_field_in_java_lang_Class(env, clss->class_handle, clss);
        tmn_suspend_enable(); // -----------------^^^
    }

    return clss;
}

Class* ClassLoader::InitClassFields(const Global_Env* env, Class* clss, const String* name)
{
    memset(clss, 0, sizeof(Class));

#ifdef POINTER64
    clss->alignment = ((GC_OBJECT_ALIGNMENT<8)?8:GC_OBJECT_ALIGNMENT);;
#else
    clss->alignment = GC_OBJECT_ALIGNMENT;
#endif
 
    clss->id = class_next_id++;
    clss->name = name;
    clss->class_loader = this;
    clss->state = ST_Start;

    clss->m_lock = new Lock_Manager();

    // Special case: ClassLoader.defineClass() call with null classname value.
    // Calling AddToReported() only if real class name is already known.
    if (clss->name != NULL)
        clss = AllocateAndReportInstance(env, clss);

    return clss;
}

void ClassLoader::ClassClearInternals(Class* clss)
{
    FieldClearInternals(clss);

    if (clss->methods != NULL)
    {
        for (int i = 0; i < clss->n_methods; i++){
            clss->methods[i].MethodClearInternals();
        }
        delete []clss->methods;
        clss->methods = NULL;
    }
    if (clss->const_pool != NULL)
    {
        if (clss->const_pool[0].tags)
            delete []clss->const_pool[0].tags;
        delete []clss->const_pool;
        clss->const_pool = NULL;
    }
    if (clss->vtable_descriptors)
        delete []clss->vtable_descriptors;
    
    if (clss->static_method_block)
        delete []clss->static_method_block;

    if (clss->m_lock)
        delete clss->m_lock;

    if (clss->array_element_type_desc)
    {
        delete clss->array_element_type_desc;
        clss->array_element_type_desc = NULL;
    }
}

void ClassLoader::FieldClearInternals(Class* clss)
{
    if (clss->fields != NULL)
    {
        delete []clss->fields;   
        clss->fields = NULL;
    }
}

void ClassLoader::LoadNativeLibrary( const char *name )
{
    // get library name from string pool
    Global_Env *env = VM_Global_State::loader_env;
    const String *lib_name = env->string_pool.lookup( name );

    // lock class loader
    LMAutoUnlock cl_lock( &m_lock );

    NativeLibInfo* info;

    // find throughout all the libraries in this class loader
    for( info = m_nativeLibraries;
         info;
         info = info->next )
    {
        if( info->name == lib_name ) {
            // found need library
            return;
        }
    }

    // load native library
    bool just_loaded;
    NativeLoadStatus status;
    NativeLibraryHandle handle = natives_load_library(name, &just_loaded, &status);
    if( !handle || !just_loaded ) {
        // create error message
        char apr_error_message[1024];
        natives_describe_error(status, apr_error_message, 
                sizeof(apr_error_message));

        std::stringstream message_stream;
        message_stream << "Failed loading library \"" << name << "\": " 
                << apr_error_message;

        // trace
        TRACE2("classloader.native", "Loader (" << this << ") native library: "
            << message_stream.str().c_str());

        // unlock class loader
        cl_lock.ForceUnlock();

        // report exception
        ReportException("java/lang/UnsatisfiedLinkError", message_stream);
        return;
    }

    // trace
    TRACE2("classloader.native", "Loader (" << this
        << ") loaded native library: " << name);

    // allocate memory
    info = (NativeLibInfo*)Alloc(sizeof(NativeLibInfo));

    // set native library
    info->name = lib_name;
    info->handle = handle;
    info->next = m_nativeLibraries;
    m_nativeLibraries = info;
    return;
}

GenericFunctionPointer ClassLoader::LookupNative(Method* method)
{
    // get class and class loader of a given method
    Class* klass = method->get_class();

    // get class name, method name and method descriptor
    const String* class_name = klass->name;
    const String* method_name = method->get_name();
    const String* method_desc = method->get_descriptor();

    // trace
    TRACE2("classloader.native", "Loader (" << this << ") find native function: "
        << class_name->bytes << "."
        << method_name->bytes << method_desc->bytes);

    // return if method is already registered
    if( method->is_registered() ) {
        return (GenericFunctionPointer)method->get_code_addr();
    }
    
    // find throughout all the libraries 
    GenericFunctionPointer func = natives_lookup_method(m_nativeLibraries,
        class_name->bytes, method_name->bytes, method_desc->bytes);
    if(func) return func;

    // create error string "<class_name>.<method_name><method_descriptor>
    int clen = class_name->len;
    int mlen = method_name->len;
    int dlen = method_desc->len;
    int len = clen + 1 + mlen + dlen;
    char *error = (char*)STD_ALLOCA(len + 1);
    memcpy(error, class_name, clen);
    error[clen] = '.';
    memcpy(error + clen + 1, method_name, mlen);
    memcpy(error + clen + 1 + mlen, method_desc, dlen);
    error[len] = '\0';

    // trace
    TRACE2("classloader.native", "Loader (" << this << ") native function not found: "
        << class_name->bytes << "."
        << method_name->bytes << method_desc->bytes);

    // raise exception
    exn_raise_by_name( "java/lang/UnsatisfiedLinkError", error );

    return NULL;
}

void class_unloading_clear_mark_bits() {
    ClassLoader::ClearMarkBits();
}

void class_unloading_start() {
    ClassLoader::StartUnloading();
}

inline void
BootstrapClassLoader::SetClasspathFromProperty(const char* prop_string,
                                               apr_pool_t* tmp_pool)
{
    // get property value
    const char *bcp_value = properties_get_string_property(
        reinterpret_cast<PropertiesHandle>(&m_env->properties), prop_string);
    assert(bcp_value);

    size_t len = strlen(bcp_value) + 1;
    char *bcp = (char *)STD_ALLOCA( len );
    memcpy(bcp, bcp_value, len);
#ifdef PLATFORM_NT
    //on windows, we change the path to lower case
    strlwr(bcp);
#endif // PLATFORM_NT

    // set bootclasspath elements
    const char separator[2] = {PORT_PATH_SEPARATOR, 0};
    char *path_name = strtok(bcp, separator);
    while (path_name)
    {
        SetBCPElement(path_name, tmp_pool);
        path_name = strtok(NULL, separator);
    }
    return;
} // BootstrapClassLoader::SetClasspathFromProperty

inline void BootstrapClassLoader::SetBCPElement(const char *path, apr_pool_t *tmp_pool)
{
    BCPElement* element;
    // check existence of a given path in bootstrap classpath
    const char* canoname = port_filepath_canonical(path, tmp_pool); 
    const String* new_path = m_env->string_pool.lookup(canoname);
    for( element = m_BCPElements.m_first; element; element = element->m_next ) {
        if( element->m_path == new_path ) {
            // found such path
            return;
        }
    }

    // check existance of a given path
    apr_finfo_t finfo;
    if(apr_stat(&finfo, new_path->bytes, APR_FINFO_SIZE, tmp_pool) != APR_SUCCESS) {
        // broken path to the file
        return;
    }

    // allocate and set a new bootsclasspath element
    element = (BCPElement*)apr_palloc(pool, sizeof(BCPElement) );
    element->m_path = new_path;
    element->m_next = NULL;

    // check if it is a archive file
    if( file_is_archive(new_path->bytes) ) {
        // create and parse a new archive file structure
        element->m_isJarFile = true;
        void* mem_JarFile = apr_palloc(pool, sizeof(JarFile) );
        element->m_jar = new (mem_JarFile) JarFile();
        if( element->m_jar && element->m_jar->Parse(new_path->bytes) ) {
            TRACE2("classloader.jar", "opened archive: " << new_path );
        } else {
            if( element->m_jar ) {
                element->m_jar->~JarFile();
            }
            return;
        }
    } else {
        element->m_isJarFile = false;
    }
    
    // insert element into collection
    if( NULL == m_BCPElements.m_first ) {
        m_BCPElements.m_first = element;
    } else {
        m_BCPElements.m_last->m_next = element;
    }
    m_BCPElements.m_last = element;

    return;
} // BootstrapClassLoader::SetBCPElement

inline void BootstrapClassLoader::SetClasspathFromJarFile(JarFile *jar, apr_pool_t* tmp_pool)
{
    // get classpath from archive file
    const char* jar_classpath = archive_get_class_path(jar);
    if( !jar_classpath ) {
        // no classpath found
        return;
    }

    // copy a given classpath
    size_t len = strlen(jar_classpath) + 1;
    char* classpath = (char*)STD_ALLOCA(len);
    memcpy(classpath, jar_classpath, len);
#ifdef PLATFORM_NT
    //on windows, we change the path to lower case
    strlwr(classpath);
#endif // PLATFORM_NT

    // get archive file directory
    const char* jar_name = jar->GetName();
    assert(jar_name);
    len = strlen(jar_name) + 1;
    char* dir_name = (char*)STD_ALLOCA(len);
    memcpy(dir_name, jar_name, len);
    char* end_dir = strrchr(dir_name, PORT_FILE_SEPARATOR);
    if( end_dir ) {
        // archive file name has file separator
        // find end of directory name
        while( *(end_dir - 1) == PORT_FILE_SEPARATOR ) {
            end_dir--;
        }
        *(end_dir + 1) = '\0';
#ifdef PLATFORM_NT
        //on windows, we change the path to lower case
        strlwr(dir_name);
#endif // PLATFORM_NT
    } else {
        // only file name without directory
        dir_name = NULL;
    }

    // combine classpath as a directory name + classpath
    // and set into bootclasspath collection
    char* path_name = strtok(classpath, " ");
    while( path_name != NULL ) {
        if( dir_name ) {
            // catenate directory name and classpath
            path_name = apr_pstrcat(tmp_pool, dir_name, path_name, NULL );
        }
        SetBCPElement(path_name, tmp_pool);
        path_name = strtok(NULL, " ");
    }

    return;
} // BootstrapClassLoader::SetClasspathFromJarFile

BootstrapClassLoader::BootstrapClassLoader(Global_Env* env) : m_env(env)
{
    // create array of primitive types
    primitive_types[0] = new TypeDesc(K_S1, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[1] = new TypeDesc(K_S2, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[2] = new TypeDesc(K_S4, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[3] = new TypeDesc(K_S8, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[4] = new TypeDesc(K_Sp, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[5] = new TypeDesc(K_U1, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[6] = new TypeDesc(K_U2, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[7] = new TypeDesc(K_U4, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[8] = new TypeDesc(K_U8, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[9] = new TypeDesc(K_Up, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[10] = new TypeDesc(K_F4, NULL, NULL, NULL,(ClassLoader*)this, NULL);
    primitive_types[11] = new TypeDesc(K_F8, NULL, NULL, NULL,(ClassLoader*)this, NULL);
    primitive_types[12] = new TypeDesc(K_Boolean, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[13] = new TypeDesc(K_Char, NULL, NULL, NULL, (ClassLoader*)this, NULL);
    primitive_types[14] = new TypeDesc(K_Void, NULL, NULL, NULL, (ClassLoader*)this, NULL);
}

BootstrapClassLoader::~BootstrapClassLoader()
{
    // destroy array of primitive types
    unsigned sz = sizeof(primitive_types) / sizeof(TypeDesc*);
    for (unsigned i = 0; i < sz; i++)
    {
        delete primitive_types[i];
        primitive_types[i] = NULL;
    }
}

bool BootstrapClassLoader::Initialize(ManagedObject* UNREF loader)
{
    // init bootstrap class loader
    ClassLoader::Initialize();

    // get list of natives libraries
    const char *lib_list = 
        properties_get_string_property(
            reinterpret_cast<PropertiesHandle>(&m_env->properties),
            "vm.other_natives_dlls" );
    size_t len = strlen( lib_list ) + 1;
    char *libraries = (char*)STD_ALLOCA( len );
    memcpy( libraries, lib_list, len );

    // separate natives libraries
    const char separator[2] = {PORT_PATH_SEPARATOR, 0};
    char *lib_name = strtok( libraries, separator );
    while( lib_name != NULL )
    {
        // load native library
        LoadNativeLibrary( lib_name );

        // find next library
        lib_name = strtok( NULL, separator );
    }

    // create temp pool for apr functions
    apr_pool_t *tmp_pool;
    apr_pool_create(&tmp_pool, NULL);

    // create a bootclasspath collection
    SetClasspathFromProperty("vm.boot.class.path", tmp_pool);

    // check if vm.bootclasspath.appendclasspath property is set to true
    Boolean is_enabled =
        vm_get_boolean_property_value_with_default("vm.bootclasspath.appendclasspath");
    if( TRUE == is_enabled ) {
        // append classpath to bootclasspath
        SetClasspathFromProperty("java.class.path", tmp_pool);
    }

    // get a classpath from archive files manifest and
    // set into bootclasspath collection
    for( BCPElement* element = m_BCPElements.m_first;
         element;
         element = element->m_next )
    {
        TRACE2("classloader.bootclasspath", "BCP: " << element->m_path->bytes );
        if( element->m_isJarFile ) {
            SetClasspathFromJarFile( element->m_jar, tmp_pool );
        }
    }

    // destroy temp pool
    apr_pool_destroy(tmp_pool);
    return true;
} // BootstrapClassLoader::Initialize

Class* BootstrapClassLoader::LoadClass(Global_Env* UNREF env,
                                       const String* className)
{
    assert(env == m_env);
    Class* klass = StartLoadingClass(m_env, className);
    
    LMAutoUnlock aulock(&m_lock);
    if(klass == NULL && m_failedClasses->Lookup(className) != NULL) {
        // there was an error in loading class
        return NULL;
    }
    aulock.ForceUnlock();

    if(klass != NULL) {
        // class is already loaded so return it
        return klass;
    }
    TRACE2("classloader.load", "Loader (" << this << ") loading class: " << className->bytes << "...");

    // Not in the class cache: load the class or, if an array class, create it
    if(className->bytes[0] == '[') {
        // array classes require special handling
        return SetupAsArray(m_env, className);
    } else {
        // load class from file
        klass = LoadFromFile(className);
    }

    return klass;
} // BootstrapClassLoader::LoadClass


Class* UserDefinedClassLoader::LoadClass(Global_Env* env, const String* className)
{
    assert( m_loader != NULL );

    Class* klass = StartLoadingClass(env, className);
    LMAutoUnlock aulock(&m_lock);
    if(klass == NULL && m_failedClasses->Lookup(className) != NULL) {
        return NULL;
    }
    aulock.ForceUnlock();
    if(klass != NULL) {
        return klass;
    }

    TRACE2("classloader.load", "Loader U (" << this << ") loading class: " << className->bytes << "...");

    char* cname = const_cast<char*>(className->bytes);
    if( cname[0] == '[' ) {
        return SetupAsArray(env, className);
    } 

    // Replace '/' with '.'
    unsigned class_name_len = className->len + 1;
    char* class_name_buf = new char[class_name_len]; // !!! replace with MM allocation
    for(unsigned i = 0; i < class_name_len; i++) {
        char c = className->bytes[i];
        if(c == '/') {
            class_name_buf[i] = '.';
        } else {
            class_name_buf[i] = c;
        }
    }
    assert(env);
    String* class_name_with_dots  = env->string_pool.lookup( class_name_buf );
    delete[] class_name_buf;

    // call the version that takes the resolve flag
    // some subclasses of ClassLoader do NOT overload the (Ljava/lang/String;) version of the method
    // they all define (Ljava/lang/String;Z) version because it is abstract
    // wgs's comment here:
    //  * (Ljava/lang/String;Z) is not abstract in current JDK version
    //  * Generally (Ljava/lang/String;) are overloaded
    assert(tmn_is_suspend_enabled());
    assert(!exn_raised());
    tmn_suspend_disable();

    jvalue args[2];

  
    // Set the arg #1 first because calling vm_instantiate_cp_string_resolved
    // can cause GC and we would like to get away with code that doesn't
    // protect references from GC.
    ManagedObject* jstr;
    if (env->compress_references) {
        jstr = uncompress_compressed_reference(class_name_with_dots->intern.compressed_ref);
    } else {
        jstr = class_name_with_dots->intern.raw_ref;
    }
    if (jstr != NULL) {
        ObjectHandle h = oh_allocate_local_handle();
        h->object = jstr;
        args[1].l = h;
    } else {
        ObjectHandle h = oh_allocate_local_handle();
        h->object = vm_instantiate_cp_string_resolved(class_name_with_dots);
        args[1].l = h;
    }

    if (exn_raised()) {
        TRACE2("classloader", "OutOfMemoryError before loading class " << className->bytes);
        tmn_suspend_enable();
        FailedLoadingClass(className);
        return NULL;
    }

    ObjectHandle hl = oh_allocate_local_handle();
    hl->object = m_loader;
    args[0].l = hl;

    Method* method = NULL;
    method = class_lookup_method_recursive(m_loader->vt()->clss, env->LoadClass_String, env->LoadClassDescriptor_String);
    assert(method);

    jvalue res;
    vm_execute_java_method_array((jmethodID) method, &res, args);

    if(exn_raised()) {
        tmn_suspend_enable();
        
        jthrowable exn = GetClassError(className->bytes);
        if(!exn) {
            exn = exn_get();
            // translate ClassNotFoundException to NoClassDefFoundError (if any)
            Class* NotFoundExn_class = env->java_lang_ClassNotFoundException_Class;
            Class *exn_class = jobject_to_struct_Class(exn);
            
            INFO("Loading of " << className->bytes << " class failed due to " 
                << exn_class->name->bytes);

            while(exn_class && exn_class != NotFoundExn_class) {
                exn_class = exn_class->super_class;
            }

            if (exn_class == NotFoundExn_class) {
                TRACE("translating ClassNotFoundException to "
                     "NoClassDefFoundError for " << className->bytes);
                exn_clear();
                jthrowable new_exn = CreateNewThrowable(
                    (JNIEnv*)jni_native_intf,
                    env->java_lang_NoClassDefFoundError_Class, 
                    className->bytes, exn);
                if (new_exn) {
                    exn = new_exn;
                } else {
                    LOG("Failed to translate ClassNotFoundException "
                        "to NoClassDefFoundError for " << className->bytes);
                }
            }
            
            FailedLoadingClass(className);
            AddFailedClass(className, exn);
        }
        exn_clear();
        return NULL;
    }
    if(res.l == NULL)
    {
        // NOTE: error in user class loader
        //       class was not loaded but exception was not thrown
        tmn_suspend_enable();
        FailedLoadingClass(className);
        REPORT_FAILED_CLASS_NAME(this, className->bytes, "java/lang/NoClassDefFoundError", className->bytes);
        return NULL;
    }
    ObjectHandle oh = (ObjectHandle) res.l;
    Class* clss = java_lang_Class_to_struct_Class(oh->object);
    tmn_suspend_enable();

    assert(clss->class_loader != NULL);
    if(clss->class_loader != this) {
        // if loading of this class was delegated to some other CL
        //      signal successful loading for our CL
        SuccessLoadingClass(className);
    }
    return clss;
} // UserDefinedClassLoader::LoadClass

void BootstrapClassLoader::ReportAndExit(const char* exnclass, std::stringstream& exnmsg) 
{
    std::stringstream ss;
    ss << exnclass << " : " << exnmsg.str().c_str();
    WARN(ss.str().c_str());
    LOGGER_EXIT(1);
}

Class* BootstrapClassLoader::LoadFromFile(const String* class_name)
{
    // the symptom of circularity (illegal) is that a null class name is passed,
    // so detect this immediately
    if( !class_name->bytes || *(class_name->bytes) == 0 ) {
        REPORT_FAILED_CLASS_NAME(this, class_name->bytes,
            "java/lang/ClassCircularityError", class_name->bytes);
        return NULL;
    }

    // first change class name from the internal fully qualified name
    // to an external file system name
    unsigned baselen = class_name->len + 7;  // includes '.class' plus extra '\0'

    // copy class name
    char* class_name_in_fs = (char*)STD_ALLOCA(baselen);
    memcpy(class_name_in_fs, class_name->bytes, class_name->len);

    // change fully qualified form to path name by replacing / with '\\' or vice versa.
    for(char* pointer = class_name_in_fs;
        pointer < class_name_in_fs + class_name->len;
        pointer++)
    {
        if((*pointer == '/') || (*pointer == '\\')) {
            *pointer = PORT_FILE_SEPARATOR;
        }
    }
    memcpy(class_name_in_fs + class_name->len, ".class", 7);

    // set class name in archive file
    char* class_name_in_jar;
    if( '/' == PORT_FILE_SEPARATOR ) {
        class_name_in_jar = class_name_in_fs;
    } else {
        class_name_in_jar = (char*)STD_ALLOCA(baselen);
        memcpy(class_name_in_jar, class_name->bytes, class_name->len);
        memcpy(class_name_in_jar + class_name->len, ".class", 7);
    }

    // find class in bootclasspath
    Class* clss = NULL;
    for( BCPElement* element = m_BCPElements.m_first;
         element;
         element = element->m_next )
    {
        bool not_found;
        if(element->m_isJarFile) {
            clss = LoadFromJarFile(element->m_jar, class_name_in_jar,
                class_name, &not_found);
        } else {
            clss = LoadFromClassFile(element->m_path, class_name_in_fs,
                class_name, &not_found);
        }

        // chech if a given class is found
        if(!not_found) {
            return clss;
        }
        assert(clss == NULL);
    }
    REPORT_FAILED_CLASS_NAME(this, class_name->bytes, 
        "java/lang/NoClassDefFoundError", class_name->bytes);
    FailedLoadingClass(class_name);
    return NULL;
} // BootstrapClassLoader::LoadFromFile


Class* BootstrapClassLoader::LoadFromClassFile(const String* dir_name,
    const char* class_name_in_fs, const String* class_name, bool* not_found)
{
    // create local temp pool
    apr_pool_t *local_pool;
    apr_pool_create(&local_pool, NULL);
    *not_found = false;

    // set full file name
    char* full_name = apr_pstrcat(local_pool, dir_name->bytes,
        PORT_FILE_SEPARATOR_STR, class_name_in_fs, NULL );

    // check file existance
    apr_finfo_t finfo;
    if(apr_stat(&finfo, full_name, APR_FINFO_SIZE, local_pool) != APR_SUCCESS) {
        // file does not exist
        *not_found = true;
        apr_pool_destroy(local_pool);
        return NULL;
    }

    // file exists, try to open it
    apr_file_t *file_handle;
    if(apr_file_open(&file_handle, full_name,
        APR_FOPEN_READ|APR_FOPEN_BINARY, 0, local_pool) != APR_SUCCESS)
    {
        // cannot open file
        *not_found = true;
        apr_pool_destroy(local_pool);
        return NULL;
    }

    // read file in buf
    size_t buf_len = (size_t)finfo.size;
    unsigned char* buf = (unsigned char*)STD_ALLOCA(buf_len);
    apr_file_read(file_handle, buf, &buf_len);

    // close file
    apr_file_close(file_handle);

    // define class
    Class* clss = DefineClass(m_env, class_name->bytes, buf, 0, buf_len); 
    if(clss) {
        clss->class_file_name = m_env->string_pool.lookup(full_name);
    }
    apr_pool_destroy(local_pool);

    return clss;
} // BootstrapClassLoader::LoadFromClassFile

Class* BootstrapClassLoader::LoadFromJarFile( JarFile* jar_file,
    const char* class_name_in_jar, const String* class_name, bool* not_found)
{
    // find archive entry in archive file
    *not_found = false;
    const JarEntry *entry = jar_file->Lookup(class_name_in_jar);
    if(!entry) {
        // file was not found
        *not_found = true;
        return NULL;
    }

    // unpack entry
    unsigned size = entry->GetContentSize();
    unsigned char *buffer = (unsigned char *)STD_ALLOCA(size);

    if(!entry->GetContent(buffer, jar_file)) {
        // cannot unpack entry
        *not_found = true;
        return NULL;
    }

    // set class into package collection
    ProvidePackage(m_env, class_name, jar_file->GetName());

    // define class
    Class *clss = DefineClass(m_env, class_name->bytes, buffer, 0, size, NULL);
    if(clss) {
        // set class file name
        clss->class_file_name = m_env->string_pool.lookup(jar_file->GetName());
    }

    return clss;
} // BootstrapClassLoader::LoadFromJarFile

// Function looks for method in native libraries of class loader.
VMEXPORT GenericFunctionPointer
classloader_find_native(const Method_Handle method)
{
    assert(tmn_is_suspend_enabled());
    assert( !exn_raised() );

    // get class and class loader of a given method
    Class_Handle klass = method_get_class( method );
    ClassLoader *loader = (ClassLoader*)class_get_class_loader( klass );
    return loader->LookupNative( method );
}

void ClassLoader::ReportException(const char* exn_name, std::stringstream& message_stream)
{
    // raise exception
    exn_raise_by_name(exn_name, message_stream.str().c_str());
}

void BootstrapClassLoader::ReportException(const char* exn_name, std::stringstream& message_stream)
{
    // if still can't create an exception, print error message and exit VM
    if (! m_env->IsReadyForExceptions())
    {
        ReportAndExit(exn_name, message_stream);
    }

    ClassLoader::ReportException(exn_name, message_stream);
}


