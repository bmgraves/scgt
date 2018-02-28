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

#define APP_VER_STRING "GT Firmware Programmer (gtprog) rev. 1.09 (2011/08/30)"

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
            int argc; char argv0[]="?\0"; char *argv[VXL]={argv0};      /**/
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
    #define APP_VxW_NAME xgtprog
#else /* GT */
    #define HW_SCGT
    #include "scgtapi.h"
    #define HAND scgtHandle
    #define APP_VxW_NAME gtprog
#endif

/**************************************************************************/
/************** APPLICATION-SPECIFIC CODE STARTS HERE *********************/
/**************************************************************************/

/**************************************************************************/
/**************************   I N C L U D E S   ***************************/
/**************************************************************************/

#if !defined PLATFORM_WIN && !defined PLATFORM_RTX
#include <unistd.h>
#endif
#include <fcntl.h>
#include "scgtapi.h"

/**************************************************************************/
/**************************  D A T A  T Y P E S  **************************/
/**************************************************************************/

/* Data structures used by option parsing code */

typedef struct {
    uint32      unit, doProgram, helpLevel, doXVF, revision;
    char       *fileName;
    char       *fileBuf;
    uint32      xvfSize;
    uint32      uspr; /* usec per register read, used to create "usleep" */
    uint32      localJR; /* local copy of JTAG register */
    HAND        handle;
} iOptionsType;


/* Data structures used by programming code */
#define MAX_LEN 7000
typedef struct var_len_byte
{
    uint32 len;   /* number of chars in this value */
    uint8  val[MAX_LEN+1];  /* bytes of data */
} lenVal;

typedef struct _jtag_env
{
    lenVal TDOMask;
    lenVal TDOExpected;
    uint8  maxRepeat;
    uint32 runTestTimes;
    lenVal dataVal;
    lenVal dataMask;
    lenVal addressMask;
    uint32 SDRSize;
} JTAGEnv;

/**************************************************************************/
/******************  F U N C T I O N  P R O T O T Y P E S  ****************/
/**************************************************************************/

static int gtJtagProg(iOptionsType * iO);
static int gtJtagVerifyFile(iOptionsType * iO);
static uint32 gtJtagDoProg(iOptionsType *iO, uint8 *buf, uint32 size);
static void gtprogPrintBoardInfo(HAND *pHandle);
static void computeCRC(unsigned int, char *, unsigned int,
                       unsigned int, unsigned int *);
static void parseCommand(int argc,char **argv,iOptionsType *iO);
static void printHelpLevel1(iOptionsType *iO);
static void printHelpLevel2();
static void printHelpLevel3();

/**************************************************************************/
/***********************  D E F I N E S & M A C R O S  ********************/
/**************************************************************************/
#if defined PLATFORM_WIN || defined PLATFORM_RTX
   #include <io.h>
   #define FILE_MODE  O_RDONLY | O_BINARY /*must have O_BINARY*/
#elif PLATFORM_UNIX
   #define FILE_MODE  O_RDONLY /*no O_BINARY in linux */
#elif PLATFORM_VXWORKS
   #define FILE_MODE  O_RDONLY /*no O_BINARY in vxworks */
#endif

/*
 * The following 5 macros must correspond to the bit definitions of the JTAG
 * programming register of the respective devices (here, the GT control
 * registers). However, they should be defined from the perspective of the
 * first JTAG/EEPROM chip in the chain, i.e. TDI corresponds to the TDI pin
 * of the EEPROM and TDO corresponds to the TDO pin of the EEPROM. Therefore,
 * when programming the EEPROM, data is written to the bit defined by macro
 * TDI and read from the bit defined by macro TDO.
 *
 * NOTE: TDI bit is called TDO bit in GT hardware manual, and TDO is called TDI.
 */

#ifdef HW_SL240
/* bit definitions for SL */
#define TCK    0x08
#define TMS    0x10
#define TDI    0x20
#define TDO    0x40
#define ENABLE 0x80

/* JTAG register offset definition for SL */
#define JTAG_REG  0x4

#elif defined HW_SCGT
/* bit definitions for GT */
#define TCK    0x010000
#define TMS    0x020000
#define TDI    0x040000
#define TDO    0x080000
#define ENABLE 0x100000

/* JTAG register offset definition for GT */
#define JTAG_REG  0x4

#endif


/* encodings of xsvf instructions */
#define XCOMPLETE        0
#define XTDOMASK         1
#define XSIR             2
#define XSDR             3
#define XRUNTEST         4
#define XREPEAT          7
#define XSDRSIZE         8
#define XSDRTDO          9
#define XSETSDRMASKS     10
#define XSDRINC          11
#define XSDRB            12
#define XSDRC            13
#define XSDRE            14
#define XSDRTDOB         15
#define XSDRTDOC         16
#define XSDRTDOE         17
#define XSTATE           18
#define XSTATE_RESET     0
#define XSTATE_RUNTEST   1

/* return number of bytes necessary for "num" bits */
#define BYTES(num) (((num%8)==0) ? (num/8) : (num/8+1))

#ifdef DEBUG_JTAG
#define DPrintF printf
#else
#define DPrintF  if(0)printf
#endif

#ifdef HW_SL240
/* function redefinitions for SL */
    #define DRIVER_OPEN(x, y)       fxsl_open(x, y)
    #define DRIVER_CLOSE(x)         fxsl_close(x)
    #define READ_CR(iO,off,pval)  (fxsl_read_CR((iO)->handle, off, (uint32*)pval), *pval)
    #define WRITE_CR(iO,off,value)  fxsl_write_CR((iO)->handle, off, value)
#elif defined HW_SCGT
/* function redefinitions for GT */
    #define DRIVER_OPEN(x, y)       scgtOpen(x, y)
    #define DRIVER_CLOSE(x)         scgtClose(&(x))
    #define READ_CR(iO,off,pval)    (*(pval)=scgtReadCR(&((iO)->handle), off))
    #define WRITE_CR(iO,off,value)  scgtWriteCR(&((iO)->handle),off,value)
