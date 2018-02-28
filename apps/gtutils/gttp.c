/******************************************************************************/
/*                              SCRAMNet GT                                   */
/******************************************************************************/
/*                                                                            */
/* Copyright (c) 2002-2005 Curtiss-Wright Controls.                           */
/*               support@systran.com 800-252-5601 (U.S. only) 937-252-5601    */
/*                                                                            */
/* This program is free software; you can redistribute it and/or              */
/* modify it under the terms of the GNU General Public License                */
/* as published by the Free Software Foundation; either version 2             */
/* of the License, or (at your option) any later version.                     */
/*                                                                            */
/* See the GNU General Public License for more details.                       */
/*                                                                            */
/******************************************************************************/
/******************************************************************************/
/* This is a project fork for maintaining the driver for working with the      */
/* 3.10 kernel in Redhat/Centos 7.                                             */
/*                                                                             */
/* This fork is created/maintained by Brandon M. Graves, https://metashell.net */
/* up to date copies can be pulled from https://github.com/bmgraves/scgt       */
/*                                                                             */
/* It is important to note all credit to the original Developer, this is merely*/
/* an ongoing support project to test for functionality in future maintained   */
/* version of linux for legacy cards.                                          */
/*                                                                             */
/*******************************************************************************/

#define APP_VER_STRING "GT Throughput Test (gttp) rev. 1.03 (2011/08/30)"

/**************************************************************************/
/****** TAKING CARE OF ALL POSSIBLE OS TYPES - COPY-PASTE *****************/
/**************************************************************************/
/* control by PLATFORM_WIN, PLATFORM_UNIX, PLATFORM_VXWORKS */          /**/
#include <math.h>                                                       /**/
#include <stdio.h>                                                      /**/
#include <time.h>                                                       /**/
#include <string.h>                                                     /**/
#include <ctype.h>                                                      /**/
                                                                        /**/
#include "systypes.h"                      /* this will get a path */   /**/
#include "usys.h"                          /* this will get a path */   /**/
                                                                        /**/
#define PARSER                                                          /**/
#define MAIN_IN      int main(int argc, char **argv) {                  /**/
                                                                        /**/
#if defined PLATFORM_WIN || defined PLATFORM_RTX                        /**/
    #include <windows.h>                                                /**/
                                                                        /**/
#elif PLATFORM_VXWORKS                                                  /**/
    #undef PARSER                                                       /**/
    #undef MAIN_IN                                                      /**/
    #define VXL     40                                                  /**/
    #define MAIN_IN int APP_VxW_NAME(char *cmdLine) { \
            int argc; char argv0[]="gttp"; char *argv[VXL]={argv0};     /**/
    #define PARSER     argc = parseCls(cmdLine, argv);                  /**/
                                                                        /**/
static int parseCls(char *cLine, char *argv[VXL])                       /**/
{                                                                       /**/
    char *token, *ptr = NULL;                                           /**/
    int numTokens = 1;                                                  /**/
    char seps[] = " ,\t\n";                                             /**/
    if(cLine == NULL) return numTokens; /* no command line given */     /**/
    printf(" \b"); token=(char*)strtok_r(cLine,seps,&ptr);              /**/
    while(token != NULL)                                                /**/
    { argv[numTokens] = token;                                          /**/
      numTokens++;                                                      /**/
      token = (char*)strtok_r(NULL, seps, &ptr);                        /**/
    }                                                                   /**/
    return numTokens;                                                   /**/
}                                                                       /**/
#endif                                                                  /**/
/************ END OF TAKING CARE OF ALL POSSIBLE OS TYPES *****************/

/**************************************************************************/
/******************* HARDWARE SPECIFIC INCLUDES ***************************/
/**************************************************************************/

#ifdef HW_SL240 /* SLxxx */
    #include "fxsl.h"
    #include "fxslapi.h"
    #define HAND HANDLE
    #define APP_VxW_NAME xgttp
#elif defined HW_SCFF /* Scramnet */
    #include "scrplus.h"
    #define HAND uint32
    #define APP_VxW_NAME sgttp
#elif defined HW_SYSTEM_MEM /* system memory */
    #define HAND uint32
    #define APP_VxW_NAME mgttp
#else /* GT */
    #define HW_SCGT
    #include "scgtapi.h"
    #define HAND scgtHandle
    #define APP_VxW_NAME gttp
#endif

/**************************************************************************/
/************** APPLICATION-SPECIFIC CODE STARTS HERE *********************/
/**************************************************************************/

/**************************************************************************/
/**************************  D A T A  T Y P E S  **************************/
/**************************************************************************/

#define COMMON_OPTIONS \
    uint32    direction,    doProcess,     doPrint,    doPIO, doSYNC; \
    uint32    doMemory,     doOneSecond,   doGraph,    doTimedExec; \
    uint32    packetSize,   iterations,    offset,     peerPciTarget; \
    uint32    doPattern,    pattern,       alignment; \
    uint32    doGraphAll;   volatile uint32  *boardAddressPIO; \
    HAND  hDriver;

typedef struct {
    uint32    timeToExit;
    uint32    running;
    usysThreadParams   pt;
} exitThreadParms, *pexitThreadParms;

typedef struct {
    COMMON_OPTIONS
    uint32    taskCount,     unit,     helpLevel,  memSizeDMA,  memSizePIO;
    exitThreadParms exitParms;
} iOptionsType;

typedef struct {
    uint32   *sysMemBuff, *timeToExit;
    uint32    gbytesTransfered,    bytesTransfered,    status,   taskID;
    COMMON_OPTIONS
    usysSemb   done;
    usysThreadParams   pt;
} taskParms, *ptaskParms;

typedef struct {
    uint32    bytes,   gBytes,     iterations,    msTestTime;
    uint32    dataRate;
    uint32    status;
} oResultsType;

/**************************************************************************/
/******************  F U N C T I O N  P R O T O T Y P E S  ****************/
/**************************************************************************/
static int parseCommand(int argc, char **argv, iOptionsType *iO, char *optionsString);
static void printHelpLevel1(iOptionsType *iO, char *optionsString);
static void printHelpLevel2();
static void printHelpLevel3();
static void gttpPrintResults(iOptionsType *iO, oResultsType *oR, uint32 line);
static void gttpGraph(iOptionsType *iO, oResultsType *oR);
static void *gttpWorkerThread(void *voidp);
static void buildOptionsString(iOptionsType *iO, char *optionsString);
static int  gttpBoss( iOptionsType *iO, oResultsType *oR);
static void *getRuntimeChar(void *voidp);
static void printResourceHeader(HAND *pHandle, uint32 unit);

#define gttpErrorPrint(x)     printf("ERROR: code %d\n",x)

/**************************************************************************/
/***********************  D E F I N E S & M A C R O S  ********************/
/**************************************************************************/

