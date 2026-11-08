// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <IOKit/IOCPU.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSSymbol.h>
#include <machine/machine_routines.h>
#include <arm/o1_timer_bridge.h>
#include <pexpert/pexpert.h>

extern "C" void rtclock_intr (unsigned int);

extern "C" {
volatile uint32_t armcpu_registered = 0;
volatile uint32_t armcpu_gic_ready = 0;
volatile uint32_t armcpu_gic_stride = 0;
volatile uint32_t armcpu_gic_boot[8];
volatile uint32_t armcpu_gic_typer_probe[2];
volatile uint32_t armcpu_boot_mpidr = 0;
volatile uint64_t armcpu_timer_interrupts = 0;
volatile uint64_t armcpu_timer_rtclock_exits = 0;
volatile uint64_t armcpu_timer_eoi_completions = 0;
volatile uint64_t armcpu_irq_dispatch_exits = 0;
volatile uint32_t armcpu_irq_registered = 0;
volatile uint32_t armcpu_start_completed = 0;
volatile uint64_t armcpu_spi_unhandled = 0;
volatile uint32_t armcpu_spi_unhandled_id = 0;
}

class ArmCpu : public IOCPU {
  OSDeclareDefaultStructors (ArmCpu)
  OSSharedPtr<IOMemoryMap> distributor;
  OSSharedPtr<IOMemoryMap> redistributors;
  IOCPUInterruptController *controller = nullptr;
  const OSSymbol *cpuName = nullptr;
  uintptr_t dist = 0;
  uintptr_t redist = 0;
  uint64_t affinity = 0;
  bool splitEoi = false;
  bool mapGic (void);
  bool prepareGic (void);
  bool enableGic (void);
  void interrupt (void);
  void completeInterrupt (uint64_t id);
  static void dispatch (void *target, void *, void *, int);
public:
  IOService *probe (IOService *provider, SInt32 *score) override;
  bool start (IOService *provider) override;
  void initCPU (bool boot) override;
  void quiesceCPU (void) override;
  void haltCPU (void) override;
  kern_return_t startCPU (vm_offset_t, vm_offset_t) override;
  void signalCPU (IOCPU *target) override;
  void enableCPUTimeBase (bool enabled) override;
  const OSSymbol *getCPUName (void) override;
};

OSDefineMetaClassAndStructors (ArmCpu, IOCPU)

static const uint32_t kTimerMask = 1U << 27;

static uint32_t read32 (uintptr_t base, unsigned offset)
{
  return *reinterpret_cast<volatile uint32_t *> (base + offset);
}

static void write32 (uintptr_t base, unsigned offset, uint32_t value)
{
  *reinterpret_cast<volatile uint32_t *> (base + offset) = value;
}

static bool waitClear (uintptr_t base, unsigned offset, uint32_t mask)
{
  uint64_t frequency, started, now;
  __asm__ volatile ("mrs %0, cntfrq_el0\nisb\nmrs %1, cntvct_el0" : "=r" (frequency), "=r" (started));
  if (frequency != 38400000) { return false; }
  for (unsigned polls = 0; polls < 1000000; polls++) {
    if ((read32 (base, offset) & mask) == 0) { return true; }
    __asm__ volatile ("isb\nmrs %0, cntvct_el0" : "=r" (now));
    if (now - started > frequency / 10) { return false; }
  }
  return false;
}

IOService *ArmCpu::probe (IOService *provider, SInt32 *score)
{
  OSData *type = OSDynamicCast (OSData, provider->getProperty ("device_type"));
  OSData *reg = OSDynamicCast (OSData, provider->getProperty ("reg"));
  uint32_t physical;
  if (type == nullptr || type->getLength () != 4 ||
      memcmp (type->getBytesNoCopy (), "cpu", 4) != 0 ||
      reg == nullptr || reg->getLength () != sizeof (physical)) { return nullptr; }
  memcpy (&physical, reg->getBytesNoCopy (), sizeof (physical));
  if (ml_get_cpu_number (physical) != ml_get_boot_cpu_number ()) { return nullptr; }
  return IOCPU::probe (provider, score);
}

