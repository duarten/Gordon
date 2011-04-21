#include <Gordon.h>

#if defined(EVENT_TRACING)
//
// The trace message header (.tmh) file must be included in a source file
// before any WPP macro calls and after defining a WPP_CONTROL_GUIDS
// macro (defined in toaster.h). During the compilation, WPP scans the source
// files for DoTraceMessage() calls and builds a .tmh file which stores a unique
// data GUID for each message, the text resource string for each message,
// and the data types of the variables passed in for each message. This file
// is automatically generated and used during post-processing.
//
#include "Driver.tmh"
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, EvtDriverUnload)
#pragma alloc_text(PAGE, EvtDriverContextCleanup)
#endif

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:
    DriverEntry initializes the driver and is the first routine
    called by the system after the driver is loaded.

Parameters Description:
    DriverObject - represents the instance of the function
                   driver that is loaded into memory.
    RegistryPath - represents the driver specific path in
                   the Registry.

Return Value:
    STATUS_SUCCESS if successful, otherwise the failure NT status code
--*/
{
    WDF_OBJECT_ATTRIBUTES Attributes;
    PWDFDEVICE_INIT DeviceInit;
    WDFDRIVER Driver;
    WDF_DRIVER_CONFIG DriverConfig;
    NTSTATUS Status;

    KdPrint(("Gordon \n"));
    KdPrint(("Built %s %s\n", __DATE__, __TIME__));
    KdPrint(("--> DriverEntry\n"));

    WDF_DRIVER_CONFIG_INIT(&DriverConfig, WDF_NO_EVENT_CALLBACK); // This is a non-pnp driver.
        
    //
    // Tell the framework that this is a non-pnp driver so that it doesn't
    // set the default AddDevice routine.
    //

    DriverConfig.DriverInitFlags |= WdfDriverInitNonPnpDriver;

    //
    // A non-pnp driver must explicitly register an unload routine for
    // the driver to be unloaded.
    //

    DriverConfig.EvtDriverUnload = EvtDriverUnload;

    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //

    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
    Attributes.EvtCleanupCallback = EvtDriverContextCleanup;
    
    //
    // Create a KMDF driver object to represent the driver.
    //
    
    Status = WdfDriverCreate(DriverObject, RegistryPath, &Attributes, &DriverConfig, &Driver);

    if (!NT_SUCCESS(Status)) {
        KdPrint(("WdfDriverCreate failed with status %!STATUS!\n", Status));
        return Status;
    }

    //
    // Since we are calling WPP_CLEANUP in the DriverContextCleanup callback, we initialize
    // WPP Tracing after the WDFDRIVER object is created to ensure that we cleanup WPP properly
    // if DriverEntry fails. This eliminates the need to call WPP_CLEANUP in every path of DriverEntry. 
    // We cannot do this in the unload callback beacause the framework does not call it if we return 
    // an error status value from DriverEntry.
    //

    WPP_INIT_TRACING(DriverObject, RegistryPath);
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "--> DriverEntry\n");

    //
    // In order to create a control device object, we first need to allocate a
    // WDFDEVICE_INIT structure and set all properties.
    //
    
    DeviceInit = WdfControlDeviceInitAllocate(Driver, &SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_RW_RES_R);

    if (DeviceInit == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "WdfControlDeviceInitAllocate failed with status %!STATUS!\n", Status);
        return Status;
    }

    //
    // Call DriverDeviceAdd to create a control device object to represent our
    // software device.
    //
    
    Status = DriverDeviceAdd(Driver, DeviceInit);
    
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "<-- DriverEntry\n");
    return Status;
}

VOID
EvtDriverUnload (
    __in WDFDRIVER Driver
    )
/*++
Routine Description:
    Called by the I/O subsystem just before unloading the driver.

Arguments:
    Driver - Handle to a framework driver object created in DriverEntry.

Return Value:
    VOID.
--*/
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "--> EvtDriverUnload\n");    
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_INIT, "<-- EvtDriverUnload\n");    
}

VOID
EvtDriverContextCleanup (
    __in WDFDRIVER Driver
    )
/*++
Routine Description:
   Called when the driver object is deleted during driver unload.

Arguments:
    Driver - Handle to a framework driver object created in DriverEntry

Return Value:
    VOID
--*/
{
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_INIT, "--> EvtDriverContextCleanup\n");

    PAGED_CODE();

    //
    // No need to free the ControlDevice object explicitly because it will
    // be deleted when the Driver object is deleted, due to the default parent
    // child relationship between Driver and ControlDevice.
    //

    WPP_CLEANUP(WdfDriverWdmGetDriverObject(Driver));
}