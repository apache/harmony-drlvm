package org.apache.harmony.drlvm.tests.regression.h3074;

public class AnotherClass {
    public static boolean status = false;

    public static void method() {
        System.out.println("Class was loaded with " +
            AnotherClass.class.getClassLoader());
        status = true;
    }
}
