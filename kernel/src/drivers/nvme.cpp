#include <cstdint>
#include <drivers/disk.hpp>
#include <drivers/nvme.hpp>
#if defined(__x86_64__)
#include <arch/x86_64/drivers/pci.hpp>
#endif
#include <utils/assert.hpp>
#include <generic/paging.hpp>
#include <utils/gobject.hpp>
#include <generic/hhdm.hpp>
#include <generic/pmm.hpp>
#include <generic/arch.hpp>
#include <klibc/stdio.hpp>
#include <utils/align.hpp>
#include <generic/time.hpp>

using namespace drivers;

nvme_controller* nvme_controllers[256];
int nvme_controller_ptr = 0;

void nvme::disable() {
    for(std::int32_t i = 0;i < nvme_controller_ptr; i++) {
        nvme_controller* ctrl = nvme_controllers[i];
        std::uint32_t cc = nvme::read32(ctrl, NVME_REG_CC);
        cc &= ~NVME_CC_EN;
        cc |= NVME_CC_SHN_NORMAL;
        nvme::write32(ctrl, NVME_REG_CC, cc);
        while(read32(ctrl, NVME_REG_CSTS) & 1) arch::pause();
    }
}

void nvme_submit_admin(nvme_controller* ctrl, nvme_command& cmd) {
    nvme_command* sq = (nvme_command*)ctrl->admin_sq.address;
    
#ifdef NVME_ORANGE_TRACE
    klibc::printf("NVME Trace: Submitting admin command to controller 0x%p with opcode %d\r\n",ctrl, cmd.cdw0);
#endif

    klibc::memcpy((void*)(((std::uint64_t)&sq[ctrl->admin_sq_tail]) + etc::hhdm()), &cmd, sizeof(nvme_command));
    
    ctrl->admin_sq_tail++;
    if (ctrl->admin_sq_tail > 63) ctrl->admin_sq_tail = 0;

    arch::memory_barrier();

    uint32_t* sq_db = (uint32_t*)((uint8_t*)ctrl->bar0 + 0x1000);
    *sq_db = ctrl->admin_sq_tail;
    arch::memory_barrier();
}

bool nvme_wait_admin(nvme_controller* ctrl, uint64_t timeout_ms) {
    volatile nvme_completion* cq = (volatile nvme_completion*)(ctrl->admin_cq.address + etc::hhdm());
    uint64_t elapsed = 0;

#ifdef NVME_ORANGE_TRACE
    klibc::printf("NVME Trace: Waiting admin command on controller 0x%p with timeout %lli\r\n",ctrl, timeout_ms);
#endif

    while (elapsed < timeout_ms * 1000) {
        arch::memory_barrier();

        if ((cq[ctrl->admin_cq_head].status & 0x1) == ctrl->admin_phase) {
            
            uint16_t status_code = (cq[ctrl->admin_cq_head].status >> 1) & 0xFF;

            ctrl->admin_cq_head++;
            if (ctrl->admin_cq_head > 63) {
                ctrl->admin_cq_head = 0;
                ctrl->admin_phase ^= 1;
            }

            uint32_t* cq_db = (uint32_t*)((uint8_t*)ctrl->bar0 + 0x1000 + (1 * ctrl->stride));
            *cq_db = ctrl->admin_cq_head;
            arch::memory_barrier();

#ifdef NVME_ORANGE_TRACE
            klibc::printf("NVME Trace: Got status %d\r\n",status_code);
#endif
            
            if(status_code != 0)
                return false;

            return true;
        }

        time::timer->sleep(10); 
        elapsed += 10;
    }

    klibc::printf("NVME: Timeout !\r\n");
    return false; 
}

bool nvme_send_io(nvme_pair_queue* queue, nvme_command* cmd) {
    nvme_command* sq = (nvme_command*)queue->sq.address;

#ifdef NVME_ORANGE_TRACE
    klibc::printf("NVME Trace: Submitting io command to queue 0x%p with opcode %d\r\n",queue, cmd->cdw0);
#endif

    klibc::memcpy((void*)(((std::uint64_t)&sq[queue->sq_tail]) + etc::hhdm()), cmd, sizeof(nvme_command));
    
    queue->sq_tail++;
    if (queue->sq_tail > 1) queue->sq_tail = 0;

    arch::memory_barrier();

    *queue->sq_doorbell = queue->sq_tail;
    arch::memory_barrier();

    return true;
}

