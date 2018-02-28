/******************************************************************************/
/*                              SCRAMNet GT                                   */
/******************************************************************************/
/*                                                                            */
/* Copyright (c) 2002-2005 Curtiss-Wright Controls.                           */
/*               support@systran.com 800-252-5601 (U.S. only) 937-252-5601    */
/*                                                                            */
/* This library is free software; you can redistribute it and/or              */
/* modify it under the terms of the GNU Lesser General Public                 */
/* License as published by the Free Software Foundation; either               */
/* version 2.1 of the License, or (at your option) any later version.         */
/*                                                                            */
/* See the GNU Lesser General Public License for more details.                */
/*                                                                            */
/******************************************************************************/
/* This is a project fork for maintaining the driver for working with the      */
/* 3.10 kernel in Redhat/Centos 7.                                             */
/*																			   */
/* This fork is created/maintained by Brandon M. Graves, https://metashell.net */
/* up to date copies can be pulled from https://github.com/bmgraves/scgt	   */
/*																			   */
/* It is important to note all credit to the original Developer, this is merely*/
/* an ongoing support project to test for functionality in future maintained   */
/* version of linux for legacy cards.										   */
/*																			   */
/*******************************************************************************/
/******************************************************************************/
/*                                                                            */
/*    Module      : scgtapi.c                                                 */
/*    Description : SCRAMNet GT API implementation                            */
/*                                                                            */
/******************************************************************************/

/* file revision, not full api revision. See scgtapiRevisionStr */
#define FILE_REV_SCGTAPI_C  "7"  /* (2011/09/07) */

/**************************************************************************/
/********************* C O M M O N   I N C L U D E S **********************/
/**************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "scgtapi.h"
#include "gtcoreIoctl.h"
#include "systypes.h"
#include "scgtdrv.h"

/**************************************************************************/
/**************** D E F I N E S  &  C O N S T A N T S *********************/
/**************************************************************************/

/* api revision information. To be updated at release? */
#define SCGTAPI_MAJOR_REVISION   "1.0"   /* (2004/1/6) */

/* the api revision string for use by applications */
const char * scgtapiRevisionStr = \
    SCGTAPI_MAJOR_REVISION"-"FILE_REV_SCGTAPI_C FILE_REV_SCGTAPI_H \
    FILE_REV_GTUCORE_H FILE_REV_GTCOREIOCTL_H \
    FILE_REV_SYSTYPES_H FILE_REV_SCGTDRV_H;

/* the macro SCGT_DEV_FILE_STR must be defined in each OS' scgtdrv.h file */

/**************************************************************************/
/************** P L A T F O R M   S P E C I F I C   C O D E ***************/
/**************************************************************************/

#ifdef PLATFORM_WIN    /*=================== OS SWITCH ===================*/

    #include <windows.h>
    #include <winioctl.h>
    #include <winbase.h>

    #define BAD_FILE_HANDLE        INVALID_HANDLE_VALUE
    #define CLOSE_HANDLE(handle)   CloseHandle(handle)
    #define OPEN_HANDLE(deviceFile)   \
                CreateFile((LPCTSTR)deviceFile, GENERIC_READ | GENERIC_WRITE, \
                     FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, \
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL )

    #define IOCTL( handle, code, ptr, size ) \
    {                  \
    DWORD count;   \
    if( DeviceIoControl(handle, code, ptr, size, ptr, size, &count, NULL) ) \
        ret = (ptr)->reserved;  \
    else           \
        ret = SCGT_BAD_PARAMETER;  \
    }
#elif PLATFORM_VISA

    #if !defined NIVISA_PXI
       #define NIVISA_PXI
    #endif
    #include "visa.h"


    #define BAD_FILE_HANDLE           VI_NULL
    #define CLOSE_HANDLE(handle)      scgtDriverClose(handle)
    #define OPEN_HANDLE(deviceFile)   (scgtOSHandle) scgtDriverOpen(unitNum)
                
    #define IOCTL( handle, code, ptr, size ) ret = scgtVisaIoctHandler( handle, code, ptr, size )
    

