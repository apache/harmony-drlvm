package org.apache.harmony.drlvm.tests.regression.H3110;

import java.lang.reflect.*;
import junit.framework.TestCase;

public class FieldTest extends TestCase {

    public float amount = .555f;

    public void test_getFloat() throws Exception {
            Field floatField = FieldTest.class.getField("amount");
            FieldTest ft = new FieldTest();
        
            float fv = floatField.getFloat(ft);
            assertEquals(amount, fv);
    }

    public void test_get() throws Exception {
            Field floatField = FieldTest.class.getField("amount");
            FieldTest ft = new FieldTest();
        
            Float fo = (Float) floatField.get(ft);
            assertEquals(amount, fo.floatValue());
    }
}