bool ArmCpu::mapGic (void)
{
  o1_driver_progress (200);
  IORegistryEntry *soc = IORegistryEntry::fromPath ("/arm-io", gIODTPlane);
  IORegistryEntry *gic = IORegistryEntry::fromPath ("/arm-io/intc", gIODTPlane);
  if (soc == nullptr || gic == nullptr) {
    if (soc != nullptr) { soc->release (); }
    if (gic != nullptr) { gic->release (); }
    return false;
  }
  OSData *ranges = OSDynamicCast (OSData, soc->getProperty ("ranges"));
  OSData *reg = OSDynamicCast (OSData, gic->getProperty ("reg"));
  OSData *compatible = OSDynamicCast (OSData, gic->getProperty ("compatible"));
  OSData *stride = OSDynamicCast (OSData, gic->getProperty ("redistributor-stride"));
  uint64_t bus[3], regions[4], increment = 0;
  bool valid = ranges != nullptr && ranges->getLength () == sizeof (bus) &&
               reg != nullptr && reg->getLength () == sizeof (regions) &&
               compatible != nullptr && compatible->getLength () == 11 &&
               memcmp (compatible->getBytesNoCopy (), "arm,gic-v3", 11) == 0 &&
               stride != nullptr && stride->getLength () == sizeof (increment);
  if (valid) {
    memcpy (bus, ranges->getBytesNoCopy (), sizeof (bus));
    memcpy (regions, reg->getBytesNoCopy (), sizeof (regions));
    memcpy (&increment, stride->getBytesNoCopy (), sizeof (increment));
  }
  soc->release ();
  gic->release ();
  if (!valid || increment != 0x40000 || regions[1] != 0x10000 || regions[3] != 0x280000) { return false; }
  o1_driver_progress (201);
  for (unsigned index = 0; index < 4; index += 2) {
    if (regions[index] < bus[0] || regions[index] - bus[0] > bus[2] ||
        regions[index + 1] > bus[2] - (regions[index] - bus[0]) ||
        bus[1] + regions[index] - bus[0] < bus[1]) { return false; }
  }
  if (bus[1] + regions[0] - bus[0] != 0xebc00000 ||
      bus[1] + regions[2] - bus[0] != 0xebc80000) { return false; }
  o1_driver_progress (202);
  OSSharedPtr<IOMemoryDescriptor> distMemory (IOMemoryDescriptor::withPhysicalAddress (
      bus[1] + regions[0] - bus[0], regions[1], kIODirectionInOut), OSNoRetain);
  OSSharedPtr<IOMemoryDescriptor> rdMemory (IOMemoryDescriptor::withPhysicalAddress (
      bus[1] + regions[2] - bus[0], regions[3], kIODirectionInOut), OSNoRetain);
  if (!distMemory || !rdMemory) { return false; }
  distributor = OSSharedPtr<IOMemoryMap> (distMemory->map (kIOMapAnywhere | kIOMapInhibitCache), OSNoRetain);
  redistributors = OSSharedPtr<IOMemoryMap> (rdMemory->map (kIOMapAnywhere | kIOMapInhibitCache), OSNoRetain);
  if (!distributor || !redistributors) { return false; }
  o1_driver_progress (203);
  dist = static_cast<uintptr_t> (distributor->getVirtualAddress ());
  const uint32_t architecture = (read32 (dist, 0xffe8) >> 4) & 15;
  o1_driver_progress (224 + architecture);
  if (architecture != 3 && architecture != 4) { return false; }
  o1_driver_progress (204);
  const uintptr_t frames = static_cast<uintptr_t> (redistributors->getVirtualAddress ());
  const uint32_t target = static_cast<uint32_t> ((affinity & 0xffffff) | ((affinity >> 8) & 0xff000000));
  for (uint64_t offset = 0; offset + 0x20000 <= regions[3]; offset += increment) {
    const uint64_t typer = *reinterpret_cast<volatile uint64_t *> (frames + offset + 8);
    armcpu_gic_typer_probe[0] = static_cast<uint32_t> (offset);
    armcpu_gic_typer_probe[1] = static_cast<uint32_t> (typer);
    if (static_cast<uint32_t> (typer >> 32) == target) { redist = frames + offset; break; }
    if ((typer & 16) != 0) { break; }
  }
  armcpu_gic_stride = static_cast<uint32_t> (increment);
  if (redist == 0) { return false; }
  o1_driver_progress (205);
  return true;
}

bool ArmCpu::prepareGic (void)
{
  if (!mapGic ()) { return false; }
  o1_driver_progress (210);
  if (!waitClear (dist, 0, 1U << 31) || !waitClear (redist, 0, 8)) { return false; }
  o1_driver_progress (211);
  // Group and routing ownership remain with EL3; the NS view can read IGROUPR0 as zero.
  const uint32_t inherited = read32 (dist, 0);
  const uint32_t enable = (inherited & ((1U << 6) | (1U << 4))) != 0 ? 2U : 1U;
  if ((inherited & enable) == 0 || (read32 (redist, 0x14) & 6U) != 0) { return false; }
  o1_driver_progress (212);
  const uintptr_t sgi = redist + 0x10000;
  if ((read32 (sgi, 0xc04) & (1U << 23)) != 0) { return false; }
  o1_driver_progress (213);
  write32 (sgi, 0x180, kTimerMask);
  __asm__ volatile ("dsb sy" ::: "memory");
  if (!waitClear (redist, 0, 8) || (read32 (sgi, 0x100) & kTimerMask) != 0) { return false; }
  o1_driver_progress (214);
  *reinterpret_cast<volatile uint8_t *> (sgi + 0x400 + 27) = 0x80;
  __asm__ volatile ("dsb sy" ::: "memory");
  if (!waitClear (redist, 0, 8) || (read32 (sgi, 0x100) & kTimerMask) != 0) { return false; }
  o1_driver_progress (215);
  return true;
}