#elif PLATFORM_UNIX   /*=================== OS SWITCH ===================*/

    #include <fcntl.h>
    #include <sys/mman.h>
    #include <sys/ioctl.h>
    #include <unistd.h>

    #define BAD_FILE_HANDLE          -1
    #define CLOSE_HANDLE(handle)     close(handle)
    #define OPEN_HANDLE(deviceFile)  open(deviceFile, O_RDWR, 0)

#ifdef PLATFORM_USE_RESERVED_RET
    #define IOCTL( handle, code, ptr, size ) \
                (ptr)->reserved = 0;       \
                if ((ioctl(handle, code, ptr)) == -1) \
                    ret = SCGT_BAD_PARAMETER;        \
                else                                 \
                    ret = (ptr)->reserved;
#else
    #define IOCTL( handle, code, ptr, size ) \
                if( (ret = ioctl(handle, code, ptr)) == -1 ) \
                    ret = SCGT_BAD_PARAMETER;
#endif


#elif PLATFORM_VXWORKS /*=================== OS SWITCH ===================*/

    #include "vxWorks.h"
    #include "ioLib.h"
    #include "sys/types.h"

    #define BAD_FILE_HANDLE          ERROR
    #define CLOSE_HANDLE(handle)     close(handle)

    extern void *gtTable[SCGT_MAX_DEVICES];
    #define OPEN_HANDLE(deviceFile) \
             ( gtTable[unitNum] ? open(deviceFile,O_RDWR,0) : BAD_FILE_HANDLE)

    #define IOCTL( handle, code, ptr, size ) \
                if( (ret = ioctl(handle, code, (int)ptr)) == -1 ) \
                    ret = SCGT_BAD_PARAMETER;
                    
#elif PLATFORM_RTX /*=================== OS SWITCH ===================*/

#ifdef RTX_SHMIOCTL  /* use shared memory IOCTL */
    int scgtDriverOpen(uint32 unitNum);
    void scgtDriverClose(uint32 unitNum);
    uint32 scgtDriverIoctl(int handle, uint32 cmd, void *ptr, uint32 size);
#else  /* use RTSS DLL directly */
    /* This method is not used currently because semaphores
       allocated inside the RTSS DLL are not valid in this context.
       The shared memory interface provides the necessary context switch. */
    int _declspec(dllimport) scgtDriverOpen(uint32 unitNum);
    void _declspec(dllimport) scgtDriverClose(uint32 unitNum);
    uint32 _declspec(dllimport) scgtDriverIoctl(int handle, uint32 cmd, void *ptr, uint32 size);
#endif

    /* macros */
    #define BAD_FILE_HANDLE          -1
    #define OPEN_HANDLE(deviceFile)  scgtDriverOpen(unitNum)
    #define CLOSE_HANDLE(handle)     scgtDriverClose(handle)

    #define IOCTL(handle, code, ptr, size) \
                if ((ret = scgtDriverIoctl(handle, code, ptr, size)) == -1) \
                    ret = SCGT_BAD_PARAMETER;

#else
#error "Must define one of PLATFORM_WIN, PLATFORM_UNIX, PLATFORM_VISA or PLATFORM_VXWORKS\n"
#endif


/**************************************************************************/
/*******P L A T F O R M   S P E C I F I C   G T   A P I   C O D E *********/
/**************************************************************************/

/**************************************************************************/
/*  function:     scgtMemMap                                              */
/*  description:  Maps GT memory into user space                          */
/*                                                                        */
/*  function:     scgtMemUnmap                                            */
/*  description:  Unmaps GT memory                                        */
/**************************************************************************/

#ifdef PLATFORM_USES_MMAP    /* Linux, IRIX, Solaris */

