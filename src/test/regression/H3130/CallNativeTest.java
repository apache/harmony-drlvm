package org.apache.harmony.drlvm.tests.regression.h3130;

import junit.framework.TestCase;

public class CallNativeTest extends TestCase {
   static { System.loadLibrary("CallNativeTest"); }

   public native void testCallNative();
   private native Object getNull();
}