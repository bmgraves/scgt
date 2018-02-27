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

#define APP_VER_STRING "GT Bit Error Rate Test (gtbert) rev. 1.05 (2011/08/30"

/**************************************************************************/
/****** TAKING CARE OF ALL POSSIBLE OS TYPES - COPY-PASTE *****************/
/**************************************************************************/
/* control by PLATFORM_WIN, PLATFORM_UNIX, PLATFORM_VXWORKS, etc */     /**/
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
#ifdef PLATFORM_WIN                                                     /**/
    #include <windows.h>                                                /**/
                                                                        /**/
#elif PLATFORM_VXWORKS                                                  /**/
    #undef PARSER                                                       /**/
    #undef MAIN_IN                                                      /**/
    #define VXL     40                                                  /**/
    #define MAIN_IN int APP_VxW_NAME(char *cmdLine) { \
            int argc; char argv0[]="gtbert"; char *argv[VXL]={argv0};   /**/
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
    #define APP_VxW_NAME xgtbert
#elif defined HW_SCFF /* Scramnet */
    #include "scrplus.h"
    #define HAND uint32
    #define APP_VxW_NAME sgtbert
#else /* GT */
    #define HW_SCGT
    #include "scgtapi.h"
    #define HAND scgtHandle
    #define APP_VxW_NAME gtbert
#endif

/**************************************************************************/
/************** APPLICATION-SPECIFIC CODE STARTS HERE *********************/
/**************************************************************************/

/**************************************************************************/
/**************************  D A T A  T Y P E S  **************************/
/**************************************************************************/

#define COMMON_OPTIONS \
    uint32    doInteractive, doPIO, boostRatio, showErrors,  testValidUnit; \
    uint32    timedExecSeconds,   exitOnErrSeconds,   resetIfErrSeconds; \
    uint32    offset,             length,             doConfig; \
    uint32    mapMemSize,         popMemSize,         mode;\
    uint32    alignment, doPIOreads, doPIOwrites; \
    volatile uint32  *boardAddressPIO; \
    HAND  hDriver;

typedef struct {
    volatile uint32    inputAvailable;
    volatile uint32    resetFlag;
    volatile uint32    showBadFlag;
    volatile uint32    timeToExit;
    volatile uint32    showOptions;
    volatile uint32    modeChange;    
    volatile uint32    running;
    usysThreadParams   pt;
} exitThreadParms;

typedef struct {
    COMMON_OPTIONS
    char *optionsString;
    uint32    unit,     helpLevel;
} iOptionsType;

/**************************************************************************/
/******************  F U N C T I O N  P R O T O T Y P E S  ****************/
/**************************************************************************/

uint32 gtbertWorker(iOptionsType *iO);
static int check_board(iOptionsType *iO);

static void *getRuntimeChar(void *voidp);
static void buildOptionsString( char *optionsString, iOptionsType *iO );
static void parseCommand(int argc, char **argv,
                         iOptionsType *iO, char *optionsString);
static void printHelpLevel1(iOptionsType *iO, char * optionsString);
static void printHelpLevel2();
static void printHelpLevel3();

/**************************************************************************/
/***********************  D E F I N E S & M A C R O S  ********************/
/**************************************************************************/

/* The following is to mask the actual hardware dependency */

#ifdef HW_SL240 /* SLxxx */
    #define APP_VARIANT "sl240 (xgtbert) Edition\n"
    #define DRIVER_SUCCESS FXSL_SUCCESS
    #define DRIVER_TIMEOUT FXSL_TIMEOUT
    #define DO_CLEANUP(w,x,y,z)  doCleanup(w,x,y,z)
    #define doPIO_DISABLED 1
    #define doDMA_DISABLED 0
    #define DRIVER_OPEN(x,y) { \
               if(fxsl_open(x,y)) \
               {  printf("ERROR: could not open driver for unit %d\n", x); \
                  return -1;\
               } \
               iO->boardAddressPIO = (volatile uint32 *)NULL; \
               iO->mapMemSize = iO->popMemSize = 0xffffffff; /* no limit */}
    #define DRIVER_CLOSE(x) fxsl_close(x)
    #define DMA_FLAGS   FXSL_USE_SYNC
    #define DMA_WRITE_FUNC(pHandle,smOffset,pSource,sizeByte,flags,pBytesWr,to)\
          (fxsl_send(*pHandle, pSource, sizeByte,&flags,to,pBytesWr))
    #define DMA_READ_FUNC(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead,to)\
               (fxsl_recv(*pHandle,pDest, sizeByte+4, &flags, to, pBytesRead))
    /*get number of errors */ /*num 8b/10b errs*/
    #define GET_8B10B(pHandle, pVal)  (fxsl_read_CR(*(pHandle), 0x24, (pVal)), *(pVal)) /*num 8b/10b MGT*/
    #define GET_HW_ID(pHandle)  0
    #define WDR (1065.0E6) /* this assumes 1 Gb/s */
    static void doCleanup(HANDLE hDriver, uint32 *recvBuff,
                          uint32 buffSize, uint32 print);
