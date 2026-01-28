#include "ehci.h"

uint8_t ehci_bus = 0;
uint8_t ehci_slot = 0;
uint8_t ehci_func = 0;

bool find_ehci() {
	for (int bus = 0; bus < 256; bus++) {
     		for (uint8_t slot = 0; slot < 32; slot++) {
            		if (pci_vendor(bus, slot, 0) == 0xFFFF) continue;
            		uint8_t header = pciConfigReadWord(bus, slot, 0, 0x0E);
            		uint8_t func_limit = (header & 0x80) ? 8 : 1;

            		for (uint8_t func = 0; func < func_limit; func++) {
                		if (pci_vendor(bus, slot, func) == 0xFFFF) continue;
                		uint32_t class = pciConfigReadDword(bus, slot, func, 0x08);
                		uint8_t progIF   = (class >> 8)  & 0xFF;
                		uint8_t subclass = (class >> 16) & 0xFF;
                		uint8_t classcode= (class >> 24) & 0xFF;

                		if (classcode == 0x0C && subclass  == 0x03 && progIF == 0x20) {
						    printf("EHCI found at %d:%d:%d\n", bus, slot, func);
						    ehci_bus = bus;
						    ehci_slot = slot;
						    ehci_func = func;
						    return true;
                		}
            		}
        	}
    	}
    printf("EHCI not found\n");
    return false;
}

#define EHCI_ABAR_VIRT 0xFFFF8000F0000000

volatile ehci_capability_regs_t *capability_regs;
volatile ehci_operation_regs_t *operation_regs;
volatile ehci_queue_head_t *queue_heads;
volatile uint32_t *periodic_frame_list;

void setup_ehci() {
    if(!find_ehci()) return;
    uint16_t cmd = pciConfigReadWord(ehci_bus, ehci_slot, ehci_func, 0x04);
    cmd |= (1 << 1); // Mem Space
    cmd |= (1 << 2); // Bus Master
    pciConfigWriteWord(ehci_bus, ehci_slot, ehci_func, 0x04, cmd);

    uint32_t abar_phys = pciConfigReadDword(ehci_bus, ehci_slot, ehci_func, 0x10);
    printf("EHCI ABAR Phys: %x\n", abar_phys);
    uint64_t *pml4 = get_pml4();
    map_page(pml4, EHCI_ABAR_VIRT, abar_phys, PTE_PRESENT | PTE_WRITABLE | PTE_PWT | PTE_PCD);

    capability_regs = (volatile ehci_capability_regs_t *)EHCI_ABAR_VIRT;
    printf("EHCI Capability Regs at %x\n", (uint64_t)capability_regs);
    
    uint8_t cap_len = capability_regs->cap_length;
    printf("EHCI Cap Length: %d\n", cap_len);

    operation_regs = (volatile ehci_operation_regs_t *)(EHCI_ABAR_VIRT + cap_len);
    printf("version : %d\n", capability_regs->hci_version);
    printf("version : %x\n", capability_regs->hci_version);

    operation_regs->usb_command &= ~(1 << 0); // stop
    volatile uint32_t timeout = 0x1FFFFFFF;
    while(!(operation_regs->usb_status & (1 << 12))) {
        timeout--;
        if(timeout <= 0) {
            printf("Error: EHCI stop timeout\n");
            return;
        }
    }
    printf("Successfully EHCI stopped\n");

    operation_regs->usb_command |= (1 << 1); // bit 1: Host Controller Reset
    
    timeout = 0x1FFFFFFF;
    while(operation_regs->usb_command & (1 << 1)) {
        timeout--;
        if(timeout <= 0) {
            printf("Error: EHCI reset timeout\n");
            return;
        }
    }
    printf("Successfully EHCI reset\n");
    
    queue_heads = (volatile ehci_queue_head_t *)malloc(sizeof(ehci_queue_head_t), 32);
    operation_regs->async_list_addr = (uint32_t)VIRT_TO_PHYS(queue_heads);
    queue_heads->horizontal_link_pointer = (uint32_t)VIRT_TO_PHYS(queue_heads) | 0x02; // 0x02 = Type QH
    queue_heads->next_qtd = 1; // Terminate bit
    queue_heads->alt_next_qtd = 1; // Terminate bit

    // Set Endpoint Characteristics:
    // Bit 15: H (Head of reclamation list) = 1
    // Bit 14: DTC (Data Toggle Control) = 1
    // Bits 16-26: Max Packet Length = 64
    // Bits 12-13: EPS (Endpoint Speed) = 2 (High Speed)
    queue_heads->endpoint_characteristics = (1 << 15) | (1 << 14) | (64 << 16) | (2 << 12);
    queue_heads->endpoint_capabilities = (3 << 28); // RL=3 (Nak count reload)

    // Initialize Periodic Frame List (1024 entries, 4KB aligned)
    periodic_frame_list = (volatile uint32_t *)malloc(1024 * sizeof(uint32_t), 4096);
    for (int i = 0; i < 1024; i++) {
        periodic_frame_list[i] = 0x1; // Terminate bit
    }
    operation_regs->periodic_list_base = (uint32_t)VIRT_TO_PHYS(periodic_frame_list);

    operation_regs->config_flag = 1; // enable configuration flag
    operation_regs->usb_command = (1 << 5) | (1 << 4) | (1 << 0); // enable asynchronous schedule + periodic schedule + run
    // operation_regs = (1 << 4) | (1 << 0);
}

