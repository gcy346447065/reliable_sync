#include "checksum.h"
#include "log.h"

WORD checksum(const void *pData, WORD wDataLen)
{
    //log_debug(LOG1, "checksum wDataLen(%u)", wDataLen);
    //log_hex(LOG1, pData, wDataLen);

    DWORD dwSum = 0;
    const WORD *pwData = (const WORD *)pData;
    
    while(wDataLen > 1)
    {
        dwSum += *pwData++;
        wDataLen -= (WORD)sizeof(WORD);
    }
    if(wDataLen)//0 or 1
    {
        dwSum += *(const BYTE *)pwData;
    }

    while(dwSum >> 16)
    {
        dwSum = (dwSum >> 16) + (dwSum & 0xffff);
    }

    return (WORD)(~dwSum);
}

