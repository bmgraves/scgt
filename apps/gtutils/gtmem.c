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

#define APP_VER_STRING "GT Memory/Register Access (gtmem) rev. 1.05 (2011/08/30)"

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
            int argc; char argv0[]="gtmem"; char *argv[VXL]={argv0};    /**/
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
    #define APP_VxW_NAME xgtmem
#elif HW_SCFF /* Scramnet */
    #include "scrplus.h"
    #define HAND uint32
    #define APP_VxW_NAME sgtmem
#else /* GT */
    #define HW_SCGT
    #include "scgtapi.h"
    #define HAND scgtHandle
    #define APP_VxW_NAME gtmem
#endif

/**************************************************************************/
/************** APPLICATION-SPECIFIC CODE STARTS HERE *********************/
/**************************************************************************/

/**************************************************************************/
/**************************  D A T A  T Y P E S  **************************/
/**************************************************************************/

#define COMMON_OPTIONS \
    uint64    data,       increment; \
    uint32    direction,  bytesPerRW,  mask,    count, length,   iteration; \
    uint32    msPeriod,   maxLength,   offset,  mode,  columns,  maxFinds; \
    uint32    doFind,     doRegisters, doDMA,   doPIO, doMaxLen; \
    uint32    doFindNotX, doNetwork, dispMode,  usrCount, doDmaFlags; \
    uint8     *sysMemBuf, *sysMemBuf2; \
    uint8     *allocBuf, *allocBuf2; \
    volatile uint32  *boardAddressPIO; \
    HAND  hDriver;

typedef struct {
    volatile uint32      timeToExit;
    uint32      running;
    usysThreadParams   pt;
} exitParms;

typedef struct {
    COMMON_OPTIONS
    uint32    unit,     helpLevel,  popMemSize,  mapMemSize;
    exitParms parms;
} iOptionsType;

/**************************************************************************/
/******************  F U N C T I O N  P R O T O T Y P E S  ****************/
/**************************************************************************/
static void buildOptionsString( char *optionsString, iOptionsType *iO );
static int parseCommand(int argc, char **argv, iOptionsType *iO, char *optionsString);
static void printHelpLevel1(iOptionsType *iO, char *optionsString);
static void printHelpLevel2(void);
static void printHelpLevel3(void);
static void *getRuntimeChar(void *voidp);
static int gtmemVerifyOptions(iOptionsType *iO);
static int gtmemPerformAccess(iOptionsType *iO);
#if defined PLATFORM_WIN || defined PLATFORM_VXWORKS || defined PLATFORM_RTX
#define strtoull strtoullx
uint64 strtoullx(const char *cp, char **endp, unsigned int base);
#endif

#define gtmemErrorPrint(x)     printf("ERROR: code %d\n",x)

/**************************************************************************/
/***********************  D E F I N E S & M A C R O S  ********************/
/**************************************************************************/

#define READ   0
#define WRITE  1
#define MEM_TYPE 0x0100
#define REG_TYPE 0x1000
#define DMA    (MEM_TYPE | 1)  /*DMA memory access*/
#define PIO    (MEM_TYPE | 2)  /*PIO memory access*/
#define REG    (REG_TYPE | 1)  /*control registers*/
#define NET    (REG_TYPE | 2)  /*network registers*/

#define DMA_BYTE_SWAP  0x1     /* swap bytes during DMA transfers */
#define DMA_WORD_SWAP  0x2     /* swap 32-bit words during DMA transfers */

/* display modes */
#define D_NO_OFFSETS   0x1
#define D_ADD_OX       0x2
#define D_ONE_COLUMN   0x4
#define D_ONE_ROW      0x8
#define D_REL_OFFSETS  0x10
#define D_WD_SIZE_OFF  0x20

#define RW_BYTES_VALID(x)   ((x==1)||(x==2)||(x==4)||(x==8))
/* MAX_BUF_LEN is the largest buffer size gtmem will use to rd/wr at a time.
   The buffer is recycled if larger lengths are selected. Compare logic is
   limited to this size.  If too small, DMA speed will suffer. */
#define MAX_BUF_LEN         0x10000   /* must be a multiple of 0x8 */
#define MAX_CREG_OFFSET     (NUM_CREGS * CREG_SIZE)
#define MAX_NREG_OFFSET     (NUM_NREGS * NREG_SIZE)
#define MAX_PIO_OFFSET      iO->mapMemSize
#define MAX_DMA_OFFSET      iO->popMemSize

/* The following is to mask the actual hardware dependency */

#ifdef HW_SL240 /* SLxxx */
    #define APP_VARIANT "sl240 (xgtmem) Edition"
    #define DRIVER_SUCCESS  FXSL_SUCCESS
    #define doPIO_DISABLED  1
    #define doDMA_DISABLED  0
    #define doCREG_DISABLED 0
    #define doNREG_DISABLED 1
    #define DRIVER_OPEN(x,y) { if(fxsl_open(x,y)) \
           { printf("ERROR: could not open driver for unit %d\n", x); \
             return 0; } \
            iO.boardAddressPIO = (volatile uint32 *)NULL; \
            iO.mapMemSize = iO.popMemSize = 0x100000; /* 1MB, arbitrary */ }       
    #define DRIVER_CLOSE(x) fxsl_close(x)

    #define DMA_READ_GT(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead) \
               (fxsl_recv(*pHandle,pDest, sizeByte, flags, 0, pBytesRead))

    #define DMA_WRITE_GT(pHandle,smOffset,pSource,sizeByte,flags,pBytesWritten)\
               (fxsl_send(*pHandle, pSource, sizeByte, flags,0,pBytesWritten))

    #define CREG_SIZE                   4
    #define NUM_CREGS                   64    
    #define READ_CR(handle,off)         (fxsl_read_CR(handle, off, &regVal), regVal)
    #define WRITE_CR(handle,off,value)  fxsl_write_CR(handle, off, value)
    #define NREG_SIZE                   4
    #define NUM_NREGS                   0
    #define READ_NR(handle,off)         0
    #define WRITE_NR(handle,off,value)  ret=0;
    #define BYTE_SWAP  0     /* Not applicable */
    #define WORD_SWAP  0     /* Not applicable */
