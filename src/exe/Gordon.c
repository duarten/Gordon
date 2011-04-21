#include "Common.h"
#include <wmistr.h>
#include <evntrace.h> 
#include <evntcons.h>

// {DB0490F7-F7C7-4304-A5B2-80F2FA741C83}
static const GUID SESSION_GUID = { 0xdb0490f7, 0xf7c7, 0x4304, { 0xa5, 0xb2, 0x80, 0xf2, 0xfa, 0x74, 0x1c, 0x83 } };

//
// The same GUID defined in sys/Trace.h
//

// {684B42AD-7C9F-4AAC-ACD5-A9EEFEE57376}
static const GUID PROVIDER_GUID = { 0x684b42ad, 0x7c9f, 0x4aac, { 0xac, 0xd5, 0xa9, 0xee, 0xfe, 0xe5, 0x73, 0x76 } };

#define LOGGER_NAME "GordonTraceSession"

#pragma pack(push, 1)
typedef struct _TRACE_PROPERTIES {
    EVENT_TRACE_PROPERTIES TraceProperties;
    CHAR LoggerName[sizeof(LOGGER_NAME)];
} TRACE_PROPERTIES, *PTRACE_PROPERTIES;
#pragma pack(pop)

VOID
PrintError (
    __in LPSTR Prefix,
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
WINAPI 
TraceCallback (
    __in  PEVENT_TRACE Event
    )
{
    UNREFERENCED_PARAMETER(Event);

    printf("Received event...\n");
}

static
BOOL
StartTraceListening (
    __out PTRACEHANDLE SessionHandle,
    __out PTRACE_PROPERTIES Properties,
    __out PTRACEHANDLE ConsumingHandle
    )
{
    EVENT_TRACE_LOGFILE TraceLogfile;

    RtlZeroMemory(Properties, sizeof(*Properties));
    Properties->TraceProperties.Wnode.BufferSize = sizeof(*Properties);
    Properties->TraceProperties.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    Properties->TraceProperties.Wnode.Guid = SESSION_GUID;    
    Properties->TraceProperties.LogFileMode = EVENT_TRACE_NO_PER_PROCESSOR_BUFFERING 
                                            | EVENT_TRACE_USE_PAGED_MEMORY 
                                            | EVENT_TRACE_REAL_TIME_MODE;
    Properties->TraceProperties.LogFileNameOffset = 0;
    Properties->TraceProperties.LoggerNameOffset = FIELD_OFFSET(TRACE_PROPERTIES, LoggerName);

    if (StartTrace(SessionHandle, LOGGER_NAME, &Properties->TraceProperties) != ERROR_SUCCESS) {
        PrintError("StartTrace failed", GetLastError());
        return FALSE;
    }

    if (EnableTrace(TRUE, 0, TRACE_LEVEL_INFORMATION, &PROVIDER_GUID, *SessionHandle) != ERROR_SUCCESS) {
        PrintError("EnableTrace failed", GetLastError());
        return FALSE;
    }

    RtlZeroMemory(&TraceLogfile, sizeof(TraceLogfile));
    TraceLogfile.LoggerName = LOGGER_NAME;
    TraceLogfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME;
    TraceLogfile.EventCallback = TraceCallback;

    if ((*ConsumingHandle = OpenTrace(&TraceLogfile)) == INVALID_PROCESSTRACE_HANDLE) {
        PrintError("OpenTrace failed", GetLastError());
        return FALSE;
    }

    if (ProcessTrace(ConsumingHandle, 1, NULL, NULL) != ERROR_SUCCESS) {
        PrintError("ProcessTrace failed", GetLastError());
        return FALSE;
    }

    return TRUE;
}

FORCEINLINE
VOID
StopTraceListening (
    __in TRACEHANDLE SessionHandle,
    __inout PTRACE_PROPERTIES Properties,
    __in TRACEHANDLE ConsumingHandle
    )
{
    ControlTrace(SessionHandle, LOGGER_NAME, &Properties->TraceProperties, EVENT_TRACE_CONTROL_STOP);
    CloseTrace(ConsumingHandle);
}

static
VOID
Read (
    __in HANDLE DeviceHandle
    )
{
    ULONG Index = 0;
    PUCHAR ReadBuffer = NULL;
    const ULONG ReadBufferLength = 2 * 1024 * 1024;
    ULONG BytesRead;

    ReadBuffer = (PUCHAR) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ReadBufferLength);
    
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
    TRACEHANDLE ConsumingHandle;  
    HANDLE DeviceHandle;
    CHAR DriverLocation[MAX_PATH];
    LONG Error;
    HMODULE Library = NULL;
    TRACE_PROPERTIES Properties;
    TRACEHANDLE SessionHandle;  
    
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    GetCurrentDirectory(MAX_PATH, DriverLocation);
    printf("%s\n", DriverLocation);

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
            printf("Failed to find and copy the " DRIVER_NAME ".sys driver.\n");
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
    
    StartTraceListening(&SessionHandle, &Properties, &ConsumingHandle);

    //DeviceIoControl(DeviceHandle, IOCTL_SHOW_DATA, NULL, 0, NULL, 0, NULL, NULL);
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

    StopTraceListening(SessionHandle, &Properties, ConsumingHandle);
    printf("+++ exiting\n");
}