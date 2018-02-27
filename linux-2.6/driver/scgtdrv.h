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
/*                                                                            */
/*    Module      : scgtdrv.h                                                 */
/*    Description : SCRAMNet GT driver external interface definition          */
/*    Platform    : Linux                                                     */
/*                                                                            */
/******************************************************************************/

#ifndef __SCGT_DRV_H__
#define __SCGT_DRV_H__

#include <asm/ioctl.h>
#include "systypes.h"
#include "gtucore.h"

#define FILE_REV_SCGTDRV_H   "2"   /* 12/10/03 */

/**************************************************************************/
/**************************  D E F I N E S ********************************/
/**************************************************************************/

#define SCGT_DEV_NAME "scgt"
#define SCGT_MAX_DEVICES  16

#define SCGT_DEV_FILE_STR  "/dev/scgt"

#define SCGT_SGPTR_ARRAY_LEN 1
#define SCGT_MAX_CHUNK_SIZE  0x40000  /* 256K chunk size */
#define SCGT_DMA_CHAIN_LEN  (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+2)

#define scgtWriteCReg(pdev, offset, val) \
        ksysWriteReg(pdev->cRegPtr, offset, val)
        
#define scgtReadCReg(pdev, offset) \
        ksysReadReg(pdev->cRegPtr, offset)

#define scgtWriteNMReg(pdev, offset, val) \
        ksysWriteReg(pdev->nmRegPtr, offset, val)
        
#define scgtReadNMReg(pdev, offset) \
        ksysReadReg(pdev->nmRegPtr, offset)


/******************************************************************/        
/************************* IOCTL Defs *****************************/
/******************************************************************/

#define SCGT_MAGIC                  'F'
#define SCGT_IOCTL_WRITE            _IOWR(SCGT_MAGIC, 1, scgtXfer)
#define SCGT_IOCTL_READ             _IOWR(SCGT_MAGIC, 2, scgtXfer)
#define SCGT_IOCTL_READ_CR          _IOWR(SCGT_MAGIC, 3, scgtRegister)
#define SCGT_IOCTL_WRITE_CR         _IOW(SCGT_MAGIC,  4, scgtRegister)
#define SCGT_IOCTL_MEM_MAP_INFO     _IOR(SCGT_MAGIC,  5, uint32)
#define SCGT_IOCTL_GET_DEVICE_INFO  _IOR(SCGT_MAGIC,  6, scgtDeviceInfo)
#define SCGT_IOCTL_GET_STATE        _IOWR(SCGT_MAGIC, 7, scgtState)
#define SCGT_IOCTL_SET_STATE        _IOW(SCGT_MAGIC,  8, scgtState)
#define SCGT_IOCTL_READ_NMR         _IOWR(SCGT_MAGIC, 9, scgtRegister)
#define SCGT_IOCTL_WRITE_NMR        _IOW(SCGT_MAGIC, 10, scgtRegister)
#define SCGT_IOCTL_GET_STATS        _IOR(SCGT_MAGIC, 11, uint32)
#define SCGT_IOCTL_GET_INTR         _IOWR(SCGT_MAGIC, 12, scgtGetIntrBuf)

/* debugging */
#define SCGT_IOCTL_PUT_INTR         _IOR(SCGT_MAGIC, 13, scgtInterrupt)

#endif /* __SCGT_DRV_H__ */
