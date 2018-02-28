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

/**
 * @file   gtcore.h
 * @brief  core portions available to only driver.
 */


#ifndef __GT_CORE_H__
#define __GT_CORE_H__

/************************************************************************/
/**************************  I N C L U D E S  ***************************/
/************************************************************************/

#include "systypes.h"
#include "ksys.h"
#include "scgt.h"
#include "gtucore.h"
#include "gtcoreTypes.h"
#include "gtcoreIoctl.h"

#define FILE_REV_GTCORE_H  "8"   /* 08/29/11 */

/******************************************************************/
/********************* COMMON DEFINITIONS *************************/
/******************************************************************/

/** exchange states */
#define GTCORE_EXCH_NOT_DONE  0   /**< Enchage not done */
#define GTCORE_EXCH_DONE      1   /**< done with exchage*/

#define GTCORE_WRITE 0           /**< Write operation */
#define GTCORE_READ 1            /**< Read opertion */

#define GTCORE_INTR_Q_SIZE   1024  /**< Interrupt Queue Size. (must be power of 2) */

/** interrupt type bitmask definitions **/
#define GTCORE_ISR_HANDLED           0x1     /**< ISR Handled */
#define GTCORE_ISR_DMA               0x2     /**< DMA Interrupt */
#define GTCORE_ISR_QUEUED_INTR       0x4     /**< queued interrupt */


/** a useful register read macro */
#define scgtWriteCRegMasked(dev, offset, val, mask)  \
               scgtWriteCReg((dev), (offset), ((scgtReadCReg(dev, offset) & ~(mask)) | ((val) & (mask))))


/******************************************************************/
/*************** TRANSACTION and CHAIN DEFINITIONS ****************/
/******************************************************************/

/** transaction queue entry */
#define GTCORE_TQE_SIZE        (8*4) /**< size of Transaction Queue Block */
#define GTCORE_TQE_TNS_CSR        0  /**< transaction CSR */
#define GTCORE_TQE_SMO            1  /**< shared memory offset */
#define GTCORE_TQE_CE_ADD32       2  /**< chain PCI A32 start address */
#define GTCORE_TQE_NI_CTL         3  /**< network interrupt control (send only) */
#define GTCORE_TQE_NI_VECT        4  /**< network interrupt vector (send only) */
#define GTCORE_TQE_CE_ADD64       5  /**< chain PCI A64 start address */
#define GTCORE_TQE_RESERVED_0     6
#define GTCORE_TQE_RESERVED_1     7

/** TQE CSR bit defs */
#define GTCORE_TQE_LEN          0x007FFFFF   /**< Trasaction lenght mask */
#define GTCORE_TQE_IDW_SWAP_OR  0x01000000   /**< Word swap data */
#define GTCORE_TQE_IDB_SWAP_OR  0x02000000   /**< Byte swap data */
#define GTCORE_TQE_SKP_ENTRY    0x04000000   /**< skip Entry */
#define GTCORE_TQE_TIMEOUT      0x10000000   /**< time out */
#define GTCORE_TQE_ERR          0x20000000   /**< error  */
#define GTCORE_TQE_FIXED_1      0x40000000   /**< Fixed 1 */

/* TQE network interrupt control (TQE_NI_CTL) bit defs */
#define GTCORE_TQE_NET_INT        0x1     /**< Enable network intertupt. 1 for interrupt */
#define GTCORE_TQE_NET_INT_TYPE   0x2     /**< Interrupt Type */
#define GTCORE_TQE_NET_INT_NUM    0xFF00  /**< Network interrupt number mask */

/* chain entry */
#define GTCORE_CE_SIZE         (8*4) /**< Size of one chain entry block */
#define GTCORE_CE_BUF_ADD32     0
#define GTCORE_CE_RESERVED_0    1
#define GTCORE_CE_TNS_CSR       2
#define GTCORE_CE_NEXT_ADD32    3
#define GTCORE_CE_BUF_ADD64     4
#define GTCORE_CE_NEXT_ADD64    5
#define GTCORE_CE_RESERVED_1    6
#define GTCORE_CE_RESERVED_2    7

/* chain entry transaction CSR bit defs */
#define GTCORE_CE_LEN        0x007FFFFF
#define GTCORE_CE_LST        0x01000000  /* last chain entry */


/******************************************************************
* PCI DEFINITIONS
*******************************************************************/

/*
 * Device and Vendor ID's
 */
#define GTCORE_VENDOR_ID     0x1387
#define GTCORE_DEVICE_ID     0x5310

/*
 * BAR definitions
 */
 
#define GTCORE_BAR_C_REGISTERS  0         /**< configuration registers */
#define GTCORE_BAR_NM_REGISTERS 1         /**< net management registers */
#define GTCORE_BAR_MEMORY       2         /**< GT memory */

/*
 * PCI config registers
 */
 
