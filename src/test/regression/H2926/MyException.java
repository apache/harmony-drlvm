package org.apache.harmony.drlvm.tests.regression.h2926;

public class MyException extends Exception {
    public MyException() {
        super();
    }

    public MyException(String msg) {
        super(msg);
    }
}