bool find_device_port() {
    uint8_t N_PORT = capability_regs->hcs_params & 0x7; // take 3 byte from hcs params
    printf("N_PORT %d\n", N_PORT);
    for(uint8_t i = 0; i < N_PORT; i++) {
        uint32_t port_status = operation_regs->port_status_or_control[i];
        if(port_status & (1 << 0)) {
            printf("Port %d is connected\n", i);
        } else {
            continue;
        }
        // reset port
        port_status &= ~(1 << 1); // Clear Connect Change bit
        port_status |= (1 << 8);  // Set Port Reset bit
        operation_regs->port_status_or_control[i] = port_status;

        // USB Spec: Reset duration should be at least 50ms for root hubs
        for(volatile int j = 0; j < 0x1FFFFFFF; j++);
        
        port_status = operation_regs->port_status_or_control[i];
        port_status &= ~(1 << 8); // Clear Port Reset bit
        operation_regs->port_status_or_control[i] = port_status;

        // Wait for reset to complete
        for(volatile int j = 0; j < 0x1FFFFFFF; j++);

        port_status = operation_regs->port_status_or_control[i];
        if(port_status & (1 << 2)) {
            printf("Port %d is Enabled (High Speed)\n", i);
        } else {
            printf("Port %d is not High Speed, handing over to companion controller\n", i);
            port_status |= (1 << 13); // Set Port Owner bit (handover to companion)
            operation_regs->port_status_or_control[i] = port_status;
        }
    }
    // Return true if at least one port is enabled and owned by EHCI
    for(uint8_t i = 0; i < N_PORT; i++) {
        if((operation_regs->port_status_or_control[i] & (1 << 2)) && 
           !(operation_regs->port_status_or_control[i] & (1 << 13))) {
            return true;
        }
    }
    return false;
}

