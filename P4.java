/**
 * P4 — Process 4 (Java, Kali Linux)
 *
 * Runs an XML-RPC server on port 9003.
 * Receives binary chunks from P3 (Windows) and writes them to stdout.
 *
 * Build:
 *   javac -cp ".:lib/*" P4.java
 *
 * Run:
 *   java -cp ".:lib/*" P4
 *   java -cp ".:lib/*" P4 | vlc -          (play the stream)
 *   java -cp ".:lib/*" P4 > output.bin     (save to a file)
 */

import org.apache.xmlrpc.server.PropertyHandlerMapping;
import org.apache.xmlrpc.server.XmlRpcServer;
import org.apache.xmlrpc.server.XmlRpcServerConfigImpl;
import org.apache.xmlrpc.webserver.WebServer;

import java.io.IOException;
import java.io.OutputStream;

public class P4 {

    private static final int PORT = 9003;
    private static OutputStream output;
    private static long totalBytes = 0;
    private static WebServer webServer;

    public static class ChunkReceiver {
        /**
         * Called by P3 for every chunk.
         * @param data    binary payload (XML-RPC base64)
         * @param isLast  true if this is the last chunk
         */
        public Boolean receive_chunk(byte[] data, Boolean isLast) {
            try {
                if (data != null && data.length > 0) {
                    totalBytes += data.length;
                    output.write(data);
                    output.flush();
                }

                if (Boolean.TRUE.equals(isLast)) {
                    System.err.println("[P4] Bytes received: " + totalBytes);
                    System.err.flush();
                    // Shut down after a short delay so the handler can still reply
                    new Thread(() -> {
                        try { Thread.sleep(300); } catch (InterruptedException ignored) {}
                        webServer.shutdown();
                        System.exit(0);
                    }).start();
                }
            } catch (IOException e) {
                System.err.println("[P4] Write error: " + e.getMessage());
                return false;
            }
            return true;
        }
    }

    public static void main(String[] args) throws Exception {
        output = System.out;

        System.err.println("[P4] Starting XML-RPC server on port " + PORT);
        System.err.flush();

        webServer = new WebServer(PORT);
        XmlRpcServer xmlRpcServer = webServer.getXmlRpcServer();

        PropertyHandlerMapping phm = new PropertyHandlerMapping();
        phm.addHandler("ChunkReceiver", ChunkReceiver.class);
        xmlRpcServer.setHandlerMapping(phm);

        XmlRpcServerConfigImpl config =
            (XmlRpcServerConfigImpl) xmlRpcServer.getConfig();
        config.setEnabledForExtensions(true);
        config.setContentLengthOptional(false);

        webServer.start();
        System.err.println("[P4] Listening for XML-RPC on port " + PORT + "...");
        System.err.flush();

        // Keep the main thread alive
        Thread.currentThread().join();
    }
}
