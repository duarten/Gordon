#include <Gordon.h>

#if defined(EVENT_TRACING)
#include "Device.tmh"
#endif

static
NTSTATUS
InitializeContext (
    __in WDFDEVICE Device
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, DriverDeviceAdd)
#pragma alloc_text(PAGE, InitializeContext)
#pragma alloc_text(PAGE, EvtDeviceCleanupCallback)
#endif

NTSTATUS
InitializeContext (
    __in WDFDEVICE Device
    )
{
    PDEVICE_CONTEXT DeviceContext;
    IO_STACK_LOCATION IoStack;
    WDF_IO_TARGET_OPEN_PARAMS OpenParams;
    WDF_REQUEST_SEND_OPTIONS Options;
    PHYSICAL_ADDRESS RcrbPhys;
    WDFREQUEST Request;
    NTSTATUS Status;
    DECLARE_CONST_UNICODE_STRING(SymbolicLinkName, LPC_DEVICE);

    PAGED_CODE();
    
    DeviceContext = GetDeviceContext(Device);

    //
    // Define an IoTarget for the LPC interface device.
    //

    Status = WdfIoTargetCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &DeviceContext->Lpc);
   
    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfIoTargetCreate failed with status %!STATUS!\n", Status);
        goto End;
    }
   
    WDF_IO_TARGET_OPEN_PARAMS_INIT_CREATE_BY_NAME(&OpenParams, &SymbolicLinkName, STANDARD_RIGHTS_ALL);

    Status = WdfIoTargetOpen(DeviceContext->Lpc, &OpenParams);

    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfIoTargetOpen failed with status %!STATUS!\n", Status);
        goto End;
    }

    //
    // Get the physical address of the Root Complex Register Block.
    //

    RtlZeroMemory(&IoStack, sizeof(IoStack));
    RcrbPhys.HighPart = 0;
    IoStack.MajorFunction = IRP_MJ_PNP;
    IoStack.MinorFunction = IRP_MN_READ_CONFIG;
    IoStack.Parameters.ReadWriteConfig.WhichSpace = PCI_WHICHSPACE_CONFIG;
    IoStack.Parameters.ReadWriteConfig.Buffer = (PVOID) &RcrbPhys.LowPart;
    IoStack.Parameters.ReadWriteConfig.Offset = RCBA_OFFSET;
    IoStack.Parameters.ReadWriteConfig.Length = sizeof(RcrbPhys.LowPart);

    Status = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES, DeviceContext->Lpc, &Request);
    
    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfRequestCreate failed with status %!STATUS!\n", Status);
        goto End;
    }

    WdfRequestWdmFormatUsingStackLocation(Request, &IoStack);
    WDF_REQUEST_SEND_OPTIONS_INIT(&Options, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);

    WdfRequestSend(Request, DeviceContext->Lpc, &Options);
    Status = WdfRequestGetStatus(Request);

    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfRequestGetStatus failed with status %!STATUS!\n", Status);
        goto End;
    } 

    //
    // Map the RCRB into virtual memory.
    //

    RcrbPhys.LowPart &= 0xffffc000;
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "RCRB physical location is: %x08x\n", RcrbPhys.LowPart);

    DeviceContext->Rcrb = (PUCHAR) MmMapIoSpace(RcrbPhys, RCRB_SIZE, MmNonCached);   

End:
    return Status;
}

NTSTATUS
DriverDeviceAdd (
    __in WDFDRIVER Driver,
    __in PWDFDEVICE_INIT DeviceInit
    )