#endif

/**************************************************************************/
/************************  G T P R O G   C O D E  *************************/
/**************************************************************************/

#ifndef GTPROG_SLOW
    #define WRITE_CR2(iO,off,value)  (WRITE_CR(iO, off, value),\
              iO->localJR = (off == JTAG_REG) ?  value : iO->localJR )
    #define READ_CR2(iO,off,pval)         (*pval = iO->localJR)

    static void USEC_SLEEP(iOptionsType *iO, uint32 us)
    {
        uint32 i;
        /* ensure the READ_CR access is performed */
        static volatile uint32 regVal, regVal2;

        if ( us >= 1000000 )
        {
            usysMsTimeDelay((us+999)/1000);
            return;
        }

        for(i = 0; i < (us*10); i += iO->uspr)
        {
            regVal=READ_CR(iO,JTAG_REG,&regVal2);
        }
    } /* end gtprog_usec_sleep */

#else
    #define WRITE_CR2(iO,off,value)       (WRITE_CR(iO, off, value))
    #define READ_CR2(iO,off,pval)         (READ_CR(iO, off, pval))
    #define USEC_SLEEP(iO,us)  if(us > 1) usysMsTimeDelay((us+999)/1000)
#endif

MAIN_IN
    iOptionsType   iO;
    int i, ret = 0;
    static volatile uint32 regVal, regVal2; /* to ensure READ_CR access is performed */
    uint32 j;
    time_t nowTime, startTime;

    PARSER

    memset(&iO, 0, sizeof(iOptionsType));

    parseCommand(argc, argv, &iO);      /* parsing options */

    if(iO.helpLevel!=0) /*1, 2, 3, ...*/
    {
        printHelpLevel1(&iO);
        return 0;
    }

    printf("\n%s\n\n", APP_VER_STRING);

    if( gtJtagVerifyFile(&iO) )
    {
        ret = -1;
        goto done;
    }
    else if( iO.doProgram )
    {
        if( iO.unit == -1 )
        {
            printf("\n!!! Must specify a valid unit with option -u !!!\n");
            ret = -1;
            goto done;
        }

        /* Open the GT device for programming */
        if (DRIVER_OPEN(iO.unit, &iO.handle) != 0)
        {
            printf("ERROR: could not open driver for unit %d\n", iO.unit);
            goto done;
        }

        gtprogPrintBoardInfo(&iO.handle);

        /* Timer calibration */
        startTime = time(NULL);

        while( ((nowTime = time(NULL)) - startTime) < 1 ){} /* align timer */

        startTime=nowTime;
        j=0;
        while(1) /* measure the rate */
        {
            regVal = READ_CR(&iO, JTAG_REG, &regVal2); /* do not optimize out */

            if ( ((++j%100) == 0) && (((nowTime=time(NULL)) - startTime) > 0 ))
            {
                break;
            }
        }

        iO.uspr=10000000/j; /* number of 1/10 of us per one read of JTAG register */
        printf("Timing calibration: JTAG read takes about %d tenths of a us.\n",
               iO.uspr);

        iO.localJR=READ_CR(&iO, JTAG_REG, &regVal); /* initialize local copy */

        gtJtagProg(&iO);
        DRIVER_CLOSE(iO.handle);
    }
    else
    {
        printf("\n");

        for ( i = 0; i < 16; i++ )
        {
            /* Open the device for printing board information. Must check for
               all 16 units, as Solaris unit numbers may not be sequential. */
            if (DRIVER_OPEN(i, &iO.handle) == 0)
            {
                gtprogPrintBoardInfo(&iO.handle);
                DRIVER_CLOSE(iO.handle);
            }
        }

        printf("\nTo proceed with firmware update, add option -B"
               " and select a unit with -u.\n\n");
    }

done:
    if( iO.fileBuf )
        free(iO.fileBuf);

    return ret;
}


/**************************************************************************/
/*  function:     parseCommand                                            */
/*  description:  Getting argv details into internal usable form          */
/**************************************************************************/
static void parseCommand(int argc,char **argv,iOptionsType *iO)
{
    char *endptr, nullchar = 0;
    int  len, tookParam=0;
    int i,j;                               /* setting defaults */

    iO->unit = -1;

    if(argc == 1)                                       /* start processing */
        { iO->helpLevel=1; return; }

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
            {   /* test for options which take parameters */
                /* these options only valid as last char of -string */
                tookParam = 1;  endptr = &nullchar;
                if(     argv[i][j]=='u') iO->unit=strtoul(argv[i+1],&endptr,0);
                else if(argv[i][j]=='f') iO->fileName = argv[i+1];
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
            if(              argv[i][j]=='B') iO->doProgram = 1;
            else if(         argv[i][j]=='X') iO->doXVF = 1;
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
}

/**************************************************************************/
/*  function:     printHelpLevel1                                         */
/*  description:  Print hints and call more help if needed                */
/**************************************************************************/
static void printHelpLevel1(iOptionsType *iO)
{
    printf("%s\n\n", APP_VER_STRING);
    printf("Usage: gtprog -f file [-u unit] [-B] [-h]\n");
#ifdef PLATFORM_VXWORKS
    printf("\nVxWorks users: enclose options list in a set of quotes \" \".\n");
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
"   -u unit : SCRAMNet GT board/unit number\n"
"   -f file : firmware filename (typically with .gfw extension)\n"
"   -B      : proceed with firmware update (burn)\n"
"   -X      : treat -f file as a raw fw file (typically with .xvf extension)\n"
"   -h      : print this help menu\n"
"\n"
"Examples: gtprog -f abc.gfw           verify and print info for file abc.gfw\n"
"          gtprog -f abc.gfw -u 2 -B   update firmware on unit 2\n\n");

    printf("Note:  run `gtprog -h 1' for more information.\n");
}

