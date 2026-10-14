const MISSING_BOOLEAN int8 = -1
const MISSING_CATEGORICAL int8 = -1
const MISSING_NOMINAL string = "_MISSING_"

var MISSING_NUMERIC float32 = float32(math.NaN())

const UNDEFINED int = 0

const SUCCESS int = 1
const NO_DATA int = 2
const BACK_PRESSURED int = 3
const NO_COMPATIBLE_NODE int = 4

const NO_BID int8 = 0
const BID int8 = 1

const NO_SEND int8 = 0
const SEND int8 = 1

const WIN_RATE_PRESERVATION = 0
const EXPECTED_SURPLUS_MAXIMIZATION = 1

const ARCHITECTURE_NOT_SUPPORTED int = 0x05
const AERON_INITIALIZATION_FAILED int = 0x06
const INVALID_RESOURCE_ID int = 0x07
const INVALID_APP_FOR_MODEL int = 0x08
const MISMATCHED_FEATURE_TYPE int = 0x09
const INVALID_FEEDBACK_LEVEL int = 0x0A
const MISSING_CREATIVES int = 0x0B
const TOO_MANY_CREATIVES int = 0x0C
const MISPLACED_CREATIVE_ID int = 0x0D

const NO_API_RESPONSE int = 0x0E
const FORBIDDEN int = 0x0F
const UNKNOWN_RESOURCE int = 0x10
const OTHER_FETCH_ERROR int = 0x11
const INVALID_CLUSTER_DESCRIPTION int = 0x12
const NO_MODEL_METADATA int = 0x13
const INVALID_HANDLE int = 0x14
const UNAUTHORIZED int = 0x15
const INTERNAL_API_ERROR int = 0x16
const OUT_OF_MEMORY int = 0x17

const APP_BID_SHADING int = 0
const APP_PROBABILITY int = 1
const APP_LINE_ITEM int = 2
const APP_ACTION_RATE int = 3
const APP_BINARY_CLASSIFICATION int = 4
const APP_REGRESSION int = 5
const APP_TRAFFIC_SHAPING = 6
const APP_UNDEFINED int = 9999

const LOG_OFF int = 0

const LOG_TRACE int = 0x01
const LOG_DEBUG int = 0x02
const LOG_INFO int = 0x04
const LOG_WARNING int = 0x08
const LOG_ERROR int = 0x10
const LOG_CRITICAL int = 0x20

const MIN_LOG_TRACE int = 0x3f
const MIN_LOG_DEBUG int = 0x3e
const MIN_LOG_INFO int = 0x3c
const MIN_LOG_WARNING int = 0x38
const MIN_LOG_ERROR int = 0x30
const MIN_LOG_CRITICAL int = 0x20

const MAX_LOG_TRACE int = 0x01
const MAX_LOG_DEBUG int = 0x03
const MAX_LOG_INFO int = 0x07
const MAX_LOG_WARNING int = 0x0f
const MAX_LOG_ERROR int = 0x1f
const MAX_LOG_CRITICAL int = 0x3f