#elif HW_SCFF /* Scramnet */
    #define APP_VARIANT "SCRAMNet (sgtmem) Edition"
    #define DRIVER_SUCCESS  0
    #define doPIO_DISABLED  0
    #define doDMA_DISABLED  0
    #define doCREG_DISABLED 0
    #define doNREG_DISABLED 1
    #define DRIVER_OPEN(x,y) {sp_scram_init(0); \
              sp_stm_mm(Long_mode); \
              scr_csr_write(2, (unsigned short) 0xd040); \
              scr_csr_write(0, (unsigned short) 0x8003); \
              scr_csr_write(10,(unsigned short)(scr_csr_read(10) | 0x11)); \
              iO.boardAddressPIO = (volatile uint32 *)get_base_mem();      \
              iO.mapMemSize = iO.popMemSize = sp_mem_size();}
    #define DRIVER_CLOSE(x)

    #define DMA_WRITE_GT(pHandle,smOffset,pSource,sizeByte,flags,pBytesWritten)\
                (scr_dma_write(pSource, smOffset, sizeByte), \
                 *pBytesWritten = sizeByte, DRIVER_SUCCESS)

    #define DMA_READ_GT(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead) \
                (scr_dma_read(pDest, smOffset, sizeByte), \
                 *pBytesRead = sizeByte, DRIVER_SUCCESS)

    extern int sp_plus_flag;
    #define CREG_SIZE                   2
    #define NUM_CREGS                  (8<<sp_plus_flag)    /*16bit regs */
    #define READ_CR(handle,off)         scr_csr_read(off/CREG_SIZE)
    #define WRITE_CR(handle,off,value)  scr_csr_write(off/CREG_SIZE, value);
    #define NREG_SIZE                   2
    #define NUM_NREGS                   0
    #define READ_NR(handle,off)         0
    #define WRITE_NR(handle,off,value)  ret=0;
    #define BYTE_SWAP  0     /* Not applicable */
    #define WORD_SWAP  0     /* Not applicable */
#elif defined HW_SCGT /* GT */
    #define APP_VARIANT ""
    #define DRIVER_SUCCESS SCGT_SUCCESS
    #define doPIO_DISABLED  0
    #define doDMA_DISABLED  0
    #define doCREG_DISABLED 0
    #define doNREG_DISABLED 0
    #define DRIVER_OPEN(x,y) {  scgtDeviceInfo devInfo;                  \
            if (scgtOpen(x,y))                                           \
            {                                                            \
                printf("ERROR: could not open driver for unit %d\n", x); \
                return 0;                                                \
            }                                                            \
            iO.boardAddressPIO = scgtMapMem(y);                          \
            scgtGetDeviceInfo(y, &devInfo);                              \
            iO.popMemSize = devInfo.popMemSize;                          \
            iO.mapMemSize = devInfo.mappedMemSize; }
    #define DRIVER_CLOSE(x) { scgtUnmapMem(&(x)); scgtClose(&(x)); }

    #define DMA_WRITE_GT(pHandle,smOffset,pSource,sizeByte,flags,pBytesWritten)\
                scgtWrite(pHandle,smOffset,pSource,sizeByte,*flags, \
                          pBytesWritten, NULL)
    #define DMA_READ_GT(pHandle,smOffset,pDest,sizeByte,flags,pBytesRead) \
                scgtRead(pHandle,smOffset,pDest,sizeByte,*flags,pBytesRead)

    #define CREG_SIZE                   4
    #define NUM_CREGS                   64
    #define READ_CR(handle,off)         scgtReadCR(&(handle), off)
    #define WRITE_CR(handle,off,value)  scgtWriteCR(&(handle),off,value)
    #define NREG_SIZE                   4
    #define NUM_NREGS                   2048
    #define READ_NR(handle,off)         scgtReadNMR(&(handle), off);
    #define WRITE_NR(handle,off,value)  scgtWriteNMR(&(handle),off,value);
    #define BYTE_SWAP  SCGT_RW_DMA_BYTE_SWAP  /* swap bytes during DMA */
    #define WORD_SWAP  SCGT_RW_DMA_WORD_SWAP  /* swap 32-bit words during DMA*/
#endif

/**************************************************************************/
/************************** G T M E M   C O D E ***************************/
/**************************************************************************/

