#include <assert.h>
#include "thread_unit_test_utils.h"

#ifdef _WIN32
#include <windows.h>

create_java_vm_func test_get_java_vm_ptr(void)
{
    HANDLE lib;
    FARPROC func;

    lib = LoadLibrary("harmonyvm.dll");
    assert(lib);
    func = GetProcAddress(lib, "JNI_CreateJavaVM");
    assert(func);

    return (create_java_vm_func)func;
}
#else
#include <dlfcn.h>

create_java_vm_func test_get_java_vm_ptr(void)
{
    void *lib, *func;

    lib = dlopen("libharmonyvm.so", RTLD_NOW);
    assert(lib);
    func = dlsym(lib, "JNI_CreateJavaVM");
    assert(func);

    return (create_java_vm_func)func;
}
#endif