void * scgtMapMem(scgtHandle *pHandle)           /************/
{
    scgtMemMapInfo mapInfo;
    void *memPtr;
    uint32 ret;

    if (pHandle->memMapPtr)
        return pHandle->memMapPtr;

    IOCTL(pHandle->osHandle, SCGT_IOCTL_MEM_MAP_INFO, &mapInfo, sizeof(scgtMemMap));

    if (ret != SCGT_SUCCESS)
    {
        return NULL;
    }

    memPtr = mmap(0, mapInfo.memSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                  pHandle->osHandle, 0);

    if ((long) memPtr == -1)
    {
        return NULL;
    }

    pHandle->memMapSize = mapInfo.memSize;
    pHandle->memMapPtr = memPtr;

    return pHandle->memMapPtr;
}

void scgtUnmapMem(scgtHandle *pHandle)          /************/
{
    if (pHandle->memMapPtr)
    {
        munmap((void *) pHandle->memMapPtr, pHandle->memMapSize);
        pHandle->memMapPtr = NULL;
    }
}

#else /* use ioctl instead: Windows, VxWorks, RTX */         /*==== OS SWITCH ====*/

DLLEXPORT void * scgtMapMem(scgtHandle *pHandle)         /************/
{
    scgtMemMapInfo mapInfo;
    uint32 ret;

    mapInfo.memPhysAddr = 0;

    IOCTL(pHandle->osHandle, SCGT_IOCTL_MAP_MEM, &mapInfo, sizeof(scgtMemMapInfo));

    if (ret != SCGT_SUCCESS)
    {
        return NULL;
    }

    pHandle->memMapPtr =  UINT64_TO_PTR(uint32, mapInfo.memVirtAddr);
    pHandle->memMapSize = mapInfo.memSize;

    return pHandle->memMapPtr;
}

DLLEXPORT void scgtUnmapMem(scgtHandle *pHandle)
{
    scgtMemMapInfo mapInfo;
    uint32 ret;

    if (pHandle->memMapPtr)
    {
        mapInfo.memVirtAddr = PTR_TO_UINT64(pHandle->memMapPtr);
        mapInfo.memSize = pHandle->memMapSize;

        IOCTL(pHandle->osHandle, SCGT_IOCTL_UNMAP_MEM,
              &mapInfo, sizeof(scgtMemMapInfo));

        pHandle->memMapPtr = NULL;
    }
}

#endif

/**************************************************************************/
/**************** S H A R E D   G T   A P I   C O D E *********************/
/**************************************************************************/

/**************************************************************************/
/*  function:     scgtOpen                                                */
/*  description:  Opens the device file and maps GT memory.               */
/**************************************************************************/
DLLEXPORT uint32 scgtOpen(uint32 unitNum, scgtHandle *pHandle)
{
    char deviceFile[16];

    /* Filter inputs */
    if ((unitNum >= SCGT_MAX_DEVICES) || (pHandle == NULL))
        return SCGT_BAD_PARAMETER;

    /* Create the device file path string to open */
    /*!!! All OSes must number units in decimal !!!*/
    sprintf(deviceFile, "%s%u", SCGT_DEV_FILE_STR, unitNum);

    pHandle->osHandle = OPEN_HANDLE(deviceFile);

    if (pHandle->osHandle == BAD_FILE_HANDLE)
        return SCGT_SYSTEM_ERROR;

    pHandle->unitNum = unitNum;
    pHandle->memMapPtr = NULL;
    pHandle->memMapSize = 0;

    return SCGT_SUCCESS;
}

/**************************************************************************/
/*  function:     scgtClose                                               */
/*  description:  Closes the device file                                  */
/**************************************************************************/
DLLEXPORT uint32 scgtClose(scgtHandle *pHandle)
{
    if ((pHandle != NULL) && (pHandle->osHandle != BAD_FILE_HANDLE))
    {
        if (pHandle->memMapPtr)
            scgtUnmapMem(pHandle);
    
        CLOSE_HANDLE(pHandle->osHandle);
    }
    return SCGT_SUCCESS;
}

