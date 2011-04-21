#include <Gordon.h>

#if defined(EVENT_TRACING)
#include "IoCtl.tmh"
#endif

NTSTATUS
EraseFirstSector (
    __in PUCHAR SpiBar
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, EvtIoDeviceControl)
#pragma alloc_text (PAGE, EraseFirstSector)
#endif

typedef struct _SPI_COMMAND {
    UCHAR Opcode;
    ULONG Offset;
} SPI_COMMAND, *PSPI_COMMAND;

FORCEINLINE
NTSTATUS
ExecuteOpcode (
    __in PUCHAR SpiBar,
    __in UCHAR OpcodeIndex,
    __in ULONG Offset
    )
{
    NTSTATUS Status;
    ULONG SwSeqRegs;
    LONG Timeout;

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_HW_ACCESS, "--> ExecuteOpcode\n");

    Timeout = TIMEOUT;

    TraceEvents(TRACE_LEVEL_ERROR, DBG_HW_ACCESS, "SSFS_SSFC: 0x%08x.\n", READ_REGISTER_ULONG((PULONG) (SpiBar + SSFS_SSFC_OFFSET)));

    do {
        SwSeqRegs = READ_REGISTER_ULONG((PULONG) (SpiBar + SSFS_SSFC_OFFSET));
        
        if ((SwSeqRegs & SSFS_SCIP) == 0) {
            break;
        }
        
        KeStallExecutionProcessor(10);
    } while (--Timeout > 0);

    if (Timeout == 0) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_HW_ACCESS, "SCIP was not cleared.\n");
        Status = STATUS_DEVICE_BUSY;
        goto End;
    }

    //
    // Program the offset, even if it is not needed (to simplify logic).
    // SPI addresses are 24 bits long.
    //
    
    WRITE_REGISTER_ULONG((PULONG) (SpiBar + FADDR_OFFSET), (Offset & 0x00FFFFFF));

    //
    // Clear error status registers while preserving reserved bits.
    //

    SwSeqRegs &= SSFS_RESERVED_MASK | SSFC_RESERVED_MASK;
    SwSeqRegs |= SSFS_CDS | SSFS_FCERR;
    WRITE_REGISTER_ULONG((PULONG) (SpiBar + SSFS_SSFC_OFFSET), SwSeqRegs);

    //
    // Set the opcode index, relative to the OPMENU register.
    //

    SwSeqRegs |= ((ULONG) (OpcodeIndex & 0x07)) << (8 + 4);

    //
    // Start.
    //

    SwSeqRegs |= SSFC_SCGO;
    WRITE_REGISTER_ULONG((PULONG) (SpiBar + SSFS_SSFC_OFFSET), SwSeqRegs);

    //
    // Wait for any of the Cycle Done Status and Flash Cycle Error registers.
    //

    Timeout = TIMEOUT;

    TraceEvents(TRACE_LEVEL_ERROR, DBG_HW_ACCESS, "SSFS_SSFC: 0x%08x.\n", READ_REGISTER_ULONG((PULONG) (SpiBar + SSFS_SSFC_OFFSET)));
    
    while ((READ_REGISTER_ULONG((PULONG) (SpiBar + SSFS_SSFC_OFFSET)) & (SSFS_CDS | SSFS_FCERR)) == 0
           && --Timeout > 0) {
        KeStallExecutionProcessor(10);
    }

    if (Timeout == 0) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_HW_ACCESS, "Timeout while waiting for a cycle to complete.\n");
        Status = STATUS_TIMEOUT;
        goto End;
    }

    SwSeqRegs = READ_REGISTER_ULONG((PULONG) (SpiBar + SSFS_SSFC_OFFSET));

    if (SwSeqRegs & SSFS_FCERR) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_HW_ACCESS, "Transaction error.\n");
        SwSeqRegs &= SSFS_RESERVED_MASK | SSFC_RESERVED_MASK;
        WRITE_REGISTER_ULONG((PULONG) (SpiBar + SSFS_SSFC_OFFSET), SwSeqRegs | SSFS_FCERR);
        Status = STATUS_DEVICE_DATA_ERROR;
        goto End;
    }

End:
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_HW_ACCESS, "<-- ExecuteOpcode\n");
    return Status;
}

FORCEINLINE
NTSTATUS
EraseFirstSector (
    __in PUCHAR SpiBar
    )
{
    WDFIOTARGET Lpc;
    PUCHAR Rcrb;
    NTSTATUS Status;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "--> EraseFirstSector\n");
    
    Status = ExecuteOpcode(SpiBar, WRITE_ENABLE_INDEX, 0);
    
    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_HW_ACCESS, "ExecuteOpcode failed with status %!STATUS!\n", Status);
        goto End;
    }

    Status = ExecuteOpcode(SpiBar, SECTOR_ERASE_INDEX, 0);
    
    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_HW_ACCESS, "ExecuteOpcode failed with status %!STATUS!\n", Status);
        goto End;
    }

End:
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "<-- EraseFirstSector\n");
    return Status;
}

VOID
EvtIoDeviceControl(
    __in WDFQUEUE IoQueue,
    __in WDFREQUEST Request,
    __in size_t OutBufferLength,
    __in size_t InBufferLength,
    __in ULONG IoControlCode
    )
{
    PDEVICE_CONTEXT DeviceContext;
    ULONG_PTR Information;
    NTSTATUS Status;
    
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "--> EvtIoDeviceControl(%x)\n", IoControlCode);

    //
    // Get the pointer to the device's context area.
    // We are using the queue handle to get the device handle.
    //
    
    DeviceContext = GetDeviceContext(WdfIoQueueGetDevice(IoQueue));
    Information = 0;
    
    switch(IoControlCode) {
        case IOCTL_SHOW_DATA:
            //Status = EraseFirstSector(WdfIoQueueGetDevice(IoQueue));
            //ShowFwhRegisters(DeviceContext->Lpc);
            //ShowRcrbRegisters(DeviceContext->Rcrb);
            //ShowSpiRegisters(DeviceContext->Rcrb + SPIBAR_OFFSET);
            Status = STATUS_SUCCESS;
            break;
        
        default:
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
    }
    
    //
    // Complete the I/O request
    //
    
    WdfRequestCompleteWithInformation(Request, Status, Information);
    
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "<-- EvtIoDeviceControl(%x)\n", IoControlCode);
}