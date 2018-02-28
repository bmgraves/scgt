/******************************************************************************/
/*                              SCRAMNet GT                                   */
/******************************************************************************/
/*                                                                            */
/* Copyright (c) 2002-2011 Curtiss-Wright Controls.                           */
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
/* This is a project fork for maintaining the driver for working with the      */
/* 3.10 kernel in Redhat/Centos 7.                                             */
/*																			   */
/* This fork is created/maintained by Brandon M. Graves, https://metashell.net */
/* up to date copies can be pulled from https://github.com/bmgraves/scgt	   */
/*																			   */
/* It is important to note all credit to the original Developer, this is merely*/
/* an ongoing support project to test for functionality in future maintained   */
/* version of linux for legacy cards.										   */
/*																			   */
/*******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*    Module      : scgt.c                                                    */
/*    Description : SCRAMNet GT driver entry points                           */
/*    Platform    : Linux                                                     */
/*                                                                            */
/******************************************************************************/

/************************************************************************/
/**************************  I N C L U D E S  ***************************/
/************************************************************************/

#include <linux/version.h>

#ifndef AUTOCONF_INCLUDED
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,10,0)
    /* nothing needed */
#else
#include <linux/config.h>   /* config.h obsolete in some kernels 
                               (in which case AUTOCONF_INCLUDED will be defined) */
#endif
#endif

#ifndef VM_RESERVED
#define VM_RESERVED 0
#endif

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/uaccess.h>

#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/sched.h>

#include "scgt.h"
#include "scgtdrv.h"
#include "systypes.h"
#include "gtcore.h"
#include "gtcoreIoctl.h"
#include "ksys.h"

/************************************************************************/
/***************************  D E F I N E S  ****************************/
/************************************************************************/

#define SCGT_DRIVER_VERSION "1.7lnx"   
char *FILE_REV_SCGT_C = "0";   /* 10/27/2015 */

#ifdef MODULE_AUTHOR
MODULE_AUTHOR("Curtiss-Wright Controls");
#endif

#ifdef MODULE_DESCRIPTION
MODULE_DESCRIPTION("SCRAMNet GT");
#endif

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#define SCGT_DEBUG 0

#if SCGT_DEBUG
#define DPrintF(fmt, args...)  printk(KERN_INFO "GTDBG: " fmt, ##args)
#else
inline void DPrintF(char *fmt, ...) {}
#endif

#define SCGT_GET_INTR_TIMER_JIFFIES  (HZ/10)  /* HZ is jiffies in 1 second */
#define SCGT_GET_INTR_TIMER_MILLISEC ((SCGT_GET_INTR_TIMER_JIFFIES * 1000) / HZ)

#define SCGT_TIMEOUT_TIME  (HZ*2)

/************************************************************************/
/****************  F U N C T I O N  P R O T O T Y P E S  ****************/
/************************************************************************/

static int scgtInitDriver(void);
static void scgtExitDriver(void);

static int scgtInitDevice(struct pci_dev *pd,
                          const struct pci_device_id *devIDEnt);
static void scgtRemoveDevice(struct pci_dev *pd);

static void scgtInitDriverRev(void);

static int scgtInitDMATools(scgtDevice *dev);
static void scgtFreeDMATools(scgtDevice *dev);

static int scgtOpen(struct inode *inode, struct file *filp);
static int scgtClose(struct inode *inode, struct file *filp);

static inline scgtDevice *scgtGetDev(struct inode *inode);

#define USE_UNLOCKED_IOCTL

#ifdef USE_UNLOCKED_IOCTL
static long scgtIoctl(struct file* filp, unsigned int cmd, unsigned long arg);
#else
static int scgtIoctl(struct inode* inode, struct file* filp,
                     unsigned int cmd, unsigned long arg);
#endif
                     
static int scgtIoctl2(scgtDevice *dev, unsigned int cmd, unsigned long arg);
static int scgtIoctlGetIntr(scgtDevice *dev, scgtGetIntrBuf *gibuf);

static void scgtInitGetIntrTimer(scgtDevice *dev);
static void scgtDestroyGetIntrTimer(scgtDevice *dev);
void scgtGetIntrTimerCallback(unsigned long devAddr);
static void scgtGetIntrTimerStart(scgtDevice *dev);

static int scgtGiveIntrSem(scgtDevice *dev);

static int scgtIoctlGetDeviceInfo(scgtDevice *dev, scgtDeviceInfo *deviceInfo);
static int scgtIoctlGetStats(scgtDevice *dev, scgtStats *stats);

irqreturn_t scgtISR(int irq, void *dev_ptr);

static int scgtMMap(struct file *file, struct vm_area_struct *vma);

static uint32 scgtIoctlXfer(scgtDevice *dev, 
                            scgtXfer *xfer, 
                            scgtInterrupt *intr,
                            uint8 direction);

static uint32 scgtXferChunk(scgtDevice *dev,
                            uint32 gtMemoryOffset, 
                            uint8  *pBuf, 
                            uint32 chunkSize,
                            uint8 lastTransfer,
                            uint32 flags,
                            scgtInterrupt *intr,
                            uint8  direction,
                            scgtDMATools *tools,
                            uint32 *bytesTransferred,
                            ksysSemS **semToGive);
                            
static int scgtMapToScatterList(scgtDevice *dev,
                                unsigned long bufAddr,
                                unsigned long numBytes,
                                struct page **pages,
                                int numPages,
                                struct scatterlist *scatList, 
                                uint8 direction);

static int scgtBuildChainList(struct scatterlist *scatList, int ntries, 
                              gtcoreExch *exch);
                              
/************************************************************************/
/***************************  G L O B A L S  ****************************/
/************************************************************************/

scgtDevice *devices[SCGT_MAX_DEVICES];
uint8 numDevices;