bool nvme_wait_io(nvme_pair_queue* queue, uint64_t timeout_ms) {
    volatile nvme_completion* cq = (volatile nvme_completion*)(queue->cq.address + etc::hhdm());
    uint64_t elapsed = 0;

    while (elapsed < timeout_ms * 1000) {
        arch::memory_barrier();

        if ((cq[queue->cq_head].status & 0x1) == queue->phase) {
            
            uint16_t status_code = (cq[queue->cq_head].status >> 1) & 0xFF;

            queue->cq_head++;
            if (queue->cq_head > 1) {
                queue->cq_head = 0;
                queue->phase ^= 1;
            }

            *queue->cq_doorbell = queue->cq_head;
            arch::memory_barrier();
            
#ifdef NVME_ORANGE_TRACE
            klibc::printf("NVME Trace: Got status %d\r\n",status_code);
#endif

            if(status_code != 0)
                return false;

            return true;
        }

        time::timer->sleep(10); 
        elapsed += 10;
    }

    klibc::printf("NVME: Timeout !\r\n");
    return false; 
}

bool nvme_send_io_cmd(nvme_pair_queue* queue, nvme_command* cmd) {
    queue->lock.lock();
    nvme_send_io(queue,cmd);
    bool status = nvme_wait_io(queue, 1000);
    queue->lock.unlock();
    return status;
}

bool nvme_identify_namespace(struct nvme_controller* ctrl, uint32_t nsid) {
    struct nvme_namespace* ns = &ctrl->namespaces[nsid - 1];
    ns->ns_data = (nvme_id_ns*)(pmm::freelist::alloc_4k() + etc::hhdm());

    nvme_command cmd;
    klibc::memset(&cmd,0,sizeof(nvme_command));
    cmd.cdw0 = 0x06; 
    cmd.nsid = nsid;
    cmd.cdw10 = 0x0; 
    cmd.prp1 = (std::uint64_t)ns->ns_data - etc::hhdm();

    nvme_submit_admin(ctrl, cmd);
    bool result = nvme_wait_admin(ctrl, 1000);

    if(result == false)
        return false;

    ns->nsid = nsid;
    ns->size = ns->ns_data->nsze;

    uint8_t lba_format = ns->ns_data->flbas & 0xF;
    if (lba_format < ns->ns_data->nlbaf) {
        ns->lba_size = 1 << ns->ns_data->lbaf[lba_format].ds;
    } else {
        ns->lba_size = 512; 
    }
    ns->valid = true;

    klibc::printf("NVME: Detected namespace %d with lba_size %lli bytes and total size %lli bytes\r\n", nsid, ns->lba_size, ns->size * ns->lba_size);
    return true;
}

bool allocate_prp2_list(std::size_t num_pages, void** out) {
    if(num_pages > 1) {
        *out = (void*)(pmm::buddy::alloc(num_pages * PAGE_SIZE).phys + etc::hhdm());
        return true;
    } else {
        *out = (void*)(pmm::freelist::alloc_4k() + etc::hhdm());
        return false;
    }
    assert(0,"wtf");
    return false;
}