/**************************************************************************/
/*  function:     scgtGetDeviceInfo                                       */
/*  description:  Retrieves the device info                               */
/**************************************************************************/
DLLEXPORT uint32 scgtGetDeviceInfo(scgtHandle *pHandle,
                         scgtDeviceInfo *pDeviceInfo)
{
    uint32 ret;

    IOCTL( pHandle->osHandle, SCGT_IOCTL_GET_DEVICE_INFO,
           pDeviceInfo, sizeof(scgtDeviceInfo) );

    pDeviceInfo->unitNum = pHandle->unitNum;
    
    if (pHandle->memMapPtr)   /* if non-NULL, this is what this application */
    {                         /* could map.. otherwise we use the drivers value. */
        pDeviceInfo->mappedMemSize = pHandle->memMapSize;
    }

    return ret;
}

/**************************************************************************/
/*  function:     scgtGetState                                            */
/*  description:  Get state of "stateID" from the driver                  */
/**************************************************************************/
DLLEXPORT uint32 scgtGetState(scgtHandle *pHandle,
                    uint32 stateID)
{
    uint32 ret;
    scgtState state;
    state.stateID = stateID;

    IOCTL( pHandle->osHandle, SCGT_IOCTL_GET_STATE, &state, sizeof(scgtState) );

    if (ret != SCGT_SUCCESS)
        return (uint32) -1;

    return state.val;
}

/**************************************************************************/
/*  function:     scgtSetState                                            */
/*  description:  Set state of "stateID" in the driver to "val"           */
/**************************************************************************/
DLLEXPORT uint32 scgtSetState(scgtHandle *pHandle,
                    uint32 stateID,
                    uint32 val)
{
    uint32 ret;
    scgtState state;
    state.stateID = stateID;
    state.val = val;

    IOCTL( pHandle->osHandle, SCGT_IOCTL_SET_STATE, &state, sizeof(scgtState) );

    return ret;
}