/**************************************************************************/
/*  function:     printHelpLevel3                                         */
/*  description:  Print even more hints                                   */
/**************************************************************************/
static void printHelpLevel3()
{
    #ifdef HW_SCGT  /* GT only message */
    printf("\nThis version of gtprog is using API revision %s.\n",
             scgtGetApiRevStr());
    #endif
    printf("\n"
"The gtprog utility facilitates updating GT device firmware.\n"
"Firmware programming is a well tested operation, and failure is not\n"
"a typical occurrence. Still, programming is a sensitive operation. Proper\n"
"precautions should be taken to avoid power-failure during programming.\n"
"\n");
    printf(
"Should programming fail for any reason, do the following:\n"
"- DO NOT TURN OFF THE COMPUTER. Devices with invalid or incomplete\n"
"  firmware may not function after power-cycle and may require service.\n"
"- Verify that you are using the proper firmware programming file and the\n"
"  checksum reported by gtprog matches the documented checksum.\n"
"- Attempt to program the firmware again. If programming fails, try different\n"
"  firmware versions.\n"
"- Should problems persist, contact Curtiss-Wright Controls Embedded\n"
"  Computing Data Communications Technical Support. Contact information can\n"
"  be found in your product manual or on the internet at www.cwcembedded.com\n"
"  or www.systran.com.\n"
"\n");
    printf(
"When programming has successfully completed, power to the host machine must\n"
"be cycled (turned off then back on) before the update will take effect. Then\n"
"the update can then be verified using the gtmon utility, e.g. gtmon -V, to\n"
"view the new firmware revision.\n"
"\n");
}

/**************************************************************************/
/*  function:     gtJtagVerifyFile                                        */
/*  description:  Handles file IO and checksum calculation                */
/**************************************************************************/
/*
Layout of .gfw header info.  All .gfw data stored in little endian format.

Bytes       Contain
0-3         .xvf checksum
4-7         .xvf size in bytes
8-11        .gfw data size in bytes
12-15       firmware revision
16-95       information string
96-251      "magic" sequence (incrementing values from 92 to 251)
252-255     .gfw checksum (excluding last 4 bytes)
*/

#define MAX_FILENAME_LEN         256
#define MAX_INFO_LEN              80
#define GFW_DATA_SIZE            256
#define SEED              0xffffffff  /* same must be used by gtprog */
#define POLYNOMIAL        0x04c11db7  /* same must be used by gtprog */

static int gtJtagVerifyFile(iOptionsType * iO)
{
    char *ptr, *fileBuf = NULL;
    char *gfwData, *infoBuf = "";
    int gfwFd = -1, ret;
    unsigned int bytes, fileSize, xvfFileSize;
    unsigned int fileCRC, xvfCRC, crc, i;

    if( !iO->fileName )
    {
        printf("Must specify firmware file with -f option.\n");
        return -1;
    }

    if( ((ptr = strrchr(iO->fileName, '.')) == NULL) ||
        (strcmp(ptr, ".gfw") && strcmp(ptr, ".xvf")) )
    {
        printf(
        "!!! WARNING: Filename does not have .gfw or .xvf extension.\n");
        usysMsTimeDelay(2000);
        ptr = NULL;
    }

    if( (gfwFd = open(iO->fileName, FILE_MODE, 0)) < 0 )
    {
        printf("!!! Failed to open input file (%s).\n", iO->fileName);
        goto error;
    }

    /* Determine the size of the file for allocation */
    fileSize= lseek(gfwFd, 0, SEEK_END);
    lseek(gfwFd, 0, SEEK_SET);  /* rewind */

    if( fileSize < GFW_DATA_SIZE )
    {
        printf("!!! File %s is too short to contain valid data.\n",
               iO->fileName);
        goto error;
    }

    /* Create user buffer and read the file into it */
    if ((fileBuf = (char *) malloc(fileSize)) == NULL)
    {
        printf("!!! Memory allocation failed.\n");
        goto error;
    }

    iO->fileBuf = fileBuf;

    bytes = read(gfwFd, fileBuf, fileSize);

    if (bytes != fileSize)
    {
        printf("!!! Failed to read complete file.\n");
        goto error;
    }

    if( iO->doXVF )
    {
        iO->xvfSize = fileSize;
        /* may not be an xvf, so 'fileCRC' */
        computeCRC(SEED, fileBuf, fileSize, POLYNOMIAL, &fileCRC);
    }
    else
    {
        gfwData = &fileBuf[fileSize - GFW_DATA_SIZE];
        infoBuf = &gfwData[16];

        for(i =16+MAX_INFO_LEN; i < (GFW_DATA_SIZE - 4); i++)
        {
            if( gfwData[i] != (char)((i * POLYNOMIAL) & 0xff) )
            {
                printf("!!! Invalid file type indicator in file %s.\n",
                       iO->fileName);
                goto error2;
            }
        }

        /* verify gfw checksum */
        crc = (gfwData[GFW_DATA_SIZE - 4]  & 0xff) |
              ((gfwData[GFW_DATA_SIZE - 3] & 0xff) << 8) |
              ((gfwData[GFW_DATA_SIZE - 2] & 0xff) << 16) |
              ((gfwData[GFW_DATA_SIZE - 1] & 0xff) << 24);

        /* 'fileCRC' is effectively .gfw CRC32 */
        computeCRC(SEED, fileBuf, fileSize-4, POLYNOMIAL, &fileCRC);

        if( crc != fileCRC )
        {
            printf("!!! Invalid .gfw checksum (0x%.8x). Expected (0x%.8x).\n",
                   fileCRC, crc);
            goto error2;
        }

        /* retrieve the firmware revision */
        iO->revision = (gfwData[12]  & 0xff) | ((gfwData[13] & 0xff) << 8) |
            ((gfwData[14] & 0xff) << 16) | ((gfwData[15] & 0xff) << 24);

        /* verify gfw header size */
        bytes = (gfwData[8]  & 0xff) | ((gfwData[9] & 0xff) << 8) |
            ((gfwData[10] & 0xff) << 16) | ((gfwData[11] & 0xff) << 24);

        if( bytes != GFW_DATA_SIZE )
        {
            printf("!!! Invalid information block size(%d). Expected (%d).\n",
                    bytes, GFW_DATA_SIZE);
            goto error2;
        }

        /* verify xvf file size */
        xvfFileSize = (gfwData[4]  & 0xff) | ((gfwData[5] & 0xff) << 8) |
            ((gfwData[6] & 0xff) << 16) | ((gfwData[7] & 0xff) << 24);

        iO->xvfSize = xvfFileSize;

        if( xvfFileSize != (fileSize - GFW_DATA_SIZE) )
        {
            printf("!!! Invalid .xvf file size (%d). Expected (%d).\n",
                    (fileSize - GFW_DATA_SIZE), xvfFileSize);
            goto error2;
        }

        if( (bytes + xvfFileSize) != fileSize )
        {
            printf("!!! Invalid .gfw file size (%d). Expected (%d).\n",
                    fileSize, (bytes + xvfFileSize));
            goto error2;
        }

        /* verify xvf checksum */

        crc = (gfwData[0]  & 0xff) | ((gfwData[1] & 0xff) << 8) |
            ((gfwData[2] & 0xff) << 16) | ((gfwData[3] & 0xff) << 24);

        computeCRC(SEED, fileBuf, fileSize-GFW_DATA_SIZE, POLYNOMIAL, &xvfCRC);

        if( crc != xvfCRC )
        {
            printf("!!! Invalid FW checksum (0x%.8x). Expected (0x%.8x).\n",
                   xvfCRC, crc);
            goto error2;
        }
    }

    printf("File Name   : %s\n",     iO->fileName);
    printf("File Size   : %d\n",     fileSize);

    if( iO->doXVF )
    {
        printf("File CRC32  : 0x%x\n",   fileCRC);
    }
    else
    {
        printf("FW CRC32    : 0x%x\n",   xvfCRC);
        printf("FW Revision : %X.%.2X\n",
                ((iO->revision>> 8) & 0xff), (iO->revision & 0xff) );
        printf("Description : %s\n", infoBuf);
    }

    ret = 0;

    if ( 0 )
    {
error2:
        printf(
        "!!! Verify that file selected is correct. Typically, firmware\n"
        "!!! files will have a .gfw extension. For raw firmware files having\n"
        "!!! a .xvf extension, use the -X option.\n"
        );
error:
        ret = -1;
    }

    if( gfwFd >=0 )
        close(gfwFd);

    return ret;
}

