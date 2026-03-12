#pragma once
#include <cstdint>
#include <drivers/disk.hpp>
#include <generic/hhdm.hpp>
#include <generic/lock/spinlock.hpp>
#include <generic/arch.hpp>

#define NVME_ORANGE_TRACE 

#define NVME_REG_CAP 0x00 // Controller Capabilities
#define NVME_REG_VS 0x08 // Version
#define NVME_REG_CC 0x14 // Controller Configuration
#define NVME_REG_CSTS 0x1C // Controller Status
#define NVME_REG_AQA 0x24 // Admin Queue Attributes
#define NVME_REG_ASQ 0x28 // Admin Submission Queue Base Address
#define NVME_REG_ACQ 0x30 // Admin Completion Queue Base Address
#define NVME_CAP_MQES_MASK 0xFFFF // Maximum Queue Entries Supported
#define NVME_CAP_CQR (1ULL << 16) // Contiguous Queues Required
#define NVME_CAP_DSTRD_MASK (0xFULL << 32) // Doorbell Stride
#define NVME_CAP_CSS_MASK (0xFFULL << 37) // Command Sets Supported
#define NVME_CAP_MPSMIN_MASK (0xFULL << 48) // Memory Page Size Minimum
#define NVME_CAP_MPSMAX_MASK (0xFULL << 52) // Memory Page Size Maximum
#define NVME_CC_EN (1 << 0) // Enable
#define NVME_CC_CSS_NVM (0 << 4) // NVM Command Set
#define NVME_CC_SHN_NORMAL (1 << 14) // Normal shutdown
#define NVME_CSTS_RDY (1 << 0) // Ready
#define NVME_CSTS_CFS (1 << 1) // Controller Fatal Status
#define NVME_MAX_IO_QUEUES	(65535)
#define NVME_MAX_QUEUE_SIZE 64
#define NVME_RESET_TIMEOUT_MS 30000
#define NVME_ENABLE_TIMEOUT_MS 30000

struct nvme_lbaf {
    std::uint16_t ms;
    std::uint8_t  ds;   
    std::uint8_t  rp;   
};

struct nvme_command {
    uint32_t cdw0;     
    uint32_t nsid;   
    uint32_t reserved[2];
    uint64_t metadata;
    uint64_t prp1;     
    uint64_t prp2;
    uint32_t cdw10;    
    uint32_t cdw11_15[5];
};

struct nvme_completion {
    uint32_t result;
    uint32_t reserved;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t command_id;
    uint16_t status;    
};

struct nvme_id_ns {
    std::uint64_t nsze;    // Namespace Size
    std::uint64_t ncap;    // Namespace Capacity
    std::uint64_t nuse;    // Namespace Utilization
    std::uint8_t  nsfeat;  // Namespace Features
    std::uint8_t  nlbaf;   // Number of LBA Formats
    std::uint8_t  flbas;   // Formatted LBA Size
    std::uint8_t  mc;      // Metadata Capabilities
    std::uint8_t  dpc;     // End-to-end Data Protection Capabilities
    std::uint8_t  dps;     // End-to-end Data Protection Type Settings
    std::uint8_t  nmic;    // Namespace Multi-path I/O and Namespace Sharing Capabilities
    std::uint8_t  rescap;  // Reservation Capabilities
    std::uint8_t  fpi;     // Format Progress Indicator
    std::uint8_t  dlfeat;  // Deallocate Logical Block Features
    std::uint16_t nawun;   // Namespace Atomic Write Unit Normal
    std::uint16_t nawupf;  // Namespace Atomic Write Unit Power Fail
    std::uint16_t nacwu;   // Namespace Atomic Compare & Write Unit
    std::uint16_t nabsn;   // Namespace Atomic Boundary Size Normal
    std::uint16_t nabo;    // Namespace Atomic Boundary Offset
    std::uint16_t nabspf;  // Namespace Atomic Boundary Size Power Fail
    std::uint16_t noiob;   // Namespace Optimal I/O Boundary
    std::uint8_t  nvmcap[16]; // NVM Capacity
    std::uint16_t npwg;    // Namespace Preferred Write Granularity
    std::uint16_t npwa;    // Namespace Preferred Write Alignment
    std::uint16_t npdg;    // Namespace Preferred Deallocate Granularity
    std::uint16_t npda;    // Namespace Preferred Deallocate Alignment
    std::uint16_t nows;    // Namespace Optimal Write Size
    std::uint16_t mssrl;   // Maximum Single Source Range Length
    std::uint32_t mcl;     // Maximum Copy Length
    std::uint8_t  msrc;    // Maximum Source Range Count
    std::uint8_t  rsvd81[11]; 
    std::uint32_t anagrpid; // ANA Group Identifier
    std::uint8_t  rsvd96[3];
    std::uint8_t  nsattr;  // Namespace Attributes
    std::uint16_t nvmsetid; // NVM Set Identifier
    std::uint16_t endgid;   // Endurance Group Identifier
    std::uint8_t  nguid[16]; // Namespace Globally Unique Identifier
    std::uint8_t  eui64[8];  // IEEE Extended Unique Identifier
    nvme_lbaf      lbaf[16];  // LBA Format Support
    std::uint8_t  rsvd192[192];
    std::uint8_t  vs[3712];  // Vendor Specific
};