/**************************************************************************/
/*  function:     scgtWrite                                               */
/*  description:  write data to GT memory                                 */
/**************************************************************************/
DLLEXPORT uint32 scgtWrite(scgtHandle *pHandle,
                 uint32 gtMemoryOffset,
                 void   *pDataBuffer,
                 uint32 bytesToTransfer,
                 uint32 flags,
                 uint32 *pBytesTransferred,
                 scgtInterrupt *pInterrupt)
{
    scgtXfer xfer;
    uint32 ret;
    
    if (pInterrupt)
    {
        /* check for invalid interrupt */
        if (!(((pInterrupt->type == SCGT_BROADCAST_INTR) && (pInterrupt->id < 32)) ||
               ((pInterrupt->type == SCGT_UNICAST_INTR) && (pInterrupt->id < 256))
               ||(pInterrupt->type == SCGT_NO_INTR)
            ))
            return SCGT_BAD_PARAMETER;
    }

    if (flags & SCGT_RW_PIO)
    {
        uint32 *memPtr;
        uint32 *dataBuff;
        uint32 i;

        if (pHandle->memMapPtr == NULL)
            return SCGT_BAD_PARAMETER;

        gtMemoryOffset &= ~0x3;
        memPtr = (uint32 *) (((char *)pHandle->memMapPtr) + gtMemoryOffset);
        dataBuff = (uint32 *) pDataBuffer;

        ret = SCGT_SUCCESS;

        bytesToTransfer >>= 2;  /* convert to longwords */
        for (i = 0; i < bytesToTransfer; i++)
            memPtr[i] = dataBuff[i];

        if (pBytesTransferred)
            *pBytesTransferred = (bytesToTransfer << 2);

        if ((pInterrupt) && (pInterrupt->type != SCGT_NO_INTR))
        {
            xfer.bytesToTransfer = 0;
            xfer.pInterrupt = PTR_TO_UINT64(pInterrupt);
            IOCTL(pHandle->osHandle, SCGT_IOCTL_WRITE, &xfer, sizeof(scgtXfer));
        }
    }
    else if (flags & SCGR_RW_PIO_8_BIT)
    {
        uint8 *memPtr;
        uint8 *dataBuff;
        uint32 i;

        if (pHandle->memMapPtr == NULL)
            return SCGT_BAD_PARAMETER;

        memPtr = (uint8 *) (((char *)pHandle->memMapPtr) + gtMemoryOffset);
        dataBuff = (uint8 *) pDataBuffer;

        ret = SCGT_SUCCESS;

        
        for (i = 0; i < bytesToTransfer; i++)
            memPtr[i] = dataBuff[i];

        if (pBytesTransferred)
            *pBytesTransferred = bytesToTransfer;

        if ((pInterrupt) && (pInterrupt->type != SCGT_NO_INTR))
        {
            xfer.bytesToTransfer = 0;
            xfer.pInterrupt = PTR_TO_UINT64(pInterrupt);
            IOCTL(pHandle->osHandle, SCGT_IOCTL_WRITE, &xfer, sizeof(scgtXfer));
        }
    }
    else if (flags & SCGR_RW_PIO_16_BIT)
    {
        uint16 *memPtr;
        uint16 *dataBuff;
        uint32 i;

        if (pHandle->memMapPtr == NULL)
            return SCGT_BAD_PARAMETER;

        gtMemoryOffset &= ~0x1;
        memPtr = (uint16 *) (((char *)pHandle->memMapPtr) + gtMemoryOffset);
        dataBuff = (uint16 *) pDataBuffer;

        ret = SCGT_SUCCESS;

        bytesToTransfer >>= 1;  /* convert to shortwords */
        for (i = 0; i < bytesToTransfer; i++)
            memPtr[i] = dataBuff[i];


        if (pBytesTransferred)
            *pBytesTransferred = (bytesToTransfer << 1);

        if ((pInterrupt) && (pInterrupt->type != SCGT_NO_INTR))
        {
            xfer.bytesToTransfer = 0;
            xfer.pInterrupt = PTR_TO_UINT64(pInterrupt);
            IOCTL(pHandle->osHandle, SCGT_IOCTL_WRITE, &xfer, sizeof(scgtXfer));
        }
    }
    else
    {
        xfer.flags = flags;
        xfer.gtMemoryOffset = gtMemoryOffset;
        xfer.bytesToTransfer = bytesToTransfer;
        xfer.pDataBuffer = PTR_TO_UINT64(pDataBuffer);
        xfer.pInterrupt = PTR_TO_UINT64(pInterrupt);

        IOCTL( pHandle->osHandle, SCGT_IOCTL_WRITE, &xfer, sizeof(scgtXfer) );

        if (pBytesTransferred)
            *pBytesTransferred = xfer.bytesTransferred;
    }

    return ret;
}