/**************************************************************************/
/*  function:     gtJtagProg                                              */
/*  description:  Handles file IO and checksum calculation                */
/**************************************************************************/
static int gtJtagProg(iOptionsType * iO)
{
    printf("Please wait while the firmware is updated. Allow at least\n");
    printf("1 minute for first 2%% of programming to complete.\n");

    if (gtJtagDoProg(iO, (uint8*)iO->fileBuf, iO->xvfSize))
    {
        printf("\n"
        "ERROR: FIRMWARE PROGRAMMING FAILED. DO NOT TURN OFF THE COMPUTER.\n"
        "       Devices with invalid or incomplete firmware may not\n"
        "       function after power-cycle and may require service.\n");
        printf("\n"
        "       Attempt to program the firmware again. If programming\n"
        "       fails, try different firmware versions. Should problems\n"
        "       persist, contact Curtiss-Wright Controls Embedded\n"
        "       Computing Data Communications Technical Support.\n");
        printf("\n"
        "       Technical Support contact information can be found in your\n"
        "       product manual or on the internet at www.cwcembedded.com or\n"
        "       www.systran.com.\n\n");
    }
    else
    {
        printf("Programming completed successfully\n");
        printf("Please cycle power on the host machine\n");
        printf("for the update to take effect. Then,\n");
        printf("verify the update using the gtmon utility\n");
        printf("to view the new firmware revision.\n");
    }

    return 0;
}

/**************************************************************************/
/*  function:     gtprogPrintBoardInfo                                    */
/*  description:  Prints info about the device being programmed           */
/**************************************************************************/

#ifdef HW_SL240
/******************* SL version *********************/
static void gtprogPrintBoardInfo(HAND *pHandle)
{
    uint32 regVal;
    char board[80];
    char volts[80];
    char *model;
    fxsl_statusstruct status;
    fxsl_configstruct cs;

    if (fxsl_status(*pHandle, &status ) != FXSL_SUCCESS)
    {
        printf("ERROR: Unable to obtain status\n");
        return;
    }

    if (fxsl_get_config(*pHandle, &cs ) != FXSL_SUCCESS)
    {
        printf("ERROR: Unable to obtain configuration\n");
        return;
    }

    fxsl_read_CR(*pHandle, 0x4, &regVal);

    if ((status.revisionID & 0x0080) == 0x80)
        strcpy(board, "SL240");
    else
        strcpy(board, "SL100");

    if ((status.revisionID & 0x0040) == 0x0040)
        strcpy(volts, "for 3.3V PCI");
    else
        strcpy(volts, "for 5.0V PCI");

    if (((regVal >> 22) & 0x3) == 0x0) /* sl100/240 standard model */
       model = "";
    else if (((regVal >> 22) & 0x3) == 0x1) /* sl100/240 model x */
       model = "x";
    else
       model = "?";

    printf("Hardware: unit/bus/slot %d/%d/%d %s%s Firm. %X.%x (%X.%x) %s \n",
           status.nBoard, status.nBus, status.nSlot, board, model,
           status.revisionID & 0x3F, (regVal>>16) & 0xff, status.revisionID,
           (regVal>>16) & 0xff, volts);
}

