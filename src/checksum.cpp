#include "checksum.h"
#include "log.h"

unsigned short checksum(const unsigned char *pData, int iDataLen)
{
    log_debug("checksum iDataLen(%d)", iDataLen);
    //log_hex(pData, iDataLen);

    unsigned long sum = 0;
    while(iDataLen > 1)
    {
        sum += *(const unsigned short *)pData++;
        iDataLen -= 2;
    }
    if(iDataLen > 0)
    {
        sum += *(const unsigned char *)pData;
    }

    while(sum >> 16)
    {
        sum = (sum & 0xffff) + (sum << 16);
    }

    return (unsigned short)~sum;
}
