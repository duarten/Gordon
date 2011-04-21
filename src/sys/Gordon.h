/*++

Module Name:
    Gordon.h

Abstract:
    Contains functions to read and write the BIOS flash ROM on a ICH8 chipset.

Environment:
    Kernel mode

--*/

#pragma once

// nameless struct/union
#pragma warning(disable:4200) 
#pragma warning(disable:4201) 
// bit field types other than int
#pragma warning(disable:4214) 

#include <ntddk.h>

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)

#include <wdf.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <wdmsec.h> // for SDDLs
#include "Public.h"
#include "Trace.h"

#define PHYS_DEVICE_NAME	L"\\Device\\Gordon"
#define SYMBOLIC_NAME		L"\\DosDevices\\Gordon"

#define LPC_DEVICE			L"\\Device\\NTPNP_PCI0021"

#define BIOS_SIZE			0x200000
#define BIOS_END			0xFFFFFFFF
#define BIOS_START			(BIOS_END - BIOS_SIZE)

#define RCBA_OFFSET			0xf0
#define RCRB_SIZE			0x4000

#define BIOS_CNTL_OFFSET	0xdc
#define FWHSEL1_OFFSET		0xd0

typedef struct _FWH_CONFIG {
    ULONG FwhSel1;
    USHORT FwhSel2;
    USHORT Reserved;
    USHORT FwhDecEn;
} FWH_CONFIG;

#define TIMEOUT             (100 * 60)

#define SPIBAR_OFFSET		0x3800

#define WRITE_ENABLE        0x6
#define WRITE_ENABLE_INDEX  6

#define SECTOR_ERASE        0x20
#define SECTOR_ERASE_INDEX  2

#define FADDR_OFFSET        0x08

#define SSFS_SSFC_OFFSET    0x90
#define SSFS_SCIP		    0x00000001
#define SSFS_CDS		    0x00000004
#define SSFS_FCERR		    0x00000008
#define SSFS_AEL		    0x00000010
#define SSFS_RESERVED_MASK	0x000000e2
#define SSFC_SCGO		    0x00000200
#define SSFC_ACS		    0x00000400
#define SSFC_SPOP		    0x00000800
#define SSFC_COP		    0x00001000
#define SSFC_DBC		    0x00010000
#define SSFC_DS			    0x00400000
#define SSFC_SME		    0x00800000
#define SSFC_SCF		    0x01000000
#define SSFC_SCF_20MHZ      0x00000000
#define SSFC_SCF_33MHZ      0x01000000
#define SSFC_RESERVED_MASK	0xf8008100

#define HSFS_OFFSET			0x04
#define PREOP_OFFSET		0x94
#define OPTYPE_OFFSET		0x96
#define OPMENU_L_OFFSET		0x98
#define OPMENU_H_OFFSET		0x9c
#define BBAR_OFFSET			0xa0
#define FDOC_OFFSET			0xb0

#define FRAP_BMWAG(x)		((x >> 24) & 0xff)
#define FRAP_BMRAG(x)		((x >> 16) & 0xff)
#define FRAP_BRWA(x)		((x >>  8) & 0xff)
#define FRAP_BRRA(x)		((x >>  0) & 0xff)

#define FREG0_OFFSET		0x54
#define FREG_BASE(x)		((x >>  0) & 0x1fff)
#define FREG_LIMIT(x)		((x >> 16) & 0x1fff)

#define PR0_OFFSET			0x74
#define PR1_OFFSET			0x78
#define PR2_OFFSET			0x7c
#define PR3_OFFSET			0x80
#define PR4_OFFSET			0x84
    
//
// Driver function prototypes
//

DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_UNLOAD EvtDriverUnload;
EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDriverContextCleanup;

EVT_WDF_DRIVER_DEVICE_ADD DriverDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtDeviceCleanupCallback;

EVT_WDF_IO_QUEUE_IO_READ EvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE EvtIoWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;

//
// Device context area
//

typedef struct _DEVICE_CONTEXT {
    WDFIOTARGET Lpc;
    PUCHAR Rcrb;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext);

//
// Hardware related functions.
//

//
// Show information about the Firmware Hub.
//

NTSTATUS
ShowFwhRegisters ( 
    __in WDFIOTARGET Lpc
    );

//
// Show some RCRB configuration registers.
//

VOID
ShowRcrbRegisters (
    __in PUCHAR Rcrb
    );

//
// Show SPI related information, including the opcode configuration, 
// according to which the erase/write code must be written.
//

NTSTATUS
ShowSpiRegisters ( 
    __in PUCHAR Rcrb
    );