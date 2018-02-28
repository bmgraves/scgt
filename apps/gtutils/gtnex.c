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

#define APP_VER_STRING "GT Network Exerciser (gtnex) rev. 1.03 (2011/08/30)"

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
                                                                        /**/
#elif PLATFORM_VXWORKS                                                  /**/
    #undef PARSER                                                       /**/
    #undef MAIN_IN                                                      /**/
    #define VXL     40                                                  /**/
    #define MAIN_IN int APP_VxW_NAME(char *cmdLine) { \
            int argc; char argv0[]="gtnex"; char *argv[VXL]={argv0};    /**/
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

#ifdef HW_SCFF /* Scramnet */
    #include "scrplus.h"
    #define HAND uint32
    #define APP_VxW_NAME sgtnex
#else /* GT */
    #define HW_SCGT
    #include "scgtapi.h"
    #define HAND scgtHandle
    #define APP_VxW_NAME gtnex
#endif

/**************************************************************************/
/************** APPLICATION-SPECIFIC CODE STARTS HERE *********************/
/**************************************************************************/

/**************************************************************************/
/**************************  D A T A  T Y P E S  **************************/
/**************************************************************************/

#define INPUT_OPTIONS \
    uint32    doPIO, doMemory, doTerminate, doQuickMemoryTest, boostRatio; \
    uint32    packetSize, secondsToRun, unit, helpLevel, mId, displayMode;

#define COMMON_OPTIONS \
    uint32    rmSize, netTime, errHwAll, errGtAll, seenCnt, segCnt; \
    uint32    sErrHw, errHwMy;

typedef struct {
    uint32    timeToExit;
    uint32    running;
    void      *iO; /*iOptionsType*/
    usysThreadParams   pt;
} exitThreadParms;

typedef struct {
    INPUT_OPTIONS
    COMMON_OPTIONS
    uint32   *aPatVer;     /* array of patterns used in verification */
    uint32   *aPatSend;    /* array of patterns used when sending */
    uint32   *bDma;        /* buffer for some DMA operations */
    uint32   *aStat;       /* array of statistics */
    uint32   *bRpt1;       /* buffer for reports (statistics) */
    uint32   *bRpt2;       /* swap for the above */
    uint32   *aSeen;       /* array of seen members/players Id */
    volatile uint32  *aRm; /* "Replicated" memory - real or emulated */
    volatile uint32  *boardAddressPIO;   /* actuall pointer to it if real */
    exitThreadParms  exitParms;  /* info for keyboard/exit thread */
    HAND  hDriver;
} iOptionsType;

typedef struct {
    uint32 nothing; /* to appease MSVC */
/*    nothing here at the moment */
} oResultsType;


/**************************************************************************/
/******************  F U N C T I O N  P R O T O T Y P E S  ****************/
/**************************************************************************/
static void parseCommand(int argc, char **argv, iOptionsType *iO, char *optionsString);
static void printHelpLevel1(iOptionsType *iO, char *optionsString);
static void printHelpLevel2();
static void printHelpLevel3();
static void *getRuntimeChar(void *voidp);
static void buildOptionsString(iOptionsType *iO, char *optionsString);
static int  gtnexWorker(iOptionsType *iO);
static int  gtnexBoss(iOptionsType *iO, oResultsType *oR);
static int  gtnexQuickMemoryTest(iOptionsType *iO);
static void gtReport(iOptionsType *p);

#define gtnexErrorPrint(x)     printf("ERROR: code %d\n",x)

/**************************************************************************/
/***********************  D E F I N E S & M A C R O S  ********************/
/**************************************************************************/

#define SEND        1
#define RECEIVE     0

#define MmId      256    /* maximum number of members/players - do not change */
#define EMULATED_SW  (0x100000 / 4) /* For running tests in system memory */

/* Replicated memory layout: */
/* postfix _SW (_SB) used for Size in 32-bit Words (Bytes) */
/* postfix _OW (_OB) used for 32bit-Word (byte) Offset  */

/* First 128kB reserved for concurrent run of other applications. */
/* gtnex interest starts at: */
#define A0_NAME   "A0 - Game field start:  "
#define A0_OB     (128*0x400)
#define A0_OW     (A0_OB/4)
#define A0_SW     0x400            /* TERMINATE in first location */

#define A1_NAME   "A1 - Reports area:      "
#define A1_OB     (A0_OB + A0_SW*4)
#define A1_OW     (A1_OB/4)
#define A1P_SB    16*4                         /* for each member/player */
#define A1P_SW    (A1P_SB/4)
#define A1_SB     (A1P_SB * 256)              /* for all members/players */
#define A1_SW     (A1_SB/4)

#define A2_NAME   "A2 - Unasigned:         "
#define A2_OB     (A1_OB + A1_SW*4)
#define A2_OW     (A2_OB/4)
#define A2_SW     0

#define A3_NAME   "A3 - Unasigned:         "
#define A3_OB     (A2_OB + A2_SW*4)
#define A3_OW     (A3_OB/4)
#define A3_SW     0

#define A4_NAME   "A4 - SEQ patterns area: "
#define A4_OB     (A3_OB + A3_SW*4)
#define A4_OW     (A4_OB/4)
#define A4P_SW    128                          /* for each member/player */
#define A4P_SB    (A4P_SW*4)
#define A4_SW     (A4P_SW * 256)              /* for all members/players */

#define A5_NAME   "A5 - Static Random area:"
#define A5_OB     (A4_OB + A4_SW*4)
#define A5_OW     (A5_OB/4)
#define A5P_SB    (37*0x400)                  /* segment size, each player */
#define A5P_SW    (A5P_SB/4)
#define A5_SW     (A5P_SW * 256)              /* for all members/players */

/* prefix S_ stands for statistics array-related define */
#define S_RUN           0
#define S_ERR_GT        1
#define S_TIME          2
#define S_BYTE_NOW      3
#define S_GBYTE         4
#define S_BYTE          5
#define S_HW_ID         6
#define S_HW_ID_UP      7
#define S_NET_LEN       8
#define S_ERR_HW        9
#define S_SEEN_CNT     10

#define NET_UPDATE_PERIOD        1                    /* seconds */