#define MAX_THREADS 9
#define SEND        1
#define RECEIVE     0

/* The following is to mask the actual hardware dependency */

#ifdef HW_SL240 /* SLxxx */
    #define APP_VARIANT "- sl240 (xgttp) Edition"
    #define DRIVER_SUCCESS FXSL_SUCCESS
    #define BUFFER_IS_PHYSADDR  FXSL_BUFFER_IS_PHYSADDR
    #define doPIO_DISABLED 1
    #define doDMA_DISABLED 0
    #define DRIVER_OPEN(x,y) { if(fxsl_open(x,y)) \
           { printf("ERROR: could not open driver for unit %d\n", x); \
             return 0; } \
            iO.boardAddressPIO = (volatile uint32 *)NULL; \
            iO.memSizeDMA = iO.memSizePIO = 0x80000000; /* 2GB, arbitrary */ }
    #define DRIVER_CLOSE(x) fxsl_close(x)
    #define DMA_WRITE_GT(pHandle,smOffset,pSource,sizeByte,flags,pBytesWritten)\
               (flags = (p->doSYNC == 1) ? flags | FXSL_USE_SYNC : flags, \
                fxsl_send(*pHandle, pSource, sizeByte,&flags,0,pBytesWritten))
    #define DMA_READ_GT(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead) \
               (flags = (p->doSYNC == 1) ? flags | FXSL_USE_SYNC : flags, \
                fxsl_recv(*pHandle,pDest, sizeByte, &flags, 0, pBytesRead))
#elif defined HW_SCFF /* Scramnet */
    #define APP_VARIANT "- SCRAMNet (sgttp) Edition"
    #define DRIVER_SUCCESS 0
    #define BUFFER_IS_PHYSADDR  0 /* 0 for not-supported */
    #define doPIO_DISABLED 0
    #define doDMA_DISABLED 0
    #define DRIVER_OPEN(x,y) {sp_scram_init(0); \
              sp_stm_mm(Long_mode); \
              scr_csr_write(2, (unsigned short) 0xd040); \
              scr_csr_write(0, (unsigned short) 0x8003); \
              scr_csr_write(10,(unsigned short)(scr_csr_read(10) | 0x11)); \
              iO.boardAddressPIO = (volatile uint32 *)get_base_mem();      \
              iO.memSizeDMA = iO.memSizePIO = sp_mem_size();}
    #define DRIVER_CLOSE(x)
    #define DMA_WRITE_GT(pHandle,smOffset,pSource,sizeByte,flags,pBytesWritten)\
                (scr_dma_write(pSource, smOffset, sizeByte), \
                 *pBytesWritten = sizeByte, DRIVER_SUCCESS)
    #define DMA_READ_GT(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead) \
                (scr_dma_read(pDest, smOffset, sizeByte), \
                 *pBytesRead = sizeByte, DRIVER_SUCCESS)
#elif defined HW_SYSTEM_MEM /* system memory */
    #define APP_VARIANT "- MEMORY-only (mgttp) Edition"
    #define DRIVER_SUCCESS 0
    #define BUFFER_IS_PHYSADDR  0 /* 0 for not-supported */
    #define doPIO_DISABLED 1
    #define doDMA_DISABLED 1
    #define DRIVER_OPEN(x,y) iO.boardAddressPIO = NULL; \
            iO.memSizeDMA = iO.memSizePIO = 0x80000000; /* 2GB, arbitrary */
    #define DRIVER_CLOSE(x)
    #define DMA_WRITE_GT(pHandle,smOffset,pSource,sizeByte,flags,pBytesWritten)\
                (*pBytesWritten = 0, DRIVER_SUCCESS)
    #define DMA_READ_GT(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead) \
                (*pBytesRead = 0, DRIVER_SUCCESS)
#elif defined HW_SCGT /* GT */
    #define APP_VARIANT ""
    #define DRIVER_SUCCESS SCGT_SUCCESS
    #define BUFFER_IS_PHYSADDR  SCGT_RW_DMA_PHYS_ADDR
    #define doPIO_DISABLED 0
    #define doDMA_DISABLED 0
    #define DRIVER_OPEN(x,y)  { scgtDeviceInfo devInfo;                  \
            if (scgtOpen(x,y))                                           \
            {                                                            \
                printf("ERROR: could not open driver for unit %d\n", x); \
                return 0;                                                \
            }                                                            \
            iO.boardAddressPIO = scgtMapMem(y);                          \
            scgtGetDeviceInfo(y, &devInfo);                              \
            iO.memSizeDMA = 0x20000000; /* 512MB */                      \
            iO.memSizePIO = devInfo.mappedMemSize; }
    #define DRIVER_CLOSE(x) { scgtUnmapMem(&(x)); scgtClose(&(x)); }
    #define DMA_WRITE_GT(pHandle,smOffset,pSource,sizeByte,flags,pBytesWritten)\
                scgtWrite(pHandle,smOffset,pSource,sizeByte,flags, \
                          pBytesWritten, NULL)
    #define DMA_READ_GT(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead) \
                scgtRead(pHandle,smOffset,pDest,sizeByte,flags, pBytesRead)
#endif

/**************************************************************************/
/*************************** G T T P   C O D E ****************************/
/**************************************************************************/