#elif defined HW_SCGT
/******************* GT version *********************/
static void gtprogPrintBoardInfo(HAND *pHandle)
{
    scgtDeviceInfo devInfo;
    uint32 val;
    char *str;

    if (scgtGetDeviceInfo(pHandle, &devInfo) != SCGT_SUCCESS)
    {
        printf("ERROR: Unable to obtain device info\n");
        return;
    }

    val = ( (scgtReadCR(pHandle, 0)) & 0x1C00) >> 10;

    if( val == 0x0 )
        str = "33MHz 64bit 5V";
    else if( val == 0x1 )
        str = "33MHz 64bit 3.3V";
    else if( val == 0x2 )
        str = "66MHz 64bit 3.3V";
    else
        str = "unknown";

    printf("Unit %i info : %s, FW %X.%.2X, %s PCI compatible\n",
           devInfo.unitNum, devInfo.boardLocationStr,
           devInfo.revisionID >> 8, devInfo.revisionID & 0xFF, str);
}
#endif

/**************************************************************************/
/*  function:     computeCRC                                              */
/*  description:  Calculates the 32-bit CRC of a buffer                   */
/**************************************************************************/
static void computeCRC(unsigned int seed, char *bptr,
                unsigned int count, unsigned int polynomial, unsigned int *crc)
{
    int i;
    unsigned int crc32 = seed;

    while (count--)
    {
        crc32 ^= (((unsigned int)*bptr++) << 24);
        for (i=0; i<8; i++)
        {
            if (crc32 & 0x80000000)
            {
                crc32 = (crc32 << 1) ^ polynomial;
            }
            else
            {
                crc32 <<= 1;
            }
        }
    }
    *crc = crc32;
}

/*************************************************************************/
/************************* JTAG PROGRAMMING CODE *************************/
/*************************************************************************/

/** JTAG port operations **/
static void setPort(iOptionsType *iO, uint32 port, uint32 val)
{                                      /* used infrequently without clocking */
    uint32 regVal = READ_CR2(iO, JTAG_REG, &regVal);

    if (val)
        regVal = regVal | port;
    else
        regVal = regVal & ~port;

    WRITE_CR2(iO, JTAG_REG, regVal);
}

static void pulseClock(iOptionsType *iO) /* used infrequently*/
{
    uint32 regVal = READ_CR2(iO, JTAG_REG, &regVal);
    WRITE_CR2(iO, JTAG_REG, regVal & ~TCK);  /* set TCK port to low  */
    WRITE_CR2(iO, JTAG_REG, regVal | TCK);   /* set TCK port to high */
    WRITE_CR2(iO, JTAG_REG, regVal & ~TCK);  /* set TCK port to low  */
}

static uint32 readTDOBit(iOptionsType *iO)
{
    /* this register access must physically read the hardware register.
       No "emulation" can be performed here */
    uint32 regVal = READ_CR(iO, JTAG_REG, &regVal);
    return (regVal & TDO)? 1 : 0;
}

static void clockOutBit(iOptionsType *iO, uint32 port, uint32 val)
{
    uint32 regVal = READ_CR2(iO, JTAG_REG, &regVal);
    if (val)
        regVal = regVal | port;
    else
        regVal = regVal & ~port;

    /* port will be set when TCK set to 0, also embedded pulseClock */
    WRITE_CR2(iO, JTAG_REG, regVal & ~TCK);
    WRITE_CR2(iO, JTAG_REG, regVal | TCK);
    WRITE_CR2(iO, JTAG_REG, regVal & ~TCK);
}

#ifdef DEBUG_JTAG
static void printLenVal(lenVal *lv)
{
    int i;

    printf("lv->len = %d", lv->len);
    for (i = 0; i < lv->len; i++)
    {
        printf("lv->val[%d] = %2.2x", i, lv->val[i]);
    }
}
#endif

/* clock out numBits from a lenVal;  the least significant bits are */
/* output on the TDI line first; exit into the exit(DR/IR) state.   */
/* if tdoStore!=0, store the TDO bits clocked out into tdoStore.    */
static void shiftOutLenValStoreTDO(iOptionsType *iO, lenVal *lv, int32 numBits,
                                   lenVal *tdoStore, uint32 last)
{
    uint32 i, j;

    /* if tdoStore is not null set it up to store the tdoValue */
    if (tdoStore)
        tdoStore->len = lv->len;

    for (i = 0; i < lv->len; i++)
    {
        /* nextByte contains the next byte of lenVal to be shifted out */
        /* into the TDI port                                           */
        uint8 nextByte = lv->val[lv->len - i - 1];
        uint8 nextReadByte=0;
        uint32 tdoBit;

        /* on the last bit, set TMS to 1 so that we go to the EXIT DR */
        /* or to the EXIT IR state */
        for (j = 0; j < 8; j++, numBits--, nextByte >>= 1)
        {
            /* send in 1 byte at a time */
            /* on last bit, exit SHIFT SDR */
            if ((last == 1) && (numBits == 1))
                setPort(iO, TMS, 1);

            if (numBits > 0)
            {
                /* read the TDO port into tdoBit */
                tdoBit = readTDOBit(iO);

                /* set TDI to last bit */
                clockOutBit(iO, TDI, (nextByte & 0x1));

                /* store the TDO value in the nextReadByte */
                if (tdoBit)
                    nextReadByte |= (1<<j);
            }
        }

        /* if storing the TDO value, store it in the correct place */
        if (tdoStore)
        {
            tdoStore->val[tdoStore->len - i - 1] = nextReadByte;
        }
    }
}