MAIN_IN                                                     /* gtmem main */
    iOptionsType   iO;
    char           optionsString[200];
    int            i, ret = 0;

    PARSER;    /* preparing argc and agrv if not provided by OS (VxWorks) */

    memset(&iO, 0, sizeof(iOptionsType));
    ret = parseCommand(argc, argv, &iO, optionsString);      /* parsing options */

    if( (iO.helpLevel != 0) || (ret != 0) ) /*1, 2, 3, ...*/
    {
        printHelpLevel1(&iO,optionsString);
        return ret;
    }

    DRIVER_OPEN(iO.unit, (HAND *) &iO.hDriver);

    if ( gtmemVerifyOptions(&iO) )
    {
        ret = -1;
        goto close;
    }

    /* No points of failure beyond this point, except api calls */

    buildOptionsString(optionsString, &iO); /* update optionsString */

    iO.parms.timeToExit = iO.parms.running = 0;

    /* spawn exit thread if periodic mode, or large length given which
       the user may wish to abort due to slow serial connection, etc. */
    if( iO.msPeriod || (iO.length > 8192/*(MAX_BUF_LEN/16)*/))
    {   /* spawn exit thread */
        iO.parms.pt.parameter = &iO.parms;
        iO.parms.pt.priority = UPRIO_HIGH; /* only use UPRIO_HIGH */
        sprintf(iO.parms.pt.threadName, "gtmemExit");
        usysCreateThread(&iO.parms.pt, getRuntimeChar);
        /* Give thread a moment to start up. Needed on VxWorks, Solaris 8 */
        usysMsTimeDelay(50);
    }

    iO.iteration = 0;
    do{
        if( !iO.msPeriod && (iO.count > 1))
            printf("\rRunning: %s\n", optionsString);
        else if ( iO.msPeriod )
            printf("\rRunning(%d): %s", iO.iteration, optionsString);

        fflush(stdout);
        ret = gtmemPerformAccess(&iO);
        iO.iteration++;

        if( iO.msPeriod )
        {   /* avoid long delays for quicker 'q' response */
            for(i=0; (i < (int)(iO.msPeriod/2000)) && !iO.parms.timeToExit; i++)
                usysMsTimeDelay( 2000 );
            if( (iO.msPeriod % 2000) && !iO.parms.timeToExit)
                usysMsTimeDelay( iO.msPeriod % 2000 );
        }

    }while(iO.msPeriod && !iO.parms.timeToExit);

close:
    DRIVER_CLOSE(iO.hDriver);

    if( iO.parms.running ) /* a VxWorks "must-have" */
        usysKillThread(&iO.parms.pt);

    if( iO.allocBuf != NULL )
        usysMemFree(iO.allocBuf);
    if( iO.allocBuf2 != NULL )
        usysMemFree(iO.allocBuf2);

    return( ret );
}                                          /* end of MAIN_IN that is main */

/***************************************************************************/
/* Function:    gtmemVerifyOptions                                         */
/* Description: Verifies that options are correct for testing              */
/***************************************************************************/
static int gtmemVerifyOptions(iOptionsType *iO)
{
    int val = iO->doRegisters + iO->doDMA + iO->doPIO + iO->doNetwork;

    if( val > 1 )
    {
        printf("Multiple access modes specified\n");
        return -1;
    }

    if( val )                     /*Forced access type, determine which one*/
    {   
        iO->mode = (iO->doDMA ? DMA : (iO->doPIO ? PIO : (iO->doRegisters ?
                                REG : (iO->doNetwork ? NET : iO->mode))));
    }

    if( iO->mode == REG )
        iO->bytesPerRW = CREG_SIZE;
    else if( iO->mode == NET )
        iO->bytesPerRW = NREG_SIZE;
    else if ( !RW_BYTES_VALID(iO->bytesPerRW) )
    {
        printf("Invalid operation size specified!\n");
        return -1;
    }
    else if((iO->mode == DMA) && (iO->bytesPerRW != 4))
    {
        if(iO->doDMA == 1)
            printf("Forcing PIO for non-32bit memory access!\n");
        iO->mode = PIO;  /* must do PIO */
    }

    if( iO->mode == PIO )
    {
        if( iO->boardAddressPIO == NULL )
        {
            printf("PIO address is NULL\n");
            return -1;
        }
        if( MAX_PIO_OFFSET == 0 )
        {
            printf("No memory mapped for PIO.\n");
            return -1;
        }
    }

    if( iO->direction == WRITE )
        iO->doFind = 0;

#if 0
    if( iO->doFind || (iO->direction == WRITE) )
        iO->msPeriod = 0;
#endif

    if( iO->doFind && !iO->length && !iO->offset && !iO->usrCount)
        iO->doMaxLen = 1;

    iO->maxLength = iO->mode==REG ? MAX_CREG_OFFSET : 
                    (iO->mode == NET ? MAX_NREG_OFFSET :
                    (iO->mode == PIO ?  MAX_PIO_OFFSET : MAX_DMA_OFFSET));

    if( iO->doMaxLen )
    {
        iO->length = iO->maxLength;
        iO->offset = 0;

        if( (iO->mode == PIO) && (MAX_PIO_OFFSET < MAX_DMA_OFFSET))
        {
            printf("WARNING: PIO range limited to mapped region (%u bytes).\n",
                    MAX_PIO_OFFSET);
        }
    }

    if( iO->dispMode & D_ONE_ROW)
        iO->columns = iO->maxLength; /* may be larger than needed */
    if( iO->dispMode & D_ONE_COLUMN)
        iO->columns = 1;
    if( !iO->columns ) /* set default number of columns */
        iO->columns = (16/iO->bytesPerRW) <= 8 ? (16/iO->bytesPerRW) : 8;
    if( !iO->length ) /* length not given, adjust for size and number of ops */
        iO->length = iO->count * iO->bytesPerRW;
    else /* length overrides count */
        iO->count = iO->length / iO->bytesPerRW;

    if( (iO->offset + iO->length ) > iO->maxLength )
    {
        iO->length = iO->maxLength - iO->offset;
        iO->count = iO->length / iO->bytesPerRW;
    }

    if( iO->count == 1 )
        iO->dispMode |= D_NO_OFFSETS | D_ADD_OX;


    iO->doDmaFlags = ((iO->doDmaFlags & DMA_BYTE_SWAP) ? BYTE_SWAP : 0) |
                     ((iO->doDmaFlags & DMA_WORD_SWAP) ? WORD_SWAP : 0);

    if(((iO->mode == PIO) && doPIO_DISABLED) ||
       ((iO->mode == DMA) && doDMA_DISABLED) ||
       ((iO->mode == REG) && doCREG_DISABLED) ||
       ((iO->mode == NET) && doNREG_DISABLED) )
        printf("%s not supported on this hardware!\n", iO->mode==PIO ? "PIO":
             (iO->mode==DMA ? "DMA": (iO->mode==REG ? "Register access" :
              "Network Register Access")));
    else if( iO->offset >= iO->maxLength )
        printf("Invalid %s offset. Max offset: 0x%x.\n",
               (iO->mode&REG_TYPE)? "register":"memory", iO->maxLength);
    else if ( iO->length % iO->bytesPerRW )
        printf("Length is not a multiple of the selected operating size!\n");
    else if( iO->offset % iO->bytesPerRW )
        printf("Offset (%u) not aligned to operation size (-b %d).\n",
               iO->offset, iO->bytesPerRW);
    else if((iO->mode==REG) && (iO->direction == WRITE) && (iO->count > 1) )
        printf("Invalid count (%d) for register write\n",iO->count);
    else if((iO->mode==REG) && (iO->direction == WRITE) && (iO->mask == 0) )
        printf("Non-zero mask required for register write\n");
/*
    else if( iO->data && (iO->bytesPerRW < 8) &&
             (iO->data >= ((uint64)0x1 << (iO->bytesPerRW*8))) )
        printf("Data value 0x%x%.8x too large for %d-bit operation\n",
               (uint32)(iO->data>>32), (uint32)(iO->data),(iO->bytesPerRW*8));
*/
    else if((val = iO->length < MAX_BUF_LEN ? iO->length : MAX_BUF_LEN)==0)
        printf("Length is zero\n");
    else if( !(iO->allocBuf = (uint8*)usysMemMalloc(val+4096+4096)) )
        printf("Memory allocation failure.\n");
    else if( iO->msPeriod && (iO->length <= MAX_BUF_LEN) &&
             !(iO->allocBuf2 = (uint8*)usysMemMalloc(val+4096+4096)) )
        printf("Memory allocation failure.\n");  /*for comparisons*/
    else
    {
        /*
         * Align buffers to 4096 byte boundary for use in DMA transfers.
         * Allocated 4096 * 2 extra bytes, so we can align to a common 
         * memory page size of 4K, and ensure the end of our buffer does
         * not share a cache line with another program.
         */
        if(iO->allocBuf)
        {
            iO->sysMemBuf =
              (uint8*)(((uintpsize)iO->allocBuf + 4095) & ~((uintpsize)4095));
        }

        if(iO->allocBuf2)
        {
            iO->sysMemBuf2 =
              (uint8*)(((uintpsize)iO->allocBuf2 + 4095) & ~((uintpsize)4095));
        }

        return 0;
    }

    return -1;
}