MAIN_IN                                                      /* gttp main */
    iOptionsType   iO;
    oResultsType   oR;
    int            j;
    char           optionsString[160];
    usysMsTimeType curTime;
    uint32 curSecond;
    
    PARSER;    /* preparing argc and agrv if not provided by OS (VxWorks) */

    memset(&iO, 0, sizeof(iOptionsType));        /* zero all the options */

    j = parseCommand(argc, argv, &iO, optionsString);    /* parsing options */

    if(iO.helpLevel!=0) /*1, 2, 3, ...*/
    {
        printHelpLevel1(&iO,optionsString);
        return j;
    }

    if( (iO.peerPciTarget != 0x0) && (BUFFER_IS_PHYSADDR == 0) )
    {
        printf("!!!!! Physical addresses not supported on your hardware !!!!!\n");
        return -1;
    }

    if( (iO.doPIO != 0) && (iO.doGraphAll == 0) && (doPIO_DISABLED != 0) )
    {
        printf("!!!!! PIO (P) option not supported on your hardware !!!!!\n");
        return -1;
    }
    else if( (iO.doPIO == 0) && (iO.doMemory == 0) && 
             (doDMA_DISABLED != 0) && (iO.doGraphAll == 0) )
    {
        printf("!!!!! DMA option (default) not supported on your hardware !!!!!\n");
        return -1;
    }

    DRIVER_OPEN(iO.unit, (HAND *) &iO.hDriver);

    if( (iO.doPIO !=0) && (iO.boardAddressPIO == NULL) )
    {
        printf("!!!!! PIO address is NULL !!!!!\n");
        DRIVER_CLOSE(iO.hDriver);
        return -1;
    }

    /* verify request is within valid memory range */
    if( (iO.doPIO !=0) && ((iO.offset + iO.packetSize) > iO.memSizePIO) )
    {
        printf("!!!!! Request violates valid PIO memory range !!!!!\n");
        DRIVER_CLOSE(iO.hDriver);
        return -1;
    }

    if( (iO.doPIO ==0) && ((iO.offset + iO.packetSize) > iO.memSizeDMA) )
    {
        printf("!!!!! Request violates valid DMA memory range !!!!!\n");
        DRIVER_CLOSE(iO.hDriver);
        return -1;
    }

    if(iO.doGraph == 0)
        printf("Running: %s\n",optionsString);
    fflush(stdout);
    
    if(iO.peerPciTarget != 0)
        printf("DANGER: physical (PCI bus) address 0x%8.8X in use!\n",
                iO.peerPciTarget);

    /* spawn exit thread */
    iO.exitParms.pt.parameter = &iO.exitParms;
    iO.exitParms.pt.priority = UPRIO_HIGH; /* only use UPRIO_HIGH */
    sprintf(iO.exitParms.pt.threadName, "gttpExit");
    iO.exitParms.timeToExit = iO.exitParms.running = 0;
    usysCreateThread(&iO.exitParms.pt, getRuntimeChar);
    /* Give thread a moment to start up. Needed on VxWorks, Solaris 8 */
    usysMsTimeDelay(50);

    if(iO.doGraph == 0)
    {
        if(iO.doTimedExec)
        {
            j = curSecond = 0;
            usysMsTimeStart(&curTime);

            while( !iO.exitParms.timeToExit && 
                  ((iO.doTimedExec==-1) || (curSecond < iO.doTimedExec)) )
            {
                if(j == 0)
                    gttpPrintResults(&iO, &oR, 1);
                gttpBoss (&iO, &oR);
                gttpPrintResults(&iO, &oR, 0);
                fflush(stdout);
                j=(j+1) % 10;
                curSecond = (usysMsTimeGetElapsed(&curTime) / 1000);
            }
            usysMsTimeStop(&curTime);
        }
        else
        {
            gttpBoss (&iO, &oR);
            gttpPrintResults(&iO, &oR, 1);
            gttpPrintResults(&iO, &oR, 0);
        }
    }
    else          /* if not the above then do the whole graph */
    {
        gttpGraph(&iO, &oR);
    }

    if( iO.exitParms.running )
    {   
        usysKillThread(&iO.exitParms.pt);
    }

    DRIVER_CLOSE(iO.hDriver);

    return( 0 );
}                                          /* end of MAIN_IN that is main */

/**************************************************************************/
/*  function:     gttpGraph                                               */
/*  description:  Runs multiple of tests for graphing purpose.            */
/**************************************************************************/
static void gttpGraph(iOptionsType *iO, oResultsType *oR)
{
    unsigned int   i, j, n;
    uint32         iterationsMultiplier = 1;
    time_t         ltime;
    uint32         goneBad;
#define GSIZE      19
#define NTEST      6
    uint32         graphSize = GSIZE;
    uint32         numOfTests = NTEST;
    uint32         final[GSIZE][NTEST];   
    uint32         test[][3]={ /* doPIO, doMemory, direction */
                   {0,0,SEND},{1,0,SEND},{0,0,RECEIVE},{1,0,RECEIVE},
                   {0,1,SEND},{0,1,RECEIVE}};
    char           heads[NTEST][15];

    uint32 graph[][2]= {{4,80000},{8,80000},{16,80000},{32,80000},
    {64,80000},{128,80000},{256,80000},{512,80000},{1024,80000},
    {2048,80000},{4096,50000}, {8192,25000},{16384,12500},{32768,6250},
    {65536,3125},{131072,1562},{262144,781},{524288,390},{2*524288,195}};

    for(i=0;i<graphSize;i++)
        for(j=0;j<numOfTests;j++)
            final[i][j]=0;

    time( &ltime );
    printf("=========== CERTIFICATE of PERFORMANCE =========== %s",ctime( &ltime ));
    printf( "%s %s\n", APP_VER_STRING, APP_VARIANT );   
    printResourceHeader(&iO->hDriver, iO->unit);
    fflush(stdout);

    iO->doPrint = 0;

    if(iO->doGraphAll ==0)
        numOfTests = 1;

    for(n=0; n < numOfTests; n++)
    {
        if(iO->doGraphAll !=0)
        {
            iO->doPIO = test[n][0];
            iO->doMemory = test[n][1];
            iO->direction = test[n][2];
        }
        sprintf(heads[n],"_%c_%c%c%c%c%c ",   iO->direction == SEND ? 'w':'r', 
            iO->doPIO == 1 ? 'P':'.',         iO->doProcess == 1 ? 'E':'.',
            iO->doMemory == 1 ? 'M':'.',      iO->doOneSecond == 1 ? 'S':'.', 
            iO->peerPciTarget == 0 ? '.':'a');

        if( (iO->doPIO !=0) &&
            ((doPIO_DISABLED != 0) || (iO->boardAddressPIO == NULL) ))
        {
            printf("!!!!! PIO not supported on your hardware !!!!!\n");
            continue;
        }
        if((iO->doPIO ==0) && (iO->doMemory == 0) && (doDMA_DISABLED != 0))
        {
            printf("!!!!! DMA not supported on your hardware !!!!!\n");
            continue;
        }

        goneBad=0;
        if(iO->doMemory == 1)
            iterationsMultiplier = 10;

        gttpPrintResults(iO, oR, 1);
        for(j=1; (j < 4) && (goneBad == 0) && !(iO->exitParms.timeToExit); j++)
        {
            for(i=0; (i < graphSize) && (goneBad == 0) &&
                     !(iO->exitParms.timeToExit); i++)
            {
                iO->packetSize = graph[i][0];
                iO->iterations =  (iterationsMultiplier * graph[i][1]) / j;
                iO->taskCount = j;
                goneBad=gttpBoss (iO, oR);

                if(oR->dataRate > final[i][n])
                    final[i][n]=oR->dataRate;

                gttpPrintResults(iO, oR, 0);
            }
        }
    }
    printf("==SUMMARY==\n");
    printf("_g    SIZE ");
    for(i=0;i<n;i++)
    {
        printf(" %s",heads[i]);
    }
    printf("\n");
    for(i=0;i<graphSize;i++)
    {
        printf("_g%8d",graph[i][0]);
        for(j=0;j<numOfTests;j++)
            printf("%7d.%02d", final[i][j]/100,final[i][j]%100);
        printf("\n");
        fflush(stdout);
    }
}

