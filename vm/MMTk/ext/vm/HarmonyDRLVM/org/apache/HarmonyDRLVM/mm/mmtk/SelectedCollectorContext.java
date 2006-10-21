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

import org.mmtk.plan.nogc.*;
import org.mmtk.plan.marksweep.*;
import org.mmtk.plan.semispace.*;
import org.vmmagic.pragma.*;
import org.mmtk.plan.CollectorContext;
import org.mmtk.plan.copyms.CopyMSCollector;
import org.mmtk.plan.generational.marksweep.GenMSCollector;

//public final class SelectedCollectorContext extends GenMSCollector implements 
//public final class SelectedCollectorContext extends SSCollector implements 
public final class SelectedCollectorContext extends MSCollector implements 
  Uninterruptible {
	public static final CollectorContext singleton = new SelectedCollectorContext();

  /**
   * Return the instance of the SelectedPlan
   */
  public static final CollectorContext get() throws InlinePragma {
    return singleton;
  }
}