/***************************************************************************/
/* Function:    gtmemPrintResults                                          */
/* Description: Prints data buffer and formats it into columns             */
/***************************************************************************/

#define DIFF(x) (iO->iteration && compareBuf && \
                 (((x*)buf)[i]!=((x*)compareBuf)[i]))? '*': ch
#define DOOX   ((iO->dispMode & D_ADD_OX) ? "0x": "")

static int gtmemPrintResults(iOptionsType *iO, void *buf, void *compareBuf,
                      int lengthInBytes, int gtMemOffset, int *column)
{
    int i, prtOffset, col = *column;
    char ch = ((iO->count==1)?  0 : ' '); /*left justify single value prints*/
    char *ox = DOOX;
    for(i=0; (i<(int)(lengthInBytes/iO->bytesPerRW)) && !iO->parms.timeToExit;
        i++, gtMemOffset+=iO->bytesPerRW)
    {
        if( !(col%iO->columns) && !(iO->dispMode & D_NO_OFFSETS))
        {
            prtOffset = gtMemOffset;

            if( iO->dispMode & D_REL_OFFSETS )
                prtOffset = prtOffset - iO->offset;
            if( iO->dispMode & D_WD_SIZE_OFF )
                prtOffset = prtOffset / iO->bytesPerRW;
                
            printf("0x%.*x: ", (iO->mode & REG_TYPE)? 2 : 7, prtOffset);
        }

        switch( iO->bytesPerRW )
        {
            case 4: printf("%c%s%.8x ", DIFF(uint32), ox,((uint32*)buf)[i]); break;
            case 1: printf("%c%s%.2x ", DIFF(uint8),  ox,((uint8* )buf)[i]); break;
            case 2: printf("%c%s%.4x ", DIFF(uint16), ox,((uint16*)buf)[i]); break;
            case 8: printf("%c%s%.8x%.8x ", DIFF(uint64), ox,
                           (uint32)(((uint64*)buf)[i] >> 32),
                           (uint32)(((uint64*)buf)[i]));
                    break;
        }

        if( !((++col)%iO->columns) )
            printf("\n");
    }
    *column = col%iO->columns;
    return 0;
}

