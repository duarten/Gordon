/*++

Module Name:
    DeviceInterface.h

Abstract:
    Definitions used by Gordon device driver and application, including IOCTL codes.

Environment:
    User & Kernel mode

--*/

#pragma once

#define DRIVER_NAME "Gordon"
#define DEVICE_NAME "\\\\.\\" DRIVER_NAME

#define FILE_DEVICE_SNFLASH 0x8000
#define IOCTL_BASE 0x800

#define IOCTL_SHOW_DATA \
    CTL_CODE(FILE_DEVICE_SNFLASH, IOCTL_BASE, \
             METHOD_BUFFERED, FILE_READ_ACCESS)
