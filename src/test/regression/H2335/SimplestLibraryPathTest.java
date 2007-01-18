package org.apache.harmony.drlvm.tests.regression.h2335;
import junit.framework.TestCase;

/**
 * Test for HARMONY-2335 does nothing - the reason of bug was in broken path to
 * system native libraries if -Djava.library.path="any subdir" is specified.
 */
public class SimplestLibraryPathTest extends TestCase {
    public static void main(String[] args) {}
    public void testNothing() {}
} // end of class 'SimplestLibraryPathTest' definition 