/***************************************************************************/
/* Function:    gtmemSearch                                                */
/* Description: Searches for pattern in buffer & prints offsets where found*/
/***************************************************************************/
static int gtmemSearch(iOptionsType *iO, uint8 *buf,  uint32 lengthInBytes,
                int gtMemOffset,  uint64 pattern, uint32 *count, int *column)
{
    int i, pr, col = *column;

    for(i=0, pr=iO->doFindNotX; i<(int)(lengthInBytes/iO->bytesPerRW) && *count;
        i++, gtMemOffset+=iO->bytesPerRW, pr=iO->doFindNotX)
    {
        switch( iO->bytesPerRW )
        {
            case 4: if( ((uint32*)buf)[i] == (uint32)pattern ) pr--; break;
            case 1: if( ((uint8* )buf)[i] == (uint8 )pattern ) pr--; break;
            case 2: if( ((uint16*)buf)[i] == (uint16)pattern ) pr--; break;
            case 8: if( ((uint64*)buf)[i] == (uint64)pattern ) pr--; break;
        }

        if( pr )
        {
            (*count)--;
            printf("%s%.*x ",DOOX, (iO->mode & REG_TYPE) ? 2 : 7, gtMemOffset);
            if( !((++col)%iO->columns) )
                printf("\n");
        }
    }

    *column = col % iO->columns;
    return 0;
}

/***************************************************************************/
/* Function:    gtmemFillBuf                                               */
/* Description: Fill buf with data                                         */
/***************************************************************************/
static int gtmemFillBuf(iOptionsType *iO, void* buf,
                        int lengthInBytes, uint64* data)
{
   int i, j = (int)(lengthInBytes/iO->bytesPerRW);
   uint64 inc = iO->increment, d = *data;

   #define FORLOOP(x)  for(i=0; i<j; i++, d+=inc){ ((x*)buf)[i] =(x)d; }

   switch( iO->bytesPerRW )
   {
       case 4: FORLOOP(uint32);   break;
       case 1: FORLOOP(uint8);    break;
       case 2: FORLOOP(uint16);   break;
       case 8: FORLOOP(uint64);   break;
   }
   *data = d;
   return 0;
}

/***************************************************************************/
/* Function:    Sized memory copying functions                             */
/* Description: Copy from source buffer to destination buffer              */
/***************************************************************************/
static void gtmemCopy8(volatile uint8* src, volatile uint8* dest, int numBytes)
{
    int i, length = numBytes/sizeof(uint8);
    for(i=0; i<length; i++) dest[i]=src[i];
}

static void gtmemCopy16(volatile uint16* src, volatile uint16* dest, int numBytes)
{
    int i, length = numBytes/sizeof(uint16);
    for(i=0; i<length; i++) dest[i]=src[i];
}

static void gtmemCopy32(volatile uint32* src, volatile uint32* dest, int numBytes)
{
    int i, length = numBytes/sizeof(uint32);
    for(i=0; i<length; i++) dest[i]=src[i];
}

static void gtmemCopy64(volatile uint64* src, volatile uint64* dest, int numBytes)
{
    int i, length = numBytes/sizeof(uint64);
    for(i=0; i<length; i++) dest[i] = src[i];
}

/***************************************************************************/
/* Function:    gtmemDoTransfer                                            */
/* Description: Transfer data to/from memory via DMA/PIO                   */
/***************************************************************************/
static int gtmemDoTransfer( iOptionsType *iO, uint8 *sys8, uint8 *pio8,
        uint32 gtByteOffset, int type, int bytes, int direction, int bytesPerRW)
{
    void *src, *dest;
    int ret = -1;
    uint32 bytesReported;
    uint32 flags =0;

    if( type == DMA )
    {
        flags = iO->doDmaFlags;

        if( direction == READ ) /*DMA READ*/
        {
            ret = DMA_READ_GT(&iO->hDriver, gtByteOffset, sys8,
                              bytes, &flags, &bytesReported);
        }
        else /*DMA WRITE*/
        {
            ret = DMA_WRITE_GT(&iO->hDriver, gtByteOffset, sys8,
                               bytes, &flags, &bytesReported);
        }
    }
    else if( type == PIO )
    {
        if( direction == READ ) /*PIO READ*/
        {
           src = &pio8[gtByteOffset];
           dest = sys8;
        }
        else /*PIO WRITE*/
        {
           dest = &pio8[gtByteOffset];
           src = sys8;
        }

        switch( bytesPerRW )
        {
            case 4: gtmemCopy32(src, dest, bytes); ret=0; break;
            case 1: gtmemCopy8 (src, dest, bytes); ret=0; break;
            case 2: gtmemCopy16(src, dest, bytes); ret=0; break;
            case 8: gtmemCopy64(src, dest, bytes); ret=0; break;
        }
    }
    else if( type == REG )
    {
        int i;
        uint32 regVal=0;
        uint32 data=(uint32)iO->data;

        if( direction == READ )
        {
            ret = 0;
            for(i=0; (i<bytes) && (gtByteOffset<iO->maxLength) && !ret;
                i+=bytesPerRW, gtByteOffset+=bytesPerRW)
            {
                regVal = READ_CR(iO->hDriver, gtByteOffset);

                switch( bytesPerRW ) /* for hardware independence */
                {
                    case 4: *((uint32*)&(sys8[i]))=(uint32)regVal; break;
                    case 1: *((uint8*)&(sys8[i]))=(uint8)regVal; break;
                    case 2: *((uint16*)&(sys8[i]))=(uint16)regVal; break;
                    case 8: *((uint64*)&(sys8[i]))=(uint64)regVal; break;
                }
            }
        }
        else if( direction == WRITE )
        {   /* limited to one register */
            ret = 0;
            regVal = READ_CR(iO->hDriver, gtByteOffset);
            regVal &= ~iO->mask; /*set all bits which the mask affects to zero*/
            regVal |= (data & iO->mask); /*set zero'd bits to the masked data*/
            WRITE_CR(iO->hDriver, gtByteOffset, regVal);
        }
    }
    else /* type == NET */
    {
        int i;
        uint32 regVal=0;

        if( direction == READ )
        {
            ret = 0;
            for(i=0; (i<bytes) && (gtByteOffset<iO->maxLength) && !ret;
                i+=bytesPerRW, gtByteOffset+=bytesPerRW)
            {
                regVal = READ_NR(iO->hDriver, gtByteOffset);

                switch( bytesPerRW ) /* for hardware independence */
                {
                    case 4: *((uint32*)&(sys8[i]))=(uint32)regVal; break;
                    case 1: *((uint8*)&(sys8[i]))=(uint8)regVal; break;
                    case 2: *((uint16*)&(sys8[i]))=(uint16)regVal; break;
                    case 8: *((uint64*)&(sys8[i]))=(uint64)regVal; break;
                }
            }
        }
        else if( direction == WRITE )
        {   /* Network register are write-to-clear */
            ret = 0;
            for(i=0; (i<bytes) && (gtByteOffset<iO->maxLength) && !ret;
                i+=bytesPerRW, gtByteOffset+=bytesPerRW)
            {
                WRITE_NR(iO->hDriver, gtByteOffset, (uint32)iO->data);
            }
        }
    }

    return ret;
}

