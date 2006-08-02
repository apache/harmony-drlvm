public class TestAastore implements TestInterface {
    public void test() {
        TestAastore2 inst = new TestAastore2();
        TestAastore[] array = new TestAastore[1];
        inst.testField = array;
        inst.test();
    }
    public void InterfaceMethod() {
        return;
    }
}