#define TERMINATE       0xfedcfedc
#define RESTART         0xf1f1f1f1
                              /* display modes - positionally coded */
#define DM_ERR   1     /* error details */
#define DM_CONF  2     /* network view updated on changes and errors */
#define DM_SUM   4     /* summary (TOTAL) line updated every second */

/* The following is to mask the specific hardware dependency */

#ifdef HW_SCFF /* Scramnet */
    #define APP_VARIANT "for SCRAMNet (sgtnex)"
    #define DRIVER_SUCCESS 0
    #define doPIO_DISABLED 0
    #define doDMA_DISABLED 0
    #define DRIVER_OPEN(x,y) sp_scram_init(0); \
              sp_stm_mm(Long_mode); \
              scr_csr_write(8,(unsigned short)(scr_csr_read(8) & 0xf800)); /* Counting errors */ \
              scr_csr_write(9,0x01a8); /* Counting errors */ \
              scr_csr_write(13, 0x0); /* clear counter */  \
              scr_csr_write(2, (unsigned short) 0xd040); \
              scr_csr_write(0, (unsigned short) 0x8003); \
              scr_csr_write(10,(unsigned short)(scr_csr_read(10) | 0x11)); \
              iO.boardAddressPIO = (volatile uint32 *)get_base_mem(); \
              iO.rmSize = EMULATED_SW; /* 32-bit words */ \
              scr_csr_write(3, (unsigned short)(iO.mId<<8)); /*temp node id */
    #define DRIVER_CLOSE(x)
    #define GET_HW_ID     (scr_csr_read(3)>>8)
    #define GET_HW_ID_UP  (99)
    #define GET_NET_LEN   (scr_csr_read(3) & 0xFF)
    #define GET_NET_ERR   (scr_csr_read(13))

    #define DMA_WRITE_GT(pHandle,smOffset,pSource,sizeByte,flags,pBytesWritten)\
                (scr_dma_write(pSource, smOffset, sizeByte), \
                 *pBytesWritten = sizeByte, DRIVER_SUCCESS)
    #define DMA_READ_GT(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead) \
                (scr_dma_read(pDest, smOffset, sizeByte), \
                 *pBytesRead = sizeByte, DRIVER_SUCCESS)

    #define ERRORDUMP  {}
#elif defined HW_SCGT /* GT */
    #define APP_VARIANT ""
    #define DRIVER_SUCCESS SCGT_SUCCESS
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
            iO.rmSize = devInfo.mappedMemSize / 4; /* 32-bit words */ }

    #define DRIVER_CLOSE(x) { scgtUnmapMem(&(x)); scgtClose(&(x)); }
    #define GET_HW_ID     (scgtGetState(&(iO->hDriver), SCGT_NODE_ID))
    #define GET_HW_ID_UP  (scgtGetState(&(iO->hDriver), SCGT_UPSTREAM_NODE_ID))
    #define GET_NET_LEN   (scgtGetState(&(iO->hDriver), SCGT_RING_SIZE))
    #define GET_NET_ERR   (scgtGetState(&(iO->hDriver), SCGT_NUM_LINK_ERRS))

    #define DMA_WRITE_GT(pHandle,smOffset,pSource,sizeByte,flags,pBytesWritten)\
                scgtWrite(pHandle,smOffset,pSource,sizeByte,flags, \
                          pBytesWritten, NULL)
    #define DMA_READ_GT(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead) \
                scgtRead(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead)

    #define ERRORDUMP {\
        if((aStat[S_ERR_HW] - iO->errHwMy) != 0) \
            printf("ERR_UpdateD: LD/SY/DE/CR/LE/EE/PR/OV %d %d %d %d %d %d %d %d at %d sec nId=%d\n",\
            scgtReadCR(&iO->hDriver, 0x84), scgtReadCR(&iO->hDriver, 0x8C),\
            scgtReadCR(&iO->hDriver, 0x88), scgtReadCR(&iO->hDriver, 0x90),\
            scgtReadCR(&iO->hDriver, 0x80), scgtReadCR(&iO->hDriver, 0x94),\
            scgtReadCR(&iO->hDriver, 0x98), scgtReadCR(&iO->hDriver, 0x9C), iO->netTime,iO->mId); }
#endif


/****** Random Number Generator - Copyright (c) 1991 Leszek D. Wronski ********/
#define irdn6(a,x) {int _irdP=a[63]; _irdP=(_irdP+1)%63; (x)=(x)-a[_irdP];\
    if((x)<0) (x)=(x)+2147483646; a[_irdP]=(x);a[63]=_irdP;}
#define irdn6_vars(a) long int a[64] = {67,111,112,121,114,105,103,104,116,32,\
  40,99,41,32,49,57,57,49,44,32,76,101,115,122,101,107,32,68,46,32,87,114,111,\
  110,115,107,105,32,32,1775811865,893145566,995405759,2102736524,271520213,\
  456633834,2058258651,1583689592,878442065,1675280054,1627576247,2054802212,\
  60335757, 684462146, 1625503827, 521827728,1266032265, 93253262, 2053047983,\
  685333180, 127474245, 948921754, 1964990155, 1228891816,0}
/*Usage: instantiate irdn6_vars and some long int randvector. To randomize
 starting point you may put some value into randvector . Call irdn6(randvector);
 - new random value will be placed in randvector - use it but do not change it*/
/********************* Random Number Generator - END **************************/


/**************************************************************************/
/*************************** G T T P   C O D E ****************************/
/**************************************************************************/

MAIN_IN                                                     /* gtnex main */
    iOptionsType   iO;
    oResultsType   oR;
    int            j;
    char           optionsString[160];
    uint8*         tp;

    PARSER;    /* preparing argc and agrv if not provided by OS (VxWorks) */

    tp=(uint8 *)&iO;
    for(j=0;j < sizeof(iOptionsType);j++)        /* clear all the options */
        tp[j]=0;

    parseCommand(argc, argv, &iO, optionsString);      /* parsing options */

    if(iO.helpLevel!=0) /*1, 2, 3, ...*/
    {
        printHelpLevel1(&iO, optionsString);
        return 0;
    }

    DRIVER_OPEN(iO.unit, (HAND *) &iO.hDriver);
    printf("Running: %s\n",optionsString);
    j=gtnexBoss (&iO, &oR);
    printf("Total of %d errors\n",j);
    DRIVER_CLOSE(iO.hDriver);
    return( 0 );
}                                         /* end of MAIN_IN, a.k.a. main */


