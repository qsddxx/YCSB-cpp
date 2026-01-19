#include "operation.h"
 const char *ycsbc::kOperationString[ycsbc::MAXOPTYPE]= {
    "INSERT",
    "READ",
    "UPDATE",
    "SCAN",
    "READMODIFYWRITE",
    "DELETE",
    "INSERT-FAILED",
    "READ-FAILED",
    "UPDATE-FAILED",
    "SCAN-FAILED",
    "READMODIFYWRITE-FAILED",
    "DELETE-FAILED"
    };