#define GTCORE_PCI_IDR                0x0
#define GTCORE_PCI_CR_SR              0x4
#define GTCORE_PCI_REV_CCR            0x8
#define GTCORE_PCI_CLSR_LTR_HTR_BISTR 0xC
#define GTCORE_PCI_BAR0               0x10
#define GTCORE_PCI_BAR1               0x14
#define GTCORE_PCI_BAR2               0x18
#define GTCORE_PCI_BAR3               0x1C
#define GTCORE_PCI_BAR4               0x20
#define GTCORE_PCI_BAR5               0x24
#define GTCORE_PCI_CIS                0x28
#define GTCORE_PCI_SVID_SID           0x2C
#define GTCORE_PCI_ERBAR              0x30
#define GTCORE_PCI_ILR_IPR_IMGR_MLR   0x3C

/* GTCORE_PCI_CR_SR bit defs */
#define GTCORE_PCI_MASTER_ACCESS_ENABLE  0x00000004


/****************************************************************/
/********************** REGISTER DEFS ***************************/
/****************************************************************/

#define GTCORE_REGISTER_SIZE     0x100

/* register offsets */
#define GTCORE_R_BRD_INFO       0x0
#define GTCORE_R_DRV_BRD_CSR    0x4
#define GTCORE_R_USR_BRD_CSR    0x8
#define GTCORE_R_INT_CSR        0xc
#define GTCORE_R_LINK_CTL       0x10
#define GTCORE_R_LINK_STAT      0x14
#define GTCORE_R_RX_HBI_MSK     0x18
#define GTCORE_R_RX_HUI_MSK     0x1c
#define GTCORE_R_MISC_FNCTN     0x20

#define GTCORE_R_NET_TMR        0x40
#define GTCORE_R_HST_TMR        0x44
#define GTCORE_R_LAT_TMR        0x48
#define GTCORE_R_SM_TRFC_CNTR   0x4c
#define GTCORE_R_INT_TRFC_CNTR  0x50
#define GTCORE_R_HNT_TRFC_CNTR  0x54
#define GTCORE_R_NHIQ_INT_CNTR  0x58
#define GTCORE_R_HST_INT_CNTR   0x5c

#define GTCORE_R_LNK_ERR_CNTR   0x80
#define GTCORE_R_LNK_DOWN_CNTR  0x84
#define GTCORE_R_DEC_ERR_CNTR   0x88
#define GTCORE_R_SYNC_ERR_CNTR  0x8c
#define GTCORE_R_CRC_ERR_CNTR   0x90
#define GTCORE_R_EOF_ERR_CNTR   0x94
#define GTCORE_R_PRTCL_ERR_CNTR 0x98
#define GTCORE_R_RXF_ERR_CNTR   0x9c

#define GTCORE_R_TC_WDT_VAL     0xbc
#define GTCORE_R_TQ_ADD32_TC0   0xc0
#define GTCORE_R_TQ_ADD64_TC0   0xc4
#define GTCORE_R_TQ_CTL_TC0     0xc8
#define GTCORE_R_TNS_LEN_TC0    0xcc
#define GTCORE_R_TQ_ADD32_TC1   0xd0
#define GTCORE_R_TQ_ADD64_TC1   0xd4
#define GTCORE_R_TQ_CTL_TC1     0xd8
#define GTCORE_R_TNS_LEN_TC1    0xdc

/****************************************************************/
/********************* Bit Definitions **************************/
/****************************************************************/

/*** BRD_INFO ***/ 
#define GTCORE_R_MEM_PMS        0x3
#define GTCORE_R_MEM_RTMS       0x3c
#define GTCORE_R_MEM_TYPE       0xc0     /*!! may be removed */
#define GTCORE_R_A64_TSPT       0x100
#define GTCORE_R_A64_ISPT       0x200
#define GTCORE_R_BUS_SPT        0x1c00
#define GTCORE_R_PHY_TYPE       0x6000
#define GTCORE_R_EXT_REV_ID     0x00ff0000
#define GTCORE_R_REV_ID         0xff000000

/*** DRV_BRD_CSR ***/
#define GTCORE_R_STC_A          0xF
#define GTCORE_R_TCB_SWAP       0x10   /**< target control byte swap */
#define GTCORE_R_ICB_SWAP       0x20   /**< TQE and CE byte swap */

/*** USR_BRD_CSR ***/
#define GTCORE_R_DW_SWAP        0x1    /**< data word swap */
#define GTCORE_R_DB_SWAP        0x2    /**< data byte swap */
#define GTCORE_R_INIT_D64_DIS   0x10   /**< 64-bit initiator disable */

/*** INT_CSR ***/
#define GTCORE_R_TC0_INT        0x1
#define GTCORE_R_TC1_INT        0x2
#define GTCORE_R_LNK_ERR_INT    0x4
#define GTCORE_R_RX_NET_INT     0x8
#define GTCORE_R_TC0_INT_EN     0x10000
#define GTCORE_R_TC1_INT_EN     0x20000
#define GTCORE_R_LNK_ERR_INT_EN 0x40000
#define GTCORE_R_RX_NET_INT_EN  0x80000