bool nvme_write(void* arg, char* buffer, std::uint64_t lba, std::size_t len_in_blocks) {
    nvme_arg_disk* disk = (nvme_arg_disk*)arg;
    nvme_command cmd;
    klibc::memset(&cmd, 0, sizeof(nvme_command));

    cmd.cdw0 = 1;
    cmd.nsid = disk->nsid;
    cmd.cdw10 =  (uint32_t)(lba & 0xFFFFFFFF);
    cmd.cdw11_15[0] = (uint32_t)(lba >> 32);
    cmd.cdw11_15[1] = (len_in_blocks - 1) & 0xFFFF;

    std::size_t lba_size = disk->ctrl->namespaces[disk->nsid - 1].lba_size;

    assert(((std::uint64_t)buffer & (PAGE_SIZE - 1)) == 0, "Unaligned buffer\r\n");

    std::size_t blocks = len_in_blocks;
    std::size_t num_pages = ((blocks * lba_size) + PAGE_SIZE - 1) / PAGE_SIZE;
    std::uint64_t current_root = arch::current_root();

    cmd.prp1 = arch::get_phys_from_page(current_root,(std::uintptr_t)buffer);

    std::uint64_t* list = nullptr;
    bool is_buddy = false;

#ifdef NVME_ORANGE_TRACE
    klibc::printf("NVME Trace: Trying to write lba %lli seek %lli num_pages %lli len %lli buffer 0x%p arg 0x%p controller 0x%p nsid %d\r\n", lba, lba, num_pages, len_in_blocks * lba_size, buffer, arg, disk->ctrl, disk->nsid);
#endif

    if(num_pages == 1) {
        cmd.prp2 = 0;
    } else if(num_pages == 2) {
        cmd.prp2 = arch::get_phys_from_page(current_root,(std::uintptr_t)buffer);
    } else {
        is_buddy = allocate_prp2_list(num_pages, (void**)&list);
        for (std::size_t i = 1; i < num_pages; i++) {
            list[i - 1] = arch::get_phys_from_page(current_root,(std::uint64_t)buffer + (i * PAGE_SIZE));
        }
        cmd.prp2 = (std::uint64_t)list - etc::hhdm();
    }

    bool status = nvme_send_io_cmd(disk->ctrl->main_io_queue, &cmd);

    if(list) {
        if(is_buddy) pmm::buddy::free((std::uint64_t)list - etc::hhdm());
        else pmm::freelist::free((std::uint64_t)list - etc::hhdm());
    }

    return status;
}

bool nvme_read(void* arg, char* buffer, std::uint64_t lba, std::size_t len_in_blocks) {
    nvme_arg_disk* disk = (nvme_arg_disk*)arg;
    nvme_command cmd;
    klibc::memset(&cmd, 0, sizeof(nvme_command));

    cmd.cdw0 = 2;
    cmd.nsid = disk->nsid;
    cmd.cdw10 = (uint32_t)(lba & 0xFFFFFFFF); 
    cmd.cdw11_15[0] = (uint32_t)(lba >> 32);
    cmd.cdw11_15[1] = (len_in_blocks - 1); 

    std::size_t lba_size = disk->ctrl->namespaces[disk->nsid - 1].lba_size;

#ifdef NVME_ORANGE_TRACE
    klibc::printf("NVME Trace: Reading with buffer 0x%p 0x%p %d\r\n",buffer, (std::uint64_t)buffer & PAGE_SIZE, ((std::uint64_t)buffer & PAGE_SIZE) == 0);
#endif
    assert(((std::uint64_t)buffer & (PAGE_SIZE - 1)) == 0, "Unaligned buffer\r\n");

    std::size_t blocks = len_in_blocks;
    std::size_t num_pages = ((blocks * lba_size) + PAGE_SIZE - 1) / PAGE_SIZE;
    std::uint64_t current_root = arch::current_root();

    cmd.prp1 = arch::get_phys_from_page(current_root,(std::uintptr_t)buffer);

    std::uint64_t* list = nullptr;
    bool is_buddy = false;

#ifdef NVME_ORANGE_TRACE
    klibc::printf("NVME Trace: Trying to read lba %lli seek %lli num_pages %lli len %lli buffer 0x%p arg 0x%p controller 0x%p nsid %d\r\n", lba, lba, num_pages, len_in_blocks * lba_size, buffer, arg, disk->ctrl, disk->nsid);
#endif

    if(num_pages == 1) {
        cmd.prp2 = 0;
    } else if(num_pages == 2) {
        cmd.prp2 = arch::get_phys_from_page(current_root,(std::uintptr_t)buffer);
    } else {
        is_buddy = allocate_prp2_list(num_pages, (void**)&list);
        for (std::size_t i = 1; i < num_pages; i++) {
            list[i - 1] = arch::get_phys_from_page(current_root,(std::uint64_t)buffer + (i * PAGE_SIZE));
        }
        cmd.prp2 = (std::uint64_t)list - etc::hhdm();
    }

    bool status = nvme_send_io_cmd(disk->ctrl->main_io_queue, &cmd);

    if(list) {
        if(is_buddy) pmm::buddy::free((std::uint64_t)list - etc::hhdm());
        else pmm::freelist::free((std::uint64_t)list - etc::hhdm());
    }

    return status;
}

