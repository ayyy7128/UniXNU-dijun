/** @file
  RAM-first serial output, with an explicitly opted-in DW16550 mirror.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>
#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/SerialPortLib.h>

#include "O1RamLog.h"

#define O1_UART_REG_SHIFT       2U
#define O1_UART_THR             (0U << O1_UART_REG_SHIFT)
#define O1_UART_LCR             (3U << O1_UART_REG_SHIFT)
#define O1_UART_LSR             (5U << O1_UART_REG_SHIFT)
#define O1_UART_DLAB            BIT7
#define O1_UART_THRE            BIT5
#define O1_UART_MAX_POLLS       4096U
#define O1_UART_DEFAULT_TIMEOUT 1000U
#define O1_UART_MAX_TIMEOUT     10000U

STATIC UINT32  mTimeout = O1_UART_DEFAULT_TIMEOUT;

STATIC
VOID
O1CommitLogHeader (
  IN volatile O1_RAM_LOG_HEADER  *Header
  )
{
  MemoryFence ();
  WriteBackDataCacheRange ((VOID *)(UINTN)Header, sizeof (*Header));
  ArmDataSynchronizationBarrier ();
}

// The platform must reserve and identity-map this region before using the lib.
// No discovery, allocation, MMU changes, or writes to another logging region.
STATIC
volatile O1_RAM_LOG_HEADER *
O1GetLogHeader (
  OUT UINT32  *Capacity
  )
{
  UINT64                      Base;
  UINT32                      Size;
  volatile O1_RAM_LOG_HEADER  *Header;

  Base = FixedPcdGet64 (PcdLogBase);
  Size = FixedPcdGet32 (PcdLogSize);
  if ((Base == 0) || ((Base & 63U) != 0) ||
      (Size <= O1_RAM_LOG_HEADER_SIZE) || ((Size & 63U) != 0) ||
      (Base > MAX_UINTN) || ((UINT64)Size - 1 > MAX_UINTN - Base))
  {
    return NULL;
  }

  *Capacity = Size - O1_RAM_LOG_HEADER_SIZE;
  Header    = (volatile O1_RAM_LOG_HEADER *)(UINTN)Base;
  if ((Header->Magic != O1_RAM_LOG_MAGIC) ||
      (Header->Version != O1_RAM_LOG_VERSION) ||
      (Header->HeaderSize != O1_RAM_LOG_HEADER_SIZE) ||
      (Header->WriteOffset >= *Capacity) || (Header->Bytes > *Capacity))
  {
    // Separate library instances in SEC and DXE must not erase a valid log.
    Header->Magic       = 0;
    Header->Version     = O1_RAM_LOG_VERSION;
    Header->HeaderSize  = O1_RAM_LOG_HEADER_SIZE;
    Header->WriteOffset = 0;
    Header->Bytes       = 0;
    Header->TotalBytes  = 0;
    MemoryFence ();
    Header->Magic = O1_RAM_LOG_MAGIC;
    O1CommitLogHeader (Header);
  }

  return Header;
}

STATIC
VOID
O1MirrorPersistentLog (
  IN CONST UINT8  *Buffer,
  IN UINTN        NumberOfBytes
  )
{
  UINT64                             Base;
  UINT32                             Size;
  UINT32                             Capacity;
  UINT32                             Retained;
  UINT32                             Offset;
  UINT32                             First;
  UINT32                             Index;
  volatile O1_PERSISTENT_LOG_HEADER  *Header;
  volatile UINT8                     *Data;

  Base = FixedPcdGet64 (PcdPersistentLogBase);
  Size = FixedPcdGet32 (PcdPersistentLogSize);
  if ((Base == 0) || (Size == 0)) {
    return;
  }

  // Optional, evidence-backed console RAM only. Never discover ramoops or
  // touch record/pmsg/ECC/MDR. BootShim owns initialization and MMU mapping.
  if (((Base & 63U) != 0) || ((Size & 63U) != 0) ||
      (Size <= O1_PERSISTENT_LOG_HEADER_SIZE) ||
      (Base > MAX_UINTN) || ((UINT64)Size - 1 > MAX_UINTN - Base))
  {
    return;
  }

  Header   = (volatile O1_PERSISTENT_LOG_HEADER *)(UINTN)Base;
  Capacity = Size - O1_PERSISTENT_LOG_HEADER_SIZE;
  if ((Header->Signature != O1_PERSISTENT_LOG_MAGIC) ||
      (Header->Start >= Capacity) || (Header->Size > Capacity))
  {
    // No fallback initialization: do not erase early checkpoints or stale data.
    return;
  }

  Data     = (volatile UINT8 *)(UINTN)Header + O1_PERSISTENT_LOG_HEADER_SIZE;
  Retained = (NumberOfBytes > Capacity) ? Capacity : (UINT32)NumberOfBytes;
  Offset   = (UINT32)(((UINT64)Header->Start + ((NumberOfBytes - Retained) % Capacity)) % Capacity);
  First    = MIN (Retained, Capacity - Offset);
  Buffer  += NumberOfBytes - Retained;
  for (Index = 0; Index < First; Index++) {
    Data[Offset + Index] = Buffer[Index];
  }

  for (Index = First; Index < Retained; Index++) {
    Data[Index - First] = Buffer[Index];
  }

  MemoryFence ();
  if (First != 0) {
    WriteBackDataCacheRange ((VOID *)(UINTN)(Data + Offset), First);
  }

  if (Retained > First) {
    WriteBackDataCacheRange ((VOID *)(UINTN)Data, Retained - First);
  }

  ArmDataSynchronizationBarrier ();
  Header->Start = (UINT32)(((UINT64)Header->Start + (NumberOfBytes % Capacity)) % Capacity);
  if (NumberOfBytes >= Capacity - Header->Size) {
    Header->Size = Capacity;
  } else {
    Header->Size += (UINT32)NumberOfBytes;
  }

  MemoryFence ();
  WriteBackDataCacheRange ((VOID *)(UINTN)Header, sizeof (*Header));
  ArmDataSynchronizationBarrier ();
}

STATIC
UINTN
O1WriteRamLog (
  IN CONST UINT8  *Buffer,
  IN UINTN        NumberOfBytes
  )
{
  BOOLEAN                     InterruptsEnabled;
  UINT32                      Capacity;
  UINT32                      Offset;
  UINT32                      NewOffset;
  UINT32                      Retained;
  UINT32                      First;
  UINT32                      Index;
  UINT64                      Total;
  volatile O1_RAM_LOG_HEADER  *Header;
  volatile UINT8              *Data;

  // Only the boot CPU writes this log. Mask IRQs instead of using a lock that
  // could deadlock if an IRQ logs through a different linked library instance.
  InterruptsEnabled = SaveAndDisableInterrupts ();
  Header            = O1GetLogHeader (&Capacity);
  if (Header == NULL) {
    SetInterruptState (InterruptsEnabled);
    return 0;
  }

  Data      = (volatile UINT8 *)(UINTN)Header + O1_RAM_LOG_HEADER_SIZE;
  Retained  = (NumberOfBytes > Capacity) ? Capacity : (UINT32)NumberOfBytes;
  Offset    = (UINT32)(((UINT64)Header->WriteOffset + ((NumberOfBytes - Retained) % Capacity)) % Capacity);
  NewOffset = (UINT32)(((UINT64)Header->WriteOffset + (NumberOfBytes % Capacity)) % Capacity);
  Buffer   += NumberOfBytes - Retained;
  First     = MIN (Retained, Capacity - Offset);

  for (Index = 0; Index < First; Index++) {
    Data[Offset + Index] = Buffer[Index];
  }

  for (Index = First; Index < Retained; Index++) {
    Data[Index - First] = Buffer[Index];
  }

  // Commit the payload before publishing new counters to a physical host dump.
  MemoryFence ();
  if (First != 0) {
    WriteBackDataCacheRange ((VOID *)(UINTN)(Data + Offset), First);
  }

  if (Retained > First) {
    WriteBackDataCacheRange ((VOID *)(UINTN)Data, Retained - First);
  }

  ArmDataSynchronizationBarrier ();
  Header->WriteOffset = NewOffset;
  if (NumberOfBytes >= Capacity - Header->Bytes) {
    Header->Bytes = Capacity;
  } else {
    Header->Bytes += (UINT32)NumberOfBytes;
  }

  Total              = Header->TotalBytes;
  Header->TotalBytes = (NumberOfBytes > MAX_UINT64 - Total) ? MAX_UINT64 : Total + NumberOfBytes;
  O1CommitLogHeader (Header);
  // Keep both rings in the same single-CPU IRQ-masked append transaction.
  O1MirrorPersistentLog (Buffer - (NumberOfBytes - Retained), NumberOfBytes);
  SetInterruptState (InterruptsEnabled);
  return NumberOfBytes;
}

STATIC
VOID
O1MirrorUart (
  IN CONST UINT8  *Buffer,
  IN UINTN        NumberOfBytes
  )
{
  UINT64   Base;
  UINT64   Start;
  UINT64   Ticks;
  UINT64   Frequency;
  UINT32   Lsr;
  UINT32   Poll;
  UINTN    Sent;
  BOOLEAN  InterruptsEnabled;

  // FALSE must be a complete MMIO gate, including initialization and controls.
  if (!FixedPcdGetBool (PcdSerialEnabled)) {
    return;
  }

  Base = FixedPcdGet64 (PcdSerialBase);
  if ((Base == 0) || ((Base & 3U) != 0) || (Base > MAX_UINTN - O1_UART_LSR)) {
    return;
  }

  InterruptsEnabled = SaveAndDisableInterrupts ();
  // Do not accidentally write DLL when the inherited port has DLAB set.
  if ((MmioRead32 ((UINTN)Base + O1_UART_LCR) & O1_UART_DLAB) != 0) {
    SetInterruptState (InterruptsEnabled);
    return;
  }

  Frequency = ArmReadCntFrq () & MAX_UINT32;
  Ticks     = (Frequency * mTimeout + 999999U) / 1000000U;
  Start     = ArmReadCntPct ();
  Sent      = 0;
  // One aggregate deadline and a hard iteration cap, even if CNT stops.
  // UART clock, pinmux, FIFOs, line format, interrupts and CRG are inherited.
  for (Poll = 0; (Poll < O1_UART_MAX_POLLS) && (Sent < NumberOfBytes); Poll++) {
    if ((Ticks != 0) && (ArmReadCntPct () - Start >= Ticks)) {
      break;
    }

    Lsr = MmioRead32 ((UINTN)Base + O1_UART_LSR);
    if (Lsr == MAX_UINT32) {
      break;
    }

    if ((Lsr & O1_UART_THRE) != 0) {
      MmioWrite32 ((UINTN)Base + O1_UART_THR, Buffer[Sent++]);
    } else {
      CpuPause ();
    }
  }

  SetInterruptState (InterruptsEnabled);
}

RETURN_STATUS
EFIAPI
SerialPortInitialize (
  VOID
  )
{
  BOOLEAN        InterruptsEnabled;
  UINT32         Capacity;
  RETURN_STATUS  Status;

  InterruptsEnabled = SaveAndDisableInterrupts ();
  Status            = (O1GetLogHeader (&Capacity) != NULL) ? RETURN_SUCCESS : RETURN_DEVICE_ERROR;
  SetInterruptState (InterruptsEnabled);
  return Status;
}

UINTN
EFIAPI
SerialPortWrite (
  IN UINT8  *Buffer,
  IN UINTN  NumberOfBytes
  )
{
  UINTN  Written;

  if ((Buffer == NULL) || (NumberOfBytes == 0) ||
      (NumberOfBytes - 1 > MAX_UINTN - (UINTN)Buffer))
  {
    return 0;
  }

  Written = O1WriteRamLog (Buffer, NumberOfBytes);
  if (Written != 0) {
    O1MirrorUart (Buffer, Written);
  }

  // Success means acceptance by RAM, not verified physical UART transmission.
  return Written;
}

UINTN
EFIAPI
SerialPortRead (
  OUT UINT8  *Buffer,
  IN  UINTN  NumberOfBytes
  )
{
  return 0;
}

BOOLEAN
EFIAPI
SerialPortPoll (
  VOID
  )
{
  return FALSE;
}

RETURN_STATUS
EFIAPI
SerialPortSetControl (
  IN UINT32  Control
  )
{
  return (Control == 0) ? RETURN_SUCCESS : RETURN_UNSUPPORTED;
}

RETURN_STATUS
EFIAPI
SerialPortGetControl (
  OUT UINT32  *Control
  )
{
  if (Control == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  // The RAM sink is synchronous and has no receive queue; no UART reads here.
  *Control = EFI_SERIAL_INPUT_BUFFER_EMPTY | EFI_SERIAL_OUTPUT_BUFFER_EMPTY;
  return RETURN_SUCCESS;
}

RETURN_STATUS
EFIAPI
SerialPortSetAttributes (
  IN OUT UINT64              *BaudRate,
  IN OUT UINT32              *ReceiveFifoDepth,
  IN OUT UINT32              *Timeout,
  IN OUT EFI_PARITY_TYPE     *Parity,
  IN OUT UINT8               *DataBits,
  IN OUT EFI_STOP_BITS_TYPE  *StopBits
  )
{
  if ((BaudRate == NULL) || (ReceiveFifoDepth == NULL) || (Timeout == NULL) ||
      (Parity == NULL) || (DataBits == NULL) || (StopBits == NULL))
  {
    return RETURN_INVALID_PARAMETER;
  }

  if (((*DataBits != 0) && (*DataBits != 8)) ||
      ((*Parity != DefaultParity) && (*Parity != NoParity)) ||
      ((*StopBits != DefaultStopBits) && (*StopBits != OneStopBit)))
  {
    return RETURN_UNSUPPORTED;
  }

  mTimeout          = (*Timeout == 0) ? O1_UART_DEFAULT_TIMEOUT : MIN (*Timeout, O1_UART_MAX_TIMEOUT);
  *Timeout          = mTimeout;
  *BaudRate         = 0; // Fixed/inherited speed, not a hardware baud assertion.
  *ReceiveFifoDepth = 1;
  *Parity           = NoParity;
  *DataBits         = 8;
  *StopBits         = OneStopBit;
  return RETURN_SUCCESS;
}