/**************************************************************************/
/*  function:     scgtRead                                                */
/*  description:  read data from GT memory                                */
/**************************************************************************/
DLLEXPORT uint32 scgtRead(scgtHandle *pHandle,
                uint32 gtMemoryOffset,
                void   *pDataBuffer,
                uint32 bytesToTransfer,
                uint32 flags,
                uint32 *pBytesTransferred)
{
    scgtXfer xfer;
    uint32 ret;

    if (flags & SCGT_RW_PIO)
    {
        uint32 *memPtr;
        uint32 *dataBuff;
        uint32 i;

        if (pHandle->memMapPtr == NULL)
            return SCGT_BAD_PARAMETER;

        gtMemoryOffset &= ~0x3;
        memPtr = (uint32 *) (((char *)pHandle->memMapPtr) + gtMemoryOffset);
        dataBuff = (uint32 *) pDataBuffer;

        ret = SCGT_SUCCESS;

        
#ifdef PLATFORM_VISA
        bytesToTransfer >>= 2;  /* convert to longwords */
        for (i = 0; i < bytesToTransfer; i++)
        {
            viPeek32(pHandle->osHandle->instr,memPtr,&dataBuff[i]);
            memPtr++;
        }
#else	
        bytesToTransfer >>= 2;  /* convert to longwords */
        for (i = 0; i < bytesToTransfer; i++)
            dataBuff[i] = memPtr[i];
#endif

        if (pBytesTransferred)
            *pBytesTransferred = (bytesToTransfer << 2);
    }
    else if (flags & SCGR_RW_PIO_8_BIT)
    {
        uint8 *memPtr;
        uint8 *dataBuff;
        uint32 i;

        if (pHandle->memMapPtr == NULL)
            return SCGT_BAD_PARAMETER;
 
        memPtr = (uint8 *) (((char *)pHandle->memMapPtr) + gtMemoryOffset);
        dataBuff = (uint8 *) pDataBuffer;

        ret = SCGT_SUCCESS;

        
#ifdef PLATFORM_VISA
        for (i = 0; i < bytesToTransfer; i++)
        {
            viPeek8(pHandle->osHandle->instr,memPtr,&dataBuff[i]);
            memPtr++;
        }
#else	
        for (i = 0; i < bytesToTransfer; i++)
            dataBuff[i] = memPtr[i];
#endif

        if (pBytesTransferred)
            *pBytesTransferred = (bytesToTransfer);
    }
    else if (flags & SCGR_RW_PIO_16_BIT)
    {
        uint16 *memPtr;
        uint16 *dataBuff;
        uint32 i;

        if (pHandle->memMapPtr == NULL)
            return SCGT_BAD_PARAMETER;

        gtMemoryOffset &= ~0x1;
        memPtr = (uint16 *) (((char *)pHandle->memMapPtr) + gtMemoryOffset);
        dataBuff = (uint16 *) pDataBuffer;

        ret = SCGT_SUCCESS;

        
#ifdef PLATFORM_VISA			
		bytesToTransfer >>= 1;  /* convert to short words */
        for (i = 0; i < bytesToTransfer; i++)
		{
			viPeek16(pHandle->osHandle->instr,memPtr,&dataBuff[i]);
			memPtr++;
		}
#else	
		bytesToTransfer >>= 1;  /* convert to short words */
        for (i = 0; i < bytesToTransfer; i++)
            dataBuff[i] = memPtr[i];
#endif		

        if (pBytesTransferred)
            *pBytesTransferred = (bytesToTransfer << 1);
    }
    else
    {
        xfer.flags = flags;
        xfer.gtMemoryOffset = gtMemoryOffset;
        xfer.bytesToTransfer = bytesToTransfer;
        xfer.pDataBuffer = PTR_TO_UINT64(pDataBuffer);
        xfer.pInterrupt = 0;

        IOCTL( pHandle->osHandle, SCGT_IOCTL_READ, &xfer, sizeof(scgtXfer) );

        if (pBytesTransferred)
            *pBytesTransferred = xfer.bytesTransferred;
    }

    return ret;
}

/**************************************************************************/
/*  function:     scgtGetInterrupt                                        */
/*  description:  get network/error interrupts                            */
/**************************************************************************/
DLLEXPORT uint32 scgtGetInterrupt(scgtHandle *pHandle,
                        scgtIntrHandle *intrHandle,
                        scgtInterrupt *interruptBuffer,
                        uint32 numInterrupts,
                        uint32 timeout,
                        uint32 *numInterruptsRet)
{
    uint32 ret;
    scgtGetIntrBuf ibuf;

    if (interruptBuffer == NULL)
        return SCGT_BAD_PARAMETER;

    ibuf.seqNum  = *intrHandle;
    ibuf.intrBuf = PTR_TO_UINT64(interruptBuffer);
    ibuf.bufSize = numInterrupts;
    ibuf.timeout = timeout;

    IOCTL(pHandle->osHandle, SCGT_IOCTL_GET_INTR, &ibuf, sizeof(ibuf));

    *intrHandle = ibuf.seqNum;
    *numInterruptsRet = ibuf.numInterruptsRet;

    return ret;
}

