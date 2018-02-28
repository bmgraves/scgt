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

#ifndef __GTIOCTL_H__
#define __GTIOCTL_H__

#include "systypes.h"
#include "gtucore.h"


#define FILE_REV_GTCOREIOCTL_H  "4"   /* 09/22/04 */

/**************************************************************************/
/**************************  D A T A  T Y P E S  **************************/
/**************************************************************************/

/*
 * The following data types are used by the API to pass information
 * to and from the driver.  They are not to be used by user applications.
 * The common data types seen by the user are defined in gtcore/gtucore.h
 * The reserved member of these structures is used by the Windows driver
 * to return status info to the driver.
 */

/*
 * STRUCTURE NOTE:
 * Each structure containing pointers must be accompanied by a alternate
 * version to be used by 64-bit drivers when interfacing a 32-bit app.
 * These special structure versions should have all pointer-type members
 * replaced with 32-bit integer-type members of the same name.
 */


/*
 * state struct - for GET_STATE and SET_STATE ioctls
 */

typedef struct _scgtState
{
    uint32 reserved;
    uint32 stateID;
    uint32 val;
} scgtState;


/*
 * scgtXfer - for READ and WRITE ioctls
 */
 
typedef struct _scgtXfer
{
    uint32 reserved;
    uint32 flags;
    uint32 gtMemoryOffset; 
    uint32 bytesToTransfer;
    uint32 bytesTransferred;
    uint64 pDataBuffer;       /* address of data buffer */
    uint64 pInterrupt;        /* address of scgtInterrupt *pInterrupt */
} scgtXfer;


/*
 * scgtRegister - for READ_CR, WRITE_CR, READ_NMR and WRITE_NMR ioctls
 */
 
typedef struct _scgtRegister
{
    uint32 reserved;
    uint32 offset;
    uint32 val;
} scgtRegister;


/*
 * scgtMemMapInfo - used by API to map/umap memory using mmap() and munmap()
 */
 
typedef struct _scgtMemMapInfo
{
    uint32 reserved;
    uint32 memSize;
    uint64 memVirtAddr;
    uint64 memPhysAddr;
} scgtMemMapInfo;


/*
 * driver statistics struct
 */

typedef struct _scgtStats
{
    uint64 stats;             /* address of uint32 *stats */
    uint64 names;             /* address of char *names; */
    uint32 firstStatIndex;
    uint32 num;
    uint32 reserved;
} scgtStats;


/*
 * get interrupt ioctl structure
 */

typedef struct _scgtGetIntrBuf
{
    uint64 intrBuf;  /* address of scgtInterrupt *intrBuf */
    uint32 bufSize;
    uint32 seqNum;
    uint32 timeout;
    uint32 numInterruptsRet;
    uint32 reserved;
} scgtGetIntrBuf;



#endif /* __GTIOCTL_H__ */
