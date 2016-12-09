
#include "checksum.h"
#include "log.h"

short checksum(const char *pData, int iDataLen)
{
    log_debug("checksum iDataLen(%d)", iDataLen);
    log_hex(pData, iDataLen);
    short sum1 = 0, sum2 = 0;

    for(int i=0; i<iDataLen; i++)
    {
        sum1 += *(pData++);
        sum2 += sum1;
    }

    return (sum1 & 0xffff) + (sum2 << 16);
}