package ai.ubiquity;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.io.FileNotFoundException;
import java.io.InputStream;


public class NativeLoader {
  private static boolean loaded = false;

  public static synchronized void load() throws Exception {
    if (loaded) {
      return;
    }

    Path libpath = Files.createTempFile("libjniUbiquityClient", ".so");
    libpath.toFile().deleteOnExit();

    try (InputStream is = NativeLoader.class.getResourceAsStream("/NATIVE/libjniUbiquityClient.so")) {
      if (is == null) {
        throw new FileNotFoundException("file '/NATIVE/libjniUbiquityClient.so' was not found inside JAR");
      }

      Files.copy(is, libpath, StandardCopyOption.REPLACE_EXISTING);
    }

    System.load(libpath.toString());
    loaded = true;
  }
}
