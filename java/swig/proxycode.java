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

  public final static short NO_BID = 0;
  public final static short BID = 1;

  public final static int WIN_RATE_PRESERVATION = 0;
  public final static int EXPECTED_SURPLUS_MAXIMIZATION = 1;

  public final static int SUCCESS = 1;
  public final static int NO_DATA = 2;
  public final static int BACK_PRESSURED = 3;
  public final static int NO_COMPATIBLE_NODE = 4;

  public final static int ARCHITECTURE_NOT_SUPPORTED = 0x05;
  public final static int AERON_INITIALIZATION_FAILED = 0x06;

  public final static int INVALID_RESOURCE_ID = 0x07;
  public final static int INVALID_APP_FOR_MODEL = 0x08;
  public final static int MISMATCHED_FEATURE_TYPE = 0x09;
  public final static int INVALID_FEEDBACK_LEVEL = 0x0A;
  public final static int MISSING_CREATIVES = 0x0B;
  public final static int TOO_MANY_CREATIVES = 0x0C;
  public final static int MISPLACED_CREATIVE_ID = 0x0D;

  public final static int NO_API_RESPONSE = 0x0E;
  public final static int FORBIDDEN = 0x0F;
  public final static int UNKNOWN_RESOURCE = 0x10;
  public final static int OTHER_FETCH_ERROR = 0x11;
  public final static int INVALID_CLUSTER_DESCRIPTION = 0x12;
  public final static int NO_MODEL_METADATA = 0x13;
  public final static int INVALID_HANDLE = 0x14;
  public final static int UNAUTHORIZED = 0x15;
  public final static int INTERNAL_API_ERROR = 0x16;
  public final static int OUT_OF_MEMORY = 0x17;
  public final static int CONNECTION_TIMEOUT = 0x18;

  public final static int APP_BID_SHADING = 0;
  public final static int APP_PROBABILITY = 1;
  public final static int APP_LINE_ITEM = 2;
  public final static int APP_ACTION_RATE = 3;
  public final static int APP_UNDEFINED = 9999;

    public final static int LOG_OFF = 0;

    public final static int LOG_TRACE = 0x01;
    public final static int LOG_DEBUG = 0x02;
    public final static int LOG_INFO = 0x04;
    public final static int LOG_WARNING = 0x08;
    public final static int LOG_ERROR = 0x10;
    public final static int LOG_CRITICAL = 0x20;

    public final static int MIN_LOG_TRACE = 0x3f;
    public final static int MIN_LOG_DEBUG = 0x3e;
    public final static int MIN_LOG_INFO = 0x3c;
    public final static int MIN_LOG_WARNING = 0x38;
    public final static int MIN_LOG_ERROR = 0x30;
    public final static int MIN_LOG_CRITICAL = 0x20;

    public final static int MAX_LOG_TRACE = 0x01;
    public final static int MAX_LOG_DEBUG = 0x03;
    public final static int MAX_LOG_INFO = 0x07;
    public final static int MAX_LOG_WARNING = 0x0f;
    public final static int MAX_LOG_ERROR = 0x1f;
    public final static int MAX_LOG_CRITICAL = 0x3f;