#elif defined HW_SCFF /* Scramnet */
    #define APP_VARIANT "SCRAMNet (sgtbert) Edition\n"
    #define DRIVER_SUCCESS 0
    #define DRIVER_TIMEOUT 1 /* !!!! not correct !!!!*/
    #define DO_CLEANUP(w,x,y,z)
    #define doPIO_DISABLED 0
    #define doDMA_DISABLED 0
    #define DRIVER_OPEN(x,y) {sp_scram_init(0); \
              sp_stm_mm(Long_mode); \
              scr_csr_write(2, (unsigned short) 0xd040 |0x0300); /*wlast*/\
              scr_csr_write(0, (unsigned short) 0x8003); \
              scr_csr_write(10,(unsigned short)(scr_csr_read(10) | 0x11)); \
              scr_csr_write(8,(unsigned short)(scr_csr_read(8) & 0xf800)); /* Counting errors */\
              scr_csr_write(9,0x01a8); /* Counting errors */\
              scr_csr_write(13, 0x0); /* clear counter */\
              scr_csr_write(3, (unsigned short)(253<<8)); /*temp node id */\
              iO->boardAddressPIO = (volatile uint32 *)get_base_mem(); \
              iO->mapMemSize = iO->popMemSize = sp_mem_size();}
    #define DRIVER_CLOSE(x)
    #define DMA_FLAGS   0
    #define DMA_WRITE_FUNC(pHandle,smOffset,pSource,sizeByte,flags,pBytesWr,to)\
                (scr_dma_write(pSource, smOffset, sizeByte), \
                 *pBytesWr = sizeByte, DRIVER_SUCCESS)
    #define DMA_READ_FUNC(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead,to)\
                (scr_dma_read(pDest, smOffset, sizeByte), \
                 *pBytesRead = sizeByte, DRIVER_SUCCESS)

    #define GET_8B10B(pHandle, pVal)  (*(pVal)=(unsigned long)scr_csr_read(13), *(pVal))
    #define GET_HW_ID(pHandle) 0
    #define WDR (155.0E6) /* this assumes SC150 link 1 vilolations per bit*/

#elif defined HW_SCGT /* GT */
    #define APP_VARIANT ""
    #define DRIVER_SUCCESS SCGT_SUCCESS
    #define DRIVER_TIMEOUT SCGT_TIMEOUT
    #define DO_CLEANUP(w,x,y,z)
    #define doPIO_DISABLED 0
    #define doDMA_DISABLED 0
    #define DRIVER_OPEN(x,y)  { scgtDeviceInfo devInfo;                  \
            if (scgtOpen(x,y))                                           \
            {                                                            \
                printf("ERROR: could not open driver for unit %d\n", x); \
                return -1;                                               \
            }                                                            \
            iO->boardAddressPIO = scgtMapMem(y);                         \
            scgtGetDeviceInfo(y, &devInfo);                              \
            iO->popMemSize = devInfo.popMemSize;                         \
            iO->mapMemSize = devInfo.mappedMemSize;  }
    #define DRIVER_CLOSE(x) { scgtUnmapMem(&(x)); scgtClose(&(x)); }
    #define DMA_FLAGS     0
    #define DMA_WRITE_FUNC(pHandle,smOffset,pSource,sizeByte,flags,pBytesWr,to)\
                scgtWrite(pHandle,smOffset,pSource,sizeByte,flags,pBytesWr,NULL)
    #define DMA_READ_FUNC(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead,to)\
                scgtRead(pHandle,smOffset,pDest,sizeByte,flags, pBytesRead)
    #define GET_8B10B(pHandle,pVal)  (*(pVal)=scgtReadCR((pHandle), 0x88),*(pVal)) /*num 8b/10b errs only*/
    #define GET_HW_ID(pHandle)  (scgtGetState(pHandle, SCGT_NODE_ID))
    #define WDR (1065.0E6) /* this assumes 1 Gb/s link and 1 vilolations per 32 bits*/
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
/************************ G T B E R T   C O D E ***************************/
/**************************************************************************/

