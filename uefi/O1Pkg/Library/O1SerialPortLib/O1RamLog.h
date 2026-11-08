/** @file
  Fixed little-endian ABI for the self-owned O1 RAM log.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef O1_RAM_LOG_H_
#define O1_RAM_LOG_H_

#include <Base.h>

#define O1_RAM_LOG_MAGIC        0x474F4C4D4152314FULL // "O1RAMLOG"
#define O1_RAM_LOG_VERSION      1U
#define O1_RAM_LOG_HEADER_SIZE  32U
#define O1_PERSISTENT_LOG_MAGIC        0x43474244U // Linux persistent_ram "DBGC"
#define O1_PERSISTENT_LOG_HEADER_SIZE  12U

// Linux console ramoops ABI. Only the separately authorized BootShim may
// initialize it; the port library appends only when this existing header is valid.
typedef struct {
  UINT32    Signature;
  UINT32    Start;
  UINT32    Size;
} O1_PERSISTENT_LOG_HEADER;

STATIC_ASSERT (sizeof (O1_PERSISTENT_LOG_HEADER) == O1_PERSISTENT_LOG_HEADER_SIZE, "Linux console log ABI");

// Payload capacity is PcdLogSize - HeaderSize. WriteOffset names the next
// payload byte; Bytes saturates at capacity. Oldest byte is
// (WriteOffset + capacity - Bytes) % capacity. TotalBytes saturates at UINT64_MAX.
// This is ordinary reserved RAM, not persistent storage or a reset guarantee.
typedef struct {
  UINT64    Magic;
  UINT32    Version;
  UINT32    HeaderSize;
  UINT32    WriteOffset;
  UINT32    Bytes;
  UINT64    TotalBytes;
} O1_RAM_LOG_HEADER;

STATIC_ASSERT (sizeof (O1_RAM_LOG_HEADER) == O1_RAM_LOG_HEADER_SIZE, "O1 log ABI size");
STATIC_ASSERT (OFFSET_OF (O1_RAM_LOG_HEADER, TotalBytes) == 24, "O1 log ABI layout");

#endif
