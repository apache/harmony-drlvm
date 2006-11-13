
public class DirectByteBufferTest {

    static { System.loadLibrary("DirectByteBufferTest");}

    public static void main(String[] args) {
        new DirectByteBufferTest().testValidBuffer();
    }

    private native String testValidBuffer0();
    
    public void testValidBuffer() {
        assertNull(testValidBuffer0());
    }
    
    public void assertNull(Object o) {
        if (o == null) {
            System.out.println("PASSED");
        } else {
            fail(o.toString());
        }
    }
    
    public void fail(String s) {
        System.out.println("FAILED: " + s);
    }
}
