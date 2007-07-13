#ifndef __INLINE_INFO_H__
#define __INLINE_INFO_H__

#include <vector>
#include "open/types.h"
#include "jit_intf.h"
#include "lock_manager.h"

struct Method;

/**
 * Information about methods inlined to a given method.
 * Instance of this class holds a collection of Entry objects.
 */
class InlineInfo
{
public:
    /**
     * Creates InlineInfo instance.
     */
    InlineInfo();

    /**
     * Adds information about inlined method.
     * @param[in] method - method which is inlined
     * @param[in] codeSize - size of inlined code block
     * @param[in] codeAddr - size of inlined code block
     * @param[in] mapLength - number of AddrLocation elements in addrLocationMap
     * @param[in] addrLocationMap - native addresses to bytecode locations
     *       correspondence table
     */
    void add(Method* method, uint32 codeSize, void* codeAddr, uint32 mapLength, 
            AddrLocation* addrLocationMap);

    /**
     * Sends JVMTI_EVENT_COMPILED_METHOD_LOAD event for every inline method 
     * recorded in this InlineInfo object.
     * @param[in] method - outer method this InlineInfo object belogs to.
     */
    void send_compiled_method_load_event(Method *method);

private:
    /**
     * Describes one inlined method code block.
     */
    struct Entry
    {
        Method* method;
        uint32 codeSize;
        void* codeAddr;
        uint32 mapLength;
        AddrLocation* addrLocationMap;
    };

    typedef std::vector<Entry>::iterator iterator;

    std::vector<Entry> _entries;
    Lock_Manager _lock;
};

#endif