/**************************************************************************/
/*  function:     gttpWorkerThread                                        */
/*  description:  Thread to perform throughput test operations.           */
/**************************************************************************/
static void *gttpWorkerThread(void *voidp)
{
    uint32     i,j,n=0;
    ptaskParms p = (ptaskParms) voidp;
    uint32     packetSize = p->packetSize;
    uint32     pSizeWord  = p->packetSize >> 2;
    uint32     iterations;
    uint32     iterationsMinusOne;
    uint32     ret = DRIVER_SUCCESS;
    uint32     bytesReported = p->packetSize;    /* must for Memory test */
    uint32     offset        = p->offset;
    uint32     flags       = 0;
    uint32     bytesTransfered =0;
    uint32     gbytesTransfered=0;
    uint32    *emulationBuff = (uint32 *)p->sysMemBuff;
    uint8     *dmaBuff       = (uint8 *)p->sysMemBuff;
    volatile uint32  *boardAddressPIO;
    uint32     doOneSecond = p->doOneSecond;
    uint32     direction   = p->direction;
    uint32     doPrint     = p->doPrint;
    uint32     doProcess   = p->doProcess;
    uint32     doMemory    = p->doMemory;
    uint32     doPIO       = p->doPIO;
    uint32     doPattern   = p->doPattern;
    usysMsTimeType    msTimer;

/* ldw do something for offset to be used in mem and x version */
/* ldw do something for packetSize to be used in x version */
/* flags, dmaBuff */
    if(p->boardAddressPIO != NULL)                        /* PIO possible */
        boardAddressPIO=(volatile uint32 *)p->boardAddressPIO + (p->offset>>2);
    else
        boardAddressPIO=(volatile uint32 *)p->sysMemBuff;

    if(doOneSecond)
        p->iterations = p->iterations / 10 + 2;

    if(p->peerPciTarget != 0)
    {
      flags = BUFFER_IS_PHYSADDR;
      dmaBuff  = (uint8 *)((uintpsize)p->peerPciTarget);
    }

    iterations =p->iterations;
    p->iterations = 0; /* this is the value to be reported back to gttpBoss */
    iterationsMinusOne = iterations - 1;
    usysMsTimeStart(&msTimer);
                                             /****** test loop start ******/
    for(i=0; (i < iterations) && !(*(p->timeToExit)); i++)
    {
        if( doOneSecond && (i == iterationsMinusOne) )
        {
            if(usysMsTimeGetElapsed(&msTimer) <1000 ) /* less then 1s */
            {
                p->iterations = p->iterations + iterations;
                i=0;
            }
        }
        if (direction == SEND)
        {
            if(!doPattern)
                emulationBuff[0] = i;
            if(doProcess == 1)                    /* processing emulation */
            {
                for(j=2;j < pSizeWord; j++)
                    emulationBuff[j]=i;
            }
            if(doPIO == 1)                        /* GT hardware PIO test */
            {
                for(j=0;j < pSizeWord; j++)
                {
                    boardAddressPIO[j]=emulationBuff[j];
/*ldw mem test*/ /*         boardAddressPIO[j+0x400]=emulationBuff[j];*/
                }
            }
            else if(doMemory == 1)                  /* system memory test */
            {
                for(j=0;j < pSizeWord; j++)
                    emulationBuff[j]=i;
            }
            else                                  /* GT hardware DMA test */
            {
                bytesReported = 0;
                ret = DMA_WRITE_GT(&p->hDriver,offset,dmaBuff,
                                packetSize,flags,&bytesReported);
            }
        }
        else   /* (direction = RECEIVE) */
        {
            if(doPIO == 1)                        /* GT hardware PIO test */
            {
                for(j=0;j < pSizeWord; j++)
                {
                    emulationBuff[j] = boardAddressPIO[j];
/*ldw mem test*/  /*        emulationBuff[j]=boardAddressPIO[j+0x400]; */
                }
            }
            else if(doMemory == 1)                  /* system memory test */
            {
                for(j=0;j < pSizeWord; j++)
                    n = n ^ emulationBuff[j];
                emulationBuff[0]=n;
            }
            else                                  /* GT hardware DMA test */
            {
                bytesReported = 0;
                ret = DMA_READ_GT(&p->hDriver,offset,dmaBuff,
                               packetSize,flags,&bytesReported);
            }
            if(doProcess == 1)                    /* processing emulation */
            {
                for(j=0;j < pSizeWord; j++)
                    n = n ^ emulationBuff[j];
                emulationBuff[0]=n;
            }
            if(doPrint == 1)                     /* printing out received */
            {
                for(j=0;(j < (bytesReported >> 2)) && (j < pSizeWord); j++)
                    printf("%8.8X%s", emulationBuff[j],(!((j+1)%8)?"\n":"  "));
                printf("%sBytes=%d Flags=%d\n", ((j%8) ? "\n" : ""), bytesReported,flags);
            }
        }
        bytesTransfered += bytesReported;           /* results accounting */
        if (bytesTransfered >= 1000000000)
        {
            gbytesTransfered += (bytesTransfered / 1000000000);
            bytesTransfered %= 1000000000;
        }
        if (ret != DRIVER_SUCCESS)
        {
            printf("%s thread %d (completed %d of %d) error code %d - ",
                direction == SEND ? "WRITE":"READ",p->taskID,
                (p->iterations + i), (p->iterations + iterations), ret);
            gttpErrorPrint(ret);
            p->status = 1;
            break;
        }
    }                                          /****** test loop end ******/
    p->iterations += i;
    usysMsTimeStop(&msTimer);
    p->gbytesTransfered = gbytesTransfered;
    p->bytesTransfered= bytesTransfered;
    usysSemBGive(p->done);
    return voidp;
}