// static inline int isprint(int c) {
//     return (c >= 0x20 && c <= 0x7E);
// }


// static inline void print_buffer(const unsigned char *buffer, std::size_t size) {
//     for (std::size_t i = 0; i < size; i++) {
//         if (isprint(buffer[i])) {
//             klibc::printf("%c ", buffer[i]);
//         } else {
//             klibc::printf("0x%02X ", buffer[i]);
//         }
//     }
//     klibc::printf("\r\n");
// }

void nvme_init_namespace(nvme_controller* ctrl, std::uint32_t nsid) {
    nvme_arg_disk* new_disk = new nvme_arg_disk;
    new_disk->ctrl = ctrl;
    new_disk->nsid = nsid;

    disk* generic_disk = new disk;
    generic_disk->arg = new_disk;
    generic_disk->lba_size = ctrl->namespaces[new_disk->nsid - 1].lba_size;
    generic_disk->read = nvme_read;
    generic_disk->write = nvme_write;

    drivers::init_disk(generic_disk);
}

bool nvme_init_namespaces(nvme_controller* ctrl) {
    void* ns_list = (void*)(pmm::freelist::alloc_4k() + etc::hhdm());

    nvme_command cmd;
    klibc::memset(&cmd,0,sizeof(nvme_command));
    cmd.cdw0 = 0x06; 
    cmd.cdw10 = 0x2; 
    cmd.prp1 = (std::uint64_t)ns_list - etc::hhdm();

    nvme_submit_admin(ctrl, cmd);
    bool result = nvme_wait_admin(ctrl, 1000);

    if(result == false)
        return false;

    uint32_t* ns_ids = (uint32_t*)ns_list;
    for (int i = 0; i < 1024; i++) {
        uint32_t nsid = ns_ids[i];
        if (nsid == 0) break; // End of list

        result = nvme_identify_namespace(ctrl, nsid);
        if(result == true) {
            nvme_init_namespace(ctrl, nsid);
            ctrl->num_namespaces++;
        }
    }
    
    return true;
}

void nvme_alloc_queue(nvme_queue* queue) {
    queue->address = pmm::freelist::alloc_4k();
    queue->size = 2;
}

std::uint8_t nvme_qid_ptr = 1;

nvme_pair_queue* nvme_create_io_queue(nvme_controller* ctrl) {
    nvme_pair_queue* queue = new nvme_pair_queue;
    nvme_alloc_queue(&queue->cq);
    nvme_alloc_queue(&queue->sq);
    queue->phase = 1;
    queue->qid = nvme_qid_ptr;
    queue->lock.unlock();
    
    nvme_command cmd;
    klibc::memset(&cmd, 0, sizeof(nvme_command));

    cmd.cdw0 = 0x05;
    cmd.cdw10 = ((uint32_t)(1)) | (((uint32_t)queue->qid) << 16);
    cmd.cdw11_15[0] = 1;
    cmd.prp1 = queue->cq.address;

    nvme_submit_admin(ctrl, cmd);
    bool status = nvme_wait_admin(ctrl, 1000);

    if(status == false)
        return nullptr;

    klibc::memset(&cmd, 0, sizeof(nvme_command));

    cmd.cdw0 = 0x01;
    cmd.cdw10 = ((uint32_t)(1)) | (((uint32_t)queue->qid) << 16);
    cmd.cdw11_15[0] = (((uint32_t)queue->qid) << 16) | 0x1;
    cmd.prp1 = queue->sq.address;

    nvme_submit_admin(ctrl, cmd);
    status = nvme_wait_admin(ctrl, 1000);

    if(status == false)
        return nullptr;

    uint8_t* doorbell_base = (uint8_t*)ctrl->bar0 + 0x1000;
    uint32_t stride_bytes = ctrl->stride;
    queue->sq_doorbell = (uint32_t*)(doorbell_base + ((2 * queue->qid) * stride_bytes));
    queue->cq_doorbell = (uint32_t*)(doorbell_base + ((2 * queue->qid + 1) * stride_bytes));
    nvme_qid_ptr++;

    return queue;
}