bool ArmCpu::enableGic (void)
{
  uint64_t value;
  __asm__ volatile ("mrs %0, icc_sre_el1" : "=r" (value));
  value |= 1;
  __asm__ volatile ("msr icc_sre_el1, %0\nisb\nmrs %0, icc_sre_el1" : "+r" (value) :: "memory");
  if ((value & 1) == 0) { return false; }
  __asm__ volatile ("mrs %0, icc_ctlr_el1" : "=r" (value));
  splitEoi = (value & 2U) != 0;
  value = 0xff;
  __asm__ volatile ("msr icc_pmr_el1, %0\nisb\nmrs %0, icc_pmr_el1" : "+r" (value) :: "memory");
  const uint8_t priority = *reinterpret_cast<volatile uint8_t *> (redist + 0x10000 + 0x400 + 27);
  if (priority >= (value & 255)) { return false; }
  write32 (redist + 0x10000, 0x100, kTimerMask);
  __asm__ volatile ("dsb sy" ::: "memory");
  if (!waitClear (redist, 0, 8) || (read32 (redist + 0x10000, 0x100) & kTimerMask) == 0) { return false; }
  value = 1;
  __asm__ volatile ("msr icc_igrpen1_el1, %0\ndsb sy\nisb\nmrs %0, icc_igrpen1_el1" : "+r" (value) :: "memory");
  if ((value & 1) == 0) { return false; }
  armcpu_gic_boot[0] = read32 (redist + 0x10000, 0x80);
  armcpu_gic_boot[1] = read32 (redist + 0x10000, 0x100);
  armcpu_gic_boot[2] = read32 (dist, 0);
  armcpu_gic_boot[3] = read32 (dist, 4);
  armcpu_gic_boot[4] = read32 (redist + 0x10000, 0xd00);
  __asm__ volatile ("mrs %0, icc_pmr_el1" : "=r" (value));
  armcpu_gic_boot[5] = static_cast<uint32_t> (value);
  __asm__ volatile ("mrs %0, icc_igrpen1_el1" : "=r" (value));
  armcpu_gic_boot[6] = static_cast<uint32_t> (value);
  __asm__ volatile ("mrs %0, icc_sre_el1" : "=r" (value));
  armcpu_gic_boot[7] = static_cast<uint32_t> (value);
  return true;
}

bool ArmCpu::start (IOService *provider)
{
  unsigned smp = 0, spi = 0, self = 0, remote = 0;
  PE_parse_boot_argn ("sheng-smp", &smp, sizeof (smp));
  PE_parse_boot_argn ("armcpu-spi-ipi", &spi, sizeof (spi));
  PE_parse_boot_argn ("armcpu-self-ipi", &self, sizeof (self));
  PE_parse_boot_argn ("sheng-smp-gic", &remote, sizeof (remote));
  if (smp || spi || self || remote || ml_get_max_cpu_number () != 0) { return false; }
  OSData *reg = OSDynamicCast (OSData, provider->getProperty ("reg"));
  uint32_t physical;
  if (reg == nullptr || reg->getLength () != sizeof (physical)) { return false; }
  memcpy (&physical, reg->getBytesNoCopy (), sizeof (physical));
  __asm__ volatile ("mrs %0, mpidr_el1" : "=r" (affinity));
  armcpu_boot_mpidr = static_cast<uint32_t> (affinity);
  if ((affinity & 0xff00ffffffULL) != physical) { return false; }
  o1_matching_checkpoint ();
  o1_driver_progress (93);
  enableCPUTimeBase (false);
  if (!prepareGic ()) { return false; }
  o1_driver_progress (94);
  if (!IOCPU::start (provider)) { return false; }
  cpuName = OSSymbol::withCStringNoCopy ("XRING O1 architectural BSP");
  if (cpuName == nullptr) { return false; }
  ml_processor_info_t info = {};
  info.cpu_id = this;
  info.phys_id = physical;
  info.log_id = 0;
  info.cluster_id = static_cast<uint32_t> (ml_get_cluster_number (physical));
  info.cluster_type = ml_get_boot_cluster_type ();
  perfmon_interrupt_handler_func pmi = nullptr;
  o1_driver_progress (95);
  kern_return_t status = ml_processor_register (&info, &machProcessor, &ipi_handler, &pmi);
  if (status != KERN_SUCCESS) { panic ("ArmCpu: O1 BSP registration failed: %d", status); }
  setCPUNumber (0);
  armcpu_registered = 1;
  o1_driver_progress (96);
  controller = new IOCPUInterruptController;
  if (controller == nullptr || controller->initCPUInterruptController (1, 1) != kIOReturnSuccess) {
    panic ("ArmCpu: O1 CPU interrupt controller allocation failed");
  }
  o1_driver_progress (97);
  controller->registerCPUInterruptController ();
  controller->enableCPUInterrupt (this);
  o1_driver_progress (98);
  if (controller->registerInterrupt (provider, 0, this, &ArmCpu::dispatch, nullptr) != kIOReturnSuccess) {
    panic ("ArmCpu: O1 CPU IRQ registration failed");
  }
  armcpu_irq_registered = 1;
  o1_driver_progress (99);
  initCPU (true);
  registerService ();
  armcpu_start_completed = 1;
  return true;
}

