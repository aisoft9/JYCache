package io.aisoft9.jycache.fs.others;

import junit.framework.TestCase;
import io.aisoft9.jycache.fs.hadoop.JYCacheFileSystem;

import java.io.IOException;
import java.net.URL;

public class TestOthers extends TestCase {
    public void testHelloWorld() throws IOException {
        URL location = JYCacheFileSystem.class.getProtectionDomain().getCodeSource().getLocation();
        System.out.println("Hello World");
        System.out.println(location.getPath());
    }
}
