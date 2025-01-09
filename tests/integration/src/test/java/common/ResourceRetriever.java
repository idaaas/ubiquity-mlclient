package common;

import java.io.IOException;
import java.net.URL;
import java.util.Enumeration;

public class ResourceRetriever {
  public static String getResource(String fileName) {
    // if param is passed to jar, use this
    String resourcePath = System.getProperty("resource.path");
    if (resourcePath != null) {
      return resourcePath + "/" + fileName;
    }

    // then try in earthly local folder
    ClassLoader classLoader = ResourceRetriever.class.getClassLoader();
    resourcePath = classLoader.getResource("testOutput/"+fileName).getPath();

    return resourcePath;
  }
}