void ArmCpu::initCPU (bool boot)
{
  if (!boot || armcpu_registered == 0 || armcpu_irq_registered == 0 || !o1_timer_handler_ready ()) {
    panic ("ArmCpu: O1 BSP handler must be installed before timer initialization");
  }
  setCPUState (kIOCPUStateRunning);
  const boolean_t irqState = ml_set_interrupts_enabled (FALSE);
  enableCPUTimeBase (false);
  if (!enableGic ()) {
    quiesceCPU ();
    panic ("ArmCpu: O1 BSP timer initialization failed");
  }
  if (!o1_timer_resync ()) {
    quiesceCPU ();
    panic ("ArmCpu: O1 BSP timer deadline resynchronization failed");
  }
  enableCPUTimeBase (true);
  armcpu_gic_ready = 1;
  ml_set_interrupts_enabled (irqState);
}

void ArmCpu::dispatch (void *target, void *, void *, int)
{
  static_cast<ArmCpu *> (target)->interrupt ();
}

void ArmCpu::completeInterrupt (uint64_t id)
{
  __asm__ volatile ("msr icc_eoir1_el1, %0\nisb" :: "r" (id) : "memory");
  if (splitEoi) { __asm__ volatile ("msr icc_dir_el1, %0\nisb" :: "r" (id) : "memory"); }
}

void ArmCpu::interrupt (void)
{
  for (unsigned drained = 0; drained < 128; drained++) {
    uint64_t id;
    __asm__ volatile ("mrs %0, icc_iar1_el1" : "=r" (id));
    const uint32_t intid = static_cast<uint32_t> (id & 0xffffff);
    if (intid >= 1020) {
      armcpu_irq_dispatch_exits = armcpu_irq_dispatch_exits + 1;
      return;
    }
    if (intid != 27) {
      armcpu_spi_unhandled = armcpu_spi_unhandled + 1;
      armcpu_spi_unhandled_id = intid;
      completeInterrupt (id);
      quiesceCPU ();
      panic ("ArmCpu: O1 unowned interrupt %u; local interface stopped", intid);
    }
    armcpu_timer_interrupts = armcpu_timer_interrupts + 1;
    rtclock_intr (0);
    armcpu_timer_rtclock_exits = armcpu_timer_rtclock_exits + 1;
    completeInterrupt (id);
    armcpu_timer_eoi_completions = armcpu_timer_eoi_completions + 1;
  }
  quiesceCPU ();
  panic ("ArmCpu: O1 timer interrupt drain exceeded its bound");
}

void ArmCpu::signalCPU (IOCPU *)
{
  panic ("ArmCpu: O1 interprocessor signaling is unsupported");
}

void ArmCpu::enableCPUTimeBase (bool enabled)
{
  uint64_t value = enabled ? 1 : 2;
  __asm__ volatile ("msr cntv_ctl_el0, %0\nisb" :: "r" (value) : "memory");
}

void ArmCpu::quiesceCPU (void)
{
  enableCPUTimeBase (false);
  uint64_t value = 0;
  __asm__ volatile ("msr icc_igrpen1_el1, %0\ndsb sy\nisb" :: "r" (value) : "memory");
}

void ArmCpu::haltCPU (void)
{
  quiesceCPU ();
  setCPUState (kIOCPUStateStopped);
  __asm__ volatile ("msr daifset, #15" ::: "memory");
  for (;;) { __asm__ volatile ("wfi"); }
}

kern_return_t ArmCpu::startCPU (vm_offset_t, vm_offset_t)
{
  return KERN_NOT_SUPPORTED;
}

const OSSymbol *ArmCpu::getCPUName (void)
{
  return cpuName;
}

extern "C" IOReturn armcpu_spi_attach (uint32_t, bool, void (*) (void *), void *, uint32_t *, uint64_t *)
{
  return kIOReturnUnsupported;
}

extern "C" uint64_t armcpu_spi_count (uint32_t)
{
  return 0;
}
