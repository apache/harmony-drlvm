/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package org.apache.HarmonyDRLVM.mm.mmtk;
 
import org.mmtk.utility.ReferenceProcessor;

import org.vmmagic.pragma.*;
import org.vmmagic.unboxed.*;

import java.lang.ref.Reference;
import java.lang.ref.SoftReference;
import java.lang.ref.WeakReference;
import java.lang.ref.PhantomReference;


/**
 * This class manages SoftReferences, WeakReferences, and
 * PhantomReferences.
 */
public final class ReferenceGlue extends org.mmtk.vm.ReferenceGlue implements Uninterruptible 
{

    /**
     * Scan through the list of references with the specified semantics.
     * @param semantics the number representing the semantics
     * @param True if it is safe to only scan new references.
     */
    public void scanReferences(int semantics, boolean nursery)
    {
        //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.ReferenceGlue.scanReferences() was called");
    }

    /**
     * Scan through all references and forward. Only called when references
     * are objects.
     */
    public void forwardReferences()
    {
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.ReferenceGlue.forwardReferences() was called");
    }

    /**
     * Put this Reference object on its ReferenceQueue (if it has one)
     * when its referent is no longer sufficiently reachable. The
     * definition of "reachable" is defined by the semantics of the
     * particular subclass of Reference.
     * @see java.lang.ref.ReferenceQueue
     * @param addr the address of the Reference object
     * @param onlyOnce <code>true</code> if the reference has ever
     * been enqueued previously it will not be enqueued
     * @return <code>true</code> if the reference was enqueued
     */
    public boolean enqueueReference(Address addr, boolean onlyOnce)
    {
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.ReferenceGlue.enqueueReference() was called");
        return false;
    }

    /***********************************************************************
     * 
     * Reference object field accessors
     */

    /**
     * Get the referent from a reference.  For Java the reference
     * is a Reference object.
     * @param addr the address of the reference
     * @return the referent address
     */
    public ObjectReference getReferent(Address addr)
    {
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.ReferenceGlue.getReferent() was called");
        return ObjectReference.nullReference();
    }

    /**
     * Set the referent in a reference.  For Java the reference is
     * a Reference object.
     * @param addr the address of the reference
     * @param referent the referent address
     */
    public void setReferent(Address addr, ObjectReference referent)
    {
        System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.ReferenceGlue.setReferent() was called");
    }
 
    /**
     * @return <code>true</code> if the references are implemented as heap
     * objects (rather than in a table, for example).  In this context
     * references are soft, weak or phantom references.
     * 
     * This must be implemented by subclasses, but is never called by MMTk users.
     */
    protected boolean getReferencesAreObjects()
    {
        //System.out.println("org.apache.HarmonyDRLVM.mm.mmtk.ReferenceGlue.getReferencesAreObjects() was called");
        return false;
    }
  
    /**
     * NOTE: This method should not be called by anything other than the
     * reflective mechanisms in org.mmtk.vm.VM, and is not implemented by
     * subclasses.
     * 
     * This hack exists only to allow us to declare getVerifyAssertions() as 
     * a protected method.
     */
    static boolean referencesAreObjectsTrapdoor(ReferenceGlue a) 
    {
        return a.getReferencesAreObjects();
    }

}
