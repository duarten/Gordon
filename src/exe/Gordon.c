#include "Common.h"

VOID
PrintError (
    __in PCHAR Prefix,
    __in_opt ULONG ErrorCode 
    )
{
    LPVOID ErrorMsgBuf;
    
    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      ErrorCode,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPSTR) &ErrorMsgBuf,
                      0,
                      NULL
                      )) {

        printf("%s; error: %s\n", Prefix, (LPSTR)ErrorMsgBuf);
        LocalFree(ErrorMsgBuf);
    }
}

static
VOID
Read (
    __in HANDLE DeviceHandle
    )
{
    ULONG Index = 0;
    PUCHAR ReadBuffer = NULL;
    const size_t ReadBufferLength = 2 * 1024 * 1024;
    ULONG BytesRead;

    ReadBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ReadBufferLength);
    
    if(ReadBuffer == NULL) {
        printf("+++ failed to allocate a buffer to read the BIOS to.\n");
        return;
    }

    if (!ReadFile(DeviceHandle, ReadBuffer, ReadBufferLength, &BytesRead, NULL)) {
        printf("+++ failed to read the BIOS. Error 0x%x\n", GetLastError());
        goto End;
    }

    printf("+++ read %d bytes\n", BytesRead);

    for (Index = 0; Index < BytesRead; ++Index) {
        printf("%c", ReadBuffer[Index]);
    }
    
End:
    HeapFree(GetProcessHeap(), 0, ReadBuffer);
}


VOID
__cdecl
main (
    __in ULONG argc,
    __in_ecount(argc) PCHAR argv[]
    )
{
    HANDLE DeviceHandle;
    CHAR DriverLocation[MAX_PATH];
    LONG Error;
    HMODULE Library = NULL;
    
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    printf("+++ trying to open the device\n");

    DeviceHandle = CreateFile( 
                        DEVICE_NAME,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL,
                        CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);
                        
    if (DeviceHandle == INVALID_HANDLE_VALUE) {
        Error = GetLastError();

        if (Error != ERROR_FILE_NOT_FOUND && Error != ERROR_PATH_NOT_FOUND) {
            PrintError("CreateFile failed", Error);
            return;
        }

        printf("+++ loading wdf co installer\n");

        Library = LoadWdfCoInstaller();

        if (Library == NULL) {		
            printf("The %s must be in the current directory.\n", CoInstallerPath);
            return;
        }

        printf("+++ finding and copying the driver\n");

        if (FindAndCopyDriver(DriverLocation, MAX_PATH) == FALSE) {
            printf("Failed to find and copy the " DRIVER_NAME ".sys driver\n");
            goto Unload;
        }

        printf("+++ installing the driver\n");

        if (ManageDriver(DRIVER_NAME, DriverLocation, DRIVER_FUNC_INSTALL) == FALSE) {
            printf("Unable to install driver.\n");
            goto Remove;
        }

        printf("+++ trying to open the device\n");

        DeviceHandle = CreateFile( 
                    DEVICE_NAME,
                    GENERIC_READ | GENERIC_WRITE,
                    0,
                    NULL,
                    CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);

        if (DeviceHandle == INVALID_HANDLE_VALUE) {
            PrintError("CreateFile failed", GetLastError());
            goto Remove;
        }
    }

    DeviceIoControl(DeviceHandle, IOCTL_SHOW_DATA, NULL, 0, NULL, 0, NULL, NULL);
    //Read(DeviceHandle);
    
    printf("+++ closing the device\n");

    if (!CloseHandle(DeviceHandle)) {
        PrintError("CloseHandle failed", GetLastError());
    }

Remove:
    printf("+++ removing the driver\n");
    ManageDriver(DRIVER_NAME, DriverLocation, DRIVER_FUNC_REMOVE);

Unload:
    printf("+++ unloading the wdf co installer\n");
    UnloadWdfCoInstaller(Library);

    printf("+++ exiting\n");
}