static_assert(sizeof(nvme_id_ns) == 4096, "nvme_id_ns must be exactly 4096 bytes");

struct nvme_queue {
    std::uint64_t address;
    std::size_t size;
};

struct nvme_namespace {
    uint32_t nsid; 
    uint64_t size;
    uint32_t lba_size; 
    uint16_t lba_shift; 
    bool valid; 
    struct nvme_id_ns* ns_data; 
};

struct nvme_pair_queue {
    nvme_queue sq;
    nvme_queue cq;
    std::uint64_t sq_tail;
    std::uint64_t cq_head;
    std::uint8_t phase;
    std::uint8_t qid;
    volatile std::uint32_t* sq_doorbell;
    volatile std::uint32_t* cq_doorbell;
    locks::spinlock lock;
};

struct nvme_controller {
    void* bar0; 
    uint32_t stride; 
    uint32_t page_size;
    uint16_t max_queue_entries; 
    nvme_queue admin_sq;
    nvme_queue admin_cq;
    std::uint64_t admin_sq_tail;
    std::uint64_t admin_cq_head;
    std::uint8_t admin_phase;
    nvme_pair_queue* main_io_queue;
    nvme_namespace namespaces[256]; 
    uint32_t num_namespaces; 
    uint8_t mpsmin;
    void* identify;
};

struct nvme_arg_disk {
    nvme_controller* ctrl;
    std::uint32_t nsid;
};

namespace drivers {
    namespace nvme {

        static inline std::uint32_t read32(struct nvme_controller* ctrl, std::uint32_t offset) {
            volatile std::uint32_t* reg = (volatile std::uint32_t*)((std::uint8_t*)ctrl->bar0 + offset);
            std::uint32_t value = *reg;
            arch::memory_barrier();
            return value;
        }

        static inline void write32(struct nvme_controller* ctrl, std::uint32_t offset, std::uint32_t value) {
            volatile std::uint32_t* reg = (volatile std::uint32_t*)((std::uint8_t*)ctrl->bar0 + offset);
            *reg = value;
            arch::memory_barrier();
        }

        static inline std::uint64_t read64(struct nvme_controller* ctrl, uint32_t offset) {
            volatile std::uint64_t* reg = (volatile std::uint64_t*)((std::uint8_t*)ctrl->bar0 + offset);
            std::uint64_t value = *reg;
            arch::memory_barrier();
            return value;
        }

        static inline void write64(struct nvme_controller* ctrl, std::uint32_t offset, std::uint32_t value) {
            volatile std::uint64_t* reg = (volatile std::uint64_t*)((std::uint8_t*)ctrl->bar0 + offset);
            *reg = value;
            arch::memory_barrier();
        }

        void init();     
        void disable();
    };
};