/***************************************************************************/
/* Function:    gtmemMemoryAccess                                          */
/* Description: Perform memory reads/writes according to input params      */
/***************************************************************************/
static int gtmemPerformAccess(iOptionsType *iO)
{
    int i, ret;
    uint32 gtByteOffset = iO->offset;
    int lengthInBytes = 0, column = 0;
    uint32 maxFinds = iO->maxFinds;
    uint64 data = iO->data;

    for( i=0; (i < (int)iO->length) && !iO->parms.timeToExit;
         i+=MAX_BUF_LEN, gtByteOffset+=MAX_BUF_LEN )
    { /* MAX_BUF_LEN must be a multiple of 8 for all sized ops to succeed*/
        lengthInBytes = ((iO->length-i)<MAX_BUF_LEN?(iO->length-i):MAX_BUF_LEN);

        if( (iO->direction == WRITE) && !(iO->mode & REG_TYPE) &&
            ((i==0)||iO->increment) )
            gtmemFillBuf(iO, iO->sysMemBuf, lengthInBytes, &data);

        if((ret = gtmemDoTransfer(iO, (uint8*)iO->sysMemBuf,
                             (uint8*)iO->boardAddressPIO,
                             gtByteOffset, iO->mode, lengthInBytes,
                             iO->direction, iO->bytesPerRW)) )
        {
             gtmemErrorPrint(ret);
             return -1;
        }

        if( iO->doFind )
        {
            gtmemSearch(iO, (uint8*)iO->sysMemBuf, lengthInBytes,
                        gtByteOffset, iO->data, &maxFinds, &column);

            if( maxFinds == 0 )
                break;
        }
        else if ( iO->direction == READ )
        {
            if(!iO->iteration && iO->sysMemBuf2) /*force first memcmp to fail*/
                iO->sysMemBuf2[0] = iO->sysMemBuf[0] + 1;

            if( (iO->length <= MAX_BUF_LEN) && iO->sysMemBuf2 &&
                 !memcmp(iO->sysMemBuf, iO->sysMemBuf2,lengthInBytes) )
                continue;

            if(iO->msPeriod && !i)
                printf("\n");
            gtmemPrintResults(iO, iO->sysMemBuf, iO->sysMemBuf2,
                              lengthInBytes, gtByteOffset, &column);

            if( iO->msPeriod && iO->sysMemBuf2 )
            {
                uint8 *temp = iO->sysMemBuf;
                iO->sysMemBuf = iO->sysMemBuf2;
                iO->sysMemBuf2 = temp;
            }
        }
    }

    if( column )
        printf("\n");

    return 0;
}

