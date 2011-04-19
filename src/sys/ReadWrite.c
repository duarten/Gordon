#include <Gordon.h>

#if defined(EVENT_TRACING)
#include "ReadWrite.tmh"
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, EvtIoRead)
#pragma alloc_text (PAGE, EvtIoWrite)
#endif

VOID 
EvtIoRead (
    __in WDFQUEUE IoQueue, 
    __in WDFREQUEST Request, 
    __in size_t Length
    )
{    
    PUCHAR BiosStart;
    PHYSICAL_ADDRESS BiosStartPhys;
    PDEVICE_CONTEXT DeviceContext;
    ULONG_PTR Information;
    PUCHAR OutputBuffer;
    NTSTATUS Status;
    
    PAGED_CODE();
    
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_READ, "--> EvtIoRead(%u)\n", Length);

    DeviceContext = GetDeviceContext(WdfIoQueueGetDevice(IoQueue));

    if (Length < BIOS_SIZE) {
        Status = STATUS_BUFFER_TOO_SMALL;
        Information = 0;
        TraceEvents(TRACE_LEVEL_ERROR, DBG_READ, "The buffer is too small.\n");
        goto End;
    }

    Status = WdfRequestRetrieveOutputBuffer(Request, Length, &OutputBuffer, NULL);
    
    if (!NT_SUCCESS(Status)) {
        Information = 0;
        TraceEvents(TRACE_LEVEL_ERROR, DBG_READ, "WdfRequestRetrieveOutputBuffer failed with status %!STATUS!\n", Status);
        goto End;
    }
   
    BiosStartPhys.HighPart = 0;
    BiosStartPhys.LowPart = BIOS_START;
    BiosStart = (PUCHAR) MmMapIoSpace(BiosStartPhys, BIOS_SIZE, MmNonCached);
    RtlCopyMemory(OutputBuffer, BiosStart, BIOS_SIZE);
    MmUnmapIoSpace(BiosStart, BIOS_SIZE);

    Information = BIOS_SIZE;

End:
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_READ, "<-- EvtIoRead(%d)\n", (ULONG) Information);
    WdfRequestCompleteWithInformation(Request, Status, Information);
}

VOID EvtIoWrite(
  __in  WDFQUEUE Queue,
  __in  WDFREQUEST Request,
  __in  size_t Length
    )
{
    ULONG_PTR Information;
    NTSTATUS Status;

    PAGED_CODE();
    
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_READ, "--> DBG_WRITE(%u)\n", Length);

    Information = Length;
    Status = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_READ, "<-- DBG_WRITE(%d)\n", Length);
    WdfRequestCompleteWithInformation(Request, Status, Information);
}