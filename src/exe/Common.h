/*++

Module Name:
    Common.h

Abstract:
    Definitions used by the Gordon application.

Environment:
    User mode

--*/

#pragma once

#include <DriverSpecs.h>
__user_code  

#include <windows.h>

#pragma warning(disable:4201)  // nameless struct/union
#include <winioctl.h>
#pragma warning(default:4201)

#include <strsafe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Public.h"

#include <wdfinstaller.h>

// 4127 -- Conditional Expression is Constant warning
#define WHILE(a) \
    while (__pragma(warning(disable:4127)) a __pragma(warning(disable:4127)))

#define SYSTEM32_DRIVERS "\\System32\\Drivers\\"
#define INF_FILENAME L"\\Gordon.inf"
#define WDF_SECTION_NAME L"Gordon.NT.Wdf"

#define CoInstallerPath "\\WdfCoInstaller01009.dll"
    
#define DRIVER_FUNC_INSTALL 0x01
#define DRIVER_FUNC_REMOVE 0x02

VOID
PrintError (
    __in PCHAR Prefix,
    __in_opt ULONG ErrorCode 
    );

HMODULE
LoadWdfCoInstaller (
    );

VOID
UnloadWdfCoInstaller (
    __inout HMODULE Library
    );

BOOL
FindAndCopyDriver (
    __out_bcount_full(BufferLength) PCHAR DriverLocation,
    __in ULONG BufferLength
    );

BOOL
ManageDriver(
    __in LPCTSTR DriverName,
    __in LPCTSTR DriverLocation,
    __in USHORT Function
    );