void get_device_descriptor() {
    uint8_t *setup_buffer = (uint8_t *)malloc(8, 32);
    setup_buffer[0] = 0x80; // Device-to-Host, Standard, Device
    setup_buffer[1] = 0x06; // GET_DESCRIPTOR
    setup_buffer[2] = 0x00; // Descriptor Index (wValue low)
    setup_buffer[3] = 0x01; // Descriptor Type (wValue high: 0x01 = Device Descriptor)
    setup_buffer[4] = 0x00; // Language ID (wIndex low)
    setup_buffer[5] = 0x00; // Language ID (wIndex high)
    setup_buffer[6] = 18;   // Length (wLength low: 18 bytes for Device Descriptor)
    setup_buffer[7] = 0x00; // Length (wLength high)
    
    volatile ehci_qtd_t *qtd_setup = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);
    volatile ehci_qtd_t *qtd_data = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);
    volatile ehci_qtd_t *qtd_status = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);
    volatile ehci_qtd_t *qtd_dummy = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);
    // setup
    uint32_t total_bytes = 8;
    uint32_t ioc = 1;      // We want interrupt when it's done
    uint32_t cerr = 3;     // Toleransi error 3x
    uint32_t pid = 2;      // 2 = SETUP
    uint32_t status = 0x80; // Bit 7 (Active)
    qtd_setup->token = (0 << 31)          | // Data Toggle (DATA0)
                (total_bytes << 16) | 
                (ioc << 15)         | 
                (cerr << 10)        | 
                (pid << 8)          | 
                status;
    qtd_setup->buffer[0] = (uint32_t)VIRT_TO_PHYS(setup_buffer);
    uint8_t *data_buffer = (uint8_t *)malloc(18, 32);
    total_bytes = 18;
    ioc = 1;      // We want interrupt when it's done
    cerr = 3;     // Tolerance error 3x
    pid = 1;      // 1 = IN
    status = 0x80; // Bit 7 (Active)
    qtd_data->token = (1 << 31)          | // Data Toggle (DATA1)
                (total_bytes << 16) | 
                (ioc << 15)         | 
                (cerr << 10)        | 
                (pid << 8)          | 
                status;
    qtd_data->buffer[0] = (uint32_t)VIRT_TO_PHYS(data_buffer);
    // status
    total_bytes = 0;
    ioc = 1;      // We want interrupt when it's done
    cerr = 3;     // Tolerance error 3x
    pid = 0;      // 0 = OUT
    status = 0x80; // Bit 7 (Active)
    qtd_status->token = (1 << 31)          | // Data Toggle (DATA1)
                (total_bytes << 16) | 
                (ioc << 15)         | 
                (cerr << 10)        | 
                (pid << 8)          | 
                status;
    
    
    // link to each other
    qtd_setup->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_data);
    qtd_data->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_status);
    qtd_status->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_dummy);
    
    qtd_dummy->next_qtd = 1;
    qtd_dummy->alt_next_qtd = 1;
    qtd_dummy->token = 0;
    // add to queue
    queue_heads->current_qtd_address = 0;
    queue_heads->alt_next_qtd = 1; // Terminate bit
    queue_heads->token = 0; // Clear Active bit and others
    queue_heads->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_setup);
    volatile uint32_t timeout = 0x1FFFFFFF;
    while((qtd_setup->token & (1 << 7)) | (qtd_data->token & (1 << 7)) | (qtd_status->token & (1 << 7))) {
        timeout--;
        if(timeout <= 0) {
            printf("Error: EHCI GET DEVICE DESCRIPTOR timeout. Tokens: setup=%x, data=%x, status=%x\n", 
                   qtd_setup->token, qtd_data->token, qtd_status->token);
            return;
        }
        if(qtd_setup->token & (1 << 6)) {
             printf("Error Setup QTD: Halted (Token: %x)\n", qtd_setup->token);
             return;
        }
        if(qtd_data->token & (1 << 6)) {
             printf("Error Data QTD: Halted (Token: %x)\n", qtd_data->token);
             return;
        }
        if(qtd_status->token & (1 << 6)) {
             printf("Error Status QTD: Halted (Token: %x)\n", qtd_status->token);
             return;
        }
    }
    printf("Successfully EHCI setup\n");
    printf("Device Descriptor: %x\n", data_buffer[0]);
    printf("Vendor ID: %x\n", data_buffer[8]);
    printf("Product ID: %x\n", data_buffer[10]);
    printf("Class ID: %x\n", data_buffer[4]);
}