MAIN_IN                                                    /* gtbert main */

    /* set variables to default */
    iOptionsType   iO;
    char optionsString[160];
    uint32 Errs = 0;

    PARSER;

    memset(&iO, 0, sizeof(iOptionsType));        /* zero all the options */
    iO.optionsString=optionsString;

    parseCommand(argc, argv, &iO, optionsString);

    if(iO.helpLevel!=0) /*1, 2, 3, ...*/
    {
        printHelpLevel1(&iO, optionsString);
        return 0;
    }

    if( iO.testValidUnit )
    {
        return check_board(&iO);
    }

    if( (iO.doPIO || iO.doPIOreads || iO.doPIOwrites) && doPIO_DISABLED )
    {
        printf("!!!!! PIO (P) option not supported on your hardware !!!!!\n");
        return -1;
    }
    if( !iO.doPIO && doDMA_DISABLED )
    {
        printf("!!!!! DMA option (default) not supported on your hardware !!!!!\n");
        return -1;
    }

    printf("Running: %s\n", optionsString);

    if( iO.doInteractive )
        printf("Hit: 'q' - quit, 'r' - reset, 'h' - help, 'v' - display & 'm' - mode toggles\n");

    Errs = gtbertWorker(&iO);

    return (Errs ? 1 : 0);
}

