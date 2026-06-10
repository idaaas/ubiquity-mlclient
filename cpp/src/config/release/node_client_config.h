#pragma once

/*
    Release configuration for node clients.
*/

// Time before an aeron connection is considered stuck and must be moved to another state
#define CONNECTION_STATUS_GRACE_PERIOD UBIQUITY_TIME_MILLISECONDS(1000)
