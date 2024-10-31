  static {
    System.out.println("[INFO] UbiquityClient: trying to load native lib");

    try {
      ai.ubiquity.NativeLoader.load();
    } catch(Exception e) {
      e.printStackTrace();
      throw new RuntimeException("Cannot load native lib");
    }

    System.out.println("[INFO] UbiquityClient: native lib loaded");
  }

  public final static byte MISSING_BOOLEAN = -1;
  public final static byte MISSING_CATEGORICAL = -1;
  public final static String MISSING_NOMINAL = "_MISSING_";
  public final static float MISSING_NUMERIC = Float.NaN;

  public final static int UNDEFINED = 0;
  public final static int SUCCESS = 1;
  public final static int NO_DATA = 2;
  public final static int BACK_PRESSURED = 3;
  public final static int NO_COMPATIBLE_NODE = 4;
  public final static int ARCHITECTURE_NOT_SUPPORTED = 5;
  public final static int AERON_INITIALIZATION_FAILED = 6;
  public final static int CLIENT_NOT_CONNECTED = 7;
  public final static int INVALID_RESOURCE_ID = 8;
  public final static int INVALID_APP_FOR_MODEL = 9;
  public final static int MISMATCHED_FEATURE_TYPE = 10;
  public final static int INVALID_FEEDBACK_LEVEL = 11;
  public final static int MISSING_CREATIVES = 12;
  public final static int TOO_MANY_CREATIVES = 13;
  public final static int MISPLACED_CREATIVE_ID = 14;
  public final static int NO_API_RESPONSE = 15;
  public final static int FORBIDDEN = 16;
  public final static int UNKNOWN_RESOURCE = 17;
  public final static int OTHER_FETCH_ERROR = 18;
  public final static int INVALID_CLUSTER_DESCRIPTION = 19;
  public final static int INVALID_SCHEMA_DESCRIPTION = 20;
  public final static int SCHEMA_UNAVAILABLE = 21;
  public final static int APP_BID_SHADING = 0;
  public final static int APP_PROBABILITY = 1;
  public final static int APP_LINE_ITEM = 2;
  public final static int APP_UNDEFINED = 3;