/***************************************************************************/
/* Function:    getRuntimeChar                                             */
/* Description: Thread that reads keyboard                                 */
/***************************************************************************/
static void *getRuntimeChar(void *voidp)
{
    exitParms *pparms = (exitParms *) voidp;
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
/*  function:     buildOptionsString                                      */
/*  description:  Updates the options string                              */
/**************************************************************************/
static void buildOptionsString( char *optionsString, iOptionsType *iO )
{
    char *str = optionsString;
    int off = 0;
    off += sprintf(&str[off],
              "gtmem -%s%s%s%s%s%s -u %d -o 0x%x -c %d -b %d -d %#x ",
              iO->direction==READ ? "r":"",  iO->mode==PIO ? "P":"",
              iO->mode==DMA ? "D":"",        iO->mode==REG ? "R":"",
              iO->mode==NET ? "N":"",        iO->doMaxLen  ? "A":"",
              iO->unit, iO->offset, iO->count, iO->bytesPerRW, iO->dispMode);

    if(iO->direction == WRITE)
    {
        off += sprintf(&str[off],"-w 0x%.0x%.8x ",
                    (uint32)(iO->data>>32), (uint32)(iO->data));

        if(iO->mode == REG)
            off += sprintf(&str[off],"-m 0x%x ", iO->mask);
        else if( iO->increment )
        {
            off += sprintf(&str[off],"-i 0x%.0x%.8x ",
                 (uint32)(iO->increment>>32),(uint32)(iO->increment));
        }
    }
    if(iO->doFind)
    {
        off += sprintf(&str[off],"-f 0x%.0x%.8x ",
                (uint32)(iO->data>>32), (uint32)(iO->data));

        if( iO->doFindNotX )
            off += sprintf(&str[off], "-E ");
        if( iO->maxFinds )
            off += sprintf(&str[off],"-n %d ", iO->maxFinds);
    }
    if(iO->msPeriod && (iO->direction == READ))
        off += sprintf(&str[off],"-p %d ", iO->msPeriod);
}

/**************************************************************************/
/*  function:     parseCommand                                            */
/*  description:  Getting argv details into internal usable form          */
/**************************************************************************/
static int parseCommand(int argc,char **argv,iOptionsType *iO,char *optionsString)
{
    char *endptr, nullchar = 0;
    int  i, j, len, tookParam=0;                         /* setting defaults */
    iO->helpLevel = 1;         iO->direction = READ;      iO->count = 1;
    iO->bytesPerRW = 4;        iO->maxFinds = 1;
    iO->mode = doPIO_DISABLED ? DMA : PIO;

    /* Initialize optionsString with default options */
    buildOptionsString(optionsString, iO);

    if(argc == 1)                                      /* start processing */
        return 0;

    for(i = 1; i < argc; i++, tookParam=0)
    {
        len = (int) strlen(argv[i]);
        if((argv[i][0] != '-') || (len < 2))
        {
            printf("\nERROR: Unexpected option: \"%s\"\n\n", argv[i]);
            return -1;
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
                    iO->offset=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='p')
                    iO->msPeriod=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='c')
                {   iO->usrCount = 1;
                    iO->count=strtoul(argv[i+1],&endptr,0);
                }
                else if(argv[i][j]=='l')
                    iO->length=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='w')
                {   iO->direction = WRITE;
                    iO->data=strtoull(argv[i+1],&endptr,0);
                }
                else if(argv[i][j]=='b')
                    iO->bytesPerRW=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='m')
                    iO->mask=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='i')
                    iO->increment=strtoull(argv[i+1],&endptr,0);
                else if(argv[i][j]=='d')
                    iO->dispMode = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='n')
                    iO->maxFinds=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='f')
                {   iO->doFind = 1;
                    iO->data=strtoull(argv[i+1],&endptr,0);
                }
                else if(argv[i][j]=='z')
                    iO->doDmaFlags = strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='k')
                    iO->columns=strtoul(argv[i+1],&endptr,0);
                else tookParam = 0;

                if( tookParam && (*endptr) )
                {
                   printf("\nInvalid parameter \"%s\" for option \"%c\"\n\n",
                           argv[i+1], argv[i][j]);
                   return -1;
                }
            }

            /* options without arguments now */
            if(              argv[i][j]=='r') iO->direction = READ;
            else if(         argv[i][j]=='P') iO->doPIO = 1;
            else if(         argv[i][j]=='D') iO->doDMA = 1;
            else if(         argv[i][j]=='R') iO->doRegisters = 1;
            else if(         argv[i][j]=='N') iO->doNetwork = 1;
            else if(         argv[i][j]=='E') iO->doFindNotX = 1;
            else if(toupper(argv[i][j])=='A') iO->doMaxLen = 1;
            else if(tolower(argv[i][j])=='h')
            {
                iO->helpLevel = (argc > 2) ? 3 : 2;
                return 0;
            }
            else if(!tookParam)
            {
                printf("\nERROR: Unexpected option: \"%c\"\n\n",argv[i][j]);
                return -1;
            }
        }
        if (tookParam) i++;
    }

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
    printf("Usage: gtmem [-P|D] [-u unit] [-o offset] [-c count] [-l length]\n"
           "             [-w data [-i increment]] [-b bytes] [-p msec_period]\n"
           "             [-A] [-f search_pattern [-E] [-n maxFinds]] [-h]\n");
    printf("       gtmem -R|N [-u unit] [-o offset] [-c count] [-l length]\n"
           "             [-A] [-w data [-m mask]] [-b bytes]\n\n");
    printf("Defaults: %s\n", optionsString);
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
"  -D   - DMA access to GT's memory\n"
"  -P   - PIO access to GT's memory\n"
"  -R   - control register access\n"
"  -N   - network register access\n"
"  -u # - unit/board number\n"
"  -r   - read memory/registers (default)\n"
"  -w # - write data pattern (see -m)\n"
"  -m # - bitmask (applicable to control register writes only, required)\n"
"  -o # - shared GT memory offset or register offset (in bytes)\n"
"  -c # - count of words/bytes on which to apply operation\n"
"  -l # - length in bytes on which to apply operation (overrides -c)\n"
"  -A   - all, operate on full memory/register range (overrides -o,-l)\n"
"  -b # - size of read/write operations in bytes: 1 - bytes,\n"
"         2 - 16bit words, 4 - 32bit words (default), 8 - 64bit words\n"
"  -p # - periodic, delay # milliseconds between iterations('q' exits).\n"
"  -i # - increment the written pattern by # for each offset written\n"
"  -f # - find/search for pattern # in memory\n"
"  -E   - modifies search (-f #) to find all patterns except # in memory\n"
"  -n # - max number of times to find the search pattern (default is 1)\n"
"  -d # - display modifier flags (positionally coded), default is 0:\n"
"         0x1 - omit offsets, 0x2 - add 0x, 0x4 - one column, 0x8 - one row\n"
"         0x10 - relative offsets, 0x20 - word offsets (see -b)\n"
"  -z # - DMA-only swapping flags: 0x1 - swap bytes, 0x2 - swap words\n"
/*"  -k # - format output into # columns\n"*/
    );
    printf("Note:  run `gtmem -h 1' for more information.\n");
}