/**************************************************************************/
/*  function:     gttpBoss                                                */
/*  description:  create and lunch appropriate workers to do the test     */
/**************************************************************************/
static int gttpBoss (iOptionsType *iO, oResultsType *oR)
{
    uint32         i, j, k;
    usysMsTimeType  msTimerTotal;
    taskParms   parms[MAX_THREADS];
    uint32     *sysMemBuff[MAX_THREADS];
    uint32     *space[MAX_THREADS];

    for(j = 0; j < iO->taskCount; j++)
    {  
        space[j] = (uint32*)usysMemMalloc(iO->packetSize +4096+4096+4+12);
        if(space[j] == NULL)
        {
            for(i = 0; i < j; i++)
                usysMemFree(space[i]);
            printf("ERROR:  not enough memory.\n");
            return 1;
        }
    }
    for(j = 0; j < iO->taskCount; j++)
    {
        /* align the buffer to 4K (4096 bytes) */
        sysMemBuff[j] = (uint32*)(((uintpsize)(space[j]) + 4095) &
                         (uintpsize)(~((uintpsize)4095)));
        /* add alignment offset */
        iO->alignment = (iO->alignment % 4096) & ~0x3;
        sysMemBuff[j] = (uint32*)((uintpsize)sysMemBuff[j] +
                                  (uintpsize)iO->alignment);

        if( iO->doPattern && !iO->doProcess )
	    k = iO->pattern;
        else
	    k = (j<< 24) | 0x555555;
	
        for (i = 0; i < iO->packetSize / 4; i++)
            sysMemBuff[j][i] = k;
	if( !iO->doPattern)
	    sysMemBuff[j][1]=iO->packetSize / 4;	
    }
    for (i = 0; i < iO->taskCount; i++)
    {
        if(iO->direction == SEND)
            sprintf(parms[i].pt.threadName, "gttpw%d", i);
        else
            sprintf(parms[i].pt.threadName, "gttpr%d", i);
        parms[i].taskID           = i;
        parms[i].bytesTransfered  = 0;
        parms[i].gbytesTransfered = 0;
        parms[i].pt.parameter     = &parms[i];
        parms[i].pt.priority      = UPRIO_LOW;        /* do not use other */
        parms[i].status           = 0;
        parms[i].sysMemBuff       = sysMemBuff[i];
        parms[i].hDriver          = iO->hDriver;
        parms[i].packetSize       = iO->packetSize;
        parms[i].direction        = iO->direction;
        parms[i].iterations       = iO->iterations;
        parms[i].doProcess        = iO->doProcess;
        parms[i].doPrint          = iO->doPrint;
        parms[i].doPIO            = iO->doPIO;
        parms[i].doPattern        = iO->doPattern;
        parms[i].doMemory         = iO->doMemory;
        parms[i].doOneSecond      = iO->doOneSecond;
        parms[i].doSYNC           = iO->doSYNC;
        parms[i].offset           = iO->offset;
        parms[i].peerPciTarget    = iO->peerPciTarget;
        parms[i].boardAddressPIO  = iO->boardAddressPIO;
        parms[i].timeToExit       = &(iO->exitParms.timeToExit);
        usysSemBCreate(&parms[i].done);
    }
    usysMsTimeStart(&msTimerTotal);
    for (i = 0; i < iO->taskCount; i++)
        usysCreateThread(&parms[i].pt,gttpWorkerThread);
    for (i = 0; i < iO->taskCount; i++)
        usysSemBTake(parms[i].done);
    usysMsTimeStop(&msTimerTotal);
    for (i = 0; i < iO->taskCount; i++)
        usysSemBDestroy(&parms[i].done);
    for(i = 0; i < iO->taskCount; i++)
        usysMemFree(space[i]);

    oR->msTestTime = usysMsTimeGetElapsed(&msTimerTotal);     /* results */
    oR->bytes      = 0;
    oR->gBytes     = 0;
    oR->dataRate   = 0;
    oR->status     = 0;
    oR->iterations =0;    
    if (oR->msTestTime == 0)
        oR->msTestTime=1;
    
    for (i = 0; i < iO->taskCount; i++)
    {
        oR->bytes = oR->bytes + parms[i].bytesTransfered;
        if (oR->bytes >= 1000000000)
        {
            oR->gBytes = oR->gBytes + (oR->bytes/1000000000);
            oR->bytes = oR->bytes % 1000000000;
        }
        oR->gBytes = oR->gBytes + parms[i].gbytesTransfered;
        oR->status = oR->status | parms[i].status;
        oR->iterations = oR->iterations +parms[i].iterations;
    }
    oR->dataRate = (100000000/oR->msTestTime) * oR->gBytes +
        ((oR->bytes/oR->msTestTime)/10);
    oR->iterations = oR->iterations / iO->taskCount;
    if (oR->status != 0)
        return 1;
    
    return 0;
}

/***************************************************************************/
/* Function:    getRuntimeChar                                             */
/* Description: Thread that reads keyboard                                 */
/***************************************************************************/
static void *getRuntimeChar(void *voidp)
{
    pexitThreadParms pparms = (pexitThreadParms) voidp;
    char ch;
    int ich;
        
    pparms->running = 1;
    while (!pparms->timeToExit)
    {
        if ((ich = getchar()) == EOF)
            break;
        
        ch = toupper(ich); /* toupper may be a macro */
        if (ch == 'Q')
        {   pparms->running = 0;
            pparms->timeToExit = 1;
        }
    }
    return voidp;
}

/**************************************************************************/
/*  function:     gttpPrintResults                                        */
/*  description:  Print test results                                      */
/**************************************************************************/
static void gttpPrintResults(iOptionsType *iO, oResultsType *oR, uint32 line)
{
    if(line == 1)
    {
        printf("DIR_PEMS_a_t_____ITER/t_____TOT.BYTES____TIME_____"
               "IO/s__us/IO_____SIZE_____MB/s\n");
        return;
    }
    printf("_%c_ %c%c%c%c %c %1d %10d",   iO->direction == SEND ? 'w':'r', 
        iO->doPIO == 1 ? 'P':'.',         iO->doProcess == 1 ? 'E':'.',
        iO->doMemory == 1 ? 'M':'.',      iO->doOneSecond == 1 ? 'S':'.', 
        iO->peerPciTarget == 0 ? '.':'a', iO->taskCount, oR->iterations);
    
    if (oR->gBytes >= 1)
        printf(" %4u%09u", oR->gBytes, oR->bytes);
    else
        printf(" %13u", oR->bytes);
    
    printf(" %4d.%02d", oR->msTestTime / 1000, (oR->msTestTime % 1000)/10);
    if(oR->iterations)
        printf(" %8d %6d", (oR->iterations*iO->taskCount*100/oR->msTestTime)*10,
        (oR->msTestTime*1000) / (oR->iterations * iO->taskCount));
    else
        printf(" %8d %6d", 0, 0);

    printf(" %8d %5d.%02d\n",
        iO->packetSize,oR->dataRate/100,oR->dataRate%100);
    fflush(stdout);
}

/**************************************************************************/
/*  function:     buildOptionsString                                      */
/*  description:  Getting argv details into internal usable form          */
/**************************************************************************/
static void buildOptionsString(iOptionsType *iO, char *optionsString)
{
    char *str = optionsString;
    int off = 0;
    off += sprintf(&str[off], "gttp -%s%s%s%s%s%s -u %d -l %d -n %d -o %d ",
        iO->direction==SEND ? "w":"r", iO->doPIO ? "P":"",
        iO->doProcess ? "E":"",        iO->doOneSecond ? "S":"",
        iO->doPrint ? "V":"",          iO->doMemory ? "M":"",
        iO->unit,      iO->packetSize, iO->iterations, iO->offset);

    if( iO->taskCount )
        off += sprintf(&str[off], "-t %d ", iO->taskCount);        
    if( iO->doTimedExec )
        off += sprintf(&str[off], "-s %d ", iO->doTimedExec);
    if( iO->doPattern )
        off += sprintf(&str[off], "-d %d ", iO->pattern);
}