/*************************************************************************/
/*  function:     gtnexBoss                                              */
/*  description:  memory allocation, initialization, and call the worker */
/*************************************************************************/
static int gtnexBoss (iOptionsType *iO, oResultsType *oR)
{
    uint32   i, j;
    volatile uint32 *aRm = (volatile uint32 *)iO->boardAddressPIO;
    uint32  *aPatVer     = NULL;                              /* DMA */
    uint32  *aPatSend    = NULL;                              /* DMA */
    uint32  *bDma        = (uint32*)usysMemMalloc(A5P_SB+3*4096);    /* DMA */
    uint32  *bRpt1       = (uint32*)usysMemMalloc(A1_SB);     /* DMA */
    uint32  *bRpt2       = (uint32*)usysMemMalloc(A1_SB);     /* DMA */
    uint32  *aStat       = (uint32*)malloc(A1P_SB);
    uint32  *aSeen       = (uint32*)malloc(256 * 4);
    uint32  *aRmEmulated = (uint32*)malloc(EMULATED_SW * 4);
    uint32   aPatSize;
    long int randvector;
    irdn6_vars(gen1);

    randvector = (13 & 0xeFFFeFFF);

    /* align to 4K boundary */
    iO->bDma = (uint32*)(((uintpsize)bDma + 4095) & ~((uintpsize)4095));    
    iO->bRpt1 = bRpt1;
    iO->bRpt2 = bRpt2;
    iO->aStat = aStat;
    iO->aSeen = aSeen;

    if(iO->mId == ~0) /* means not specified */
        iO->mId = GET_HW_ID;

    /*make sure that iO->rmSize comes in correctly !!!!*/
    if((iO->doMemory != 0)||(iO->boardAddressPIO==NULL)||(iO->rmSize==0))
    {
       iO->doMemory = 1;
       iO->doPIO    = 1;
       iO->rmSize   = EMULATED_SW;                      /* in 32-words */
       aRm          = (volatile uint32 *)aRmEmulated;
    }
    iO->aRm =aRm;

    if(iO->doTerminate != 0)           /* test restart or termination option */
    {
        if(iO->doTerminate == 1)
            aRm[A0_OW] = TERMINATE;
        else
            aRm[A0_OW] = RESTART;
        printf("Test termination or restart request sent\n");
        j=0;
        goto end_it;
    }

    if(iO->doQuickMemoryTest != 0)
    {
        j=gtnexQuickMemoryTest(iO);                   /* run memory test */
        goto end_it;
    }

    iO->segCnt   = (iO->rmSize * 4 - A5_OB ) / A5P_SB;
    aPatSize     = A5P_SW + 256 + iO->segCnt;
    aPatVer      = (uint32*)malloc(aPatSize * 4);
    aPatSend     = (uint32*)usysMemMalloc(aPatSize * 4);          /* DMA */
    iO->aPatVer  = aPatVer;
    iO->aPatSend = aPatSend;
                                                 /* show all the goodies */
    printf("Memory Size:  0x%X bytes\n", iO->rmSize *4);
    printf("%s 0x%7.7X\n", A0_NAME, A0_OB);
    printf("%s 0x%7.7X (0x%7.7X for mId %d)\n",
            A1_NAME, A1_OB, A1_OB + iO->mId* A1P_SB, iO->mId );
    printf("%s 0x%7.7X (0x%7.7X) - 0x%X Bytes per member\n",
            A4_NAME, A4_OB, A4_OB + iO->mId*A4P_SB, A4P_SB);
    printf("%s 0x%7.7X (%d segments of 0x%X bytes each)\n",
            A5_NAME, A5_OB, iO->segCnt, A5P_SB);

    if((aPatVer==NULL) || (aPatSend==NULL) || (bDma==NULL) || (aStat==NULL) ||
       (aRmEmulated==NULL) || (bRpt1==NULL)|| (bRpt2==NULL))
    {
        printf("ERROR: not enough system memory !!! \n");
        j=1;
        goto end_it;
    }

    aRm[A0_OW] = ~TERMINATE;

    /* we are going to play the game! - initialize remaining stuff       */

    iO->sErrHw = GET_NET_ERR;   /* get the initial values of hw counters */
    iO->errHwMy = 1;     /* to display initial values for error counters */
    ERRORDUMP;
    for (i = 0; i < aPatSize; i++)                /* prepare the pattern */
    {
        irdn6(gen1,randvector);
        aPatVer[i]  = (randvector<< 8);
        aPatSend[i] = aPatVer[i] | iO->mId;
    }
    for (i = 0; i < iO->segCnt; i++)          /* initialize A5 GT memory */
    {                                                   /* pretend mId=0 */
        for (j = 0; j < A5P_SW; j++)
            aRm[A5_OW + i*A5P_SW +j] = aPatVer[j + i + 0];
    }
    for (j = 0; j < A4P_SW; j++)              /* initialize A4 GT memory */
        aRm[A4_OW + iO->mId*A4P_SW + j] = 0;

    for(j=1;j<A1P_SW;j++)               /* clear our local and net stats */
    {
        aStat[j]=0;
        aRm[A1_OW + iO->mId*A1P_SW + j] = 0;
    }
    aStat[0]=0;             /* do not include this one in the above loop */
    for(j=0;j< A1_SW;j++)                      /* get stats from the net */
    {
        bRpt1[j]    = aRm[A1_OW + j];
        bRpt2[j] = bRpt1[j];
    }

    /* spawn exit thread */
    iO->exitParms.pt.parameter = &iO->exitParms;
    iO->exitParms.pt.priority = UPRIO_HIGH; /* only use UPRIO_HIGH */
    sprintf(iO->exitParms.pt.threadName, "gtnexExit");
    iO->exitParms.timeToExit = iO->exitParms.running = 0;
    iO->exitParms.iO = iO;
    usysCreateThread(&iO->exitParms.pt, getRuntimeChar);
    /* Give thread a moment to start up. Needed on VxWorks, Solaris 8 */
    usysMsTimeDelay(50);

    j=gtnexWorker(iO);                                 /* start the game */

    if( iO->exitParms.running )
    {
        usysKillThread(&iO->exitParms.pt);
    }

end_it:
    if(aRmEmulated !=NULL) free(aRmEmulated);
    if(aStat !=NULL)       free(aStat);
    if(aSeen !=NULL)       free(aSeen);
    if(bRpt1 !=NULL)       usysMemFree(bRpt1);
    if(bRpt2 !=NULL)       usysMemFree(bRpt2);
    if(aPatVer !=NULL)     free(aPatVer);
    if(bDma !=NULL)        usysMemFree(bDma);
    if(aPatSend !=NULL)    usysMemFree(aPatSend);
    return(j);
}