/* lenVal structure manipulation functions */
/* return the value represented by this lenval */
static uint32 value(lenVal *x)
{
    uint32 i;
    uint32 result = 0; /* result to hold the accumulated result */

    for (i = 0; i < x->len; i++)
    {
        result <<= 8;         /* shift the accumulated result */
        result += x->val[i];  /* get the last byte first */
    }

    return result;
}

/* read from XSVF numBytes bytes of data into x */
static void readVal(uint8 **buf, lenVal *x, uint32 numBytes)
{
    uint32 i;

    for (i = 0; i < numBytes; i++)
    {
        /* read a byte of data into the lenVal */
        x->val[i] = **buf;
        (*buf)++;
    };

    x->len=numBytes; /* set the length of the lenVal */
}

static uint32 checkExpected(JTAGEnv *env, lenVal *l)
{
    uint32 i;

    for (i = 0; i < env->TDOExpected.len; i++)
    {
        if ((env->TDOExpected.val[i] & env->TDOMask.val[i]) !=
            (l->val[i] & env->TDOMask.val[i]))
        {
            /* There was an error - return failure */
#ifdef DEBUG_JTAG
            printf("verify error - len = %x, offset = %x", env->TDOExpected.len, i);
            printLenVal(&env->TDOExpected);
            printLenVal(l);
            printLenVal(&env->TDOMask);
#endif
            return 0;
        }
    }

    return 1;
}

/* return 0 if the TDO doesn't match what is expected */
/* lenVal type is large. actualTDO storage passed in so malloc not needed. */
static uint32 loadSDR(iOptionsType *iO, JTAGEnv *env, lenVal *actualTDO)
{
    int failTimes = 0;
    uint32 runTestTime = env->runTestTimes;
    int quit = 0;
    uint32 ret_val = 0;

    actualTDO->len = env->dataVal.len;

    /* data processing loop */
    while (!quit)
    {
        clockOutBit(iO, TMS,1);  /* Select-DR-Scan state  */
        clockOutBit(iO, TMS,0);  /* Capture-DR state      */
        clockOutBit(iO, TMS,0);  /* Shift-DR state        */

        /* output dataVal onto the TDI ports; store the TDO value returned */
        shiftOutLenValStoreTDO(iO, &env->dataVal, env->SDRSize, actualTDO, 1);

        /* compare the TDO value against the expected TDO value */
        if (checkExpected(env, actualTDO))
        {
            /* TDO matched what was expected */
            clockOutBit(iO, TMS, 1);  /* Update-DR state    */
            clockOutBit(iO, TMS, 0);  /* Run-Test/Idle state*/

            /* wait in Run-Test/Idle state */
            USEC_SLEEP(iO,runTestTime);

            ret_val = 1;
            quit = 1;
        }
        else
        {
            /* TDO did not match the value expected */
            failTimes++;  /* update failure count */
            if (failTimes > env->maxRepeat)
            {
                ret_val = 0; /* ISP failed */
                quit = 1;
            }
            else
            {
                clockOutBit(iO, TMS, 0); /* Pause-DR state      */
                clockOutBit(iO, TMS, 1); /* Exit2-DR state      */
                clockOutBit(iO, TMS, 0); /* Shift-DR state      */
                clockOutBit(iO, TMS, 1); /* Exit1-DR state      */
                clockOutBit(iO, TMS, 1); /* Update-DR state     */
                clockOutBit(iO, TMS, 0); /* Run-Test/Idle state */

                /* wait in Run-Test/Idle state */
                USEC_SLEEP(iO,runTestTime);
                runTestTime += runTestTime>>2; /* Add 25% onto the time */
            }
        }
    }

    return ret_val;
}

/* lenVal type is large. actualTDO storage passed in so malloc not needed. */
static uint32 loadSDRTDOB(iOptionsType *iO, JTAGEnv *env, lenVal *actualTDO)
{
    uint32 ret;

    actualTDO->len = env->dataVal.len;

    clockOutBit(iO, TMS, 1);  /* Select-DR-Scan state  */
    clockOutBit(iO, TMS, 0);  /* Capture-DR state      */
    clockOutBit(iO, TMS, 0);  /* Shift-DR state        */

    /* output dataVal onto the TDI ports; store the TDO value returned */
    shiftOutLenValStoreTDO(iO, &env->dataVal, env->SDRSize, actualTDO, 0);

    /* compare the TDO value against the expected TDO value */
    ret = checkExpected(env, actualTDO);
    return ret;
}

/* lenVal type is large. actualTDO storage passed in so malloc not needed. */
static uint32 loadSDRTDOC(iOptionsType *iO, JTAGEnv *env, lenVal *actualTDO)
{
    uint32 ret;
    actualTDO->len = env->dataVal.len;

    /* output dataVal onto the TDI ports; store the TDO value returned */
    shiftOutLenValStoreTDO(iO, &env->dataVal, env->SDRSize, actualTDO, 0);

    /* compare the TDO value against the expected TDO value */
    ret = checkExpected(env, actualTDO);
    return ret;
}

/* lenVal type is large. actualTDO storage passed in so malloc not needed. */
static uint32 loadSDRTDOE(iOptionsType *iO, JTAGEnv *env, lenVal *actualTDO)
{
    uint32 retVal;

    actualTDO->len = env->dataVal.len;

    /* output dataVal onto the TDI ports; store the TDO value returned */
    shiftOutLenValStoreTDO(iO, &env->dataVal, env->SDRSize, actualTDO, 0);

    /* compare the TDO value against the expected TDO value */
    retVal = checkExpected(env, actualTDO);

    if (retVal)
    {
        clockOutBit(iO, TMS, 1); /* Update-DR State */
        clockOutBit(iO, TMS, 0); /* Run-Test/Idle State */
        USEC_SLEEP(iO,env->runTestTimes);
    }

    return retVal;
}