/**************************************************************************/
/*  function:     gtbertWorker                                            */
/*  description:  Runs random data through a transceiver and verfies that */
/*                the received data matches the sent data                 */
/**************************************************************************/
uint32 gtbertWorker(iOptionsType *iO)
{
    uint32 execTime = iO->timedExecSeconds;
    uint32 errExitTime = iO->exitOnErrSeconds;
    uint32 resetTime = iO->resetIfErrSeconds;
    uint32 showErrs = iO->showErrors;
    uint32 doPIO = iO->doPIO;
    uint32 doPIOreads = iO->doPIOreads;
    uint32 doPIOwrites = iO->doPIOwrites;
    uint32 bytes = iO->length;
    uint32 words = bytes >> 2;
    uint32 offset = iO->offset;
    uint32 mode = iO->mode;
    HAND   hDriver;
    uint32 *sendBuffBase,*sendBuffBase2, *recvBuffBase;
    uint32 *sendBuff,*sendBuff2,*sendBuffTmp, *recvBuff;
    uint32 bytesSent, bytesRecv, timeout, err = DRIVER_SUCCESS;
    uint32 elapsedTime = 0, elapsedTimePast = 0;
    volatile uint32 *pioBuf = NULL;
    uint32 sdata = 0x11223344, sdata2=0x55667788;
    time_t startTime;
    #define TM 2
    unsigned long Tx = 0, Rx = 0, Ex = 0, Tout = 0, Tout1 = 0, Eod=0;
    exitThreadParms exitParms;
    int i;
    uint32 j, flags, val;

    long int randvector;
    irdn6_vars(gen1);

    uint32 v8b10bCurrent, v8b10bOld, v8b10bDiff;
    double dv8b10b, dv8b10bRate;

    #define CLEAR8B10B {dv8b10b=0.0; v8b10bCurrent=GET_8B10B(&iO->hDriver,&val); \
                        v8b10bOld=v8b10bCurrent;}
    #define SEE8B10B { v8b10bCurrent = GET_8B10B(&iO->hDriver,&val);\
    if(v8b10bCurrent > v8b10bOld) v8b10bDiff = v8b10bCurrent - v8b10bOld; else\
    v8b10bDiff = v8b10bOld - v8b10bCurrent; v8b10bOld = v8b10bCurrent;\
    dv8b10b = dv8b10b + (double)v8b10bDiff;\
    dv8b10bRate = log10(dv8b10b/((double)(elapsedTime+1) * WDR) + 1.0E-99);}
    /* Don't change RESET_CNTS without first checking what it will damage */
    #define RESET_CNTS { time(&startTime); Tx=Rx=Ex=Tout=Eod=Tout1=0;\
                         CLEAR8B10B; elapsedTimePast = elapsedTime = 0; }
    #define GIG 1.E9
    unsigned long RxG;
    double dTxB,dRxB,dbytes;

    #define PRINTOUT {SEE8B10B; dbytes=(double)bytes; dRxB=(double)Rx * dbytes;\
      RxG=(unsigned long)(dRxB/GIG); dTxB=((double)Tx * dbytes + 1.0E-99);\
    printf("dT=%d Rx=%lu Mx=%lu To=%lu/%lu Eo=%lu Ex=%lu V=%lu LgB=%4.2f LgV=%4.2f \r", \
      elapsedTime,   Rx,\
      (long int)((Tx-Rx)), Tout,Tout1, Eod, Ex, (unsigned long)dv8b10b, \
      log10(((double)(Ex-Tout) + 1.0E-90)/dTxB), dv8b10bRate);}

    DRIVER_OPEN(iO->unit, &iO->hDriver);                    /* Open device */
    hDriver = iO->hDriver;
                                       /* Allocate the send and receive buffers */
    #define ALIGN   4096

    sendBuffBase =  (uint32*)usysMemMalloc(bytes + (ALIGN * 3));
    sendBuffBase2 = (uint32*)usysMemMalloc(bytes + (ALIGN * 3));
    recvBuffBase =  (uint32*)usysMemMalloc(bytes + (ALIGN * 3));
    if(sendBuffBase == NULL || sendBuffBase2 == NULL || recvBuffBase == NULL)
    {
        if(sendBuffBase  != NULL) usysMemFree(sendBuffBase);
        if(sendBuffBase2 != NULL) usysMemFree(sendBuffBase2);
        if(recvBuffBase  != NULL) usysMemFree(recvBuffBase);
        printf("ERROR:  not enough memory.\n");
        return -1;    
    }

    /* align to 4K boundary */
    sendBuff =  (uint32*)(((uintpsize)sendBuffBase + (ALIGN-1))  & ~((uintpsize)(ALIGN-1)));
    sendBuff2 = (uint32*)(((uintpsize)sendBuffBase2 + (ALIGN-1)) & ~((uintpsize)(ALIGN-1)));
    recvBuff =  (uint32*)(((uintpsize)recvBuffBase + (ALIGN-1))  & ~((uintpsize)(ALIGN-1)));

    /* Offset from 4K alignment by iO->alignment */
    if( iO->alignment )
    {
        iO->alignment = (iO->alignment % ALIGN) & ~0x3;
        sendBuff =  (uint32*)((uintpsize)sendBuff +  iO->alignment);
        sendBuff2 = (uint32*)((uintpsize)sendBuff2 + iO->alignment);
        recvBuff =  (uint32*)((uintpsize)recvBuff +  iO->alignment);
    }

    /* -1 check, lower digit of iO->offset masked during options parsing */
    if((iO->offset & 0xfffffff0) == 0xfffffff0)
    {
        /* auto-offset in 0.5 MB pieces */
        iO->offset = GET_HW_ID(&iO->hDriver) * 0x80000;
        offset = iO->offset;
        printf("Offset set to 0x%X due to auto-offset selection\n",iO->offset);
    }
    if( doPIO || doPIOreads || doPIOwrites )
    {
        if(iO->boardAddressPIO == NULL)
            printf("!!!!! WARNING: PIO address is NULL !!!!!\n");
        if( (iO->offset + iO->length) > iO->mapMemSize )
        {
            printf("ERROR: offset + length exceeds mapped memory range!!!\n");
            return -1;
        }
    }

    if( !doPIO && ((iO->offset + iO->length) > iO->popMemSize) )
    {
        printf("ERROR: offset + length exceeds populated memory range!!!\n");
        return -1;
    }

    memset( &exitParms, 0, sizeof(exitThreadParms) );

    if( iO->doInteractive )
    {                               /* create thread to accept user input */
        sprintf(exitParms.pt.threadName, "gtbertExit");
        exitParms.pt.parameter = (void *) &exitParms;
#ifdef PLATFORM_VXWORKS
        exitParms.pt.priority = UPRIO_MED;
#endif
        usysCreateThread(&(exitParms.pt), getRuntimeChar);
        /* Give thread a moment to start up. Needed on VxWorks, Solaris 8 */
        usysMsTimeDelay(50);
    }

    if( iO->doConfig )  /* configure the device */
    {
#ifdef HW_SL240
        fxsl_configstruct cfg;
        fxsl_get_config(hDriver, &cfg);
/*        cfg.use_flow_control = 1;     cfg.halt_on_link_errors = 1; tmp ldw*/
        cfg.use_flow_control = 1;     cfg.halt_on_link_errors = 0;
        cfg.allow_qing_on_lerror = 1; cfg.loopConfig = 0;
        cfg.useCRC=1;                 cfg.convert_dsync=0;
        /* cfg.max_send_timeout=100; cfg.max_recv_timeout=100; */
        fxsl_set_config(hDriver, &cfg);
        printf("\r   Clearing FIFO\r");
        DO_CLEANUP(hDriver, recvBuff, bytes, 1);            /* Clear FIFO */

#elif defined HW_SCGT /* Scramnet GT */
        scgtSetState(&hDriver, SCGT_WRITE_ME_LAST, 1); /* turn on write-last */
        scgtSetState(&hDriver, SCGT_TRANSMIT_ENABLE, 1);       /* turn on tx */
        scgtSetState(&hDriver, SCGT_RECEIVE_ENABLE, 1);        /* turn on rx */
        scgtSetState(&hDriver, SCGT_EWRAP, 0);             /* turn off ewrap */
#endif
    }

    err = DRIVER_SUCCESS;
    bytesRecv = bytesSent = bytes;

    pioBuf = (volatile uint32*)((uintpsize)iO->boardAddressPIO + offset);

    RESET_CNTS;              /* Start timer, zero counters including 8B10B */
    randvector = (startTime & 0xeFFFeFFF);              /* randomize irdn6 */

    for(i=1; (elapsedTime < execTime) || (execTime == 0); i++)
    {
        if( ((i % 1000) == 0) && ((iO->mode & 0x80000000) != 0) )
        {
            mode = (mode + 1) % 8;     /* do not update iO->mode */
        }

        if(exitParms.inputAvailable)               /* check for user input */
        {
            exitParms.inputAvailable = 0;

            if(exitParms.timeToExit == 1)
                break;
            if(exitParms.resetFlag == 1)
            {
                RESET_CNTS;
                exitParms.resetFlag = 0;
            }
            if(exitParms.showBadFlag == 1)
            {
                showErrs = (showErrs + 1) % 3;
                exitParms.showBadFlag = 0;
                exitParms.showOptions = 1; /* to display settings next */
            }
            if(exitParms.modeChange == 1)
            {
                mode = (mode + 1) % 8;     /* do not update iO->mode */
                exitParms.modeChange = 0;
                exitParms.showOptions = 1; /* to display settings next */
            }
            if(exitParms.showOptions == 1)
            {
                char * str = "???";
                printf("\r\nRunning: %s\n", iO->optionsString);
                printf("Real offset: 0x%X    Current mode m: %d\n", offset, mode);
                printf("Error display (toggle with 'v') is: %s  \n",
                    showErrs==0 ? "OFF" : (showErrs==2 ? 
                    "ON for errors and old data" :"ON for errors only"));

                switch( mode )
                {
                    case 0: str="RANDOM and the same within a packet"; break;
                    case 1: str="RANDOM every word in every packet";   break;
                    case 2: str="0x55555555-0xaaaaaaaa and swapping";  break;
                    case 3: str="0x55555555-0xaaaaaaaa no swapping";   break;
                    case 4: str="0x00000000-0xffffffff and swapping";  break;
                    case 5: str="0x00000000-0xffffffff no swapping";   break;
                    case 6: str="0x67676767-0x67676767 no swapping";   break;
                    case 7: str="0x7e7e7e7e-0x7e7e7e7e no swapping";   break;
                }

                printf("Data pattern  (toggle with 'm') is: m=%d %s\n",mode,str);
                printf("Other options: 'q' - quit, 'r' - reset, 'h' - help\n");
                exitParms.showOptions = 0;
            } 
        }

        memset(recvBuff, 0, bytes);        /* zero receive buffer */
        sendBuffTmp=sendBuff;              /*swapping send buffers */
        sendBuff=sendBuff2;
        sendBuff2=sendBuffTmp;
                                          /* select the mode */
        if(mode == 2 || mode == 3)     /* every bit changing DC maintained */
        {
            sdata=0x55555555;
            if((i % 2) == 0 && mode == 2) sdata=~sdata;
            sdata2=~sdata;
        }
        else if(mode == 4 || mode == 5)    /* every bit changing DC biased */
        {
            sdata=0x0;
            if((i % 2) == 0 && mode == 4) sdata=~sdata;
            sdata2=~sdata;
        }
        else if(mode == 6)    /* FC bad sequence 40*/
        {
            sdata=0x67676767; 
            sdata2=sdata;
        }
        else if(mode == 7)    /* FC bad sequence 25*/
        {
            sdata=0x7e7e7e7e; 
            sdata2=sdata;
        }
        else if(mode == 1)           /* new random mode - random every word*/
        {
            for ( j=3; j<words; j++)
            {
                irdn6(gen1,randvector);    /* generate new random variable */
                sendBuff[j]=randvector | ( (randvector << (i%32)) & 0x80000000);
            }
        }
        else    /* standard old random mode - the same data within a packet*/
        {
            irdn6(gen1,randvector);        /* generate new random variable */
            sdata=randvector | (  (randvector << (i % 32)) & 0x80000000);
            sdata2=sdata;
        }

        sendBuff[0] = i;             /* Add sequence signature to the data */
        sendBuff[1] = ~i;
        sendBuff[2] = words;

        if(mode != 1) /* in "all random the data is in the buffer already */
        {
            for ( j=3; j<words; j=j+2)     /* Fill packet with random data */
            {   
                sendBuff[j] = sdata;
                sendBuff[j+1] = sdata2;
            }
        }

        err = DRIVER_SUCCESS; /* in case PIO/DMA mix */
                                                              /* Send data */
        if(doPIO || doPIOwrites)
        {
            for( j = 0; j < words; j++ )
                pioBuf[j] = sendBuff[j];
        }
        else /* DMA */
        {
            timeout = 5 * TM;
            flags = DMA_FLAGS;
            err = DMA_WRITE_FUNC(&hDriver, offset, (uint8*)sendBuff,
                                 bytes, flags, &bytesSent, timeout);
        }

        if (err == DRIVER_SUCCESS)
        {
            Tx++;
        }
        else if ((err == DRIVER_TIMEOUT) && (bytesSent ==  bytes))
        {
            Tx++; Tout1++;
        }
        else                                            /* Transmit error */
        {
            Ex++;
            if(err == DRIVER_TIMEOUT)
                Tout++;
        }

        if((i%10000) == 0)
            usysMsTimeDelay(1);                        /* be nice to others*/
        usysMsTimeDelay(0);
                                                           /* Receive data */
        if((i % iO->boostRatio) != 0)
        {
            Rx++;
        }
        else
        {
            err = DRIVER_SUCCESS; /* in case PIO/DMA mix */

            if(doPIO || doPIOreads)
            {
                for( j = 0; j < words; j++ )
                    recvBuff[j] = pioBuf[j];
            }
            else /* DMA */
            {
                timeout = 5 * TM;
                flags = DMA_FLAGS;
                err=DMA_READ_FUNC(&hDriver,offset,(uint8*)recvBuff,bytes,
                                  flags, &bytesRecv, timeout);
            }

            if( (err == DRIVER_SUCCESS || err == DRIVER_TIMEOUT ) && 
                (bytesRecv == bytes) && (recvBuff[0] != 0) )
            {
                if(err == DRIVER_TIMEOUT)
                    Tout1++;
                Rx++;
                for( j=0; j<words; j++)                            /* data verification */
                {
                    if(recvBuff[j] == sendBuff[j])
                    {
                    }
                    else
                    {
                        if(recvBuff[j] != sendBuff[j])
                        {
                            if((sendBuff2[j] ^ recvBuff[j]) == 0) /*data from previous send*/
                            {
                                Eod=Eod+4; /* old data not accounted for an error at this place */
                            }
                            else
                            {
                                if( (recvBuff[j] ^ sendBuff[j]) & 0xff000000) Ex++;
                                if( (recvBuff[j] ^ sendBuff[j]) & 0x00ff0000) Ex++;
                                if( (recvBuff[j] ^ sendBuff[j]) & 0x0000ff00) Ex++;
                                if( (recvBuff[j] ^ sendBuff[j]) & 0x000000ff) Ex++;
                            }
                            if( showErrs )
                            {
                                if( (showErrs == 2) || ((showErrs == 1) && (sendBuff2[j] ^ recvBuff[j])))
                                {
                                printf("seq=%X off=%7.7X d=%8.8X w=%8.8X bxor=%8.8X %s\n",
                                   sendBuff[0],j,recvBuff[j],sendBuff[j],sendBuff[j]^recvBuff[j],
                                   sendBuff2[j] ^ recvBuff[j] ? "wrong data  " : "old data  ");
                                }
                            }
                        }
                    }
                }                                  /* end of data verification */
            }
            else                                              /* Error occured */
            {
                Ex++;
                if(err == DRIVER_TIMEOUT)
                    Tout++;
                else
                    DO_CLEANUP(hDriver, recvBuff, bytes, 0);
            }
        }
        elapsedTime = (uint32)difftime( time(NULL), startTime);
        if(elapsedTime != elapsedTimePast)           /* Print every second */
        {
            elapsedTimePast = elapsedTime;
            PRINTOUT;
            if(elapsedTimePast % 600 == 0)
                printf("\r\n");
            fflush(stdout);

            if((Ex > 0) || (dv8b10b > 0.1) || (Eod > 0))
            {
                if( (errExitTime != 0) && (elapsedTime >= errExitTime) )
                {
                    printf("\nExitted due to errors after %d seconds.\n",
                           elapsedTime);
                    break;
                }
                /* If a reset time was set */
                if( (resetTime != 0) && (elapsedTime <= resetTime) )
                {
                    resetTime -= elapsedTime;
                    printf("\nReset due to errors.  "
                           "%d reset seconds remaining\n", resetTime);
                    RESET_CNTS;
                }
            }
        }
    }

    PRINTOUT;
    printf("\n");
    fflush(stdout);
    exitParms.timeToExit = 1;
    if( iO->doInteractive && exitParms.running )
        usysKillThread(&(exitParms.pt));         /* kill user input thread */

    DRIVER_CLOSE(hDriver);                            /* Free up resources */
    usysMemFree(sendBuffBase);
    usysMemFree(sendBuffBase2);
    usysMemFree(recvBuffBase);

    /* Return the number of bad transmissions */
    return (Ex + (unsigned long)dv8b10b + Eod);
}