void set_device_address(uint8_t address) {
    uint8_t *setup_buffer = (uint8_t *)malloc(8, 32);
    setup_buffer[0] = 0x0; // host to device + standard + device
    setup_buffer[1] = 0x05; // set a request to set the address
    setup_buffer[2] = address; // the address we want to set
    for(int i = 3; i < 8; i++) setup_buffer[i] = 0;

    // Ensure QH is currently pointing to Address 0 for this request
    queue_heads->endpoint_characteristics &= ~0x7F;

    volatile ehci_qtd_t *qtd_setup = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);
    volatile ehci_qtd_t *qtd_status = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);
    volatile ehci_qtd_t *qtd_dummy = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);

    // setup

    qtd_setup->token = (1 << 7) | // active 
                        (2 << 8) | // SETUP PID
                        (3 << 10) | // error counter so it will try if error until 3 times
                        (0 << 31) | // DATA0 yeah just data toogle
                        (8 << 16) | // it will take 8 bytes like the setup_buffer has set
                        (1 << 15); // interrupt on complete
    qtd_setup->buffer[0] = (uint32_t)VIRT_TO_PHYS(setup_buffer);
    
    // status
    
    qtd_status->token = (1 << 7) | // active 
                        (1 << 8) | // IN PID
                        (3 << 10) | // error counter so it will try if error until 3 times
                        (1 << 31) | // DATA1 yeah just data toogle
                        (0 << 16) | // status dont take a byte
                        (1 << 15); // interrupt on complete
    
    // dummy
    qtd_dummy->next_qtd = 1;
    qtd_dummy->alt_next_qtd = 1;
    qtd_dummy->token = 0;

    // link each others
    qtd_setup->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_status);
    qtd_status->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_dummy);

    // link it to queue_heads
    queue_heads->alt_next_qtd = 1;
    queue_heads->token = 0;

    queue_heads->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_setup);

    volatile uint32_t timeout = 0x1FFFFFFF;
    while((qtd_setup->token & (1 << 7)) | (qtd_status->token & (1 << 7))) {
        timeout--;
        if(timeout <= 0) {
            printf("Error Set Address: Timeout\n");
            return;
        }
        if(qtd_setup->token & (1 << 6)) {
            printf("Error Setup QTD: Halted (Token: %x)\n", qtd_setup->token);
            return;
        }
        if(qtd_status->token & (1 << 6)) {
            printf("Error Status QTD: Halted (Token: %x)\n", qtd_status->token);
            return;
        }
    }
    printf("Successfully set Address device to %d\n", address);

    // Update the Queue Head to the new address for future communication
    queue_heads->endpoint_characteristics = (queue_heads->endpoint_characteristics & ~0x7F) | (address & 0x7F);
}

void set_device_configuration(uint8_t config_value) {    
    uint8_t *setup_buffer = (uint8_t *)malloc(8, 32);
    setup_buffer[0] = 0x0; // host to device + standard + device
    setup_buffer[1] = 0x09; // set a request to set the configuration
    setup_buffer[2] = config_value; // the configuration we want to set
    for(int i = 3; i < 8; i++) setup_buffer[i] = 0;

    volatile ehci_qtd_t *qtd_setup = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);
    volatile ehci_qtd_t *qtd_status = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);
    volatile ehci_qtd_t *qtd_dummy = (volatile ehci_qtd_t *)malloc(sizeof(ehci_qtd_t), 32);

    // setup

    qtd_setup->token = (1 << 7) | // active 
                        (2 << 8) | // SETUP PID
                        (3 << 10) | // error counter so it will try if error until 3 times
                        (0 << 31) | // DATA0 yeah just data toogle
                        (8 << 16) | // it will take 8 bytes like the setup_buffer has set
                        (1 << 15); // interrupt on complete
    qtd_setup->buffer[0] = (uint32_t)VIRT_TO_PHYS(setup_buffer);
    
    // status
    
    qtd_status->token = (1 << 7) | // active 
                        (1 << 8) | // IN PID
                        (3 << 10) | // error counter so it will try if error until 3 times
                        (1 << 31) | // DATA1 yeah just data toogle
                        (0 << 16) | // status dont take a byte
                        (1 << 15); // interrupt on complete
    
    // dummy
    qtd_dummy->next_qtd = 1;
    qtd_dummy->alt_next_qtd = 1;
    qtd_dummy->token = 0;

    // link each others
    qtd_setup->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_status);
    qtd_status->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_dummy);

    // link it to queue_heads
    queue_heads->alt_next_qtd = 1;
    queue_heads->token = 0;

    queue_heads->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_setup);

    volatile uint32_t timeout = 0x1FFFFFFF;
    while((qtd_setup->token & (1 << 7)) | (qtd_status->token & (1 << 7))) {
        timeout--;
        if(timeout <= 0) {
            printf("Error Set Configuration: Timeout\n");
            return;
        }
        if(qtd_setup->token & (1 << 6)) {
            printf("Error Setup QTD: Halted (Token: %x)\n", qtd_setup->token);
            return;
        }
        if(qtd_status->token & (1 << 6)) {
            printf("Error Status QTD: Halted (Token: %x)\n", qtd_status->token);
            return;
        }
    }
    printf("Successfully set Configuration device to %d\n", config_value);
}

void setup_mouse_ehci() {
    printf("find device\n");
    if(!find_device_port()) {
        printf("EHCI: No high speed devices found, skipping setup.\n");
        return;
    }
    // get descriptor
    get_device_descriptor();
    // set address to 1
    set_device_address(1);
    set_device_configuration(1);
}