#define GTCORE_R_ALL_INT_EN  (GTCORE_R_TC0_INT_EN     | \
                              GTCORE_R_TC1_INT_EN     | \
                              GTCORE_R_LNK_ERR_INT_EN | \
                              GTCORE_R_RX_NET_INT_EN)


/*** LINK_CTL ***/
#define GTCORE_R_LAS_0_EN       0x1
#define GTCORE_R_LAS_1_EN       0x2
#define GTCORE_R_LNK_SEL        0x10
#define GTCORE_R_NODE_ID        0xFF00
#define GTCORE_R_TX_EN          0x10000
#define GTCORE_R_RX_EN          0x20000
#define GTCORE_R_RT_EN          0x40000
#define GTCORE_R_WRAP           0x80000
#define GTCORE_R_WML            0x100000
#define GTCORE_R_INT_SELF       0x200000
#define GTCORE_R_HNT_ID         0xFF000000


/*** LINK_STAT ***/
#define GTCORE_R_LNK_DOWN       0x1
#define GTCORE_R_DEC_ERR        0x2
#define GTCORE_R_SYNC_ERR       0x4
#define GTCORE_R_CRC_ERR        0x8
#define GTCORE_R_EOF_ERR        0x10
#define GTCORE_R_PRTCL_ERR      0x20
#define GTCORE_R_RXF_ERR        0x40
#define GTCORE_R_RX_STRP_ERR    0x80
#define GTCORE_R_EXP_MSG_ERR    0x1000
#define GTCORE_R_LNK_UP         0x20000
#define GTCORE_R_LAS_0_SD       0x40000
#define GTCORE_R_LAS_1_SD       0x80000
#define GTCORE_R_RLC            0x800000
#define GTCORE_R_AGE            0xFF000000

/*** MISC_FNCTN ***/
#define GTCORE_R_DIS_RD_BYP     0x200
#define GTCORE_R_UNID           0xFF000000


/*** TQ_CTL_TC0 ***/
#define GTCORE_R_TQ_PRD_IDX     0x0000001F
#define GTCORE_R_TQ_CON_IDX     0x00001F00
#define GTCORE_R_TQ_LEN         0x001F0000
#define GTCORE_R_TQ_EN          0x01000000
#define GTCORE_R_TQ_RST         0x02000000
#define GTCORE_R_TQ_ABRT        0x04000000
#define GTCORE_R_TQ_PSD         0x20000000
#define GTCORE_R_TQ_AVAIL       0x40000000
#define GTCORE_R_TQ_PRSRV       0x80000000

/******* Network Management Registers ******/
#define GTCORE_NM_REGISTER_SIZE    0x2000

#define GTCORE_NM_NHI_QID     0x0800
#define GTCORE_NM_TX_HBI      0x1000    /**< use (GTCORE_NM_TX_HBI + HBI Number * 4) */
#define GTCORE_NM_TX_HUI      0x1800    /**< use (GTCORE_NM_TX_HUI + (destination ID * 4)) */

/* NM_NHI_QID bit defs */
#define GTCORE_NM_HI_TYPE   0x10000

/**********************************************************/
/********************* GLOBAL DEFS ************************/
/**********************************************************/

extern char gtcoreStatNames[];
extern uint32 gtcoreStatNameIndex[];

/**********************************************************/
/****************** CORE FUNCTION DEFS ********************/
/**********************************************************/

/* the following can be used before calling gtcoreInit() */
void gtcoreFixRegSwapping(scgtDevice *dev);
uint32 gtcoreGetPopMemSize(scgtDevice *dev);


/* use the remaining functions after calling gtcoreInit() */
uint32 gtcoreInit(scgtDevice *dev);
void gtcoreDestroy(scgtDevice *dev);
uint32 gtcoreGetState(scgtDevice *dev, uint32 stateID, uint32 *val);
uint32 gtcoreSetState(scgtDevice *dev, uint32 stateID, uint32 val);
uint32 gtcoreGetDeviceInfo(scgtDevice *dev, scgtDeviceInfo *deviceInfo);

void gtcorePutIntr(scgtDevice *dev, scgtInterrupt *intr);
uint32 gtcoreGetIntr(scgtDevice *dev, scgtGetIntrBuf *gibuf);
uint32 gtcoreSendIntr(scgtDevice *dev, scgtInterrupt *intr);

uint32 gtcoreGetStats(scgtDevice *dev, scgtStats *stats);

gtcoreExch * gtcoreGetExchange(scgtDevice *dev, uint8 direction);
void gtcoreTransfer(scgtDevice *dev, gtcoreExch *exch, uint8 direction);
void gtcoreCancelTransfer(scgtDevice *dev, gtcoreExch *exch, uint8 direction);

uint32 gtcoreHandleInterrupt(scgtDevice *dev);
void gtcoreCompleteDMA(scgtDevice *dev);

uint32 gtcoreSizeOfSCGTDevice(void);  /**< debugging for precompiled core */

#endif /* __GT_CORE_H__ */
