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

  public final static int ARCHITECTURE_NOT_SUPPORTED = 0x20001;
  public final static int AERON_INITIALIZATION_FAILED = 0x20002;
  public final static int INVALID_RESOURCE_ID = 0x20003;
  public final static int INVALID_APP_FOR_MODEL = 0x20004;
  public final static int MISMATCHED_FEATURE_TYPE = 0x20005;
  public final static int INVALID_FEEDBACK_LEVEL = 0x20006;
  public final static int MISSING_CREATIVES = 0x20007;
  public final static int TOO_MANY_CREATIVES = 0x20008;
  public final static int MISPLACED_CREATIVE_ID = 0x20009;
  public final static int NO_API_RESPONSE = 0x2000A;
  public final static int FORBIDDEN = 0x2000B;
  public final static int UNKNOWN_RESOURCE = 0x2000C;
  public final static int OTHER_FETCH_ERROR = 0x2000D;
  public final static int INVALID_CLUSTER_DESCRIPTION = 0x2000E;
  public final static int SCHEMA_UNAVAILABLE = 0x2000F;
  public final static int INVALID_HANDLE = 0x20010;

  public final static int APP_BID_SHADING = 0;
  public final static int APP_PROBABILITY = 1;
  public final static int APP_LINE_ITEM = 2;
  public final static int APP_UNDEFINED = 3;

