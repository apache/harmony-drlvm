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

import org.mmtk.plan.Plan;
import org.mmtk.plan.CollectorContext;
import org.mmtk.plan.MutatorContext;
import org.mmtk.plan.PlanConstraints;
import org.mmtk.vm.*;
import org.vmmagic.pragma.*;

/**
 * This class defines factory methods for VM-specific types which must
 * be instantiated within MMTk.  Since the concrete type is defined at
 * build time, we leave it to a concrete vm-specific instance of this class
 * to perform the object instantiation.
 * 
 */
/* 888888888888888888888888888888888888888
 * 
     xom = (ObjectModel) Class.forName(vmPackage+".ObjectModel").newInstance();

      xas = (Assert) Class.forName(vmPackage+".Assert").newInstance();
      xba = (Barriers) Class.forName(vmPackage+".Barriers").newInstance();
      xco = (Collection) Class.forName(vmPackage+".Collection").newInstance();
      xme = (Memory) Class.forName(vmPackage+".Memory").newInstance();
      xop = (Options) Class.forName(vmPackage+".Options").newInstance();
      xrg = (ReferenceGlue) Class.forName(vmPackage+".ReferenceGlue").newInstance();
      xsc = (Scanning) Class.forName(vmPackage+".Scanning").newInstance();
      xst = (Statistics) Class.forName(vmPackage+".Statistics").newInstance();
      xsr = (Strings) Class.forName(vmPackage+".Strings").newInstance();
      xtr = (TraceInterface) Class.forName(vmPackage+".TraceInterface").newInstance();
 * 88888888888888888888888888888888888888888888888 */

public class Factory  extends org.mmtk.vm.Factory {

	private static final String vmPackage = "org.apache.HarmonyDRLVM.mm.mmtk";

  /**
   * Create a new ActivePlan instance using the appropriate VM-specific
   * concrete ActivePlan sub-class.
   * 
   * @see ActivePlan
   * @return A concrete VM-specific ActivePlan instance.
   */
	public ActivePlan newActivePlan()
	{
		ActivePlan xx = null;
		try { xx = (ActivePlan)Class.forName(vmPackage + ".ActivePlan").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}

  /**
   * Create a new Assert instance using the appropriate VM-specific
   * concrete Assert sub-class.
   * 
   * @see Assert
   * @return A concrete VM-specific Assert instance.
   */
	public Assert newAssert()
	{
		Assert xx = null;
		try { xx = (Assert)Class.forName(vmPackage + ".Assert").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}

  /**
   * Create a new Barriers instance using the appropriate VM-specific
   * concrete Barriers sub-class.
   * 
   * @see Barriers
   * @return A concrete VM-specific Barriers instance.
   */
	public Barriers newBarriers()
	{
		Barriers xx = null;
		try { xx = (Barriers)Class.forName(vmPackage + ".Barriers").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}

  /**
   * Create a new Collection instance using the appropriate VM-specific
   * concrete Collection sub-class.
   * 
   * @see Collection
   * @return A concrete VM-specific Collection instance.
   */
	public Collection newCollection()
	{
		Collection xx = null;
		try { xx = (Collection)Class.forName(vmPackage + ".Collection").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}

  /**
   * Create a new Lock instance using the appropriate VM-specific
   * concrete Lock sub-class.
   * 
   * @see Lock
   * @param name The string to be associated with this lock instance
   * @return A concrete VM-specific Lock instance.
   */
  public Lock newLock(String name)
	{
		Lock xx = null;
		try { xx = (Lock)Class.forName(vmPackage + ".Lock").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}
  
  /**
   * Create a new Memory instance using the appropriate VM-specific
   * concrete Memory sub-class.
   * 
   * @see Memory
   * @return A concrete VM-specific Memory instance.
   */
  public Memory newMemory()
	{
		Memory xx = null;
		try { xx = (Memory)Class.forName(vmPackage + ".Memory").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}

  /**
   * Create a new ObjectModel instance using the appropriate VM-specific
   * concrete ObjectModel sub-class.
   * 
   * @see ObjectModel
   * @return A concrete VM-specific ObjectModel instance.
   */
  public ObjectModel newObjectModel()
	{
		ObjectModel xx = null;
		try { xx = (ObjectModel)Class.forName(vmPackage + ".ObjectModel").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}

  /**
   * Create a new Options instance using the appropriate VM-specific
   * concrete Options sub-class.
   * 
   * @see Options
   * @return A concrete VM-specific Options instance.
   */
  public Options newOptions()
	{
		Options xx = null;
		try { xx = (Options)Class.forName(vmPackage + ".Options").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}
  /**
   * Create a new ReferenceGlue instance using the appropriate VM-specific
   * concrete ReferenceGlue sub-class.
   * 
   * @see ReferenceGlue
   * @return A concrete VM-specific ReferenceGlue instance.
   */
  public ReferenceGlue newReferenceGlue()
	{
		ReferenceGlue xx = null;
		try { xx = (ReferenceGlue)Class.forName(vmPackage + ".ReferenceGlue").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}
  /**
   * Create a new Scanning instance using the appropriate VM-specific
   * concrete Scanning sub-class.
   * 
   * @see Scanning
   * @return A concrete VM-specific Scanning instance.
   */
  public  Scanning newScanning()
	{
		Scanning xx = null;
		try { xx = (Scanning)Class.forName(vmPackage + ".Scanning").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}
  /**
   * Create a new Statistics instance using the appropriate VM-specific
   * concrete Statistics sub-class.
   * 
   * @see Statistics
   * @return A concrete VM-specific Statistics instance.
   */
  public  Statistics newStatistics()
	{
		Statistics xx = null;
		try { xx = (Statistics)Class.forName(vmPackage + ".Statistics").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}
  /**
   * Create a new Strings instance using the appropriate VM-specific
   * concrete Strings sub-class.
   * 
   * @see Strings
   * @return A concrete VM-specific Strings instance.
   */
  public  Strings newStrings()
	{
		Strings xx = null;
		try { xx = (Strings)Class.forName(vmPackage + ".Strings").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	}  
  /**
   * Create a new SynchronizedCounter instance using the appropriate
   * VM-specific concrete SynchronizedCounter sub-class.
   * 
   * @see SynchronizedCounter
   * 
   * @return A concrete VM-specific SynchronizedCounter instance.
   */
  public  SynchronizedCounter newSynchronizedCounter()
 	{
		SynchronizedCounter xx = null;
		try { xx = (SynchronizedCounter)Class.forName(vmPackage + ".SynchronizedCounter").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
	} 
  /**
   * Create a new TraceInterface instance using the appropriate VM-specific
   * concrete TraceInterface sub-class.
   * 
   * @see TraceInterface
   * @return A concrete VM-specific TraceInterface instance.
   */
  public  TraceInterface newTraceInterface()
  {
		TraceInterface xx = null;
		try { xx = (TraceInterface)Class.forName(vmPackage + ".TraceInterface").newInstance(); }
		catch (Exception e)
		{
			e.printStackTrace();
			System.exit(-1);     // we must *not* go on if the above has failed
		}
		return xx;
  }  
}