/*******************************************************************************
** Function:    check_board
** Description: checks to see if you have the board specified
*******************************************************************************/
static int check_board(iOptionsType *iO)
{
    HAND hDriver;
    DRIVER_OPEN(iO->unit, &hDriver);  /* macro, calls "return -1" on error */
    DRIVER_CLOSE(hDriver);
    return 0;                                       /* return 0 on success */
}

/***************************************************************************/
/* Function:    getRuntimeChar                                             */
/* Description: Thread that reads keyboard                                 */
/***************************************************************************/
static void *getRuntimeChar(void *voidp)
{
    exitThreadParms *eP = (exitThreadParms*) voidp;
    char ch;
    int ich;
    eP->running = 1;
    while (!(eP->timeToExit))
    {
        /* RTX has no stdio, so break on EOF so we won't spin */
        if ((ich = getchar()) == EOF)
            break;

        ch = toupper(ich); /* toupper may be a macro */
        eP->inputAvailable = 1;
        if (ch == 'Q')
        {
            eP->running = 0;
            eP->timeToExit = 1;
        }
        else if (ch == 'R')
            eP->resetFlag = 1;
        else if (ch == 'V')
            eP->showBadFlag = 1;
        else if (ch == 'H')
            eP->showOptions = 1;
        else if (ch == 'M')
            eP->modeChange = 1;

    }
    return voidp;
}