/**************************************************************************/
/*  function:     parseCommand                                            */
/*  description:  Getting argv details into internal usable form          */
/**************************************************************************/
static int parseCommand(int argc,char **argv,iOptionsType *iO,char *optionsString)
{
    char *endptr, nullchar = 0;
    int  len, tookParam=0;
    int i,j;                               
                                     /* configure non-zero default settings */
    iO->helpLevel=1;   iO->packetSize = 0x20000;    iO->direction = RECEIVE;

    buildOptionsString(iO, optionsString);

    if(argc == 1)                          /* start processing */
        return 0;
    for(i = 1; i < argc; i++, tookParam=0)
    {
        len = (int) strlen(argv[i]);
        if( (argv[i][0] != '-') || (len < 2) )
        {
            printf( "\nERROR: Unexpected option: \"%s\"\n\n", argv[i] );
            return (iO->helpLevel=1, -1);
        }

        if ( (argv[i][1] == '-') )
        {
            if( !strcmp( &argv[i][2], "version" ) )
            {
                printf("%s\n", APP_VER_STRING);
                #ifdef HW_SCGT
                printf(" - built with API revision %s\n", scgtGetApiRevStr());
                #endif
                exit (0);
            }
            else if( !strcmp( &argv[i][2], "help" ) )
            {
                iO->helpLevel=2;
                return 0;
            }
        }

                /* do options with arguments first*/
        for(j=1; j<len; j++)
        {
            if( (j == (len-1)) && ((i+1) < argc))
            {
                tookParam = 1;  endptr = &nullchar;
                /* test for options which take parameters */
                /* these options only valid as last char of -string */
                if(     argv[i][j]=='u')
                    iO->unit=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='o')
                    iO->offset=(strtoul(argv[i+1],&endptr,0)>>2)<<2;
                else if(argv[i][j]=='l')
                    iO->packetSize=(strtoul(argv[i+1],&endptr,0)>>2)<<2;
                else if(argv[i][j]=='n')
                    iO->iterations=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='t')
                    iO->taskCount=strtoul(argv[i+1],&endptr,0) % MAX_THREADS;
                else if(argv[i][j]=='s')
                    iO->doTimedExec=strtol(argv[i+1],&endptr,0);
                else if(argv[i][j]=='d')
                {   iO->doPattern = 1;
                    iO->pattern = strtoul(argv[i+1],&endptr,0);
                }
                else if(argv[i][j]=='z')
                    iO->peerPciTarget=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='k')
                    iO->alignment = strtoul(argv[i+1],&endptr,0);
                else tookParam = 0;

                if( tookParam && (*endptr))
                {
                    printf("\nInvalid parameter \"%s\" for option \"%c\"\n\n",
                           argv[i+1], argv[i][j]);
                    return (iO->helpLevel=1, -1);
                }
            }
            /* options without arguments now */
            if(     argv[i][j]=='w') iO->direction = SEND;
            else if(argv[i][j]=='r') iO->direction = RECEIVE;
            else if(argv[i][j]=='P') iO->doPIO = 1;
            else if(argv[i][j]=='M') iO->doMemory = 1;
            else if(argv[i][j]=='E') iO->doProcess = 1;
            else if(argv[i][j]=='S') iO->doOneSecond = 1;
            else if(toupper(argv[i][j])=='V') iO->doPrint = 1;
            else if(toupper(argv[i][j])=='G') iO->doGraph = 1;
            else if(toupper(argv[i][j])=='A') iO->doGraphAll = 1;
            else if(toupper(argv[i][j])=='X') iO->doSYNC = 1;
            else if(tolower(argv[i][j])=='h')
            {
                iO->helpLevel=2;
                if(argc > 2) iO->helpLevel=3;
                return 0;
            }
            else if(!tookParam)
            {
                printf("\nERROR: Unexpected option: \"%c\"\n\n",argv[i][j]);
                return (iO->helpLevel=1, -1);
            }
        }
        if (tookParam) i++;
    }
    /* reconcile conflicting and missing options */
    if((iO->doGraphAll != 0) || (iO->doMemory != 0) || (iO->doPIO != 0))
        iO->peerPciTarget = 0x0;
    if(iO->doMemory != 0)
        iO->doPIO = 0;
    if(iO->peerPciTarget != 0x0)
        iO->doProcess = 0;
    /* unless specified, iO->taskCount of 1 for PIO, 2 for DMA */
    if( (iO->taskCount == 0) && iO->doPIO )
        iO->taskCount = 1;
    else if( (iO->taskCount == 0) && !iO->doPIO )
        iO->taskCount = 2;

    if((iO->iterations == 0) || (iO->doGraph == 1))
    {
        iO->iterations = 1000;
        iO->doOneSecond = 1;
    }
    buildOptionsString(iO, optionsString);
    iO->helpLevel=0;
    return 0;
}

/**************************************************************************/
/*  function:     printHelpLevel1                                         */
/*  description:  Print hints and call more help if needed                */
/**************************************************************************/
static void printHelpLevel1( iOptionsType *iO, char *optionsString)
{
    printf( "%s %s\n\n", APP_VER_STRING, APP_VARIANT ); 
    printf("Usage: gttp [-w|-r] [-PEMSVX] [-u unit] [-l packetLength]\n"
           "            [-n iterations]  [-t numOfTasks] [-o offset]\n"
           "            [-s seconds] [-d dataToWrite] [-h]\n"
           "       gttp -G [-w|-r] [-PEMSX] [-u unit] [-o offset]\n\n");
    printf("Defaults: %s\n",optionsString);
#ifdef PLATFORM_VXWORKS
    printf("VxWorks users: enclose options list in a set of quotes \" \".\n");
#endif
    printf("\n");
    if(iO->helpLevel>1)
        printHelpLevel2();
    if(iO->helpLevel>2)
        printHelpLevel3();
}