static void setBit(lenVal *l, uint32 byte, uint32 bit, uint32 val)
{
    uint8 mask = 1 << (7 - bit);

    if (val)
        l->val[byte] |= mask;
    else
        l->val[byte] &= ~mask;
     /* l->val[byte] &= mask;  this is from Systran's original but most likely wrong */
    printf("setBit used - look inside the source code for comments\n");
}

static int getBit(lenVal *l, uint32 byte, uint32 bit)
{
    uint8 mask = 1 << (7 - bit);

    return (l->val[byte] & mask) >> (7 - bit);
}

/* determine the next data value from the XSDRINC instruction and store     */
/* it in dataVal.                                                           */
/* Example:  dataVal=0x01ff, nextData=0xab, addressMask=0x0100,             */
/*           dataMask=0x00ff, should set dataVal to 0x02ab                  */
static void doSDRMasking(JTAGEnv *env, lenVal *nextData)
{
    uint32 j, count=0;
    uint32 carry;
    int i;

    /* add the address Mask to dataVal and return as a new dataVal */
    for (carry = 0, i = env->dataVal.len - 1; i >= 0; i--)
    {
        j = env->dataVal.val[i] + env->addressMask.val[i] + carry;
        carry = (j > 255)?1:0;
        env->dataVal.val[i] = (uint8)(j & 0xff);
    }

    for (i = 0; i < (int)env->dataMask.len; i++)
    {
        /* look through each bit of the dataMask. If the bit is    */
        /* 1, then it is data and we must replace the corresponding*/
        /* bit of dataVal with the appropriate bit of nextData     */
        for (j=0; j<8; j++)
        {
            if (getBit(&env->dataMask,i,j))
            {
                /* replace the bit of dataVal with a bit from nextData */
                setBit(&env->dataVal, i, j, getBit(nextData, count/8, count%8));
                count++;  /* count how many bits have been replaced */
            }
        }
    }
}