/**************************************************************************/
/*  function:     buildOptionsString                                      */
/*  description:  Updates the options string                              */
/**************************************************************************/
static void buildOptionsString( char *optionsString, iOptionsType *iO )
{
    char *str = optionsString;
    int off = 0;
    off += sprintf(&str[off], "gtbert -%s%s%s%s%s%s%s -u %d -o 0x%x -l 0x%x -m %d",
            iO->doPIO ? "P":"D",           iO->doInteractive ? "":"Y",
            iO->testValidUnit ? "X":"",    iO->showErrors ? "V":"",
            iO->doConfig ? "":"Z",         iO->doPIOreads ? "R":"",
            iO->doPIOwrites ? "W":"", iO->unit, iO->offset, iO->length, iO->mode);

    if(iO->timedExecSeconds)
        off += sprintf(&str[off],"-s %i ", iO->timedExecSeconds);
    if(iO->exitOnErrSeconds)
        off += sprintf(&str[off],"-z %i ", iO->exitOnErrSeconds);
    if(iO->resetIfErrSeconds)
        off += sprintf(&str[off],"-y %i ", iO->resetIfErrSeconds);
}

/**************************************************************************/
/*  function:     parseCommand                                            */
/*  description:  Getting argv details into internal usable form          */
/**************************************************************************/
static void parseCommand(int argc, char **argv, iOptionsType *iO, char *optionsString)
{
    char *endptr, nullchar = 0;
    int  i, j, len, tookParam=0;
                                     /* configure non-zero default settings */
    iO->doInteractive = 1;        iO->helpLevel = 0;
    iO->length = 0x400 * 4 * 4;   iO->doConfig = 1;

    buildOptionsString(optionsString, iO);       /* default optionsString */

    if(argc == 1)                                     /* start processing */
        {iO->helpLevel=1; return;}

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
                if(argv[i][j]=='u') iO->unit=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='o')
                    iO->offset=strtoul(argv[i+1],&endptr,0) & ~0x3;
                else if(argv[i][j]=='l')
                    iO->length=strtoul(argv[i+1],&endptr,0) & ~0x3;
                else if(argv[i][j]=='s')
                    iO->timedExecSeconds=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='z')
                    iO->exitOnErrSeconds=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='y')
                    iO->resetIfErrSeconds=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='b')
                    iO->boostRatio = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='k')
                    iO->alignment = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='m')
                    iO->mode = strtoul(argv[i+1],&endptr,0);
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
            if(              argv[i][j]=='Y') iO->doInteractive = 0;
            else if(         argv[i][j]=='P') iO->doPIO = 1;
            else if(         argv[i][j]=='D') iO->doPIO = 0;
            else if(         argv[i][j]=='X') iO->testValidUnit = 1;
            else if(         argv[i][j]=='Z') iO->doConfig = 0;
            else if(         argv[i][j]=='R') iO->doPIOreads = 1;
            else if(         argv[i][j]=='W') iO->doPIOwrites = 1;
            else if(toupper(argv[i][j])=='V') iO->showErrors = 1;
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
    if(iO->boostRatio == 0)
       iO->boostRatio = 1;

    if(doPIO_DISABLED != 0 && iO->doPIO == 1)
    {
        printf("forced DMA");
        iO->doPIO =0;
    }

    if( !iO->doPIO && iO->doPIOreads )
        printf("!!!!WARNING: PIO reads overriding DMA reads due to -R !!!!\n");
    if( !iO->doPIO && iO->doPIOwrites )
        printf("!!!!WARNING: PIO writes overriding DMA writes due to -W !!!!\n");
    if( iO->doPIOreads && iO->doPIOwrites )
        iO->doPIO = 1;

    buildOptionsString(optionsString, iO);        /* update optionsString */
}

