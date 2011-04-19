#include "Common.h"

#define ARRAY_SIZE(x) (sizeof(x) /sizeof(x[0]))

PFN_WDFPREDEVICEINSTALL FnWdfPreDeviceInstall;
PFN_WDFPOSTDEVICEINSTALL FnWdfPostDeviceInstall;
PFN_WDFPREDEVICEREMOVE FnWdfPreDeviceRemove;
PFN_WDFPOSTDEVICEREMOVE FnWdfPostDeviceRemove;

#define GET_PROC(name, type)                                            \
    Fn##name = (type) GetProcAddress(Library, #name);                   \
    if (Fn##name == NULL) {                                             \
        Error = GetLastError();                                         \
        PrintError("GetProcAddress(\"" #name "\") failed", Error);      \
        return NULL;                                                    \
    }

LONG
GetPathToInf (
    __out_ecount(InfFilePathSize) PWCHAR InfFilePath,
    __in ULONG InfFilePathSize
    )
{
    LONG Error = ERROR_SUCCESS;

    if (GetCurrentDirectoryW(InfFilePathSize, InfFilePath) == 0) {
        PrintError("InstallDriver failed", (Error = GetLastError()));
    } else if (FAILED(StringCchCatW(InfFilePath, InfFilePathSize, INF_FILENAME))) {
        Error = ERROR_BUFFER_OVERFLOW;
    }

    return Error;
}

static
BOOL
InstallDriver (
    __in SC_HANDLE ScManager,
    __in LPCTSTR DriverName,
    __in LPCTSTR DriverLocation
    )
{   
    LONG Error;
    WCHAR InfPath[MAX_PATH];
    SC_HANDLE Service;
    
    if ((Error = GetPathToInf(InfPath, ARRAY_SIZE(InfPath))) != ERROR_SUCCESS ||
        (Error = FnWdfPreDeviceInstall(InfPath, WDF_SECTION_NAME)) != ERROR_SUCCESS) {
        return  FALSE;
    }

    //
    // Create a new a service object.
    //

    Service = CreateService(ScManager,              // handle of service control manager database
                            DriverName,             // name of service to start
                            DriverName,             // display name
                            SERVICE_ALL_ACCESS,     // type of access to service
                            SERVICE_KERNEL_DRIVER,  // type of service
                            SERVICE_DEMAND_START,   // when to start service
                            SERVICE_ERROR_NORMAL,   // severity if service fails to start
                            DriverLocation,         // location of binary file
                            NULL,                   // service does not belong to a group
                            NULL,                   // no tag requested
                            NULL,                   // no dependency names
                            NULL,                   // use LocalSystem account
                            NULL                    // no password for service account
                            );

    if (Service == NULL) {        
        if ((Error = GetLastError()) == ERROR_SERVICE_EXISTS) {

            //
            // Ignore this error.
            //

            return TRUE;
        } else {
            PrintError("CreateService failed", Error);
            return  FALSE;
        }
    }

    CloseServiceHandle(Service);
    
    if ((Error = FnWdfPostDeviceInstall(InfPath, WDF_SECTION_NAME)) != ERROR_SUCCESS) {
        PrintError("WdfPostDeviceInstall failed", Error);
        return FALSE;
    }

    return TRUE;
} 

static
BOOL
RemoveDriver (
    __in SC_HANDLE ScManager,
    __in LPCTSTR DriverName
    )
{
    LONG Error;
    WCHAR InfPath[MAX_PATH];
    BOOL Result;
    SC_HANDLE Service;    
    
    if ((Error = GetPathToInf(InfPath, ARRAY_SIZE(InfPath))) != ERROR_SUCCESS ||
        (Error = FnWdfPreDeviceRemove(InfPath, WDF_SECTION_NAME)) != ERROR_SUCCESS) {
        return  FALSE;
    }

    //
    // Open the handle to the existing service.
    //

    Service = OpenService(ScManager, DriverName, SERVICE_ALL_ACCESS);

    if (Service == NULL) {
        PrintError("OpenService failed", GetLastError());
        return FALSE;
    }

    //
    // Mark the service for deletion from the service control manager database.
    //

    if ((Result = DeleteService(Service)) == FALSE) {
        PrintError("DeleteService failed", GetLastError());

        //
        // Fall through to properly close the service handle.
        //
    }

    CloseServiceHandle(Service);
    
    if (FnWdfPostDeviceRemove(InfPath, WDF_SECTION_NAME) != ERROR_SUCCESS) {
        Result = FALSE;
    }

    return Result;
}

static
BOOL
StartDriver (
    __in SC_HANDLE ScManager,
    __in LPCTSTR DriverName
    )
{
    LONG Error;
    BOOL Result;
    SC_HANDLE Service; 

    //
    // Open the handle to the existing service.
    //

    Service = OpenService(ScManager, DriverName, SERVICE_ALL_ACCESS);

    if (Service == NULL) {
        PrintError("OpenService failed", GetLastError());
        return FALSE;
    }

    //
    // Start the execution of the service (i.e. start the driver).
    //

    Result = StartService(Service, 0, NULL) || (Error = GetLastError()) == ERROR_SERVICE_ALREADY_RUNNING;
    if (Result == FALSE) {
        PrintError("StartService failed", Error);
    }

    CloseServiceHandle(Service);
    return Result;
}

static
BOOL
StopDriver (
    __in SC_HANDLE ScManager,
    __in LPCTSTR DriverName
    )
{
    LONG Error;
    BOOL Result;
    SC_HANDLE Service;
    SERVICE_STATUS ServiceStatus;

    //
    // Open the handle to the existing service.
    //

    Service = OpenService(ScManager, DriverName, SERVICE_ALL_ACCESS);

    if (Service == NULL) {
        PrintError("OpenService failed", GetLastError());
        return FALSE;
    }

    //
    // Stop the service (i.e., stop the driver).
    //

    if ((Result = ControlService(Service, SERVICE_CONTROL_STOP, &ServiceStatus)) == FALSE &&
        (Error = GetLastError()) != ERROR_SERVICE_NOT_ACTIVE) {
        
        PrintError("ControlService failed", GetLastError());
    }

    CloseServiceHandle(Service);
    return Result;
}

BOOL
ManageDriver (
    __in LPCTSTR DriverName,
    __in LPCTSTR DriverLocation,
    __in USHORT Function
    )
{
    BOOL Result;
    SC_HANDLE ScManager;

    //
    // Connect to the Service Control Manager and open the Services database.
    //

    ScManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (ScManager == NULL) {
        PrintError("Failed to open the SC Manager", GetLastError());
        return FALSE;
    }

    //
    // Do the requested function.
    //

    switch (Function) {
        case DRIVER_FUNC_INSTALL:
            Result = InstallDriver(ScManager, DriverName, DriverLocation)
                  && StartDriver(ScManager, DriverName);
            break;
        case DRIVER_FUNC_REMOVE:
            StopDriver(ScManager, DriverName);
            RemoveDriver(ScManager, DriverName);

            //
            // Ignore all errors.
            //

            Result = TRUE;
            break;
        default:
            printf("Unknown ManageDriver function. \n");
            Result = FALSE;
            break;
    }

    CloseServiceHandle(ScManager);
    return Result;
}

static
BOOL
BuildDriversDirPath(
    __out_bcount_full(BufferLength) PCHAR DriverLocation,
    __in ULONG BufferLength,
    __in PCHAR DriverName
    )
{
    LONG Length;
    LONG Remaining = BufferLength - 1;

    //
    // Get the base windows directory path.
    //

    Length = GetWindowsDirectory(DriverLocation, Remaining);

    if (Length == 0 || (Remaining - Length) < sizeof(SYSTEM32_DRIVERS)) {
        return FALSE;
    }

    Remaining -= Length;

    //
    // Build dir to have "%windir%\System32\Drivers\<DriverName>".
    //

    if (FAILED(StringCchCat(DriverLocation, Remaining, SYSTEM32_DRIVERS))) {
        return FALSE;
    }

    Remaining -= sizeof(SYSTEM32_DRIVERS);
    Length += sizeof(SYSTEM32_DRIVERS) + strlen(DriverName);

    if (Remaining < Length || FAILED(StringCchCat(DriverLocation, Remaining, DriverName))) {
        return FALSE;
    }

    DriverLocation[Length] = '\0';
    return TRUE;
}

BOOL
FindAndCopyDriver (
    __out_bcount_full(BufferLength) PCHAR DriverLocation,
    __in ULONG BufferLength
    )
{
    CHAR CurrentDriverLocation[MAX_PATH + 1];
    HANDLE FileHandle;    

    //
    // Setup path name to the driver file.
    //

    if (GetCurrentDirectory(MAX_PATH, CurrentDriverLocation) == 0) {
        PrintError("GetCurrentDirectory failed", GetLastError());
        return FALSE;
    }

    if (FAILED(StringCchCat(CurrentDriverLocation, BufferLength, "\\" DRIVER_NAME ".sys") )) {
        return FALSE;
    }

    //
    // Ensure driver file is in the specified directory.
    //

    FileHandle = CreateFile(CurrentDriverLocation,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

    if (FileHandle == INVALID_HANDLE_VALUE) {
        printf("Driver: " DRIVER_NAME ".sys is not in the %s directory. \n", DriverLocation );
        return FALSE;
    }

    //
    // Build %windir%\System32\drivers\<DRIVER_NAME> path and
    // copy the driver to %windir%\System32\drivers.
    //

    if (BuildDriversDirPath(DriverLocation, MAX_PATH + 1, DRIVER_NAME ".sys") == FALSE) {
        printf("BuildDriversDirPath failed.\n");
        return FALSE;
    }

    if (CopyFile(CurrentDriverLocation, DriverLocation, FALSE) == FALSE) {
        PrintError("CopyFile failed", GetLastError());
        return FALSE;
    }

    CloseHandle(FileHandle);
    return TRUE;
}   

HMODULE
LoadWdfCoInstaller (
    )
{
    LONG Error = ERROR_SUCCESS;
    HMODULE Library = NULL;   
    CHAR Path[MAX_PATH];

    if (GetCurrentDirectory(MAX_PATH, Path) == 0) {
        PrintError("GetCurrentDirectory failed", GetLastError());
        goto End;
    }

    if (FAILED(StringCchCat(Path, MAX_PATH, CoInstallerPath))) {
        goto End;
    }

#pragma prefast(suppress:28160, "Suppressing false positive from PFD")        
    Library = LoadLibrary(Path);
        
    if (Library == NULL) {
        Error = GetLastError();
        PrintError("LoadLibrary failed", Error);
        goto End;
    }

    GET_PROC(WdfPreDeviceInstall, PFN_WDFPREDEVICEINSTALL);
    GET_PROC(WdfPostDeviceInstall, PFN_WDFPOSTDEVICEINSTALL);
    GET_PROC(WdfPreDeviceRemove, PFN_WDFPREDEVICEREMOVE);
    GET_PROC(WdfPostDeviceRemove, PFN_WDFPOSTDEVICEREMOVE);

End:
    if (Error != ERROR_SUCCESS && Library != NULL) {
        FreeLibrary(Library);
        Library = NULL;
    }

    return Library;
}

VOID
UnloadWdfCoInstaller (
    __inout HMODULE Library
    )
{
    if (Library != NULL) {
        FreeLibrary(Library);
    }
}