/**************************************************************************/
/*  function:     scgtRead                                                */
/*  description:  read GT control register                                */
/**************************************************************************/
DLLEXPORT uint32 scgtReadCR(scgtHandle *pHandle,
                  uint32 offset)
{
    scgtRegister cr;
    uint32 ret;

    cr.offset = offset & (~0x3);  /* round down to multiple of 4 */

    IOCTL( pHandle->osHandle, SCGT_IOCTL_READ_CR, &cr, sizeof(scgtRegister) );

    if (ret != SCGT_SUCCESS)
        return (uint32) -1;

    return cr.val;
}

/**************************************************************************/
/*  function:     scgtWriteCR                                             */
/*  description:  write GT control register                               */
/**************************************************************************/
DLLEXPORT uint32 scgtWriteCR(scgtHandle *pHandle,
                   uint32 offset,
                   uint32 val)
{
    scgtRegister cr;
    uint32 ret;

    cr.offset = offset & (~0x3);  /* round down to multiple of 4 */
    cr.val = val;
    IOCTL( pHandle->osHandle, SCGT_IOCTL_WRITE_CR, &cr, sizeof(scgtRegister) );

    return ret;
}

/**************************************************************************/
/*  function:     scgtReadNMR                                             */
/*  description:  read GT network management register                     */
/**************************************************************************/
DLLEXPORT uint32 scgtReadNMR(scgtHandle *pHandle,
                   uint32 offset)
{
    scgtRegister nmr;
    uint32 ret;

    nmr.offset = offset & (~0x3);  /* round down to multiple of 4 */
    IOCTL( pHandle->osHandle, SCGT_IOCTL_READ_NMR, &nmr, sizeof(scgtRegister) );

    if (ret != SCGT_SUCCESS)
        return (uint32) -1;

    return nmr.val;
}

/**************************************************************************/
/*  function:     scgtWriteNMR                                            */
/*  description:  write GT network management register                    */
/**************************************************************************/
DLLEXPORT uint32 scgtWriteNMR(scgtHandle *pHandle,
                    uint32 offset,
                    uint32 val)
{
    scgtRegister nmr;
    uint32 ret;

    nmr.offset = offset & (~0x3);  /* round down to multiple of 4 */
    nmr.val = val;
    IOCTL( pHandle->osHandle, SCGT_IOCTL_WRITE_NMR, &nmr, sizeof(scgtRegister) );

    return ret;
}

/**************************************************************************/
/*  function:     scgtGetErrStr                                           */
/*  description:  convert GT error code into a string                     */
/**************************************************************************/
DLLEXPORT char *scgtGetErrStr(uint32 retcode)
{
    char *errStr[] = { "Success",
                       "System Error",
                       "Bad Parameter",
                       "Driver Error",
                       "Timeout",
                       "Call Unsupported",
                       "Insufficient Resources",
                       "Link Error",
                       "Missed Interrupts",
                       "Driver Missed Interrupts",
                       "DMA Unsupported",
                       "Hardware Error" };

    if (retcode > (sizeof(errStr)/sizeof(char*)))
         return "Unknown Error";

    return errStr[retcode];
}

/**************************************************************************/
/*  function:     scgtGetStats                                            */
/*  description:  retrieve driver statistics                              */
/**************************************************************************/
DLLEXPORT uint32 scgtGetStats(scgtHandle *pHandle,
                    uint32 *statsArray,
                    char *statNames,
                    uint32 firstStatIndex,
                    uint32 numStats)
{
    scgtStats stats;
    uint32 ret;

    stats.stats = PTR_TO_UINT64(statsArray);
    stats.names = PTR_TO_UINT64(statNames);
    stats.firstStatIndex = firstStatIndex;
    stats.num = numStats;

    IOCTL(pHandle->osHandle, SCGT_IOCTL_GET_STATS, &stats, sizeof(scgtStats));

    return ret;
}

/**************************************************************************/
/*  function:     scgtGetStats                                            */
/*  description:  retrieve driver statistics                              */
/**************************************************************************/
DLLEXPORT const char * scgtGetApiRevStr( void )
{
  return scgtapiRevisionStr;
}
