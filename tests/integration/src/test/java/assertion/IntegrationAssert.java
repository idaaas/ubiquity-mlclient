package assertion;

public class IntegrationAssert {
  private final String logFilePath;
  public IntegrationAssert(String logFilePath) {
    this.logFilePath = logFilePath;
    //TODO check file existance
  }

  public AssertionRule node(String id) {
    return new AssertionRule(new AssertContext(logFilePath, id));
  }

  public AssertionRule anyNode() {
    return new AssertionRule(new AssertContext(logFilePath, ""));
  }

  public AssertionRule nodeClient(String ip) {
    return new AssertionRule(new AssertContext(logFilePath, ip));
  }

  public AssertionRule anyNodeClient() {
    return new AssertionRule(new AssertContext(logFilePath, ""));
  }
}