bool nvme_identify(nvme_controller* ctrl) {
    uint64_t phys_buffer = pmm::freelist::alloc_4k();
    
#ifdef NVME_ORANGE_TRACE
    klibc::printf("NVME Trace: Trying to identify controller 0x%p\r\n",ctrl);
#endif

    nvme_command cmd = {};
    cmd.cdw0 = 0x06;
    cmd.prp1 = phys_buffer;
    cmd.cdw10 = 1;
    cmd.nsid = 0;

    nvme_submit_admin(ctrl, cmd);
    bool status = nvme_wait_admin(ctrl, 1000);
    if(status == false)
        return false;
    arch::memory_barrier();

    char model[41];
    klibc::memcpy(model, (void*)(phys_buffer + etc::hhdm() + 24), 40);
    model[40] = '\0';
    klibc::printf("NVME: Controller Model: %s\r\n", model);

    ctrl->identify = (void*)(phys_buffer + etc::hhdm());

    return true;
}

void nvme_init(std::uint64_t base) {

    if(!time::timer) {
        klibc::printf("NVME: Can't init without timer !\r\n");
        return;
    }

    paging::map_range(gobject::kernel_root, base, base + etc::hhdm(), PAGE_SIZE * 4, PAGING_PRESENT | PAGING_RW | PAGING_NC);

    nvme_controller* controller = new nvme_controller;
    controller->bar0 = (void*)(base + etc::hhdm());
    std::uint64_t cap = nvme::read64(controller, NVME_REG_CAP);
    controller->max_queue_entries = (cap & 0xffff) + 1;
    controller->stride = 4 << ((cap >> 32) & 0xf);
    controller->admin_phase = 1;

    std::uint8_t mpsmin = (cap >> 48) & 0xf;
    controller->mpsmin = mpsmin;
    controller->page_size = 1 << (12 + mpsmin);
    if(!((cap >> 37) & 1)) {
        klibc::printf("NVME: Impossible to init because nvm is not supported\r\n");
        return;
    }

    nvme::write32(controller, 0x14, nvme::read32(controller, 0x14) & ~(1 << 0));
    while(nvme::read32(controller, NVME_REG_CSTS) & 1) arch::pause();

    controller->admin_cq.address = pmm::freelist::alloc_4k();
    controller->admin_sq.address = pmm::freelist::alloc_4k();
    controller->admin_cq.size = 63;
    controller->admin_sq.size = 63;
    nvme::write64(controller, 0x30, controller->admin_cq.address);
    nvme::write64(controller, 0x28, controller->admin_sq.address);
    nvme::write32(controller, 0x24, 63 | (63 << 16));

    nvme::write32(controller, 0x14, (1 << 0) | (4 << 20) | (6 << 16));
    while(!(nvme::read32(controller, NVME_REG_CSTS) & 1)) arch::pause();

    if(!nvme_identify(controller)) {
        klibc::printf("NVME: Failed to identify !\r\n");
        return;
    }

    controller->main_io_queue = nvme_create_io_queue(controller);
    if(controller->main_io_queue == nullptr) {
        klibc::printf("NVME: Failed to create io queue\r\n");
        return;
    }

    if(!nvme_init_namespaces(controller)) {
        klibc::printf("NVME: Failed to init namespaces\r\n");
    }

    nvme_controllers[nvme_controller_ptr++] = controller;
}

#if defined(__x86_64__)
void nvme_pci_init(pci_t pci, std::uint8_t a, std::uint8_t b, std::uint8_t c) {
    if(pci.progIF == 2) { // nvme 
        std::uint32_t cmd = x86_64::pci::pci_read_config32(a,b,c,0x4);
        klibc::printf("NVME_PCI: Bus mastering %d, memory access %d\r\n",(cmd & (1 << 2)) ? 1 : 0, (cmd & (1 << 1)) ? 1 : 0);
        cmd |= 1 << 2;
        cmd |= 1 << 1;
        x86_64::pci::pci_write_config32(a,b,c,0x4,cmd);
        nvme_init((std::uint64_t)(((std::uint64_t)pci.bar1 << 32) | (pci.bar0 & 0xFFFFFFF0)));
    }
}
#endif

void drivers::nvme::init() {
    klibc::memset(nvme_controllers,0,sizeof(nvme_controllers));
#if defined(__x86_64__)
    x86_64::pci::reg(nvme_pci_init,1,8);
#else   
    klibc::printf("todo implement nvme finding on other arches\r\n");
#endif
}