/**************************************************************************/
/*  function:     printHelpLevel2                                         */
/*  description:  Print more hints                                        */
/**************************************************************************/
static void printHelpLevel2()
{
    printf(
"Options:\n"
"  -u # - board/unit number\n"
"  -w|r - data transfer direction,  -w for write, -r for read        DIR\n"
"  -P   - PIO - use CPU (as opposed to DMA) for data movement          P\n"
"  -E   - do data processing emulation (CPU will touch all data)       E\n"
"  -M   - use system's memory instead of GT's memory                   M\n"
"  -S   - run test for about one second                                S\n"
"  -t # - number of threads/tasks to use                               t\n"
"  -n # - number of iterations (per task) to transfer the packet  ITER/t\n"
"  -l # - packet length in bytes                                    SIZE\n"
"  -o # - offset (address) in the shared GT memory\n"
"  -s # - seconds to repeatedly run the test (-1 for forever)\n"
"  -d # - data value to write to memory (-E overrides)\n"
"  -G   - graph throughput (many packet sizes exercised)\n"
"  -A   - graph all, exercises many transfer settings (only valid with -G)\n"
"  -V   - verbose, print read data\n"
"  -X   - use SYNC (xgttp edition only)\n"
//"  -k # - Align transfer buffers to # bytes above 4K alignment,\n"
//"         0 to 4095 and will be rounded down to a multiple of 4\n"
);
    printf(
"Other reported values:\n"
"     - total number of bytes transfered                       TOT.BYTES\n"
"     - test time in seconds                                        TIME\n"
"     - number of IO operations per second                          IO/s\n"
"     - number of microseconds per IO                              us/IO\n"
"     - throughput in mega (million) bytes per second               MB/s\n");
    printf(
"Example: gttp -u 0                      - use defaults on unit 0\n"
"         gttp -r -l 8 -n 100 -t 3       - read test\n"
"         gttp -w -l 0x100 -n 10000 -t 2 - write test\n"
"         gttp -wG                       - graph write throughput\n");
    printf(
"Notes: Run `gttp -h 1' for more information.\n"
"       Press 'q' to stop a running test.\n");
}

/**************************************************************************/
/*  function:     printHelpLevel3                                         */
/*  description:  Print really detailed hints                             */
/**************************************************************************/
static void printHelpLevel3()
{
    printf(
"SYNOPSIS:\n"
"    Gttp executes performance tests on SCGT hardware. It will read (-r) data\n"
"from or write (-w) data to SCGT shared memory. The data transfer operation\n"
"is performed on a user packet of (-l) bytes and it is repeated (-n) number\n"
"of times by specified (-t) number of threads. The whole process is timed\n"
"and the final results are presented to the user. The results include the\n"
"overall throughput, time required to accomplish a single IO operation\n"
"(here the packet write or read), the number of IO operations per second,\n"
"and additional data which follows requested command options (packet size,\n"
"bytes transferred, etc.).\n"
"\n");
    printf(
"    For valid results, you need to set the number of iterations (-n) such\n"
"that the total test time is not shorter than 1 second. You may use option\n"
"-S instead and gttp will do it for you.\n"
"\n");
    printf(
"    The data transfer on SCGT hardware can be performed with the built-in\n"
"DMA engine (default) or with CPU-driven PIO (-P). By default, gttp does not\n"
"prepare new data set for every write operation, nor does it inspect the\n"
"data in system memory after the completion of read operations. This behavior\n"
"can be changed with (-E) option, instructing gttp to touch every word\n"
"(32-bit) of data before it is written out to SCGT memory or after it is\n"
"read in from SCGT memory.\n"
"\n");
    printf(
"    Gttp writes data to (-w) or reads data from (-r) SCGT memory. The\n"
"actual address within this memory can be specified as (-o) offset. The\n"
"offset 0 is naturally the beginning of SCGT memory. By default, the\n"
"source or destination for transferred data is the system's memory.\n"
"This default behavior can be changed with (-z) physAddress option, where\n"
"physAddress is a physical address as seen from the SCGT PCI bus\n"
"standpoint. This option is only valid for DMA-driven data transfers.\n"
"The user is totally responsible for providing the valid physAddress and\n"
"for any damage caused by non-compliance with this requirement.  Due to\n"
"the high risk nature of this option, it is not being disclosed under\n"
"the \"Usage\" clause. This option can be applied with any other legal\n"
"combination of options except for -P, -M, and -E\n"
"\n");
    printf(
"    Gttp may use a system memory buffer in place of SCGT memory. This (-M)\n"
"option enables testing of the performance of a host's CPU and its memory.\n"
"Presence of data caches and their respective sizes will strongly affect\n"
"the performance for a given packet size.\n"
"\n");
    printf(
"    By default, gttp runs the test on the requested (-l) packet length\n"
"and exits. This behavior can be changed by option (-s) asking for timed\n"
"execution in seconds. When specified, the same test (as specified by all\n"
"other options) will run repeatedly until (at least) the number of seconds\n"
"supplied have passed. The performance results will be printed on every\n"
"repetition of the test. The other way to change the default behavior is to\n"
"apply the (-G) graphing option. In this mode, gttp will execute a series of\n"
"tests varying the packet size, the number of iterations, and the number of\n"
"threads according to some hard-coded rules. The result of each of the\n"
"tests is printed out and enables comprehensive analysis of a system's\n"
"performance.  Option (-S) is valid for use with (-G) and provides quicker\n"
"execution of the graphing option on poorly-performing machines.\n"
"\n");
    printf(
"    During write (-w) operations w/o emulation, gttp fills the first 32-bit\n"
"word of a packet with the iteration number, and each remaining word with the\n"
"value 0xNN555555, where NN is the thread number. Alternately, a data value\n"
"may be specified using option (-d). The value specified will be written to\n"
"the entire packet, including the first word.  The emulation option (-E)\n"
"overrides option (-d).\n"
"\n");
    printf(
"    Gttp does not perform data verification. However, it will report any\n"
"error condition reported by the driver. With option -V it will print the\n"
"data read from SCGT memory thus enabling some network activity monitoring.\n"
"\n");
    printf(
"   Throughput data obtained with gttp provides a good starting point for\n"
"evaluating system performance.  Factors including system load, PCI bus\n"
"performance, host memory performance, and GT network load can have\n"
"considerable impact on overall performance. Analysis of performance under\n"
"expected operating conditions is recommended.\n");
    printf(
"    Gttp does not change any SCGT hardware settings.  Since the performance\n"
"results or even the success of gttp execution may depend on these settings,\n"
"the user should configure the desired hardware settings before executing\n"
"gttp.  The gtmon utility can be used to modity the settings.\n"
"\n");
}

/**************************************************************************/
/****************** Below are just hardware specific things ***************/
/**************** most likely to be removed in final version **************/
/**************************************************************************/

