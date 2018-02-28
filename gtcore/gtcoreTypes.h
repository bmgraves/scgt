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

/*
 * @file gtcoreTypes.h
 */

#ifndef __GTCORE_TYPES_H__
#define __GTCORE_TYPES_H__

#include "ksys.h"
#include "gtucore.h"
#include "scgtdrv.h"

#define FILE_REV_GTCORETYPES_H  "4"   /* 09/1/2011 */

#define GTCORE_EXCH_CNT  4  /**< number of exchanges per direction */

/********************************************************************/
/************************* STATISTICS *******************************/
/********************************************************************/

enum
{
    SCGT_STATS_NS_ENTRY,
    SCGT_STATS_NAMES_STR_LEN,
    SCGT_STATS_ISR,
    SCGT_STATS_DPC,
    SCGT_STATS_DPC_DMA,
    SCGT_STATS_INTRS,
    SCGT_STATS_TC0_INTRS,
    SCGT_STATS_TC1_INTRS,
    SCGT_STATS_W_ENTRY_SEM_1,
    SCGT_STATS_W_ENTRY_SEM_2,
    SCGT_STATS_R_ENTRY_SEM_1,
    SCGT_STATS_R_ENTRY_SEM_2,
    SCGT_STATS_W_EXCH_SEM_0,
    SCGT_STATS_W_EXCH_SEM_1,
    SCGT_STATS_W_EXCH_SEM_2,
    SCGT_STATS_W_EXCH_SEM_3,
    SCGT_STATS_R_EXCH_SEM_0,
    SCGT_STATS_R_EXCH_SEM_1,
    SCGT_STATS_R_EXCH_SEM_2,
    SCGT_STATS_R_EXCH_SEM_3,
    SCGT_STATS_NET_INTRS,
    SCGT_STATS_LINK_ERRORS,
    SCGT_STATS_GET_INTR_TIMER,
    SCGT_STATS_GET_INTR_WAIT_CNT,
    SCGT_STATS_NET_INT_CNT_ROLL,
    SCGT_STATS_NET_INT_CNT_FIX,
    SCGT_STATS_NET_INT_CNT_0,
    SCGT_STATS_NET_INT_CNT_1,
    SCGT_STATS_NET_INT_CNT_FIX_FAILED,
    SCGT_STATS_SW_NET_INT_CNT_VAL,
    SCGT_STATS_HW_NET_INT_CNT_VAL,
    SCGT_STATS_NUM_STATS
};

/********************************************************************/
/************************* DEVICE CORE ******************************/
/********************************************************************/

#define SCGT_DEVICE_CORE  \
        /*************************************************************/ \
        /* the following members should be initialized by the driver */ \
        uint8 unitNum;                                                  \
        char boardLocationStr[128];                                     \
        void *mapData;   /**< passed to ksysMapVirtToBus */               \
        uint32 memSize;  /**< mapped memSize */                           \
        /*************************************************************/ \
        /* the following members are initialized by gtcoreInit()     */ \
        uint32 popMemSize;  /**< populated memSize */                   \
        gtcoreExchMgrData rexch;  /**< Receive Exchange */              \
        gtcoreExchMgrData wexch;  /**< Write  Exchange */               \
        gtcoreExch *completedExchQ[GTCORE_EXCH_CNT * 2];                \
        uint32 completedHeadIndex;                                      \
        uint32 completedTailIndex;                                      \
        uint32 stats[SCGT_STATS_NUM_STATS];    /**< statistics */         \
        gtcoreIntrQData intrQData;         /**< interupt queue data */   \
        uint32 hwNHIQIntCntrVal;

 
/**
 * core data types (kernel level)
 */
 
typedef struct _gtcoreExch
{
    /*
     * SGPTR_ARRAY_LEN, DMA_CHAIN_LEN and CACHE_LINE_SIZE are defined
     * in scgtdrv.h since they are system specific
     */
    volatile uint32         gtMemoryOffset;    /**< Offset into shared memory offset */
    volatile uint32         bytesToTransfer;   /**< total bytes to be transferred */
    volatile uint32         bytesTransferred;  /**< running total of bytes transferred */
    volatile scgtInterrupt *intr;              /**< network interrupt */
    volatile uint32         flags;             /**< control flags */

    volatile uint32*   sgList[SCGT_SGPTR_ARRAY_LEN]; /**< scatter gather list */
    ksysSemB           compSem;        /* signals exchange completion */
    volatile uint32    state;          /**< state of exchange (GTCORE_EXCH_UNUSED etc.) */ 
    volatile uint32    status;         /**< completion status of the exchange  */
    volatile uint32    *tqe;           /**< Pointer to the transaction queue entry */
    volatile uint64    sgListPhysAddr[SCGT_SGPTR_ARRAY_LEN]; /**< physical address of sgList */
    volatile uint32    exchQIndex;     /**< index in the exchange array for this exchange */
    volatile uint32*   sgSpace;        /**< unaligned pointer to memory holding sgList */
    void*              sgDmaHandle;    /**< passed to ksysDma2Malloc and friends */
    uint8              direction;      /**< direction read/write */
    uint32             notUsed[8];     /**< space reserved to accomodate unusual cases */
} gtcoreExch;


typedef struct _gtcoreExchMgrData
{
    volatile uint32   headIndex;    /**< current head index of exchange queue */
    volatile uint32   tailIndex;    /**< current tail index of exchange queue */
    uint32*           trQueue;      /**< aligned transaction queue address */
    uint32*           trSpace;      /**< transaction queue address space */
    uint64            trPhysical;   /**< Memory aligned PCI bus address of transaction queue*/
    void*             trDmaHandle;  /**< passed to ksysDma1Malloc and friends */
    uint32            notUsed[8];   /**< space reserved to accomodate unusual cases */
    gtcoreExch        exchQ[GTCORE_EXCH_CNT]; 
} gtcoreExchMgrData;


typedef volatile struct _gtcoreIntrQData
{
    scgtInterrupt *intrQ;   /**< interupt data */
    uint32 head;            /**< interupt queue head pointer */
    uint32 seqNum;          /**< sequence number */
} gtcoreIntrQData;

#endif   /* __GTCORE_TYPES_H__ */
