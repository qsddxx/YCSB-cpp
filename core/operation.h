#ifndef YCSBC_OPERATION_H
#define YCSBC_OPERATION_H
namespace ycsbc
{
    enum Operation {
    INSERT = 0,
    READ,
    UPDATE,
    SCAN,
    READMODIFYWRITE,
    DELETE,
    INSERT_FAILED,
    READ_FAILED,
    UPDATE_FAILED,
    SCAN_FAILED,
    READMODIFYWRITE_FAILED,
    DELETE_FAILED,
    MAXOPTYPE,
    EXIT
    };
    extern const char *kOperationString[ycsbc::MAXOPTYPE];
}
#endif