/**************************************************************************/
/*  function:     gtnexWorker                                             */
/*  description:  test body                                               */
/**************************************************************************/
static int gtnexWorker(iOptionsType *iO)
{
    uint32            i,j;
    uint32           *aPatV;
    volatile uint32  *aRmV;
    volatile uint32  *aRmRpt =(volatile uint32 *)&iO->aRm[A1_OW+iO->mId*A1P_SW];
    uint32           *bDma   = (uint32 *)iO->bDma;
    uint32            doPIO  = iO->doPIO;
    uint32            mId    = iO->mId;                 /* our member's Id */
    uint32            oId    = 0;      /* some Other member's/player's Id */
    uint32            tW;                            /* Words to transfer */
    uint32            ret         = DRIVER_SUCCESS;
    uint32            inRmO, inSegO, inPatO; /* O stands for offsets here */
    uint32            seg;
    uint32            df, dn;                    /* data first, data next */
    uint32            bytesReported;
    time_t            nowTime, startTime, pastTime, testTime=0;


    long int randvector;
    irdn6_vars(gen1);


    time( &startTime );
    pastTime=startTime;
    nowTime=startTime;
    randvector=startTime & 0xeFFFeFFF;
    for (i = 0; !iO->exitParms.timeToExit; i++) /****** test loop start ******/
    {
#ifdef PLATFORM_VXWORKS
        if((i%10) == 0)
            usysMsTimeDelay(0);                /* be nice to others*/
#endif
        irdn6(gen1,randvector);
        /*randomize it more latter - more generators or different bits*/
        seg    = randvector % iO->segCnt;                /* pick the segment */
        inSegO = (randvector >> 3) % (A5P_SW-1);    /* pick the offset in it */
        if((i%32) != 0)
            inSegO = inSegO & 0xfffffffe;    /* make DMA possibe - to be removed when fixed*/
        tW     = (randvector >> 4) % (A5P_SW - inSegO); /* pick tranfer size */
        tW  %= (iO->packetSize >> 2);
        if(tW == 0) tW++;
        inRmO  = A5_OW + inSegO + A5P_SW * seg; /* transform to rm offset */
        inPatO = inSegO + seg;                /* and pattern array offset */

        aRmRpt[0]=i;                       /* let others know we are here */

        /**************************** SEQ test ***************************/
        if(iO->seenCnt != 0 && (i % iO->boostRatio) == 0 ) /*SEQ test - verify*/
        {
            oId  = iO->aSeen[(randvector >> 1) % iO->seenCnt]; /* pick a member/player */
            aRmV = &iO->aRm[A4_OW + oId * A4P_SW];
            df   = aRmV[0];         /* read the first (reference) pattern */
            for(j=1; j < A4P_SW; j++)   /* scan and check the rest of A4P */
            {
                dn = aRmV[j];
                if(dn == df)                /* should be most of the time */
                {
                }
                else if(dn > df)                      /* possible overrun */
                {
                    df=aRmV[j-1];                    /* correct reference */
                    if(dn > df)             /* definitelly error unless 0 */
                    {
                        iO->aStat[S_ERR_GT]++;
                        if((iO->displayMode & DM_ERR) != 0)
                        {
                            printf("\nERR_A4 o=%7.7X d=%8.8X w=%8.8X v=%8.8X",
                                (A4_OW+oId*A4P_SW+j)*4,dn,df,dn^df);
                            printf(" l=%8.8X mId=%d wId=%d t=%d",
                                df-dn, iO->mId, oId, iO->netTime);
                            fflush(stdout);
                        }
                    }
                }
                else                      /* (dn < df) - possible underun */
                {
                    if((df-dn) !=1)         /* definitelly error unless 0 */
                    {
                        iO->aStat[S_ERR_GT]++;
                        if((iO->displayMode & DM_ERR) != 0)
                        {
                            printf("\nERR_A4 o=%7.7X d=%8.8X w=%8.8X v=%8.8X",
                                (A4_OW+oId*A4P_SW+j)*4,dn,df,dn^df);
                            printf(" l=%8.8X mId=%d wId=%d t=%d",
                                df-dn, iO->mId, oId, iO->netTime);
                            fflush(stdout);
                        }
                    }
                    df=dn;                       /* correct the reference */
                }
            }
        }

        aRmV = &iO->aRm[A4_OW + mId * A4P_SW];   /* SEQ - send our pattern */
        for(j=0;j<A4P_SW; j++)                            /* PIO writing to rm */
            aRmV[j]=i;

        /********************** Static Random test ***********************/
        aRmV    = &iO->aRm[inRmO];

        if((i % iO->boostRatio) == 0)
        {
            /* printf("basked=%8.8X offset=%8.8X %d\n", tW*4,inRmO * 4 , inRmO % 2 ); */
            /* mod 2 is a work around for firmware bug (64 bit aligment offset need) */
            /* 128 condition is to avoid very short and inefficient DMAs */
            if((doPIO == 1) || ((inRmO % 2) != 0) || (tW*4) < 128)    /* PIO reading rm */
            {
                for(j=0; j < tW; j++)
                    bDma[j] = aRmV[j];
            }
            else                                        /* DMA reading rm */
            {
                bDma[tW-1]=0x65432100;                 /* mark DMA buffer */
                bytesReported = 0;
                ret = DMA_READ_GT(&iO->hDriver, inRmO * 4, (uint8 *)bDma,
                                  tW*4, 0, &bytesReported);
                if( (ret != DRIVER_SUCCESS) || (bytesReported != tW*4) )
                    printf("DMA read failed!!! ret=%8.8X brep=%8.8X basked=%8.8X offset=%8.8X\n",
                    ret,bytesReported,tW*4,inRmO * 4 );
            }

            aPatV = &iO->aPatVer[inPatO+oId];
            for(j=0;j < tW; j++)                           /* verify data */
            {
                if((aPatV[j] ^ (bDma[j] & ~0xFF)) != 0)
                {
                    oId=bDma[j] & 0xFF;      /* picking seen Id  - adjust */
                    aPatV = &iO->aPatVer[inPatO+oId];
                    if((aPatV[j] ^ (bDma[j] & ~0xFF)) != 0)      /* error */
                    {
                        iO->aStat[S_ERR_GT]++;
                        if((iO->displayMode & DM_ERR) != 0)
                        {
                            printf("\nERR_A5 o=%7.7X d=%8.8X w=%8.8X v=%8.8X",
                               (inRmO+j)*4, bDma[j], aPatV[j] | (bDma[j] & 0xFF),
                               (bDma[j]^aPatV[j]) & ~0xFF);
                            printf(" mId=%d wId=%d t=%d ec=%d", iO->mId,bDma[j]&0xFF,
                                iO->netTime,iO->aStat[S_ERR_GT]);
                            fflush(stdout);
                        }
                    }
                }
            }
        }

        /* write our patterns to the same rm place */
        aPatV = &iO->aPatSend[inPatO+mId];
        /* do not remove 1 condition before concurrent DMA and PIO writes is fixed on firmware
           and gtnex code is corrected to secure proper aligments.
           The following (after "1" removal) is just for testing in house.
        */
        if(1 || doPIO == 1 || ((inRmO % 2) != 0) || ((tW*4) < 128) || (((uintpsize)aPatV & 0xf) != 0) )  /* PIO writing to rm */
        {
            for(j=0;j < tW; j++)
                aRmV[j]=aPatV[j];
        }
        else                                         /* DMA writing to rm */
        {
            bytesReported = 0;
            ret = DMA_WRITE_GT(&iO->hDriver, inRmO * 4, (uint8 *)aPatV,
                               tW*4, 0, &bytesReported);

            if( (ret != DRIVER_SUCCESS) || (bytesReported != tW*4) )
                 printf("DMA write failed!!! ret=%8.8X brep=%8.8X basked=%8.8X offset=%8.8X\n",
                         ret,bytesReported,tW*4,inRmO * 4 );

        }

        /******** update sent data counters to reflect all above tests ***/
        iO->aStat[S_BYTE_NOW] += A4P_SW + tW;

        /******* process network statistics if it is time to do so *******/
        time( &nowTime );
        if((nowTime - pastTime) >= NET_UPDATE_PERIOD)  /* time to report */
        {
            testTime=nowTime-startTime;
            pastTime=nowTime;
            iO->aStat[S_TIME]=testTime;
            gtReport(iO);
            if(iO->aRm[A0_OW] == TERMINATE)
            {
                iO->aRm[A0_OW] = TERMINATE;          /* just in case ... */
                printf("\nTerminate request received - quitting\n");
                break;
            }
            if(iO->aRm[A0_OW] == RESTART)
            {
                iO->aRm[A0_OW] = RESTART;          /* just in case ... */
                printf("\nRestart request received - waiting\n");
                usysMsTimeDelay(2000);
                iO->aRm[A0_OW] = ~RESTART;
                for(j=1;j<A1P_SW;j++)               /* clear our local and net stats */
                {
                    iO->aStat[j]=0;
                }
                iO->aStat[0]=0;             /* do not include this one in the above loop */
                time( &startTime );
                usysMsTimeDelay(1000);
                printf("Continue\n");
            }

            if((uint32)testTime >= iO->secondsToRun)
                break;
            iO->aStat[S_BYTE_NOW]=0;           /* clear short term stats */
        }
    }                                         /****** test loop end ******/
    return(iO->errGtAll + iO->errHwAll);
}

