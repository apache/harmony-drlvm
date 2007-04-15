package org.apache.harmony.drlvm.tests.regression.h1654;

import junit.framework.TestCase;
import java.security.*;

public class H1654Test extends TestCase {

    public void test1() {
        ProtectionDomain pd = H1654Test.class.getProtectionDomain();
        System.out.println(pd.getPermissions().toString());
        boolean permissionGranted = pd.getPermissions().toString().indexOf("java.lang.RuntimePermission exitVM") != -1;
        assertTrue("permission was not granted", permissionGranted);
    }

    public void test2() {
        ProtectionDomain pd = H1654Test.class.getProtectionDomain();
        assertTrue("pd does not imply the exitVM permission ", pd.implies(new RuntimePermission("exitVM")));
    }
}