/*++
Routine Description:
    Called by the DriverEntry to create a control-device. This call is
    responsible for freeing the memory for DeviceInit.

Arguments:
    DriverObject - a pointer to the object that represents this device
    driver.

    DeviceInit - Pointer to a driver-allocated WDFDEVICE_INIT structure.

Return Value:
    STATUS_SUCCESS if initialized; an error otherwise.
--*/
{

    WDFDEVICE ControlDevice;
    WDF_OBJECT_ATTRIBUTES DeviceAttributes;
    WDFQUEUE IoQueue;
    WDF_OBJECT_ATTRIBUTES IoQueueAttributes;
    WDF_IO_QUEUE_CONFIG IoQueueConfig;
    NTSTATUS Status;
    DECLARE_CONST_UNICODE_STRING(DeviceName, PHYS_DEVICE_NAME);
    DECLARE_CONST_UNICODE_STRING(SymbolicLinkName, SYMBOLIC_NAME);

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();
    
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "--> DriverDeviceAdd\n");
    
    //
    // Set exclusive to TRUE so that no more than one app can talk to the
    // control device at any time.
    //

    WdfDeviceInitSetExclusive(DeviceInit, TRUE);

    Status = WdfDeviceInitAssignName(DeviceInit, &DeviceName);

    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfDeviceInitAssignName failed %!STATUS!", Status);
        goto End;
    }

    //
    // Associate the context area to the WDFDEVICE object.
    //
    
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&DeviceAttributes, DEVICE_CONTEXT);

    //
    // Specify the cleanup callback. 
    //
    
    DeviceAttributes.EvtCleanupCallback = EvtDeviceCleanupCallback;

    //
    // Create the device.
    //

    Status = WdfDeviceCreate(&DeviceInit, &DeviceAttributes, &ControlDevice);

    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfDeviceCreate failed %!STATUS!", Status);
        goto End;
    }

    // 
    // Initialize the LPC IoTarget and map the RCRB into virtual memory.
    //
    
    //InitializeContext(ControlDevice);
        
    //
    // Create a symbolic link for the control object so that applications can open
    // the device.
    //

    Status = WdfDeviceCreateSymbolicLink(ControlDevice, &SymbolicLinkName);

    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfDeviceCreateSymbolicLink failed with status %!STATUS!", Status);
        goto End;
    }

    //
    // Set up the default IoQueue.
    //

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&IoQueueConfig, WdfIoQueueDispatchSequential);
    IoQueueConfig.PowerManaged = WdfFalse;
    IoQueueConfig.EvtIoRead = EvtIoRead;
    IoQueueConfig.EvtIoWrite = EvtIoWrite;
    IoQueueConfig.EvtIoDeviceControl = EvtIoDeviceControl;

    WDF_OBJECT_ATTRIBUTES_INIT(&IoQueueAttributes);
    IoQueueAttributes.ExecutionLevel = WdfExecutionLevelPassive;

    Status = WdfIoQueueCreate(ControlDevice, &IoQueueConfig, &IoQueueAttributes, &IoQueue);

    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_INIT, "WdfIoQueueCreate failed with status %!STATUS!\n", Status);
        goto End;
    }

    //
    // Control devices must notify WDF when they are done initializing. I/O is
    // rejected until this call is made.
    //

    WdfControlFinishInitializing(ControlDevice);

End:
    //
    // If the device is created successfully, the framework clears the
    // DeviceInit value. Otherwise, device creation must have failed so we
    // should free the memory ourselves.
    //

    if (DeviceInit != NULL) {
        WdfDeviceInitFree(DeviceInit);
    } else if (!NT_SUCCESS(Status)) {
        WdfObjectDelete(ControlDevice);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "<-- DriverDeviceAdd\n");
    return Status;
}

VOID
EvtDeviceCleanupCallback (
    __in WDFDEVICE Device
    )
{
    PDEVICE_CONTEXT DeviceContext;
    
    PAGED_CODE();
    
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "--> EvtDeviceCleanupCallback\n");

    DeviceContext = GetDeviceContext(Device);	

    if ((PVOID) DeviceContext->Lpc != NULL) {
        WdfObjectDelete(DeviceContext->Lpc);
    }

    if (DeviceContext->Rcrb != NULL) {
        MmUnmapIoSpace(DeviceContext->Rcrb, RCRB_SIZE);
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "<-- EvtDeviceCleanupCallback\n");
}