/**************************************************************************/
/*  function:     gtReport                                                */
/*  description:  read, compute, write statistics to the net and screen   */
/**************************************************************************/
static void gtReport(iOptionsType *iO)
{
    uint32            j, k;
    volatile uint32  *aRmRpt = (volatile uint32 *)&iO->aRm[A1_OW];
    uint32            seenCnt;
    uint32            dRate,        dRateNow;
    uint32            dRateAll = 0, dRateAllNow=0;
    uint32            errGtAll = 0, errHwAll = 0;
    uint32           *bRpt1    = iO->bRpt1;
    uint32           *bRpt2    = iO->bRpt2;
    uint32           *aStat    = iO->aStat;
    uint32           *aSeen    = iO->aSeen;
    uint32            update   = 0;
    time_t            atime;

    time(&atime);

    aStat[S_BYTE] += aStat[S_BYTE_NOW];           /* calculate throughput */
    if (aStat[S_BYTE] >= (1000000000 / 4))
    {
        aStat[S_GBYTE] += (aStat[S_BYTE] / (1000000000 / 4));
        aStat[S_BYTE] %= (1000000000 / 4);
    }

    aStat[S_BYTE] *=4;                                 /* change to bytes */
    aStat[S_BYTE_NOW] *=4;

    aStat[S_HW_ID]    = GET_HW_ID;         /* get hardware reported items */
    aStat[S_HW_ID_UP] = GET_HW_ID_UP;
    aStat[S_NET_LEN]  = GET_NET_LEN;
    aStat[S_ERR_HW]   = GET_NET_ERR;  aStat[S_ERR_HW] -=iO->sErrHw;
    aStat[S_NET_LEN]  = GET_NET_LEN;

    k=iO->mId * A1P_SW;
    for(j=1; j<A1P_SW; j++) /* place our stats on the net */
        aRmRpt[j+k] = aStat[j];

    aStat[S_BYTE] /=4;                            /* change back to words */
    aStat[S_BYTE_NOW] /=4;
    for(j=0;j< A1P_SW*256;j++)              /* getting stats from the net */
        bRpt1[j]=aRmRpt[j];

    for(j=0,seenCnt=0; j< 256; j++)                  /* who is out there? */
        if(bRpt1[j*A1P_SW+S_RUN] != bRpt2[j*A1P_SW+S_RUN])
        {
            aSeen[seenCnt]=j;
            seenCnt++;
        }

    for(j=0;j < seenCnt; j++)                  /* process net information */
    {
        k=aSeen[j]*A1P_SW;
        errGtAll += bRpt1[k+S_ERR_GT];
        errHwAll += bRpt1[k+S_ERR_HW];
        if(bRpt1[k+S_TIME] == 0)
                bRpt1[k+S_TIME]=9999999;
        if(j==0)
            iO->netTime = bRpt1[k+S_TIME]; /* lowest Id member makes time */

        dRate = (bRpt1[k+S_GBYTE]*1000*100)/ bRpt1[k+S_TIME] +
                (bRpt1[k+S_BYTE] / bRpt1[k+S_TIME]) / 10000;
        dRateAll += dRate;
        dRateNow = (bRpt1[k+S_BYTE_NOW] / NET_UPDATE_PERIOD) / 10000;
        dRateAllNow += dRateNow;
        if(bRpt1[k+S_HW_ID] != bRpt2[k+S_HW_ID] ||
           bRpt1[k+S_HW_ID_UP] != bRpt2[k+S_HW_ID_UP] ||
           bRpt1[k+S_NET_LEN] != bRpt2[k+S_NET_LEN] ||
           bRpt1[k+S_SEEN_CNT] != bRpt2[k+S_SEEN_CNT])
            update=1;
        if(((((int32)bRpt1[k+S_BYTE_NOW]-(int32)bRpt2[k+S_BYTE_NOW]) * 100) /
             ((int32)bRpt1[k+S_BYTE_NOW]+1) >= 5)  ||
            (((int32)bRpt2[k+S_BYTE_NOW]-(int32)bRpt1[k+S_BYTE_NOW]) * 100) /
             ((int32)bRpt1[k+S_BYTE_NOW]+1) >= 5)
            update=1;
    }

    if(seenCnt != iO->seenCnt)
        update=1;
    if((update != 0)  && ((iO->displayMode & DM_CONF) != 0)  )
        printf("\nConfiguration_Update: at %d second - %s",
                iO->netTime, ctime(&atime));

    if(((errGtAll != iO->errGtAll) || (errHwAll != iO->errHwAll)) &&
       ((iO->displayMode & DM_ERR) != 0))
    {
        update=1;
        printf("\nERR_Update: +%d GT_ERRa +%d HW_ERRa +%d HW_ERR at %d sec nId=%d - %s",
            errGtAll - iO->errGtAll, errHwAll - iO->errHwAll,
            aStat[S_ERR_HW] - iO->errHwMy, iO->netTime,iO->mId,ctime(&atime));
        ERRORDUMP;
        fflush(stdout);
    }
    iO->seenCnt = seenCnt;
    iO->errGtAll = errGtAll;
    iO->errHwAll = errHwAll;
    iO->errHwMy = aStat[S_ERR_HW];
    aStat[S_SEEN_CNT]=seenCnt;

    if(((update != 0) || ((iO->netTime % 600) == 0)) &&
       ((iO->displayMode & DM_CONF) != 0))
    {
        printf("mId  [nId<uId RS  M]    GT_ERR   HW_ERR  ~MB/s   MB/s     TIME \n");
        for(j=0;j < seenCnt; j++)                          /* print stats */
        {
            k=aSeen[j] * A1P_SW;
            printf("%3d: ", aSeen[j]);
            printf("[%3d<%-3d %2d %2d] ",
                bRpt1[k+S_HW_ID],bRpt1[k+S_HW_ID_UP],
                bRpt1[k+S_NET_LEN],bRpt1[k+S_SEEN_CNT]);
            printf(" %8u", bRpt1[k+S_ERR_GT]);
            printf(" %8u", bRpt1[k+S_ERR_HW]);
            dRate = (bRpt1[k+S_GBYTE]*1000*100)/ bRpt1[k+S_TIME] +
                (bRpt1[k+S_BYTE] / bRpt1[k+S_TIME]) / 10000;
            printf(" %3d.%02d", dRate/100,dRate%100);
            dRateNow = (bRpt1[k+S_BYTE_NOW] / NET_UPDATE_PERIOD) / 10000;
            printf(" %3d.%02d", dRateNow/100,dRateNow%100);
            printf(" %8d", bRpt1[k+S_TIME]);
            /* printf(" %9u", bRpt1[k+S_BYTE_NOW]); */
            printf("\n");
            fflush(stdout);
        }
    }
    if(((iO->displayMode & DM_SUM) != 0) ||
      (((update != 0) || ((iO->netTime % 600) == 0)) && ((iO->displayMode & DM_CONF) != 0)))
    {
        printf("%3d* ", iO->mId);
        printf("[%3d<%-3d %2d %2d] ",
            aStat[S_HW_ID],aStat[S_HW_ID_UP], aStat[S_NET_LEN],aStat[S_SEEN_CNT]);
        printf(" %8d", errGtAll);
        printf(" %8d", errHwAll);
        printf(" %3d.%02d", dRateAll/100,dRateAll%100);
        printf(" %3d.%02d", dRateAllNow/100,dRateAllNow%100);
        printf(" %8d", iO->netTime);
        printf(" -- TOTALS\r");
        fflush(stdout);
     }
    iO->bRpt2 = bRpt1;                /* buffer swap */
    iO->bRpt1 = bRpt2;
}


