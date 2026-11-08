/** @file
  EFI Serial I/O over the synchronous RAM-first SerialPortLib sink.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/SerialPortLibVendor.h>
#include <Library/SerialPortLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SerialIo.h>

STATIC EFI_HANDLE  mSerialHandle;

STATIC EFI_SERIAL_IO_MODE  mMode = {
  EFI_SERIAL_INPUT_BUFFER_EMPTY | EFI_SERIAL_OUTPUT_BUFFER_EMPTY,
  1000,
  0, // The inherited physical UART speed is deliberately not claimed.
  1,
  8,
  NoParity,
  OneStopBit
};

typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  UART_DEVICE_PATH            Uart;
  EFI_DEVICE_PATH_PROTOCOL    End;
} O1_SERIAL_DEVICE_PATH;

STATIC O1_SERIAL_DEVICE_PATH  mDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH), 0 } },
    EDKII_SERIAL_PORT_LIB_VENDOR_GUID
  },
  {
    { MESSAGING_DEVICE_PATH, MSG_UART_DP, { sizeof (UART_DEVICE_PATH), 0 } },
    0,
    0,
    8,
    NoParity,
    OneStopBit
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC
EFI_STATUS
EFIAPI
O1SerialReset (
  IN EFI_SERIAL_IO_PROTOCOL  *This
  )
{
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Library initialization preserves a valid SEC/DXE log and does no UART reset.
  return (EFI_STATUS)SerialPortInitialize ();
}

STATIC
EFI_STATUS
EFIAPI
O1SerialSetAttributes (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT64                  BaudRate,
  IN UINT32                  ReceiveFifoDepth,
  IN UINT32                  Timeout,
  IN EFI_PARITY_TYPE         Parity,
  IN UINT8                   DataBits,
  IN EFI_STOP_BITS_TYPE      StopBits
  )
{
  RETURN_STATUS  Status;

  if ((This == NULL) || (This->Mode == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  // Software normalization only: never change clock, DLAB, FIFO or line control.
  Status = SerialPortSetAttributes (&BaudRate, &ReceiveFifoDepth, &Timeout, &Parity, &DataBits, &StopBits);
  if (RETURN_ERROR (Status)) {
    return (EFI_STATUS)Status;
  }

  This->Mode->BaudRate         = BaudRate;
  This->Mode->ReceiveFifoDepth = ReceiveFifoDepth;
  This->Mode->Timeout          = Timeout;
  This->Mode->Parity           = Parity;
  This->Mode->DataBits         = DataBits;
  This->Mode->StopBits         = StopBits;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
O1SerialSetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT32                  Control
  )
{
  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  return (EFI_STATUS)SerialPortSetControl (Control);
}

STATIC
EFI_STATUS
EFIAPI
O1SerialGetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  OUT UINT32                 *Control
  )
{
  if ((This == NULL) || (Control == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  return (EFI_STATUS)SerialPortGetControl (Control);
}

STATIC
EFI_STATUS
EFIAPI
O1SerialWrite (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN               *BufferSize,
  IN VOID                    *Buffer
  )
{
  UINTN  Requested;
  UINTN  Written;

  if ((This == NULL) || (BufferSize == NULL) || ((*BufferSize != 0) && (Buffer == NULL))) {
    return EFI_INVALID_PARAMETER;
  }

  Requested   = *BufferSize;
  Written     = SerialPortWrite ((UINT8 *)Buffer, Requested);
  *BufferSize = Written;
  // Acceptance by RAM is reliable; the optional physical UART remains unverified.
  return (Written == Requested) ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}

STATIC
EFI_STATUS
EFIAPI
O1SerialRead (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN               *BufferSize,
  OUT VOID                   *Buffer
  )
{
  UINTN  Requested;

  if ((This == NULL) || (BufferSize == NULL) || ((*BufferSize != 0) && (Buffer == NULL))) {
    return EFI_INVALID_PARAMETER;
  }

  Requested   = *BufferSize;
  *BufferSize = 0;
  // No receive support or wait loop in this output-only bring-up endpoint.
  return (Requested == 0) ? EFI_SUCCESS : EFI_NOT_READY;
}

STATIC EFI_SERIAL_IO_PROTOCOL  mSerialIo = {
  EFI_SERIAL_IO_PROTOCOL_REVISION1p1,
  O1SerialReset,
  O1SerialSetAttributes,
  O1SerialSetControl,
  O1SerialGetControl,
  O1SerialWrite,
  O1SerialRead,
  &mMode,
  NULL
};

EFI_STATUS
EFIAPI
O1SerialDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  CHAR8       Message[] = "O1SerialDxe: RAM-backed output; physical UART unverified\r\n";

  if (mSerialHandle != NULL) {
    return EFI_ALREADY_STARTED;
  }

  Status = O1SerialReset (&mSerialIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mSerialHandle,
                  &gEfiSerialIoProtocolGuid,
                  &mSerialIo,
                  &gEfiDevicePathProtocolGuid,
                  &mDevicePath,
                  NULL
                  );
  if (!EFI_ERROR (Status)) {
    SerialPortWrite ((UINT8 *)Message, sizeof (Message) - 1);
  }

  return Status;
}
