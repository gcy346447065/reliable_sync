
#include "sync.h"


int g_iBackupFlag = BACKUP_NULL;
const void *g_pAddr = NULL;
int g_iLength = 0;

int SetBackupFlag(int iType, const void *pAddr, int iLength)
{
    g_iBackupFlag = iType;
    g_pAddr = pAddr;
    g_iLength = iLength;
    
    return 0;
}

int GetBackupFlag(int *piType, const void **ppAddr, int *piLength)
{
    *piType = g_iBackupFlag;
    *ppAddr = g_pAddr;
    *piLength = g_iLength;
    
    return 0;
}

int ResetBackupFlag(void)
{
    g_iBackupFlag = BACKUP_NULL;
    g_pAddr = NULL;
    g_iLength = 0;
    
    return 0;
}

int Send2SyncThread(int iType, const void *pAddr, int iLength)
{
    //TO DO: may use message queue to send from thread

    return 0;
}

int batch_backup(const void *pAddr, int iLength)
{
    if(iLength > 0)
    {
        SetBackupFlag(BACKUP_BATCH, pAddr, iLength);
    }
    
    return 0;
}

int realtime_backup(const void *pAddr, int iLength, bool bIsInstant)
{
    if(iLength > 0)
    {
        if(bIsInstant == false)
        {
            SetBackupFlag(BACKUP_REALTIME_WAITING, pAddr, iLength);
        }
        else
        {
            SetBackupFlag(BACKUP_REALTIME_INSTANT, pAddr, iLength);
        }
    }

    return 0;
}
