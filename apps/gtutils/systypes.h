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

#ifndef __SYS_TYPES_H__
#define __SYS_TYPES_H__

#define FILE_REV_SYSTYPES_H    "6"     /* 2011/08/30 */


#if defined PLATFORM_VISA || defined _CVI_

#include <vpptype.h>
#include <cvidef.h>
typedef ViInt64          int64;
typedef ViUInt64         uint64;
typedef ViInt32          int32;
typedef ViUInt32         uint32;

typedef ViInt16          int16;
typedef ViUInt16         uint16;
typedef ViInt8           int8;
typedef ViUInt8          uint8;
typedef ViUInt64         uintpsize;

#elif defined PLATFORM_WIN || defined PLATFORM_RTX

typedef __int64             int64;
typedef unsigned __int64    uint64;
typedef __int32             int32;
typedef unsigned __int32    uint32;
typedef __int16             int16;
typedef unsigned __int16    uint16;
typedef __int8              int8;
typedef unsigned __int8     uint8;

typedef unsigned long       uintpsize;   /* int the size of pointer 
                                            64-bit on 64-bit apps,
                                            32-bit on 32-bit apps */

#else

typedef long long           int64;
typedef unsigned long long  uint64;
typedef int                 int32;
typedef unsigned int        uint32;
typedef short               int16;
typedef unsigned short      uint16;
typedef char                int8;
typedef unsigned char       uint8;

typedef unsigned long       uintpsize;   /* int the size of pointer 
                                            64-bit on 64-bit apps,
                                            32-bit on 32-bit apps */

#endif

/* 
   Macros used to use for casting pointers to 64-bit ints
   and 64-bit ints to pointers (without compiler warnings).  
   example:
       uint64 val = 0x12345678;
       uint32 *ptr = UINT64_TO_PTR(uint32, val);
   Use with caution. 
*/
   
#define PTR_TO_UINT64(ptr)              ((uint64)((uintpsize)ptr))
#define UINT64_TO_PTR(ptr_type, val64)  ((ptr_type*)((uintpsize)val64))


#endif /* __SYS_TYPES_H__ */