/*************************************************************************/
/*  function:     gtnexQuickMemoryTest                                   */
/*  description:  as name says. To be optimized and upgraded if needed.  */
/*************************************************************************/
static int gtnexQuickMemoryTest(iOptionsType *iO)
{
    uint32            i;
    uint32            e=0;
    volatile uint32  *aRm    = iO->aRm;
    uint32            rmSize = iO->rmSize;
    time_t            rSeed;
    long int randvector;
    irdn6_vars(gen1);
    irdn6_vars(gen2);


    for(i = rmSize-1; i != 0; i--)     /* reverse sequential pattern fill */
        aRm[i] = ~i;
    for(i = rmSize-1; i != 0; i--)                              /* verify */
        if(aRm[i] != ~i)
        {
            e++;
            if((iO->displayMode & DM_ERR) != 0)
                printf("ERR_Q0 o=%7.7X d=%8.8X w=%8.8X v=%8.8X l=%8.8X ec=%d\n",
                    i*4,aRm[i],~i,~i^aRm[i],aRm[i]-~i,e);
        }

    for(i = 0; i < rmSize; i++)            /* sequential pattern fill */
        aRm[i] = i;
    for(i = 0; i < rmSize; i++)                             /* verify */
        if(aRm[i] != i)
        {
            e++;
            if((iO->displayMode & DM_ERR) != 0)
                printf("ERR_Q1 o=%7.7X d=%8.8X w=%8.8X v=%8.8X l=%8.8X ec=%d\n",
                    i*4,aRm[i],i,i^aRm[i],aRm[i]-i,e);
        }

    time(&rSeed);
    randvector = (rSeed & 0xeFFFeFFF);              /* Randomize irdn6 */
    for(i = 0; i < rmSize; i++)                 /* random pattern fill */
    {
        irdn6(gen1,randvector);
        aRm[i] = randvector;
    }
    randvector = (rSeed & 0xeFFFeFFF); /* Randomize (reinit here) irdn6 */
    for(i = 0; i < rmSize; i++)                               /* verify */
    {
        irdn6(gen2,randvector);
        if(aRm[i] != (uint32)randvector)
        {
            e++;
            if((iO->displayMode & DM_ERR) != 0)
                printf("ERR_Q2 o=%7.7X d=%8.8X w=%8.8X v=%8.8X ec=%d\n",
                    i*4,aRm[i],(uint32)randvector,(uint32)randvector^aRm[i],e);
        }
    }

    return(e);
}