#ifdef HW_SL240
static void printResourceHeader(HAND *pHandle, uint32 unit)
{
    /* prints driver and hardware information */
    char *tempboard = "SL100";
    char *volts = "for 5.0V PCI";
    char *dmode = "(D64)";
    char *model = "?";
    uint32 regVal;
    fxsl_statusstruct status;
    fxsl_configstruct cs;
    
    if (( fxsl_status( *pHandle, &status ) != FXSL_SUCCESS ) ||
        ( fxsl_get_config( *pHandle, &cs ) != FXSL_SUCCESS ))
    {
        printf( "ERROR: Getting status or conf for Unit %d failed\n",unit);
        return;
    }
    
    if ((status.revisionID & 0x0080) == 0x80)
        tempboard = "SL240";
    if ((status.revisionID & 0x0040) == 0x0040)
        volts = "for 3.3V PCI";
    
    printf( "Driver:   rev. %s for %s (API rev. %s) \n",
        status.driver_revision_str, OS_VER_STRING, CORE_VER_STRING );
    
    fxsl_read_CR(*pHandle, 0x4, &regVal); /* extended rev and D32/D64 info */
    if (regVal & 0x02000000)
        dmode = "(D32)";
    
    if (((regVal >> 22) & 0x3) == 0x0)        /* sl100/240 standard model */
        model = "\0";
    else if (((regVal >> 22) & 0x3) == 0x1)          /* sl100/240 model x */
        model = "x";
    
    printf( "Hardware: unit/bus/slot %d/%d/%d %s%s %s Firm. %X.%x (%X.%x) %s \n",
        unit,status.nBus,status.nSlot, tempboard,model, dmode,
        status.revisionID & 0x3F,(regVal>>16)&0xff,status.revisionID,
        (regVal>>16)&0xff, volts);
    return;
}
#elif defined HW_SCGT

/***************************************************************************/
/* Macro:  gtDiffCounter                                                   */
/* Description: Calculates difference between current counter value and    */
/*    previous counter value, accounting for (at most 1) roll-over.        */
/***************************************************************************/
#define gtDiffCounter(currCnt, prevCnt, maxCnt) \
           (currCnt>=prevCnt)?(currCnt-prevCnt):(currCnt+(maxCnt-prevCnt)+1)

/***************************************************************************/
/* Function:    getSpeedInfo()                                             */
/* Description: Calculate info related to linkSpeed and PCI frequency      */
/***************************************************************************/
static void getSpeedInfo(HAND *pHandle, uint32 *linkMpbs, uint32 *pciMHz)
{
    #define NET_CLK_CNT_REG  0x40
    #define PCI_CLK_CNT_REG  0x44
    usysMsTimeType curTime;
    uint32 oscTicks[2], pciTicks[2];
    uint32 startMS1, startMS2, stopMS1, stopMS2, elapsedMS;
    uint32 val, linkSpeed, pciSpeed;

    usysMsTimeStart(&curTime);
    usysMsTimeDelay(1);

    /* read counters */
    startMS1 = usysMsTimeGetElapsed(&curTime);
    oscTicks[0] = scgtReadCR(pHandle, NET_CLK_CNT_REG);
    pciTicks[0] = scgtReadCR(pHandle, PCI_CLK_CNT_REG);
    startMS2 = usysMsTimeGetElapsed(&curTime);

    usysMsTimeDelay(80); /* may need to raise if too inacurrate */
    /* Don't go lower than 80. MS Windows tested inacurrate at 50. */

    /* read counters */
    stopMS1 = usysMsTimeGetElapsed(&curTime);
    oscTicks[1] = scgtReadCR(pHandle, NET_CLK_CNT_REG);
    pciTicks[1] = scgtReadCR(pHandle, PCI_CLK_CNT_REG);
    stopMS2 = usysMsTimeGetElapsed(&curTime);

    /* try for accurate elapsed time */
    elapsedMS = (stopMS2 + stopMS1)/2 - (startMS2 + startMS1)/2;

    if( elapsedMS == 0 ) /* prevent divide-by-zero */
        elapsedMS = 1;

    usysMsTimeStop(&curTime);

    oscTicks[0] = gtDiffCounter(oscTicks[1], oscTicks[0], 0xFFFFFFFF);
    pciTicks[0] = gtDiffCounter(pciTicks[1], pciTicks[0], 0xFFFFFFFF);

    /* calculate link speed related info */
    oscTicks[0] *= 20;
    val = (oscTicks[0]/elapsedMS)/1000; /* Mbps */

    /* ~+-200 Mbps from actual, short delay used for calculation atop system
      scheduling yields inaccurate results*/
    if( (val >= 860) && (val < 1260) )
        linkSpeed = 1000; /* 1062.5 Mbps actual */
    else if( (val > 1925) && (val < 2300) )
        linkSpeed = 2000; /* 2125 Mbps actual */
    else if( (val >= 2300) && (val < 2700) )
        linkSpeed = 2500; /* 2500 Mbps actual */
    else
        linkSpeed = 0; /* Error */

    /* calculate PCI bus frequency */
    val = (pciTicks[0]/elapsedMS)/1000; /* MHz */

    if( (val > 23) && (val < 43) )
        pciSpeed = 33; /* MHz */
    else if( (val > 56 ) && (val < 76) )
        pciSpeed = 66; /* MHz */
    else
        pciSpeed = 0; /* MHz, error */

    *linkMpbs = linkSpeed;
    *pciMHz = pciSpeed;
}

static void printResourceHeader(HAND *pHandle, uint32 unit)
{
    scgtDeviceInfo devInfo;
    uint32 bir = scgtReadCR(pHandle, 0);
    uint32 val;
    char * str;
    uint32 linkMpbs, pciMHz;

    if (scgtGetDeviceInfo(pHandle, &devInfo))
    {
        printf("scgtGetDeviceInfo failed, unit %d\n", unit);
        return;
    }

    getSpeedInfo(pHandle, &linkMpbs, &pciMHz);

    printf("\n");

    val = (bir & 0x6000) >> 13;
    printf("%s ", ((val==0x0) ? "GT100" : ((val==0x1) ? "GT200" : "GT???")));

    val = (bir & 0x1C00) >> 10;

    if( val == 0x0 )
        str = "33MHz 64bit 5V";
    else if( val == 0x1 )
        str = "33MHz 64bit 3.3V";
    else if( val == 0x2 )
        str = "66MHz 64bit 3.3V";
    else
        str = "unknown";

    printf("unit %i %s - %s PCI compatible\n", devInfo.unitNum,
            devInfo.boardLocationStr, str );

    val = (bir & 0xC0) >> 6;

    if( val == 0x0 )
        str = "ZBT SRAM";
    else if( val == 0x1 )
        str = "DDR SDRAM";
    else
        str = "unknown";

    val = scgtGetState(pHandle, SCGT_D64_ENABLE);

    printf("Memory %iMB(PIO)/%iMB(POP) %s - Link %d.%1d Gbps - PCI %d MHz"
           " - D64 %s\n",
            devInfo.mappedMemSize/1048576, devInfo.popMemSize /1048576, str,
            linkMpbs/1000, (linkMpbs/100)%10, pciMHz, (val? "on" : "off"));
    printf("Driver %s - API %s - Firmware %X.%.2X\n\n",
            devInfo.driverRevisionStr, 	scgtGetApiRevStr(),
            devInfo.revisionID >> 8, devInfo.revisionID & 0xFF);
}

#else /* Any other hardware abstraction */
static void printResourceHeader(HAND *pHandle, uint32 unit){}
#endif