/* parse the xsvf file and pump the bits */
static uint32 gtJtagDoProg(iOptionsType *iO, uint8 *buf, uint32 size)
{
    uint8 inst; /* instruction */
    JTAGEnv *env;
    lenVal *tmp;

    uint32 bitLengths; /* hold the length of the arguments to read in */
    uint8 sdrInstructs;
    uint32 i;
    uint32 failed = 0, bytesProgrammed;
    int32 percent = 0, lastPercent = -1;
    uint8 *startBuf = buf;

    env = (JTAGEnv *) malloc(sizeof(JTAGEnv));
    tmp = (lenVal *) malloc(sizeof(lenVal));

    /* Goto the idle state */
    setPort(iO, TMS, 1);
    setPort(iO, TCK, 1);
    setPort(iO, TDO, 1);
    setPort(iO, TDI, 1);
    setPort(iO, ENABLE, 1);

    for (i = 0; i < 5; i++)
        pulseClock(iO);
    setPort(iO, TMS, 0);
    pulseClock(iO);

#ifdef DEBUG_JTAG
    for (i = 0; i < 5; i++)
        printf("%x ", buf[i]);
    printf("\n%x \n", (uint32) buf);
#endif

    while (((inst = *buf++) != XCOMPLETE) && !failed)
    {
        switch (inst)
        {
            case XTDOMASK:
                /* readin new TDOMask */
                DPrintF("XTDOMASK");
                readVal(&buf, &env->TDOMask, BYTES(env->SDRSize));
                break;

            case XREPEAT:
                /* read in the new XREPEAT value */
                DPrintF("XREPEAT");
                env->maxRepeat = *buf++;
                break;

            case XRUNTEST:
                /* read in the new RUNTEST value */
                DPrintF("XRUNTEST");
                readVal(&buf, tmp, 4);
                env->runTestTimes = value(tmp);
                break;

            case XSIR:
                /* load a value into the instruction register */
                DPrintF("XSIR");
                bitLengths = *buf++; /* get number of bits to shift in */

                /* store instruction to shift in */
                readVal(&buf, &env->dataVal, BYTES(bitLengths));

                clockOutBit(iO, TMS, 1);  /* Select-DR-Scan state  */
                clockOutBit(iO, TMS, 1);  /* Select-IR-Scan state  */
                clockOutBit(iO, TMS, 0);  /* Capture-IR state      */
                clockOutBit(iO, TMS, 0);  /* Shift-IR state        */

                /* send the instruction through the TDI port and end up   */
                /* dumped in the Exit-IR state                            */
                shiftOutLenValStoreTDO(iO, &env->dataVal, bitLengths, NULL, 1);

                clockOutBit(iO, TMS,1);  /* Update-IR state       */
                if (env->runTestTimes)
                {
                    clockOutBit(iO, TMS,0);  /* Run-Test/Idle state   */
                    USEC_SLEEP(iO,env->runTestTimes);
                }
                break;

            case XSDRTDO:
                /* get the data value to be shifted in */
                DPrintF("XSDRTDO");
                readVal(&buf, &env->dataVal, BYTES(env->SDRSize));

                /* store the TDOExpected value    */
                readVal(&buf, &env->TDOExpected, BYTES(env->SDRSize));

                /* shift in the data value and verify the TDO value against */
                /* the expected value                                       */
                if (env->SDRSize && !loadSDR(iO, env, tmp))
                {
                    /* The ISP operation TDOs failed to match expected */
                    printf("\nXSDRTDO: TDOs did not match!\n");
                    failed = 1;
                }
                break;

            case XSDRTDOB:
               DPrintF("XSDRTDOx");
               /* get the data value to be shifted in */
               readVal(&buf, &env->dataVal, BYTES(env->SDRSize));

               /* store the TDOExpected value    */
               readVal(&buf, &env->TDOExpected, BYTES(env->SDRSize));

               /* shift in the data value and verify the TDO value against */
               /* the expected value                                       */
               if (!loadSDRTDOB(iO, env, tmp))
               {
                    /* The ISP operation TDOs failed to match expected */
                    printf("\nXSDRTDOB: TDOs did not match!\n");
                    failed = 1;
               }
               break;

            case XSDRTDOC:
                DPrintF("XSDRTDOx");

                /* get the data value to be shifted in */
                readVal(&buf, &env->dataVal, BYTES(env->SDRSize));

                /* store the TDOExpected value    */
                readVal(&buf, &env->TDOExpected, BYTES(env->SDRSize));

                /* shift in the data value and verify the TDO value against */
                /* the expected value                                       */
                if (!loadSDRTDOC(iO, env, tmp))
                {
                    /* The ISP operation TDOs failed to match expected */
                    printf("\nXSDRTDOB: TDOs did not match!\n");
                    failed = 1;
                }
                break;

            case XSDRTDOE:
                DPrintF("XSDRTDOx");
                /* get the data value to be shifted in */
                readVal(&buf, &env->dataVal, BYTES(env->SDRSize));

                /* store the TDOExpected value    */
                readVal(&buf, &env->TDOExpected, BYTES(env->SDRSize));

                /* shift in the data value and verify the TDO value against */
                /* the expected value                                       */
                if (!loadSDRTDOE(iO, env, tmp))
                {
                    /* The ISP operation TDOs failed to match expected */
                    printf("\nXSDRTDOB: TDOs did not match!\n");
                    failed = 1;
                }
                break;

            case XSDR:
                DPrintF("XSDR");
                readVal(&buf, &env->dataVal, BYTES(env->SDRSize));

                /* use TDOExpected from last XSDRTDO instruction */
                if (env->SDRSize && !loadSDR(iO, env, tmp))
                {
                    printf("\nXSDR: TDOs did not match!\n");
                    failed = 1;  /* TDOs failed to match expected */
                }
                break;

            case XSDRB:
                 DPrintF("XSDRB");
                 readVal(&buf, &env->dataVal, BYTES(env->SDRSize));
                 clockOutBit(iO, TMS,1);  /* Select-DR-Scan state  */
                 clockOutBit(iO, TMS,0);  /* Capture-DR state      */
                 clockOutBit(iO, TMS,0);  /* Shift-DR state        */
                 shiftOutLenValStoreTDO(iO, &env->dataVal, env->SDRSize, NULL, 0);
                 break;

            case XSDRC:
                DPrintF("XSDRC");
                readVal(&buf, &env->dataVal,BYTES(env->SDRSize));
                shiftOutLenValStoreTDO(iO, &env->dataVal, env->SDRSize, NULL, 0);
                break;

            case XSDRE:
                DPrintF("XSDRE");
                readVal(&buf, &env->dataVal,BYTES(env->SDRSize));
                shiftOutLenValStoreTDO(iO, &env->dataVal, env->SDRSize, NULL, 0);
                clockOutBit(iO, TMS, 1);  /* Update-DR state    */
                clockOutBit(iO, TMS, 0);  /* Run-Test/Idle state*/
                break;

            case XSDRINC:
                DPrintF("XSDRINC");
                readVal(&buf, &env->dataVal, BYTES(env->SDRSize));
                if (!loadSDR(iO, env, tmp))
                {
                    printf("\nXSDRINC: TDOs did not match!\n");
                    failed = 1;  /* TDOs failed to match expected */
                }

                sdrInstructs = *buf++;
                for (i = 0; !failed && (i < sdrInstructs); i++)
                {
                   readVal(&buf, tmp, 1);
                   doSDRMasking(env, tmp);
                   if (!loadSDR(iO, env, tmp))
                   {
                       printf("\nXSDRINC: TDOs did not match!\n");
                       failed = 1;  /* TDOs failed to match expected */
                   }
                }
                break;

            case XSETSDRMASKS:
                DPrintF("XSETSDRMASKS");
                /* read the addressMask */
                readVal(&buf, &env->addressMask, BYTES(env->SDRSize));
                /* read the dataMask    */
                readVal(&buf, &env->dataMask, BYTES(env->SDRSize));
                break;

            case XSDRSIZE:
                DPrintF("XSDRSIZE");
                /* set the SDRSize value */
                readVal(&buf, tmp, 4);
                env->SDRSize = value(tmp);
                break;

            case XSTATE:
                /* Goto a specified JTAG stable TAP state */
                readVal(&buf, &env->dataVal, 1);

                if (value(&env->dataVal) == XSTATE_RESET)
                {
                    for (i = 0; i < 5; i++)
                        pulseClock(iO);
                    setPort(iO, TMS, 0);
                    pulseClock(iO);
                }
                else if (value(&env->dataVal) == XSTATE_RUNTEST)
                {
                    /* Can only come from Test-Logic-Reset or Run-Test/Idle */
                    clockOutBit(iO, TMS, 0);  /* Run-Test/Idle state   */
                }
                break;

                case XCOMPLETE:
                    /* Should never get here */
                    break;

                default:
                    printf("\nCorrupt input file\n");
                    failed = 1;
                    break;
        }

        bytesProgrammed = (uint32)((uintpsize)buf - (uintpsize)startBuf + 1);
        percent = (int32)(((float)bytesProgrammed / (float)size) * 100.0);

        if( percent > lastPercent )
        {
            printf("%i/%i bytes - %i%% programmed\r", bytesProgrammed,
                   size, percent);
            lastPercent = percent;
            fflush(stdout);
        }
    }

    printf("\n");

    if (failed)
    {
        printf("JTAG programming failed\n");
    }
    else
    {
        /* Goto reset */
        setPort(iO, TMS, 1);
        for(i = 0; i < 5; i++)
            pulseClock(iO);
    }

    setPort(iO, ENABLE, 0);

    free(env);
    free(tmp);

    return failed;
}