/***************************************************************************/
/* Function:    getRuntimeChar                                             */
/* Description: Thread that reads keyboard                                 */
/***************************************************************************/
static void *getRuntimeChar(void *voidp)
{
    exitThreadParms *pparms = (exitThreadParms*) voidp;
    iOptionsType *iO = pparms->iO;
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
        else if(ch == 'D')
            iO->displayMode = (iO->displayMode + 1) % 8;
    }
    return voidp;
}

/**************************************************************************/
/*  function:     buildOptionsString                                      */
/*  description:  Getting argv details into internal usable form          */
/**************************************************************************/
static void buildOptionsString(iOptionsType *iO, char *optionsString)
{
    int off = 0;
    off += sprintf(&optionsString[off],
        "gtnex -%s%s%s%s%s -u %d -d %d -s %d",
        iO->doPIO ? "P":"D",iO->doMemory ? "M":"",iO->doQuickMemoryTest?"Q":"",
        iO->doTerminate==1?"T":"", iO->doTerminate==2?"R":"",
        iO->unit, iO->displayMode, iO->secondsToRun);
    if( iO->mId != (~0) )
        off += sprintf(&optionsString[off]," -m %d", iO->mId);
    if( iO->packetSize != (~0) )
        off += sprintf(&optionsString[off]," -l %d", iO->packetSize);
}

/**************************************************************************/
/*  function:     parseCommand                                            */
/*  description:  Getting argv details into internal usable form          */
/**************************************************************************/
static void parseCommand(int argc,char **argv,iOptionsType *iO,char *optionsString)
{
    char *endptr, nullchar = 0;
    int  len, tookParam=0;
    int i,j;                /* setting defaults - 0's are set in the main */
    iO->packetSize = ~0; iO->secondsToRun = ~0; iO->mId = ~0;
    iO->displayMode = DM_ERR | DM_CONF | DM_SUM;

    buildOptionsString(iO, optionsString); /* default options */

    if(argc == 1)                          /* start processing */
    {   iO->helpLevel=1; return; }

    for(i = 1; i < argc; i++, tookParam=0)
    {
        len = (int) strlen(argv[i]);
        if((argv[i][0] != '-') || (len < 2))
        {
            printf("\nERROR: Unexpected option: \"%s\"\n\n", argv[i]);
            iO->helpLevel=1;
            return;
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
                return;
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
                    iO->unit = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='m')
                    iO->mId = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='l')
                    iO->packetSize = (strtoul(argv[i+1],&endptr,0)>>2)<<2;
                else if(argv[i][j]=='s')
                    iO->secondsToRun = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='d')
                    iO->displayMode = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='b')
                    iO->boostRatio = strtoul(argv[i+1],&endptr,0);
                else tookParam = 0;

                if( tookParam && (*endptr))
                {
                   printf("\nInvalid parameter \"%s\" for option \"%c\"\n\n",
                           argv[i+1], argv[i][j]);
                   iO->helpLevel=1;
                   return;
                }
            }
            /* options without arguments now */
            if(              argv[i][j]=='P') iO->doPIO = 1;
            else if(         argv[i][j]=='M') iO->doMemory = 1;
            else if(         argv[i][j]=='T') iO->doTerminate = 1;
            else if(         argv[i][j]=='R') iO->doTerminate = 2;
            else if(         argv[i][j]=='Q') iO->doQuickMemoryTest = 1;
            else if(tolower(argv[i][j])=='h')
            {
                iO->helpLevel=2;
                if(argc > 2) iO->helpLevel=3;
                return;
            }
            else if(!tookParam)
            {
                printf("\nERROR: Unexpected option: \"%c\"\n\n",argv[i][j]);
                iO->helpLevel=1;
                return;
            }
        }
        if (tookParam) i++;
    }

    /* reconcile conflicting options */
    if((iO->doPIO == 0) && (iO->doMemory == 0) && (doDMA_DISABLED != 0))
        printf("!!!!! DMA option (default) not supported on your hardware - ignored!!!!!\n");
    if((iO->doMemory != 0) || ((doDMA_DISABLED != 0)))
        iO->doPIO = 1;
    if(iO->boostRatio == 0)
       iO->boostRatio = 1;
    if((iO->doPIO != 0) && (doPIO_DISABLED != 0))
    {
        printf("!!!!! PIO (P) option must be supported on your hardware - quiting!!!!!\n");
        exit(0);
    }

    buildOptionsString(iO, optionsString); /* update options */
}

