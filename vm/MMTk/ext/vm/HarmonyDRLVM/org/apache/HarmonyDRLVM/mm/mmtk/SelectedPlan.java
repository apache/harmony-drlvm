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

import org.mmtk.plan.marksweep.MS;
import org.mmtk.plan.nogc.*;
import org.mmtk.plan.semispace.SS;
import org.vmmagic.pragma.*;
import org.mmtk.plan.Plan;
import org.mmtk.plan.copyms.*;
import org.mmtk.plan.generational.marksweep.GenMS;


public final class SelectedPlan implements Uninterruptible 
{
    //public static final Plan singleton = new NoGC();
    //public static final MS singleton = new MS();
    //private static MS singleton;  // work around a DRLVM bug
    //private static SS singleton;
    //private static CopyMS singleton;
    private static GenMS singleton;

    public static org.apache.HarmonyDRLVM.mm.mmtk.ActivePlan ap = new org.apache.HarmonyDRLVM.mm.mmtk.ActivePlan();

    public static final Plan get() throws InlinePragma 
    {
        if (SelectedPlan.singleton == null) 
        {
            //SelectedPlan.singleton = new MS();
            //SelectedPlan.singleton = new SS();
            //SelectedPlan.singleton = new CopyMS();
            SelectedPlan.singleton = new GenMS();
        }
        return singleton;
    }
}
