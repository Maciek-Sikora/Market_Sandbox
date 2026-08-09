#pragma once
enum ORDER_STATUS : int {
    ORDER_STATUS_UNSPECIFIED = 0,
    FILLED = 1,
    PARTIAL = 2,
    QUEUED = 3,
    REJECTED = 4
};