static void printHelpLevel1( iOptionsType *iO, char *optionsString)
{
    printf( "%s %s\n\n", APP_VER_STRING, APP_VARIANT );
    printf(
"Usage: gtbert [-u unit] [-s seconds] [-l packetLength] [-o offset] [-m mode]\n"
"              [-z exitOnErrorSeconds] [-y resetOnErrorSeconds] [-hPDYXV]\n\n");
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
"  -u # - unit number\n"
"  -P   - PIO, perform data transfers with PIO\n"
"  -D   - DMA, perform data transfers with DMA\n"
"  -l # - packet length in bytes\n"
"  -o # - offset (address) in the shared GT memory\n"
"  -s # - seconds to execute the test (default = forever)\n"
"  -m # - data pattern mode (-1 for automatic mode variation)\n"
"  -h   - print this help menu\n"
"  -y # - reset, ignore errors occurring before # seconds\n"
"  -z # - exit after # seconds if errors have occurred\n"
"  -V   - verbose, displays errors\n"
"  -X   - tests if unit specified with -u exists. (returns 0 on success)\n"
"  -Y   - non-interactive mode. (No exit/reset thread)\n"
"  -Z   - do not modify device configuration (See Notes)\n"
"  -b # - boost (more writes then reads)\n"
/*
"  -R   - Use PIO for reads, even if DMA is selected\n"
"  -W   - Use PIO for writes, even if DMA is selected\n"
"  -k # - Align transfer buffers to # bytes above 4K alignment,\n"
"         0 to 4095 and will be rounded down to a multiple of 4\n"
*/
"Runtime options:\n"
"   q - quit,                  r - reset,\n"
"   v - toggle error display,  m - toggle data pattern modes,\n"
"   h - runtime help\n"
"Notes:   Run `gtbert -h 1' for more information.\n"
"         By default, gtbert will set write-last, tx-path, and rx-path\n"
"         switches to on for GT devices.\n");
}

