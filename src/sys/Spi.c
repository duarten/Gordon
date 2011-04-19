#include <Gordon.h>

#if defined(EVENT_TRACING)
#include "Spi.tmh"
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, ShowSpiRegisters)
#endif

FORCEINLINE
VOID
ShowHsfs (
    __in USHORT HsfsReg
    )
{
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "HSFS: 0x%04x\n", HsfsReg);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FLOCKDN %i, ", (HsfsReg >> 15 & 1));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FDV %i, ", (HsfsReg >> 14) & 1);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FDOPSS %i, ", (HsfsReg >> 13) & 1);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "SCIP %i, ", (HsfsReg >> 5) & 1);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "BERASE %i, ", (HsfsReg >> 3) & 3);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "AEL %i, ", (HsfsReg >> 2) & 1);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FCERR %i, ", (HsfsReg >> 1) & 1);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FDONE %i\n", (HsfsReg >> 0) & 1);
}

FORCEINLINE
VOID
ShowFreg (
    __in PUCHAR SpiBar,
    __in ULONG FrapReg,
    __in ULONG FregRegIndex
    )
{
    static const char * const AccessNames[4] = { "locked", "read-only", "write-only", "read-write" };
    static const char * const RegionNames[4] = { "Flash", "BIOS", "Management Engine", "Gigabit Ethernet" };
    
    ULONG Base;
    ULONG FregReg;
    ULONG Limit;
    ULONG RwPerms;
    ULONG Offset;

    RwPerms = (((FRAP_BRWA(FrapReg) >> FregRegIndex) & 1) << 1) | (((FRAP_BRRA(FrapReg) >> FregRegIndex) & 1) << 0);
    Offset = FREG0_OFFSET + (FregRegIndex * 4);
    FregReg = READ_REGISTER_ULONG((PULONG) (SpiBar + Offset));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FREG%i (%s): 0x%08x\n", FregRegIndex, RegionNames[FregRegIndex], FregReg);
                
        
    Base = FREG_BASE(FregReg);
    Limit = FREG_LIMIT(FregReg);

    if (Base == 0x1fff && Limit == 0) {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "%s region is unused.\n", RegionNames[FregRegIndex]);
    } else {
        TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "0x%08x-0x%08x is %s\n", Base << 12, (Limit << 12) | 0x0fff, AccessNames[RwPerms]);
    }
}

FORCEINLINE
VOID
ShowFrap (
    __in PUCHAR SpiBar
    )
{
    ULONG FrapReg;
    ULONG Index;

    FrapReg = READ_REGISTER_ULONG((PULONG) (SpiBar + 0x50));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FRAP: 0x%08x\n", FrapReg);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "BMWAG 0x%02x, ", FRAP_BMWAG(FrapReg));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "BMRAG 0x%02x, ", FRAP_BMRAG(FrapReg));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "BRWA 0x%02x, ", FRAP_BRWA(FrapReg));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "BRRA 0x%02x\n", FRAP_BRRA(FrapReg));

    for (Index = 0; Index < 4; ++Index) {
        ShowFreg(SpiBar, FrapReg, Index);
    }
}

FORCEINLINE
VOID
ShowProtectedRegions (
    __in PUCHAR SpiBar
    )
{
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "PR0: 0x%08x\n", READ_REGISTER_ULONG((PULONG) (SpiBar + PR0_OFFSET)));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "PR1: 0x%08x\n", READ_REGISTER_ULONG((PULONG) (SpiBar + PR1_OFFSET)));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "PR2: 0x%08x\n", READ_REGISTER_ULONG((PULONG) (SpiBar + PR2_OFFSET)));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "PR3: 0x%08x\n", READ_REGISTER_ULONG((PULONG) (SpiBar + PR3_OFFSET)));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "PR4: 0x%08x\n", READ_REGISTER_ULONG((PULONG) (SpiBar + PR4_OFFSET)));
}

FORCEINLINE
VOID
ShowSsfs (
    __in UCHAR SsfsReg
    )
{
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "0x90: 0x%02x (SSFS)\n", SsfsReg & 0xff);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "AEL %i, ", (SsfsReg >> 4) & 1);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FCERR %i, ", (SsfsReg >> 3) & 1);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FDONE %i, ", (SsfsReg >> 2) & 1);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "SCIP %i\n", (SsfsReg >> 0) & 1);
}

NTSTATUS
ShowSpiRegisters (
    __in PUCHAR SpiBar
    )
{
    USHORT HsfsReg;
    ULONG SwSeqRegs;

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "--> ShowSpiRegisters\n");

    HsfsReg = READ_REGISTER_USHORT((PUSHORT) (SpiBar + HSFS_OFFSET));
    SwSeqRegs = READ_REGISTER_ULONG((PULONG) (SpiBar + SSFS_SSFC_OFFSET));

    ShowHsfs(HsfsReg);
    ShowFrap(SpiBar);
    ShowProtectedRegions(SpiBar);
    ShowSsfs((UCHAR) (SwSeqRegs & 0xff));

    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "SSFC: 0x%06x\n", SwSeqRegs >> 8);
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "PREOP: 0x%04x\n", READ_REGISTER_USHORT((PUSHORT) (SpiBar + PREOP_OFFSET)));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "OPTYPE: 0x%04x\n", READ_REGISTER_USHORT((PUSHORT) (SpiBar + OPTYPE_OFFSET)));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "OPMENU Low: 0x%08x\n", READ_REGISTER_ULONG((PULONG) (SpiBar + OPMENU_L_OFFSET)));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "OPMENU High: 0x%08x\n", READ_REGISTER_ULONG((PULONG) (SpiBar + OPMENU_H_OFFSET)));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "BBAR: 0x%08x\n", READ_REGISTER_ULONG((PULONG) (SpiBar + BBAR_OFFSET)));
    TraceEvents(TRACE_LEVEL_INFORMATION, DBG_IOCTLS, "FDOC: 0x%08x\n", READ_REGISTER_ULONG((PULONG) (SpiBar + FDOC_OFFSET)));

    if (HsfsReg & (1 << 15)) {
        TraceEvents(TRACE_LEVEL_WARNING, DBG_IOCTLS, "SPI configuration lockdown active.\n");
    }

    TraceEvents(TRACE_LEVEL_VERBOSE, DBG_IOCTLS, "<-- ShowSpiRegisters\n");
    return STATUS_SUCCESS;
}