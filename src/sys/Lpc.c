#include <Gordon.h>

#if defined(EVENT_TRACING)
#include "Lpc.tmh"
#endif

static
NTSTATUS
ShowBiosCtl (
    __in WDFIOTARGET Lpc
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ShowBiosCtl)
#pragma alloc_text(PAGE, ShowFwhRegisters)
#endif

//
// TODO: Reuse requests?
//

NTSTATUS
ShowBiosCtl (
    __in WDFIOTARGET Lpc
    )
{
    UCHAR BiosCtl;
    IO_STACK_LOCATION IoStack;
    WDF_REQUEST_SEND_OPTIONS Options;
    WDFREQUEST Request;
    NTSTATUS Status;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "--> ShowBiosCtl\n");

    //
    // Get the BIOS control register.
    //

    RtlZeroMemory(&IoStack, sizeof(IoStack));
    IoStack.MajorFunction = IRP_MJ_PNP;
    IoStack.MinorFunction = IRP_MN_READ_CONFIG;
    IoStack.Parameters.ReadWriteConfig.WhichSpace = PCI_WHICHSPACE_CONFIG;
    IoStack.Parameters.ReadWriteConfig.Buffer = (PVOID) &BiosCtl;
    IoStack.Parameters.ReadWriteConfig.Offset = BIOS_CNTL_OFFSET;
    IoStack.Parameters.ReadWriteConfig.Length = sizeof(BiosCtl);
    
    Status = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES, Lpc, &Request);
    
    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "WdfRequestCreate failed with status %!STATUS!\n", Status);
        goto End;
    }

    WdfRequestWdmFormatUsingStackLocation(Request, &IoStack);
    WDF_REQUEST_SEND_OPTIONS_INIT(&Options, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);

    WdfRequestSend(Request, Lpc, &Options);
    Status = WdfRequestGetStatus(Request);

    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "WdfRequestGetStatus failed with status %!STATUS!\n", Status);
        goto Cleanup;
    }
    
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "BIOS_CNTL is 0x%x\n", BiosCtl);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "BIOS Lock Enable: %sabled\n", (BiosCtl & (1 << 1)) ? "en" : "dis");
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "BIOS Write Enable: %sabled\n", (BiosCtl & 1) ? "en" : "dis");

Cleanup:
    WdfObjectDelete(Request);
End:    
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "<-- ShowBiosCtl\n");
    return Status;
}

NTSTATUS
ShowFwhRegisters ( 
    __in WDFIOTARGET Lpc
    )
{
    ULONG BiosSize;
    UCHAR Contiguous;
    FWH_CONFIG FwhConfig;
    ULONG FhwReg;
    LONG Index;
    IO_STACK_LOCATION IoStack;
    ULONG MaxDecodeFwhDecode;
    ULONG MaxDecodeFwhIdsel;
    WDF_REQUEST_SEND_OPTIONS Options;
    WDFREQUEST Request;
    NTSTATUS Status;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "--> ShowFwhRegisters\n");

    //
    // Get the control register for the FWH.
    //

    RtlZeroMemory(&IoStack, sizeof(IoStack));
    IoStack.MajorFunction = IRP_MJ_PNP;
    IoStack.MinorFunction = IRP_MN_READ_CONFIG;
    IoStack.Parameters.ReadWriteConfig.WhichSpace = PCI_WHICHSPACE_CONFIG;
    IoStack.Parameters.ReadWriteConfig.Buffer = (PVOID) &FwhConfig;
    IoStack.Parameters.ReadWriteConfig.Offset = FWHSEL1_OFFSET;
    IoStack.Parameters.ReadWriteConfig.Length = sizeof(FwhConfig);    

    Status = WdfRequestCreate(WDF_NO_OBJECT_ATTRIBUTES, Lpc, &Request);
    
    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "WdfRequestCreate failed with status %!STATUS!\n", Status);
        goto End;
    }

    WdfRequestWdmFormatUsingStackLocation(Request, &IoStack);
    WDF_REQUEST_SEND_OPTIONS_INIT(&Options, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);

    WdfRequestSend(Request, Lpc, &Options);
    Status = WdfRequestGetStatus(Request);

    if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, DBG_IOCTLS, "WdfRequestGetStatus failed with status %!STATUS!\n", Status);
        goto Cleanup;
    }

    //
    // Find out the FWH chip size. We consider only IDSEL 0 and FwhSel1
    // and show only the address ranges starting from 0xffffffff.
    //

    Contiguous = 1;
    MaxDecodeFwhIdsel = 0;

    for (Index = 7; Index >= 0; --Index) {
        FhwReg = (FwhConfig.FwhSel1 >> (Index * 4)) & 0xf;
        
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, 
                    "0x%08x/0x%08x FWH IDSEL: 0x%x\n", 
                    (0x1ff8 + Index) * 0x80000 /* 512K */, 
                    ((0x1ff9 + Index) * 0x80000) - 1, 
                    FhwReg);

        if (FhwReg == 0 && Contiguous) {
            MaxDecodeFwhIdsel += 2 * 0x80000;
        } else {
            Contiguous = 0;
        } 
    }

    Contiguous = 1;
    MaxDecodeFwhDecode = 0;
    
    for (Index = 7; Index >= 0; --Index) {
        FhwReg = (FwhConfig.FwhDecEn >> (Index + 0x8)) & 0x1;

        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, 
                    "\n0x%08x/0x%08x FWH decode %sabled",
                    (0x1ff8 + Index) * 0x80000,
                    ((0x1ff9 + Index) * 0x80000) - 1,
                    FhwReg ? "en" : "dis");

        if (FhwReg == 1 && Contiguous) {
            MaxDecodeFwhDecode += 2 * 0x80000;
        } else {
            Contiguous = 0;
        }
    }

    BiosSize = min(MaxDecodeFwhIdsel, MaxDecodeFwhDecode);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "Maximum FWH chip size: 0x%x bytes\n", BiosSize);

    ShowBiosCtl(Lpc);

Cleanup:
    WdfObjectDelete(Request);
End:
    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "<-- ShowFwhRegisters\n");
    return Status;
}

VOID
ShowRcrbRegisters (
    __in PUCHAR Rcrb
    )
{
    UCHAR Bbs;
    UCHAR Buc;
    ULONG Gcs;
    static const PCHAR StrapNames[] = { "SPI", "reserved", "PCI", "LPC" };

    //
    // Read the General Control and Status register.
    //

    Gcs = READ_REGISTER_ULONG((PULONG) (Rcrb + 0x3410));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "GCS = 0x%x\n", Gcs);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "BIOS Interface Lock-Down: %sabled", (Gcs & 0x1) ? "en" : "dis");

    //
    // Get the Boot BIOS Straps.
    //

    Bbs = (Gcs >> 10) & 0x3;
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "BOOT BIOS Straps: 0x%x (%s)\n", Bbs, StrapNames[Bbs]);

    //
    // Get the Backed Up Control register.
    //

    Buc = READ_REGISTER_UCHAR(Rcrb + 0x3414);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_CREATE_CLOSE, "Top Swap : %s\n", (Buc & 1) ? "enabled (A16 inverted)" : "not enabled");
}