/**************************************************************************/
/*  function:     printHelpLevel3                                         */
/*  description:  Print really detailed hints                             */
/**************************************************************************/
static void printHelpLevel3()
{
    printf("\n"
"   Gtbert performs data transfers and performs verification on the data.\n"
"It calculates the bit-error-rate (BER) based on the number of bytes received,\n"
"sent, and the number of bytes in error as well as some other error statistics\n"
"available on particular hardware. The status line shows the elapsed time (dT)\n"
"in seconds, receive/read request count (Rx), missed request count (Mx) of data\n"
"sent but not received, timeout count (To), error count including timeouts (Ex),\n"
"old data bytes being received (Eo), logarithm of the byte error rate (LgB),\n"
"and logarithm of the code violations rate (LgV). Due to the variety of\n"
"error detectors and error filters available on the particular hardware used\n"
"(GT/SCRAMNet/SL240), the number of errors accounted for can be greater\n"
"or smaller than actually experienced in the intended test data stream.\n"
"For small BER though the estimated error rates are sufficiently accurate for\n"
"the intended purposes.\n"
"   Currently, Ex count does not include Eo count. Eo is not (though it should\n"
"be) included in LgB calculation as well. LgB displayed might be misleading\n"
"if either no data is being received (Rx not changing), or only old data is\n"
"seen (Eo changing), or there are many timouts (To changing).\n"
"   The purpose of gtbert is to test a device's transceiver and estimate its\n"
"receiver sensitivity. Gtbert modifies the device configuration to ensure \n"
"that data is passed through the transceiver (use -Z to override).\n");
    printf(
"   To run gtbert the device must be able to read its own written data.\n"
"This may be accomplished via a loop-back connector or a loop-topology.\n");
    printf(
"   To perform PIO operations specify option -P.  By default data is\n"
"transferred through DMA.\n");
    printf(
"   Use the -s option to limit the number of seconds that the test will run.\n"
"The -y option specifies the number of seconds for which gtbert will reset\n"
"error statistics and timers when error occurs.  The -z option limits the\n"
"number of seconds the test will run if errors have occurred.\n");
    printf(
"   The -X option may be used to test for the existence of a unit.  The -Y\n"
"option eliminates the keyboard input thread.  The -y, -z, -X, and -Y options\n"
"are intended for use in spawning or exec-ing from within other tasks.\n");
}

#ifdef HW_SL240
static void doCleanup(HANDLE hDriver, uint32 *recvBuff,
                      uint32 buffSize, uint32 print)
{
    int i;
    uint32 flags, bytesRecv;
    time_t startTime = time(NULL);

    for(i = 0; (i < 2000000); i++)
    {
        flags=0;
        if( fxsl_recv(hDriver, (uint8*)recvBuff, buffSize,
                      &flags, 5*TM, &bytesRecv) == DRIVER_TIMEOUT )
        {
            break;
        }

        if(((uint32)difftime(time(NULL), startTime)) > 2)
        {
            if( print ) printf("\ncleanout failure \n");
            break;
        }

        if( print && ((i & 0xFF) == 0xFF) )
        {
            printf("\r%2.2X \r",(i>>8) & 0xFF);
            fflush(stdout);
            usysMsTimeDelay(0);
        }
    }
}
#endif