/**************************************************************************/
/*  function:     printHelpLevel3                                         */
/*  description:  Print really detailed hints                             */
/**************************************************************************/
static void printHelpLevel3()
{
    printf("\n"
"Examples:\n"
"gtmem -P -o 0x100 -c 40     -read 40 32bit words at offset 0x100 using PIO\n"
"gtmem -D -o 4 -c 5 -w 1     -write 1 to 5 32bit words at offset 4 using DMA\n"
"gtmem -c 10 -b 2            -read first 10 2-byte words of memory\n"
"gtmem -f 4 -n 10            -find 1st 10 memory offsets containing value 4\n"
"gtmem -A -w 0 -i 8          -fill memory with 32bit values incrementing by 8\n"
"gtmem -R -u 0 -c 16 -o 0           -read 16 registers of unit 0, offset 0x00\n"
"gtmem -R -o 0x4 -w 0x80 -m 0x82    -write 1 in bit7, 0 in bit2 of CSR 0x4\n"
"gtmem -R -u 1 -o 0x4 -w 0x55 -m 0xff    -write 0x55 to CSR 0x4 of unit 1\n"
"\n");
    printf(
"SYNOPSIS:\n"
"   Gtmem is a front-end application that provides access to GT hardware\n"
"registers and memory.  Basic register read, write, and read-modify-bit-write\n"
"operations are supported on all platforms.  Memory read, write, and search\n"
"operation are supported on all platforms.\n"
"\n");
    printf(
"   Memory operations can be performed either with PIO or DMA. Gtmem\n"
"operates on memory by default, selecting the best access mode based on\n"
"specified options.  When applicable, PIO operation may be forced with -P,\n"
"and DMA operation may be forced with -D.  DMA operations are limited to\n"
"32-bit (-b 4) operation. By default, DMA uses the same swapping settings as\n"
"PIO. The (-z) option can be used to toggle DMA byte and word swapping\n"
"such that DMA transfers are performed in the mode opposite the PIO settings.\n"
"\n");
    printf(
"   Control register operations can be performed by specifying the -R option.\n"
"Network management register operations can be performed by specifying -N.\n"
"Register reads/writes are limited to multiples of the register size,\n"
"and control register writes are further restricted to one register at a time.\n"
"The -b option is ignored during register access\n"
"\n");
    printf(
"   When read/write flags are not specified, gtmem will default to\n"
"read (-r) operation.  Writes may be performed by specifying -w #, where\n"
"# is the pattern to be written. For control register writes, a bitmask must\n"
"also be provided using the -m option. By default, read/write operations are\n"
"performed on one 32-bit word at offset 0x0.  The amount of memory that gtmem\n"
"will operate on may be varied using options -c, -l, and -A.  Option -c\n"
"specifies the word count, where the size of a word equals the size given\n"
"by -b.  Option -l specifies the length in bytes.  Option -A specifies to\n"
"read/write all memory/registers.  Option -A overrides"
" -l which overrides -c.\n"
"\n");
    printf(
"   The size of reads/writes may be varied using the -b option to specify\n"
"the number of bytes to read/write at a time.  To performs byte-sized reads,\n"
"for example, specify -b 1.  For 64-bit reads, specify -b 8.  Specifying the\n"
"options -c 3 -b 4 will cause three 32-bit words to be read from memory.\n"
"\n");
    printf(
"   Memory search operations may also be performed by specifying -f #, where\n"
"# is the pattern for which to search.  By default, the search operation will\n"
"find the first occurrence of the pattern.  The number of occurrences to find\n"
"may be increased with the -n option.  The option -E is provided as a search\n"
"modifier, and causes all patterns EXCEPT the search pattern to be found.\n"
"By default, searches are performed on the full range of memory.  The -c, -l\n"
"and -o options may be used to refine the search range.  A successful search\n"
"displays the offsets into memory at which the search pattern was found\n"
"Search options are ignored during write operations.\n"
"\n");
    printf(
"   During read mode operation, the -p option may be used to monitor memory\n"
"or registers.  This option specifies the millisecond period at which the\n"
"display will be updated.  If no values have changed, the display will not be\n"
"updated.  Instead, a running count of total passes will be displayed.  In\n"
"order to help identify changes, modified values will be marked with a '*'.\n"
"For memory ranges in excess of %d bytes, data comparison and marking is\n"
"NOT performed and data is re-displayed each period.  To exit this periodic\n"
"mode of operation, type 'q <Enter>'.\n", MAX_BUF_LEN);
}

#if defined PLATFORM_WIN || defined PLATFORM_VXWORKS || defined PLATFORM_RTX
/*NOTE: MSVC versions >7.0 have function strtoui64 which has the exact
semantics as gcc's strtoull. strtoui64 is not available in MSVC vers <7.0.
Vxworks has no strtoull*/
uint64 strtoullx(const char *cp, char **endp, unsigned int base)
{
    uint64 result = 0;
    char ch;

    if ( *cp == '-' )
        return (uint64)(-(int64)(strtoull(cp+1, endp, base)));

    if (!base)
    {
        base = 10;
        if (*cp == '0')
        {
            base = 8;
            cp++;
            if ((*cp == 'x') && isxdigit((uint32)cp[1]))
            {
                base = 16;
                cp++;
            }
        }
    }

    while( ((base==16) && isxdigit((uint32)*cp)) ||
           ((base==10) && isdigit((uint32)*cp)) ||
           ((base==8) && (*cp <= '7') && (*cp >= '0')) )
    {
        if( result != ((result * base)/base)) /*overflow*/
        {
            result = -1; /* 0xffffffffffffffff yields compiler warnings */
        }
        else
        {
            ch = tolower(*cp);
            result = (result * base) + ((ch>='a') ? (10+ch-'a') : (ch-'0'));
        }
        cp++;
    }

    if (endp)
        *endp = (char *)cp;
    return result;
}
#endif
