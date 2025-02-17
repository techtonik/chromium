// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebProcessMemoryDump_h
#define WebProcessMemoryDump_h

#include "WebCommon.h"
#include "WebMemoryAllocatorDump.h"
#include "WebMemoryDumpProvider.h"
#include "WebString.h"

class SkTraceMemoryDump;

namespace blink {

// A container which holds all the dumps for the various allocators for a given
// process. Embedders of WebMemoryDumpProvider are expected to populate a
// WebProcessMemoryDump instance with the stats of their allocators.
class BLINK_PLATFORM_EXPORT WebProcessMemoryDump {
public:
    virtual ~WebProcessMemoryDump();

    // Creates a new MemoryAllocatorDump with the given name and returns the
    // empty object back to the caller. |absoluteName| uniquely identifies the
    // dump within the scope of a ProcessMemoryDump. It is possible to express
    // nesting by means of a slash-separated path naming (e.g.,
    // "allocator_name/arena_1/subheap_X").
    // |guid| is  an optional identifier, unique among all processes within the
    // scope of a global dump. This is only relevant when using
    // AddOwnershipEdge(). If omitted, it will be automatically generated.
    virtual WebMemoryAllocatorDump* createMemoryAllocatorDump(const WebString& absoluteName, WebMemoryAllocatorDumpGuid guid)
    {
        BLINK_ASSERT_NOT_REACHED();
        return nullptr;
    }

    virtual WebMemoryAllocatorDump* createMemoryAllocatorDump(const WebString& absoluteName)
    {
        BLINK_ASSERT_NOT_REACHED();
        return nullptr;
    }

    // Gets a previously created MemoryAllocatorDump given its name.
    virtual WebMemoryAllocatorDump* getMemoryAllocatorDump(const WebString& absoluteName) const
    {
        BLINK_ASSERT_NOT_REACHED();
        return nullptr;
    }

    // Removes all the WebMemoryAllocatorDump(s) contained in this instance.
    // This WebProcessMemoryDump can be safely reused as if it was new once this
    // method returns.
    virtual void clear()
    {
        BLINK_ASSERT_NOT_REACHED();
    }

    // Merges all WebMemoryAllocatorDump(s) contained in |other| inside this
    // WebProcessMemoryDump, transferring their ownership to this instance.
    // |other| will be an empty WebProcessMemoryDump after this method returns
    // and can be reused as if it was new.
    virtual void takeAllDumpsFrom(WebProcessMemoryDump* other)
    {
        BLINK_ASSERT_NOT_REACHED();
    }

    // Adds an ownership relationship between two MemoryAllocatorDump(s) with
    // the semantics: |source| owns |target|, and has the effect of attributing
    // the memory usage of |target| to |source|. |importance| is optional and
    // relevant only for the cases of co-ownership, where it acts as a z-index:
    // the owner with the highest importance will be attributed |target|'s
    // memory.
    virtual void AddOwnershipEdge(WebMemoryAllocatorDumpGuid source, WebMemoryAllocatorDumpGuid target, int importance)
    {
        BLINK_ASSERT_NOT_REACHED();
    }

    virtual void AddOwnershipEdge(WebMemoryAllocatorDumpGuid source, WebMemoryAllocatorDumpGuid target)
    {
        BLINK_ASSERT_NOT_REACHED();
    }

    // Utility method to add a suballocation relationship with the following
    // semantics: |source| is suballocated from |target_node_name|.
    // This creates a child node of |target_node_name| and adds an ownership
    // edge between |source| and the new child node. As a result, the UI will
    // not account the memory of |source| in the target node.
    virtual void AddSuballocation(WebMemoryAllocatorDumpGuid source, const WebString& targetNodeName)
    {
        BLINK_ASSERT_NOT_REACHED();
    }

    // Returns the SkTraceMemoryDump proxy interface that can be passed to Skia
    // to dump into this WebProcessMemoryDump. Multiple SkTraceMemoryDump
    // objects can be created using this method. The created dumpers are owned
    // by WebProcessMemoryDump and cannot outlive the WebProcessMemoryDump
    // object owning them. |dumpNamePrefix| is prefix appended to each dump
    // created by the SkTraceMemoryDump implementation, if the dump should be
    // placed under different namespace and not "skia".
    virtual SkTraceMemoryDump* CreateDumpAdapterForSkia(const WebString& dumpNamePrefix)
    {
        BLINK_ASSERT_NOT_REACHED();
        return nullptr;
    }
};

} // namespace blink

#endif // WebProcessMemoryDump_h