static struct pci_device_id scgtPciIdTbl[] = {
    { GTCORE_VENDOR_ID, GTCORE_DEVICE_ID,  // vendor, device 
      PCI_ANY_ID,                      // subvendor
      PCI_ANY_ID,                      // subdevice
      0,                               // class
      0,                               // class_mask
      0                                // driver_data
    },
    { 0,}
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
#define __devexit_p(x) x
#endif

static struct pci_driver scgtDriver = {
    name:        "scgt",
    id_table:    scgtPciIdTbl,
    probe:       scgtInitDevice,
    remove:      __devexit_p(scgtRemoveDevice),
};

/*
 * OS entry point jump table
 */
struct file_operations scgtFileOps = {
    owner:   THIS_MODULE,
#ifdef USE_UNLOCKED_IOCTL
    unlocked_ioctl: scgtIoctl,
#else
    ioctl:   scgtIoctl,
#endif
    open:    scgtOpen,
    release: scgtClose,
    mmap:    scgtMMap,
};

int scgtMajor = 0;
int getIntrTimerExit = 0;

extern char *FILE_REV_SCGT_C, *FILE_REV_GTCORE_C, *FILE_REV_GTCOREXFER_C,
            *FILE_REV_KSYS_C;

char driverRevStr[128];

/*
 * define the module init and exit entry points
 */

module_init(scgtInitDriver);
module_exit(scgtExitDriver);

/************************************************************************/
/******************************  C O D E  *******************************/
/************************************************************************/


/*
 * scgtInitDriver()
 *     Driver initialization entry point.  Registers the driver with the
 *     Linux PCI subsystem.
 */
 
static int scgtInitDriver()
{
    int err;

    scgtInitDriverRev();
    
    printk(KERN_INFO "SCGT: (c) 2002-2004 Curtiss-Wright Controls.\n");
    printk(KERN_INFO "SCGT: SCRAMNet GT(TM) Linux driver.\n");
    printk(KERN_INFO "SCGT: Revision %s built %s\n", driverRevStr, __DATE__);
    
    printk(KERN_INFO "SCGT: Device structure size: 0x%x  Core thinks: 0x%x\n",
           (uint32) sizeof(scgtDevice), gtcoreSizeOfSCGTDevice());

    memset(devices, 0, sizeof(scgtDevice *) * SCGT_MAX_DEVICES);
    numDevices = 0;

    if ((err = pci_register_driver(&scgtDriver)) < 0)
    {
        return err;
    }
    
    if ((scgtMajor = register_chrdev(0, SCGT_DEV_NAME, &scgtFileOps)) < 0)
    {
        printk("SCGT: register_chrdev() call failed!\n");
        return scgtMajor; 
    }
    
    return err;
}

/*
 * scgtExitDriver()
 *     Driver de-initialization entry point.  Unregisters driver with
 *     the PCI subsystem.
 */

static void scgtExitDriver()
{
    DPrintF("scgtExitDriver()\n");
    
    unregister_chrdev(scgtMajor, SCGT_DEV_NAME);
    
    pci_unregister_driver(&scgtDriver); /* this call causes scgtRemoveDevice()
                                           to be called for each device */
                                           
    
}

/*
 * scgtInitDriverRev()
 *     Initialize driver revision string.  This is returned to user via
 *     GetDeviceInfo ioctl.
 */
 
static void scgtInitDriverRev()
{
    char coreFiles[20];
    char driverFiles[20];
    char sysFiles[20];
   
    sprintf(driverFiles, "%s%s%s",
            FILE_REV_SCGT_C, FILE_REV_SCGT_H, FILE_REV_SCGTDRV_H);
            
    sprintf(sysFiles, "%s%s%s",
            FILE_REV_KSYS_C, FILE_REV_KSYS_H, FILE_REV_SYSTYPES_H);

    sprintf(coreFiles, "%s%s%s%s%s%s",
            FILE_REV_GTCORE_C, FILE_REV_GTCOREXFER_C,  FILE_REV_GTCORE_H,
            FILE_REV_GTCOREIOCTL_H, FILE_REV_GTCORETYPES_H, FILE_REV_GTUCORE_H);

    sprintf(driverRevStr, "%s-%s:%s:%s", SCGT_DRIVER_VERSION, driverFiles, 
            sysFiles, coreFiles);
}




#ifndef IRQF_SHARED
#define IRQF_SHARED SA_SHIRQ   /* IRQF_SHARED deprecated SA_SHIRQ in later kernels */
#endif

/*
 * scgtInitDevice
 *     Device initialization callback.  Called once per device by the Linux
 *     PCI subsystem.
 */

static int scgtInitDevice(struct pci_dev *pd,
                          const struct pci_device_id *devIDEnt)
{
    scgtDevice *dev;
    unsigned long cRegPhysAddr, cRegSize;
    unsigned long nmRegPhysAddr, nmRegSize;
    
    DPrintF("scgtInitDevice()\n");
    
    if (numDevices >= SCGT_MAX_DEVICES)
    {
        printk(KERN_ERR "SCGT: Max number of devices (%i) reached\n", 
               SCGT_MAX_DEVICES);
        goto err_out;
    }
    
    /* wake up and enable device */
    if (pci_enable_device(pd))
    {
        printk(KERN_ERR "SCGT: Unable to enable device\n");
        goto err_out;
    }
    
    pci_set_master(pd);
    
    if ((dev = kmalloc(sizeof(scgtDevice), GFP_KERNEL)) == NULL)
    {
        printk(KERN_ERR "SCGT: memory allocation error\n");
        goto err_out;
    }
    
    /****** map configuration registers ******/
    cRegPhysAddr = pci_resource_start(pd, GTCORE_BAR_C_REGISTERS);
    cRegSize = pci_resource_len(pd, GTCORE_BAR_C_REGISTERS);
    
    if (!request_mem_region(cRegPhysAddr, cRegSize, "scgt")) 
    {
        printk(KERN_ERR "SCGT: cannot reserve SCGT register region\n");
        goto err_req_reg_region_failed;
    }
    
    if ((dev->cRegPtr = ioremap(cRegPhysAddr, cRegSize)) == NULL)
    {
        printk(KERN_ERR "SCGT: failed to map config registers\n");
        goto err_ioremap_failed;
    }

#if 0
    printk("registers mapped!\n");
    /* test to see if readl does swapping... */
    printk("ptr Deref: 0x%x  readl: 0x%x\n", ((uint32 *)dev->cRegPtr)[0], 
                                              ksysReadReg(dev->cRegPtr, 0));
#endif
    
    /* turn on register byte swapping if needed */
    gtcoreFixRegSwapping(dev);
    
#ifdef __powerpc__
    /* registers are swapped by register reading macro
       We still need to swap memory and queue fetches 
       because gtcoreFixRegSwapping() thinks no swapping is needed */
    
    /* enable byte swapping for memory */
    scgtWriteCRegMasked(dev, GTCORE_R_USR_BRD_CSR, 
                        GTCORE_R_DB_SWAP, GTCORE_R_DB_SWAP);

    /* enable byte swapping when fetching transaction queue 
       entries and chains */                    
    scgtWriteCRegMasked(dev, GTCORE_R_DRV_BRD_CSR, 
                        GTCORE_R_ICB_SWAP, GTCORE_R_ICB_SWAP);
#endif
    
    /* disable interrupts if enabled (we may have crashed etc.) */
    scgtWriteCReg(dev, GTCORE_R_INT_CSR, 0);

    /****** map net management registers ******/
    nmRegPhysAddr = pci_resource_start(pd, GTCORE_BAR_NM_REGISTERS);
    nmRegSize = pci_resource_len(pd, GTCORE_BAR_NM_REGISTERS);
    
    if (!request_mem_region(nmRegPhysAddr, nmRegSize, "scgt")) 
    {
        printk(KERN_ERR "SCGT: cannot reserve SCGT net management register region\n");
        goto err_req_nm_reg_region_failed;
    }
    
    if ((dev->nmRegPtr = ioremap(nmRegPhysAddr, nmRegSize)) == NULL)
    {
        printk(KERN_ERR "SCGT: failed to map net management registers\n");
        goto err_nm_ioremap_failed;
    }

    /****** lock gt memory bar *********/
    dev->memPhysAddr = pci_resource_start(pd, GTCORE_BAR_MEMORY);
    dev->memSize = pci_resource_len(pd, GTCORE_BAR_MEMORY);
    
    /* lock GT memory so no one else can have it ... */
    if (!request_mem_region(dev->memPhysAddr, dev->memSize, "scgt")) 
    {
        printk(KERN_ERR "SCGT: Failed to reserve SCGT Memory region\n");
        printk(KERN_ERR "Phys Address: 0x%x  Size: 0x%x\n", dev->memPhysAddr,
               dev->memSize);
        goto err_req_mem_region_failed;
    }    
    
    /* initialize board location string */
    snprintf(dev->boardLocationStr, 128, "bus %i slot %i", 
             pd->bus->number, PCI_SLOT(pd->devfn));

    dev->pciDev = pd;
    dev->mapData = pd;  /* used for ksysDmaMalloc etc. */
    
    /*
     * Search for free unit number... This allows hot-plug to play nicely
     * with the devices array.
     */
    for (dev->unitNum = 0; 
        (dev->unitNum < SCGT_MAX_DEVICES) && (devices[dev->unitNum] != NULL); 
         dev->unitNum++) 
    {}

    devices[dev->unitNum] = dev;
    numDevices++;
    
    pci_set_drvdata(pd, dev);
    
    /*
     * initialize DMA resources
     */
    
    if (scgtInitDMATools(dev))
    {
        printk(KERN_ERR "SCGT: could not allocate DMA resources\n");
        goto err_init_dma_tools_failed;
    }
    
    /* enable interrupts */
    if (request_irq(pd->irq,                   /* interrupt */
                    &scgtISR,                  /* handler */
                    IRQF_SHARED,               /* flags - shared */
                    "scgt",                    /* proc name for stats */
                    (void*)dev) != 0)          /* pass in device struct */
    {
         printk(KERN_ERR "SCGT: could not connect interrupt - quiting!\n");
         goto err_req_irq_failed;
    }
    
    scgtInitGetIntrTimer(dev);
    
    /* initialize core */
    if (gtcoreInit(dev) != SCGT_SUCCESS)
    {
        printk(KERN_ERR "SCGT: failed to initialize gtcore resources!\n");
        goto err_gtcore_init_failed;
    }
    
    DPrintF("scgtInitDevice() successful for unit: %i\n", dev->unitNum);
    
    return 0;

err_gtcore_init_failed:
    scgtFreeDMATools(dev);
err_init_dma_tools_failed:    
err_req_irq_failed: 
    release_mem_region(dev->memPhysAddr, dev->memSize);
err_req_mem_region_failed:
err_nm_ioremap_failed:
    release_mem_region(nmRegPhysAddr, nmRegSize);
err_req_nm_reg_region_failed:
    iounmap(dev->cRegPtr);
err_ioremap_failed:
    release_mem_region(cRegPhysAddr, cRegSize);
err_req_reg_region_failed:
    kfree(dev);
err_out:
    return -ENODEV;
}

/*
 * scgtRemoveDevice
 *     Device de-initialization callback.  Called once per device by the Linux
 *     PCI subsystem on module unload.  Also called when a device is removed
 *     where hot-plug is available.
 */

static void scgtRemoveDevice(struct pci_dev *pd)
{
    scgtDevice *dev = pci_get_drvdata(pd);

    DPrintF("scgtRemoveDevice(): Removing UNIT %i\n", dev->unitNum);

    gtcoreDestroy(dev);          /* interrupts are disabed here */
    scgtDestroyGetIntrTimer(dev);
    
    free_irq(dev->pciDev->irq, dev);
    
    pci_disable_device(pd);
    
    scgtFreeDMATools(dev);
    
    iounmap(dev->cRegPtr);
    iounmap(dev->nmRegPtr);
    
    release_mem_region(dev->memPhysAddr, dev->memSize);
    release_mem_region(pci_resource_start(pd, GTCORE_BAR_C_REGISTERS), 
                       pci_resource_len(pd, GTCORE_BAR_C_REGISTERS));
    release_mem_region(pci_resource_start(pd, GTCORE_BAR_NM_REGISTERS), 
                       pci_resource_len(pd, GTCORE_BAR_NM_REGISTERS));
    
    numDevices--;
    devices[dev->unitNum] = NULL;
    
    kfree(dev);
}

/*
 * scgtGetDev()
 *     Utility function that returns the device associated with specified
 *     inode
 */
 
static inline scgtDevice *scgtGetDev(struct inode *inode)
{
    int unit;

    /*
     * Determine which device this request is for
     */
    unit = MINOR(inode->i_rdev);
    if (unit < 0 || unit > SCGT_MAX_DEVICES)
        return NULL;

    return devices[unit];
}


/*
 * scgtOpen()
 *     Open is responsible for letting the driver know
 *     it is in use, nothing else
 */
static int scgtOpen(struct inode *inode, struct file *filp)
{
    scgtDevice *dev;
    
    DPrintF("scgtOpen()\n");

    if ((dev = scgtGetDev(inode)) == NULL)  /* see if device exists */
        return -ENODEV;     
    
   /*
    * Set up driver file operations
    */
    filp->f_op = &scgtFileOps;
    filp->private_data = dev;

    return 0;
}

/*
 * scgtClose()
 *     Close is responsible for letting the driver know that
 *     there is one less instance using it; nothing else
 */
static int scgtClose(struct inode *inode, struct file *filp)
{
    DPrintF("scgtClose()\n");

    return 0;
}


/*
 * scgtIoctl
 *     I/O Control performs configuration and I/O in the driver.
 *     If it accesses the hardware, it'll be called from here.
 *     Taking care of write and read ioctl's and passing everything else
 *     to scgtIoctl2.
 */
#ifdef USE_UNLOCKED_IOCTL
static long scgtIoctl(struct file* filp, unsigned int cmd, unsigned long arg)
#else
static int scgtIoctl(struct inode* inode, struct file* filp,
                     unsigned int cmd, unsigned long arg)
#endif
{
    scgtDevice *dev;
    scgtXfer xfer;
    scgtInterrupt intr;
    scgtGetIntrBuf getIntrBuf;
    int ret;

    DPrintF("scgtIoctl()\n");
    
    if ((dev = filp->private_data) == NULL)
    {
        return -ENXIO;
    }

    switch (cmd)
    {
        case SCGT_IOCTL_WRITE:     
            if (copy_from_user(&xfer, (void *) arg, sizeof(scgtXfer)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                if (xfer.pInterrupt)
                {
                    if (copy_from_user(&intr, UINT64_TO_PTR(void, xfer.pInterrupt), sizeof(scgtInterrupt)))
                        ret = SCGT_BAD_PARAMETER;
                    else
                        ret = scgtIoctlXfer(dev, &xfer, &intr, GTCORE_WRITE);
                }
                else
                {
                    ret = scgtIoctlXfer(dev, &xfer, NULL, GTCORE_WRITE);            
                }
                if (copy_to_user((void *) arg, &xfer, sizeof(scgtXfer)))
                    ret = SCGT_BAD_PARAMETER;
            }
            break;

        case SCGT_IOCTL_READ:
            if (copy_from_user(&xfer, (void *) arg, sizeof(scgtXfer)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                ret = scgtIoctlXfer(dev, &xfer, NULL, GTCORE_READ);
                if (copy_to_user((void *) arg, &xfer, sizeof(scgtXfer)))
                    ret = SCGT_BAD_PARAMETER;
            }
            break;
            
        case SCGT_IOCTL_GET_INTR:
            if (copy_from_user(&getIntrBuf, (void *) arg, sizeof(scgtGetIntrBuf)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                ret = scgtIoctlGetIntr(dev, &getIntrBuf);
                if (copy_to_user((void *) arg, &getIntrBuf, sizeof(scgtGetIntrBuf)))
                    ret = SCGT_BAD_PARAMETER;
            }
            break;

        default:
            ret = scgtIoctl2(dev, cmd, arg);
            break;
    }
   
    DPrintF("scgtIoctl() exit...\n");
   
    return ret;
}

static int scgtIoctl2(scgtDevice *dev, unsigned int cmd, unsigned long arg)
{
    int ret = SCGT_SUCCESS;
    
    /* we only use one of the following per ioctl call.. The union
       will save some stack. */
    union
    {
        scgtRegister reg;
        scgtMemMapInfo mmi;
        scgtDeviceInfo deviceInfo;
        scgtState state;
        scgtStats stats;
        scgtInterrupt intr;   /* debugging */
    } u;
    
    DPrintF("scgtIoctl2()\n");
    
    switch (cmd)
    {
        case SCGT_IOCTL_GET_DEVICE_INFO:
            if (copy_from_user(&u.deviceInfo, (void*) arg, sizeof(u.deviceInfo)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                ret = scgtIoctlGetDeviceInfo(dev, &u.deviceInfo);
                if (copy_to_user((void *) arg, &u.deviceInfo, sizeof(u.deviceInfo)))
                    ret = SCGT_BAD_PARAMETER;
            }
            break;
            
        case SCGT_IOCTL_GET_STATE:
            if (copy_from_user(&u.state, (void*) arg, sizeof(u.state)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                ret = gtcoreGetState(dev, u.state.stateID, &u.state.val);           
                if (copy_to_user((void *) arg, &u.state, sizeof(u.state)))
                    ret = SCGT_BAD_PARAMETER;
            }
            break;
        
        case SCGT_IOCTL_SET_STATE:
            if (copy_from_user(&u.state, (void*) arg, sizeof(u.state)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                ret = gtcoreSetState(dev, u.state.stateID, u.state.val);
            }
            break;  
        
        case SCGT_IOCTL_GET_STATS:
            if (copy_from_user(&u.stats, (void*) arg, sizeof(u.stats)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                ret = scgtIoctlGetStats(dev, &u.stats);
            }
            break;

        case SCGT_IOCTL_MEM_MAP_INFO:
            u.mmi.memPhysAddr = (uint64) dev->memPhysAddr;
            u.mmi.memSize = dev->memSize;
            if (copy_to_user((void*)arg, &u.mmi, sizeof(u.mmi)))
                ret = SCGT_BAD_PARAMETER;
            break;
            
        case SCGT_IOCTL_WRITE_CR:
            if (copy_from_user(&u.reg, (void*)arg, sizeof(u.reg)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                if (u.reg.offset < GTCORE_REGISTER_SIZE)
                {
                    scgtWriteCReg(dev, u.reg.offset, u.reg.val);   
                    ret = SCGT_SUCCESS;
                }
                else
                {
                    ret = SCGT_BAD_PARAMETER;
                }
            }
            break;
        
        case SCGT_IOCTL_READ_CR:
            if (copy_from_user(&u.reg, (void*)arg, sizeof(u.reg)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                if (u.reg.offset < GTCORE_REGISTER_SIZE)
                {
                    u.reg.val = scgtReadCReg(dev, u.reg.offset);   
                    ret = SCGT_SUCCESS;
                }
                
                if (copy_to_user((void*)arg, &u.reg, sizeof(u.reg)))
                {
                    ret = SCGT_BAD_PARAMETER;
                }
            }
            break;

        case SCGT_IOCTL_WRITE_NMR:
            if (copy_from_user(&u.reg, (void*)arg, sizeof(u.reg)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                if (u.reg.offset < GTCORE_NM_REGISTER_SIZE)
                {
                    scgtWriteNMReg(dev, u.reg.offset, u.reg.val);   
                    ret = SCGT_SUCCESS;
                }
                else
                {
                    ret = SCGT_BAD_PARAMETER;
                }
            }
            break;
        
        case SCGT_IOCTL_READ_NMR:
            if (copy_from_user(&u.reg, (void*)arg, sizeof(u.reg)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                if (u.reg.offset < GTCORE_NM_REGISTER_SIZE)
                {
                    u.reg.val = scgtReadNMReg(dev, u.reg.offset);   
                    ret = SCGT_SUCCESS;
                }
                
                if (copy_to_user((void*)arg, &u.reg, sizeof(u.reg)))
                {
                    ret = SCGT_BAD_PARAMETER;
                }
            }
            break;
            
        case SCGT_IOCTL_PUT_INTR:       /* used for debugging interrupt queue */
            if (copy_from_user(&u.intr, (void *) arg, sizeof(u.intr)))
            {
                ret = SCGT_BAD_PARAMETER;
            }
            else
            {
                gtcorePutIntr(dev, &u.intr);
                scgtGiveIntrSem(dev);
                ret = SCGT_SUCCESS;
            }
            break;
                        
        default:
            printk(KERN_WARNING "SCGT: unsupported IOCTL 0x%x\n", cmd);
            ret = SCGT_CALL_UNSUPPORTED;
            break;
    }

    DPrintF("scgtIoctl2() End\n");
       
    return ret;
}


/*
 * scgtInitGetIntrEntry()
 *     initialize GetInterrupt timer and semaphore
 *     this must be called before scgtIoctlGetIntr() is called.
 */
 
static void scgtInitGetIntrTimer(scgtDevice *dev)
{
    ksysSemSCreate(&dev->getIntrSem);
    
    ksysSpinLockCreate(&dev->getIntrWaitCountSpinLock);
    ksysSpinLockCreate(&dev->getIntrTimerSpinLock);
    
    dev->getIntrWaitCount = 0;
    dev->getIntrTimerStarted = 0;
    dev->getIntrTmrUseCnt = 0;
    
    dev->getIntrTimer = kmalloc(sizeof(struct timer_list), GFP_KERNEL);    
    init_timer(dev->getIntrTimer);

    dev->getIntrTimer->data = (unsigned long) dev;
    dev->getIntrTimer->function = scgtGetIntrTimerCallback;
}

static void scgtDestroyGetIntrTimer(scgtDevice *dev)
{
    getIntrTimerExit = 1;
    del_timer_sync(dev->getIntrTimer);
    
    kfree(dev->getIntrTimer);

    ksysSemSDestroy(&dev->getIntrSem);
    ksysSpinLockDestroy(&dev->getIntrWaitCountSpinLock);
    ksysSpinLockDestroy(&dev->getIntrTimerSpinLock);
}

/*
 * scgtGetIntrTimerStart()
 *     start the timer interrupt
 */
 
static void scgtGetIntrTimerStart(scgtDevice *dev)
{
    struct timer_list *timer;
    ksysSpinLockFlags  spinLockFlags;
    
    timer = dev->getIntrTimer;

    ksysSpinLockLock(&dev->getIntrTimerSpinLock, &spinLockFlags);
        
    if (!dev->getIntrTimerStarted)
    {
        dev->getIntrTimerStarted = 1;
        ksysSpinLockUnlock(&dev->getIntrTimerSpinLock, &spinLockFlags);
        timer->expires = jiffies + SCGT_GET_INTR_TIMER_JIFFIES;
        add_timer(timer);
    }
    else
    {
        ksysSpinLockUnlock(&dev->getIntrTimerSpinLock, &spinLockFlags);
    }
}

/*
 * scgtGetIntrTimerCallback()
 *     callback function for timer interrupt used in
 *     GetInterrupt() timeouting
 */
 
void scgtGetIntrTimerCallback(unsigned long devAddr)
{
    scgtDevice *dev = (scgtDevice *) devAddr;
    ksysSpinLockFlags spinLockFlags;

    scgtGiveIntrSem(dev);
    
    ksysSpinLockLock(&dev->getIntrTimerSpinLock, &spinLockFlags);
    if (dev->getIntrTmrUseCnt && (!getIntrTimerExit))
    {
        ksysSpinLockUnlock(&dev->getIntrTimerSpinLock, &spinLockFlags);
        dev->getIntrTimer->expires = jiffies + SCGT_GET_INTR_TIMER_JIFFIES;
        add_timer(dev->getIntrTimer);
    }
    else
    {
        dev->getIntrTimerStarted = 0;
        ksysSpinLockUnlock(&dev->getIntrTimerSpinLock, &spinLockFlags);
    }
    
    dev->stats[SCGT_STATS_GET_INTR_TIMER]++;
}


/*
 * scgtGiveIntrSem()
 *     returns non-zero if given
 *     else, returns zero.
 */
 
static int scgtGiveIntrSem(scgtDevice *dev)
{
    int count;
    ksysSpinLockFlags spinLockFlags;
    
    ksysSpinLockLock(&dev->getIntrWaitCountSpinLock, &spinLockFlags);
    count = dev->getIntrWaitCount;
    dev->getIntrWaitCount = 0;
    ksysSpinLockUnlock(&dev->getIntrWaitCountSpinLock, &spinLockFlags); 

    if (count > 0) /* somebody waiting */
    {
        for (; count > 0; count--)
            ksysSemSGive(&dev->getIntrSem);
    }

    return count;
}


/*
 * notgtcoreGetIntr()
 *     This code is the same as gtcoreGetIntr() except a spinlock has been added
 *     to adequately protect dev->getIntrWaitCount.  This change will be placed
 *     in the core when other OS's are updated.  (Not all of them need this change).
 *     NOTE: This fixes race condition between a thread preparing to wait and the ISR
 *           running with a network interrupt.  Previously the thread had a chance of not being
 *           counted in time for the ISR so the semaphore wasn't being given by the ISR.
 *
 *     get a buffer of interrupts from interrupt queue
 *     returns SCGT_SUCCESS if there were interrupt waiting (buffer has stuff in it)
 *     returns SCGT_MISSED_INTERRUPTS if there were interrupt missed (buffer has stuff in it)
 *     returns SCGT_TIMEOUT if there were no interrupts waiting
 *         note: This call does not include timeout functionality (You have to perform a wait
 *               after calling this function if it returns SCGT_TIMEOUT.  Then call this function 
 *               again if something comes in or on a certain interval if using a timer etc).
 */


#define GTCORE_SEQ_NUM_MASK   0xFFFFFFF    /* make sure this matches value in gtcore.c! */
                                           

uint32 notgtcoreGetIntr(scgtDevice *dev, scgtGetIntrBuf *gibuf, int wantToWait)
{
    scgtInterrupt *iBufPtr;
    uint32 intrQSeqNum;
    uint32 intrQHead;
    uint32 seqNum;
    uint32 n2Copy;
    uint32 startIndex;
    uint32 ret = SCGT_TIMEOUT;
    uint32 missed = 0;
    ksysSpinLockFlags spinLockFlags;


    iBufPtr = UINT64_TO_PTR(scgtInterrupt, gibuf->intrBuf);
    seqNum = gibuf->seqNum;
    gibuf->numInterruptsRet = 0;

    while (1)
    {
        ksysSpinLockLock(&dev->getIntrWaitCountSpinLock, &spinLockFlags);
	
        /* save local copy of queue head */
        intrQSeqNum = dev->intrQData.seqNum;  /* critical operation (race cond.) */
        
        if (intrQSeqNum == seqNum)
        {
            if (wantToWait)
            {
                dev->getIntrWaitCount++;  /* no interrupts this time... and we will be waiting */
            }
            ksysSpinLockUnlock(&dev->getIntrWaitCountSpinLock, &spinLockFlags);

            ret = SCGT_TIMEOUT;
	    
            break; /* timeout */
        }

        /* no waiting for us */
        ksysSpinLockUnlock(&dev->getIntrWaitCountSpinLock, &spinLockFlags);	
	
        if (intrQSeqNum > seqNum)
        {
            n2Copy = intrQSeqNum - seqNum;

gtcoreGetIntr_docopy:                
            /* check for missed interrupts (-1 is so we don't touch the one being updated) */
            if (n2Copy > (GTCORE_INTR_Q_SIZE - 1))
            {
                /* missed interrupts */
                /* set sequence number to oldest and call gtcoreGetIntr() again to get them */

                /* & GTCORE_SEQ_NUM_MASK is used here for rollover case */                
                seqNum = (intrQSeqNum - (GTCORE_INTR_Q_SIZE - 1)) & GTCORE_SEQ_NUM_MASK;
                missed++;
                
                if (missed >= 5)   /* avoid looping forever */
                    break; 
            }
            else
            {
                /* everything is kewl... do the copy */               
                if (n2Copy > gibuf->bufSize)
                    n2Copy = gibuf->bufSize;
                gibuf->numInterruptsRet = n2Copy;
    
                startIndex = seqNum % GTCORE_INTR_Q_SIZE;
                intrQHead = intrQSeqNum % GTCORE_INTR_Q_SIZE;

                seqNum += n2Copy;
    
                if (startIndex < intrQHead)
                {
                    ksysCopyToUser(iBufPtr, 
                                   &dev->intrQData.intrQ[startIndex], 
                                   sizeof(scgtInterrupt) * n2Copy);
                }
                else
                {
                    uint32 nFirstCopy = GTCORE_INTR_Q_SIZE - startIndex;
                    if (nFirstCopy > n2Copy)
                        nFirstCopy = n2Copy;
    
                    ksysCopyToUser(iBufPtr, 
                                   &dev->intrQData.intrQ[startIndex], 
                                   sizeof(scgtInterrupt) * nFirstCopy);
    
                    n2Copy -= nFirstCopy;
                    if (n2Copy)
                    {
                        ksysCopyToUser(iBufPtr + nFirstCopy,
                                       &dev->intrQData.intrQ[0], 
                                       sizeof(scgtInterrupt) * n2Copy);                
                    }
                }
    
                ret = SCGT_SUCCESS;
                break;
            }
        }
        else /* if (intrQSeqNum < seqNum) */
        {
            if (seqNum == (uint32) -1)
            {
                /* this is the apps first time in... init seqNum
                   to current and check to see if anything came in
                   by calling gtcoreGetIntr() again. */
                seqNum = intrQSeqNum;
            }
            else if (seqNum > GTCORE_SEQ_NUM_MASK)
            {
                return SCGT_BAD_PARAMETER;
            }
            else
            {
                /* assume intrQSeqNum overflowed 
                   (rather than invalid seqNum passed in) */
                
                n2Copy = (GTCORE_SEQ_NUM_MASK + 1) - seqNum + intrQSeqNum;
                goto gtcoreGetIntr_docopy;
            }
        }
    }

    gibuf->seqNum = seqNum & GTCORE_SEQ_NUM_MASK;
    return (missed ? SCGT_MISSED_INTERRUPTS : ret);
}


/*
 * scgtIoctlGetIntr()
 */

static int scgtIoctlGetIntr(scgtDevice *dev, scgtGetIntrBuf *gibuf)
{
    uint32 ret;
    uint32 waitTime = 0;    /* how long I've waited */
    int wantToWait;
    ksysSpinLockFlags spinLockFlags;

    ksysSpinLockLock(&dev->getIntrTimerSpinLock, &spinLockFlags);    
    dev->getIntrTmrUseCnt++;
    ksysSpinLockUnlock(&dev->getIntrTimerSpinLock, &spinLockFlags);
    
    scgtGetIntrTimerStart(dev);  
        
    while (1)
    {
        wantToWait = (waitTime >= gibuf->timeout || getIntrTimerExit)? 0 : 1;
        if ((ret = notgtcoreGetIntr(dev, gibuf, wantToWait)) != SCGT_TIMEOUT)
        {
            break;  /* we got something */
        }
        
        /* if we got here.. we have to wait unless our time is already up (wantToWait)*/
        
        if (wantToWait == 0)
        {
            gibuf->numInterruptsRet = 0;
            break;
        }


        ksysSemSTake(&dev->getIntrSem);
        
        waitTime += SCGT_GET_INTR_TIMER_MILLISEC;	
    }

    ksysSpinLockLock(&dev->getIntrTimerSpinLock, &spinLockFlags);    
    dev->getIntrTmrUseCnt--;
    ksysSpinLockUnlock(&dev->getIntrTimerSpinLock, &spinLockFlags); 
    
    return ret;
}


/*
 * scgtIoctlGetDeviceInfo()
 *     call gtcoreGetDeviceInfo() and fill in the remaining members of struct.
 */

static int scgtIoctlGetDeviceInfo(scgtDevice *dev, scgtDeviceInfo *devInfo)
{
    int ret;

    ret = gtcoreGetDeviceInfo(dev, devInfo);
    
    strncpy(devInfo->driverRevisionStr, driverRevStr, 128);
    strncpy(devInfo->boardLocationStr, dev->boardLocationStr, 128);

    return ret;
}


/*
 * scgtIoctlGetStats()
 *     driver statistics.
 */

static int scgtIoctlGetStats(scgtDevice *dev, scgtStats *stats)
{
    /* update any stats that need to be updated */
    dev->stats[SCGT_STATS_GET_INTR_WAIT_CNT] = dev->getIntrWaitCount;

    /* do the copy */
    return gtcoreGetStats(dev, stats);
}


/*********************************************************************/
/*********************** MMAP entry code *****************************/
/*********************************************************************/

/*
 * scgtMMap()
 *     mmap entry point
 */

static int scgtMMap(struct file *file, struct vm_area_struct *vma)
{
    scgtDevice *dev;
    unsigned long offset;
    unsigned long physAddr;
    unsigned long vsize;
    unsigned long psize;
    
    if ((dev = (scgtDevice *) file->private_data) == NULL)
        return -EINVAL;   /* private_data should be set in open */
    
    offset = vma->vm_pgoff << PAGE_SHIFT;
    physAddr = dev->memPhysAddr + offset;
    vsize = vma->vm_end - vma->vm_start;
    psize = dev->memSize - offset;

    if (offset > dev->memSize || vsize > psize)
        return -EINVAL;   /* trying to map too much! */
    
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);  
    vma->vm_flags |= VM_RESERVED | VM_IO;  

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10))
    /* remap_pfn_range replaces remap_page_range in 2.6.10 */
    if (remap_pfn_range(vma, vma->vm_start, physAddr >> PAGE_SHIFT, vsize, vma->vm_page_prot))
        return -EAGAIN;
#else
    if (remap_page_range(vma, vma->vm_start, physAddr, vsize, vma->vm_page_prot))
        return -EAGAIN;
#endif

    return 0;
} 


/***********************************************************************/
/**************************** DMA CODE *********************************/
/***********************************************************************/

/*
 * scgtInitDMATools()
 *     Allocate and initialize DMA tools for device.
 *     returns 0 on success, non-zero on failure.
 */
 
static int scgtInitDMATools(scgtDevice *dev)
{
    scgtDMATools *tools;
    int i;
    
    tools = &dev->writeTools;
    
    for (i = 0; i < 2; i++)
    {
        tools->pages_1 = kmalloc(sizeof(struct page *) * 
                                 (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1), GFP_KERNEL);
                                 
        tools->pages_2 = kmalloc(sizeof(struct page *) * 
                                 (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1), GFP_KERNEL);
        
        if (tools->pages_1 == NULL || tools->pages_2 == NULL)
            return -1;
        
        tools->scatterList_1 = kmalloc(sizeof(struct scatterlist) * 
                                       (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1), GFP_KERNEL);
        tools->scatterList_2 = kmalloc(sizeof(struct scatterlist) * 
                                       (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1), GFP_KERNEL);
    
        if (tools->scatterList_1 == NULL || 
            tools->scatterList_2 == NULL)
                return -2;
        
        memset(tools->pages_1, 0, sizeof(struct page *) * (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1));
        memset(tools->pages_2, 0, sizeof(struct page *) * (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1));

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))        
        /* we must clear scatterList so unused members are 0 */
        memset(tools->scatterList_1, 0, sizeof(struct scatterlist) * (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1));
        memset(tools->scatterList_2, 0, sizeof(struct scatterlist) * (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1));
#else
        sg_init_table(tools->scatterList_1, (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1));
        sg_init_table(tools->scatterList_2, (SCGT_MAX_CHUNK_SIZE/PAGE_SIZE+1));
#endif
        
        ksysSemSCreate(&tools->entrySem_1);
        ksysSemSCreate(&tools->entrySem_2);
        ksysSemSGive(&tools->entrySem_1);    /* semaphores start as taken */
        ksysSemSGive(&tools->entrySem_2);
        
        tools->pending = 0;        
        tools = &dev->readTools;
    }

    return 0;
}

/*
 * scgtFreeDMATools()
 *     Deallocate DMA tools for device.
 */
 
static void scgtFreeDMATools(scgtDevice *dev)
{
    kfree(dev->writeTools.scatterList_1);
    kfree(dev->writeTools.scatterList_2);
    kfree(dev->readTools.scatterList_1);
    kfree(dev->readTools.scatterList_2);
    
    kfree(dev->writeTools.pages_1);
    kfree(dev->writeTools.pages_2);
    kfree(dev->readTools.pages_1);
    kfree(dev->readTools.pages_2);
    
    ksysSemSDestroy(&dev->writeTools.entrySem_1);
    ksysSemSDestroy(&dev->writeTools.entrySem_2);
    ksysSemSDestroy(&dev->readTools.entrySem_1);
    ksysSemSDestroy(&dev->readTools.entrySem_2);
}


/*
 * scgtIoctlXfer()
 *     process write or read transfer
 */
 
static uint32 scgtIoctlXfer(scgtDevice *dev, 
                            scgtXfer *xfer,
                            scgtInterrupt *intr,
                            uint8 direction)
{
    uint32 gtMemoryOffset;
    uint32 bytesToTransfer;
    uint32 bytesTransferred;
    uint32 chunkSize;
    uint32 flags;
    uint8  lastTransfer;
    uint32 ret = SCGT_SUCCESS;
    uint8  *pBuf;
    scgtDMATools *dmaTools;
    ksysSemS *semToGive;
    
    DPrintF(KERN_WARNING "scgtIoctlXfer begin\n");
    
    pBuf = UINT64_TO_PTR(uint8, xfer->pDataBuffer);
    bytesToTransfer = xfer->bytesToTransfer;
    gtMemoryOffset = xfer->gtMemoryOffset;
    
    if (bytesToTransfer == 0 || pBuf == NULL)
        return gtcoreSendIntr(dev, intr);
    
    flags = xfer->flags;
    dmaTools = (direction == GTCORE_WRITE)? &dev->writeTools : &dev->readTools;

    ksysSemSTake(&dmaTools->entrySem_1);
    semToGive = &dmaTools->entrySem_1;
    
    while (bytesToTransfer > 0 && ret == SCGT_SUCCESS)
    {
        /* calculate chunk size */
        chunkSize = (bytesToTransfer >= SCGT_MAX_CHUNK_SIZE)? SCGT_MAX_CHUNK_SIZE : bytesToTransfer;
        lastTransfer = (chunkSize == bytesToTransfer)? 1 : 0;
    
        ret = scgtXferChunk(dev, gtMemoryOffset, pBuf, chunkSize, lastTransfer,
                            flags, intr, direction, dmaTools, &bytesTransferred, 
                            &semToGive);
                            
        bytesToTransfer -= bytesTransferred;
        pBuf += bytesTransferred;
        gtMemoryOffset += bytesTransferred;
    }
    
    ksysSemSGive(semToGive);
    
    xfer->bytesTransferred = xfer->bytesToTransfer - bytesToTransfer;
    
    DPrintF(KERN_WARNING "scgtIoctlXfer end\n");
    
    return ret;
}

/*
 * scgtXferChunk()
 *     Reads or writes chunkSize bytes of the specified buffer.  
 *     bytesTransferred is filled with the number of bytes
 *     transferred.  bytesTransferred should always be == to chunkSize unless
 *     an error is returned.
 */

static uint32 scgtXferChunk(scgtDevice *dev,
                            uint32 gtMemoryOffset, 
                            uint8  *pBuf, 
                            uint32 chunkSize,
                            uint8 lastTransfer,
                            uint32 flags,
                            scgtInterrupt *intr,
                            uint8  direction,
                            scgtDMATools *tools,
                            uint32 *bytesTransferred,
                            ksysSemS **semToGive)
{
    uint32 ret;
    uint32 numScatterBuffers;
    gtcoreExch *exch;
    int numPages = 0, i;
    
    struct page **pages = tools->pages_1;
    struct scatterlist *scatterList = tools->scatterList_1;
    
    exch = gtcoreGetExchange(dev, direction);
    exch->compSem.given = 0;
    
    DPrintF("scgtXferChunk() begin\n");

    exch->bytesToTransfer = chunkSize;
    exch->gtMemoryOffset = gtMemoryOffset;
    exch->flags = flags;

    if (lastTransfer)     /* setup intr for last transfer */
        exch->intr = intr;
    else
        exch->intr = NULL;

    if (!(flags & SCGT_RW_DMA_PHYS_ADDR))  /* not physical address */
    {
        /* map the buffer */
        
        numPages = ((((unsigned long)pBuf) & ~PAGE_MASK) + chunkSize + ~PAGE_MASK) >> PAGE_SHIFT;
        
        down_read(&current->mm->mmap_sem);
        if (get_user_pages(current,
                           current->mm,
                           (unsigned long) pBuf,
                           numPages,
                           direction == GTCORE_READ? 1 : 0,
                           0,
                           pages,
                           NULL) != numPages)
        {   
            up_read(&current->mm->mmap_sem);
            printk(KERN_WARNING "SCGT: unable to get_user_pages!!!\n");
            *bytesTransferred = 0;
            return SCGT_INSUFFICIENT_RESOURCES;
        }
        up_read(&current->mm->mmap_sem);

        
        /* map pages to scatter list */
        numScatterBuffers = scgtMapToScatterList(dev, (unsigned long) pBuf, chunkSize, 
                                                 pages, numPages, scatterList, direction);

        /* build the hardware chain list */
        scgtBuildChainList(scatterList, numScatterBuffers, exch);
    }
    else
    {
        /* physical address supplied */
        uint32 *chainEntry = (uint32 *) exch->sgList[0];
        
        chainEntry[GTCORE_CE_TNS_CSR] = (chunkSize >> 2) | GTCORE_CE_LST;
        chainEntry[GTCORE_CE_BUF_ADD32] = (uint32) ((uintpsize) pBuf);
        ksysCacheFlush(NULL, chainEntry, GTCORE_CE_SIZE);        
    }
   
    /* do transfer */
    gtcoreTransfer(dev, exch, direction);
    
    if (lastTransfer)
    {
        ksysSemSTake(&tools->entrySem_2);
        
        tools->pending = 1;
        
        /* swap the tool resources so the next thread can begin */
        
        tools->pages_1 = tools->pages_2;
        tools->pages_2 = pages;
        
        tools->scatterList_1 = tools->scatterList_2;
        tools->scatterList_2 = scatterList;
        
        /* let the next thread enter */
        ksysSemSGive(&tools->entrySem_1);
        *semToGive = &tools->entrySem_2;  /* swap the exit semaphore to sem 2 */
    }
    else if (tools->pending)
    {
        ksysSemSTake(&tools->entrySem_2);
        ksysSemSGive(&tools->entrySem_2);
    }

    /* wait until its done. */
    if (ksysSemBTakeWithTimeout(&exch->compSem, SCGT_TIMEOUT_TIME))
    {
        /* hardware did not complete transfer! */
        ret = SCGT_TIMEOUT;  
        gtcoreCancelTransfer(dev, exch, direction);
    }
    else
    {
        ret = exch->status;
    }

    *bytesTransferred = exch->bytesTransferred;
    
    if ((*bytesTransferred != chunkSize) && (ret == SCGT_SUCCESS))
        ret = SCGT_HARDWARE_ERROR;
    
    tools->pending = 0;
    
    if (!(flags & SCGT_RW_DMA_PHYS_ADDR))  /* not physical address */
    {    
        /* unmap the scatter list */
        pci_unmap_sg(dev->pciDev, scatterList, numPages, (direction == GTCORE_WRITE)? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE);

        /* unmap the buffer */
        for (i = 0; i < numPages; i++)
        {
            if (direction == GTCORE_READ)
                SetPageDirty(pages[i]);
            page_cache_release(pages[i]);
        }
    }

    DPrintF("scgtXferChunk() end\n");
    
    return ret;
}
                         

/*
 * scgtMapToScatterList()
 *     map user buffer to scatter list
 *     returns the number of DMA buffers to transfer 
 *     May be less than numPages do to optimizations on some platforms.
 */

static int scgtMapToScatterList(scgtDevice *dev,
                                unsigned long bufAddr,
                                unsigned long numBytes,
                                struct page **pages,
                                int numPages,
                                struct scatterlist *scatList, 
                                uint8 direction)
{
    int i;
    uint32 offset = bufAddr & ~PAGE_MASK;  /* buffer may have non-zero offset only on first page */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))

    scatList[0].page = pages[0];
    scatList[0].offset = offset;   

    if (numPages == 1)
    {
        scatList[0].length = numBytes;
    }
    else
    {
        scatList[0].length = PAGE_SIZE - offset;
    
        for (i = 1; i < numPages; i++)
        {
            scatList[i].page = pages[i];
            scatList[i].length = PAGE_SIZE;
        }
    
        /* fix the length of the last buffer */
        scatList[i-1].length = numBytes - (PAGE_SIZE - offset) - ((numPages - 2) * PAGE_SIZE);    
    }

#else
    /* kernels 2.6.23 and later, use sg_set_page */
    /* sg_init_table(scatList, numPages);  only done at init 
       mark end is not being done (I know how many there are) */
    if (numPages == 1)
    {
        sg_set_page(&scatList[0], pages[0], numBytes, offset);
    }
    else
    {
        /* first page */
        sg_set_page(&scatList[0], pages[0], PAGE_SIZE - offset, offset); 
        
        /* middle pages */
        for (i = 1; i < numPages - 1; i++)
        {
            sg_set_page(&scatList[i], pages[i], PAGE_SIZE, 0);
        }
        
        /* last page */
        sg_set_page(&scatList[i], pages[i], 
                    numBytes - (PAGE_SIZE - offset) - ((numPages - 2) * PAGE_SIZE),
                    0);
    }
#endif
    
    return pci_map_sg(dev->pciDev, scatList, numPages, 
                     (direction == GTCORE_WRITE)? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE);
}

/*
 * scgtBuildChainList()
 *     Convert a scatterlist to a board-readable scatter-gather list
 */

static int scgtBuildChainList(struct scatterlist *scatList, int ntries, 
                              gtcoreExch *exch)
{
    int i;
    uint32 *chainEntry = (uint32 *) exch->sgList[0];
    uint64 dmaAddr;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24))
    
    for (i = 0; i < ntries; i++)
    {
        dmaAddr = (uint64) sg_dma_address(&scatList[i]);
        
        chainEntry[GTCORE_CE_TNS_CSR] = sg_dma_len(&scatList[i]) >> 2;
        chainEntry[GTCORE_CE_BUF_ADD32] = (uint32) dmaAddr;
        chainEntry[GTCORE_CE_BUF_ADD64] = (uint32) (dmaAddr >> 32);
        chainEntry += (GTCORE_CE_SIZE / 4);
    }

#else

    /* for kernels 2.6.23 and later use for_each_sg() and sg_next() */
    struct scatterlist *curList = scatList;
    for_each_sg(scatList, curList, ntries, i)
    {
        dmaAddr = (uint64) sg_dma_address(curList);

        chainEntry[GTCORE_CE_TNS_CSR] = sg_dma_len(curList) >> 2;
        chainEntry[GTCORE_CE_BUF_ADD32] = (uint32) dmaAddr;
        chainEntry[GTCORE_CE_BUF_ADD64] = (uint32) (dmaAddr >> 32);
        chainEntry += (GTCORE_CE_SIZE / 4);
    }

#endif
    
    /*
     * set the last chain entry bit
     */
    chainEntry -= (GTCORE_CE_SIZE / 4);
    chainEntry[GTCORE_CE_TNS_CSR] |= GTCORE_CE_LST;
    
    ksysCacheFlush(NULL, (void *) exch->sgList[0], ntries * GTCORE_CE_SIZE);

    return 0;
}



/*******************************************************************/
/************************ INTERRUPT SERVICE ************************/
/*******************************************************************/
/*
 * scgtISR()
 *     Interrupt service routine
 */
 
irqreturn_t scgtISR(int irq, void *dev_ptr)
{
    scgtDevice *dev = (scgtDevice *) dev_ptr;
    uint32 intRet;

    intRet = gtcoreHandleInterrupt(dev); 

    if (intRet & GTCORE_ISR_DMA)
    {
        gtcoreCompleteDMA(dev);
    }
    
    if (intRet & GTCORE_ISR_QUEUED_INTR)
    {
        scgtGiveIntrSem(dev);
    }
    
    return IRQ_RETVAL(intRet);
}