/**************************************************************************/
/*  function:     printHelpLevel1                                         */
/*  description:  Print hints and call more help if needed                */
/**************************************************************************/
static void printHelpLevel1( iOptionsType *iO, char *optionsString)
{
    printf( "%s %s\n\n", APP_VER_STRING, APP_VARIANT );
    printf("Usage: gtnex [-PMQTR] [-u unit] [-m memberId] [-d displayMode]\n"
           "             [-s secondsToRun] [-l packetLength] [-h]\n\n");
    printf("Defaults: %s\n",optionsString);
#ifdef PLATFORM_VXWORKS
    printf("VxWorks users: Enclose options list in a set of quotes \" \".\n");
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
"  -P   - PIO - use CPU exclusively (do not use DMA)\n"
"  -T   - terminate the test on all the nodes on the ring\n"
"  -R   - restart for all the nodes on the ring\n"
"  -Q   - run quick memory test, PIO only\n"
"  -M   - use system's memory instead of GT's memory\n"
"  -u # - board/unit number (default 0)\n"
"  -m # - use member Id # (mId=#) instead of node Id (mId=nId), (0 to 255)\n"
"  -l # - limit the maximum packet length to # of bytes\n"
"  -s # - limit the test time to # of seconds\n"
"  -d # - display mode flags (positionally coded):\n"
"            1 - error details, 2 - network layout, 4 - summary line\n");
    printf(
"Example: gtnex -u 0    - use defaults on unit 0\n"
"         gtnex -M      - use system memory instead of GT's\n");
    printf(
"Notes:   Run `gtnex -h 1' for more information.\n"
"         Press 'q' to stop a running test.\n\n");
}

/**************************************************************************/
/*  function:     printHelpLevel3                                         */
/*  description:  Print detailed hints                                    */
/**************************************************************************/
static void printHelpLevel3()
{
    printf(
"   The gtnex utility performs memory verification operations. Each instance\n"
"of gtnex verifies it own written data, and the data written by other gtnex\n"
"instances that are participating on the ring. In addition, error and\n"
"throughput statistics are shared among the instances. These statistics are\n"
"shared using a predetermined area of GT replicated memory as a reporting\n"
"area (A1).\n"
"\n");
    printf(
"   The operations performed by gtnex attempt to detect data corruption\n"
"errors and writes/reads from incorrect memory locations. Gtnex operates on\n"
"the full range of memory above the 131072 byte offset (see startup printout\n"
"for exact starting offset, e.g. \"A0 - Game field start: 0x0020000\"). One\n"
"exception to this is the quicktest (-Q) which tests all memory.\n"
"\n");
    printf(
"   Each gtnex instance has a predetermined area of memory (A4) which it fills\n"
"with its iteration count during each iteration. Also each iteration, gtnex\n"
"randomly selects a part of A4 from the active instances for data verification.\n"
"\n"
"   Psuedo-random data is also written by all instances at randomly selected\n"
"memory offsets within another predetermined, and shared, memory area (A5).\n"
"A region of this memory is randomly selected for verification on each\n"
"iteration.\n"
"\n");
    printf(
"   Gtnex uses a member ID (see -m) to choose its predetermined memory areas\n"
"and in the generation and marking of psuedo-random data patterns. By default\n"
"gtnex uses the device node ID at test initialization as the member ID.\n"
"However, the member ID can be specifically assigned using the -m option.\n"
"This allows multiple instances of gtnex to be executed on the same GT device.\n"
"For example, given a device with node ID 7:\n"
"\n"
"   gtnex -P -u 0\n"
"   gtnex -P -u 0 -m 8\n"
"\n"
"would start two gtnex instances on the same GT device. For a given network,\n"
"each member ID (0 to 255) can be used only once, otherwise data errors will\n"
"be generated by the competing instances.\n"
"\n");
    printf(
"   By default, gtnex will display information similar to the following:\n"
"\n"
"mId  [nId<uId RS  M]    GT_ERR   HW_ERR  ~MB/s   MB/s     TIME\n"
"  2: [  2<138  2  2]         0        0   6.61   9.21        2\n"
"138: [138<2    2  2]         0        0   6.56   6.65       11\n"
"138* [138<2    2  2]         0        0  15.69  15.86       38 -- TOTALS\n"
"\n");
    printf(
"The above display shows:\n"
"  mId    - member ID of an active gtnex instance\n"
"  nId    - node ID on which the instance mId is running\n"
"  uId    - upstream node ID\n"
"  RS     - ring size\n"
"  M      - number of participating members\n"
"  GT_ERR - data errors\n"
"  HW_ERR - hardware errors\n"
"  ~MB/s  - cumulative data rate for since beginning of test, or reset\n"
"  MB/s   - data rate since last update\n"
"  TIME   - seconds the test has been running\n"
"\n");
    printf(
"   For the TOTALS line, statistics to the left of GT_ERR are for the\n"
"reporting member. Statistics from GT_ERR to MB/s are collective from all\n"
"reporting members. TIME is the time reported by the member having lowest mId.\n"
"\n"
"   The TOTALS line is updated about once per second.  The other lines are\n"
"updated when a significant change occurs, and at extended intervals.\n"
"\n");
    printf(
"   Here are some helpful hints. The -R option will reset the statistics\n"
"(except HW_ERR) for all running instances. The -T option will terminate all\n"
"running instances.  These are useful options for large networks, and when\n"
"running backgrounded instances.  Option -d 0 will eliminate many printouts,\n"
"and can be helpful when terminals windows are limited, or backgrounding\n"
"gtnex.\n"
"\n");
    printf(
"   Note: Data presented by gtnex may be difficult to interpret if the\n"
"network is not configured as a regular ring and nodes are not configured\n"
"in typical operational mode (e.g. tx enable, rx enable, laser on, etc.).\n"
"In such cases, the information presented by each instance of gtnex may be\n"
"different.\n"
"\n");
}
