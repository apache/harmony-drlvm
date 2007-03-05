package org.apache.harmony.drlvm.tests.regression.h2086;
import junit.framework.TestCase;

public class Test extends TestCase {
   public void test() {
       java.util.concurrent.atomic.AtomicLong al =
           new java.util.concurrent.atomic.AtomicLong();
   }
}
