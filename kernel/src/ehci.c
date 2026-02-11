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

uint8_t* get_descriptor(volatile ehci_queue_head_t *qh, uint8_t descriptor_type, uint8_t descriptor_index, uint8_t descriptor_length) {
    uint8_t *setup_buffer = (uint8_t *)malloc(8, 32);
    setup_buffer[0] = 0x80; // Device-to-Host, Standard, Device
    setup_buffer[1] = 0x06; // GET_DESCRIPTOR
    setup_buffer[2] = descriptor_index; // Descriptor Index (wValue low)
    setup_buffer[3] = descriptor_type; // Descriptor Type (wValue high: 0x01 = Device Descriptor)
    setup_buffer[4] = 0x00; // Language ID (wIndex low)
    setup_buffer[5] = 0x00; // Language ID (wIndex high)
    setup_buffer[6] = descriptor_length;   // Length (wLength low: 18 bytes for Device Descriptor)
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
    uint8_t *data_buffer = (uint8_t *)malloc(descriptor_length, 32);
    total_bytes = descriptor_length;
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
    qh->current_qtd_address = 0;
    qh->alt_next_qtd = 1; // Terminate bit
    qh->token = 0; // Clear Active bit and others
    qh->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_setup);
    volatile uint32_t timeout = 0x1FFFFFFF;
    while((qtd_setup->token & (1 << 7)) | (qtd_data->token & (1 << 7)) | (qtd_status->token & (1 << 7))) {
        timeout--;
        if(timeout <= 0) {
            printf("Error: EHCI GET DESCRIPTOR timeout. Tokens: setup=%x, data=%x, status=%x\n", 
                   qtd_setup->token, qtd_data->token, qtd_status->token);
            return NULL;
        }
        if(qtd_setup->token & (1 << 6)) {
             printf("Error Setup QTD: Halted (Token: %x)\n", qtd_setup->token);
             return NULL;
        }
        if(qtd_data->token & (1 << 6)) {
             printf("Error Data QTD: Halted (Token: %x)\n", qtd_data->token);
             return NULL;
        }
        if(qtd_status->token & (1 << 6)) {
             printf("Error Status QTD: Halted (Token: %x)\n", qtd_status->token);
             return NULL;
        }
    }
    printf("Successfully GET DESCRIPTOR\n");
    return data_buffer;
}

void set_device_address(volatile ehci_queue_head_t *qh, uint8_t address) {
    uint8_t *setup_buffer = (uint8_t *)malloc(8, 32);
    setup_buffer[0] = 0x0; // host to device + standard + device
    setup_buffer[1] = 0x05; // set a request to set the address
    setup_buffer[2] = address; // the address we want to set
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
    qh->alt_next_qtd = 1;
    qh->token = 0;

    qh->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_setup);

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
    qh->endpoint_characteristics = (qh->endpoint_characteristics & ~0x7F) | (address & 0x7F);
}

void set_device_configuration(volatile ehci_queue_head_t *qh, uint8_t config_value) {    
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
    qh->alt_next_qtd = 1;
    qh->token = 0;

    qh->next_qtd = (uint32_t)VIRT_TO_PHYS(qtd_setup);

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

// total address is same as total device so device port 0 will assigned to address 1 like that
static int total_device = 0; // if total device is 1 then address is 1, if 2 then address is 2 and so on

void find_device_port_and_sign_address() {
    uint8_t N_PORT = capability_regs->hcs_params & 0x7;
    for(uint8_t i = 0; i < N_PORT; i++) {
        uint32_t port_status = operation_regs->port_status_or_control[i];
        if(port_status & (1 << 0)) {
            printf("Port %d is Connected\n", i);
        } else {
            continue;
        }
        port_status &= ~(1 << 1);
        port_status |= (1 << 8);

        operation_regs->port_status_or_control[i] = port_status;

        for(volatile int j = 0;j < 0x1FFFFF; j++);

        port_status = operation_regs->port_status_or_control[i];
        port_status &= ~(1 << 8);
        operation_regs->port_status_or_control[i] = port_status;

        for(volatile int j = 0;j < 0x1FFFFF; j++);

        port_status = operation_regs->port_status_or_control[i];
        if(port_status & (1 << 2)) {
            total_device++;
            printf("Port %d is Enabled (High Speed)\n", i);
            set_device_address(queue_heads, total_device);
        } else {
            printf("Port %d is not High Speed, handing over to companion controller\n", i);
            port_status |= (1 << 13);
            operation_regs->port_status_or_control[i] = port_status;
        }
    }
    printf("Total Device %d\n", total_device);
}

// mouse ehci

uint8_t endpoint = 0;
uint16_t max_packet_size = 0;

static volatile ehci_queue_head_t* mouse_qh = NULL;
static volatile ehci_qtd_t* mouse_qtd = NULL;
static volatile uint8_t* mouse_data_buf = NULL;
static uint8_t mouse_toggle = 0;

void setup_mouse_ehci() {
    printf("find device\n");
    if(!find_device_port()) {
        printf("EHCI: No high speed devices found, skipping setup.\n");
        return;
    }
    // find_device_port_and_sign_address();
    // get device descriptor
    uint8_t* dev_desc_raw = get_descriptor(queue_heads, 1, 0, 18);
    if (!dev_desc_raw) return;
    usb_device_descriptor_t* dev_desc = (usb_device_descriptor_t*)dev_desc_raw;
    printf("USB Version: %x\n", dev_desc->bcd_usb);
    printf("Vendor ID: %x, Product ID: %x\n", dev_desc->vendor_id, dev_desc->product_id);

    // set address to 1
    set_device_address(queue_heads, 1);
    
    // USB spec: Recovery time after SET_ADDRESS (at least 2ms)
    for(volatile int j = 0; j < 0x0FFFFFFF; j++);

    // Update Control QH to use address 1
    // Bit 15: H (Head of reclamation list) MUST BE 1
    // Max packet 64, DTC=1, EPS=High Speed (2), Address=1
    queue_heads->endpoint_characteristics = (1 << 15) | (64 << 16) | (1 << 14) | (2 << 12) | 1;

    set_device_configuration(queue_heads, 1);

    // Get configuration descriptor header to find total length
    uint8_t* conf_header_raw = get_descriptor(queue_heads, 2, 0, 9);
    if (!conf_header_raw) return;
    usb_config_descriptor_t* conf_desc = (usb_config_descriptor_t*)conf_header_raw;
    uint16_t total_len = conf_desc->total_length;
    printf("Config Total Length: %d\n", total_len);

    // Get full configuration descriptor
    uint8_t* conf_full_raw = get_descriptor(queue_heads, 2, 0, total_len);
    if (!conf_full_raw) return;

    uint8_t* ptr = conf_full_raw;
    uint16_t processed = 0;
    while (processed < total_len) {
        uint8_t len = ptr[0];
        uint8_t type = ptr[1];
        if (len == 0) break;

        if(type == 4) {
            usb_interface_descriptor_t* interface = (usb_interface_descriptor_t*)ptr;
            uint8_t interface_number = interface->interface_number;
            printf("Found Interface: %d\n", interface_number);
            if(interface->interface_protocol == 0x02) {
                printf("Found Mouse Interface\n");
            } else {
                printf("this are not mouse interface\n");
                return;
            }
        }

        if (type == 5) { // Endpoint Descriptor
            usb_endpoint_descriptor_t* ep = (usb_endpoint_descriptor_t*)ptr;
            endpoint = ep->endpoint_address & 0x0F;
            max_packet_size = ep->max_packet_size & 0x7FF;
            printf("Found Endpoint: %d, Max Packet Size: %d, Direction: %s\n", 
                   endpoint, max_packet_size, (ep->endpoint_address & 0x80) ? "IN" : "OUT");
            break;
        }
        ptr += len;
        processed += len;
    }
    
    printf("Final Endpoint Configuration: EP=%d, MaxPacketSize=%d\n", endpoint, max_packet_size);
}

void input_mouse_ehci() {
    if (max_packet_size == 0) {
        printf("Mouse endpoint not found\n");
        return;
    }

    printf("ini jalan ya\n");

    if (mouse_qh == NULL) {
        mouse_qh = (volatile ehci_queue_head_t*)malloc(sizeof(ehci_queue_head_t), 32);
        mouse_qtd = (volatile ehci_qtd_t*)malloc(sizeof(ehci_qtd_t), 32);
        mouse_data_buf = (volatile uint8_t*)malloc(max_packet_size, 32);
        
        memset((void*)mouse_qh, 0, sizeof(ehci_queue_head_t));
        memset((void*)mouse_qtd, 0, sizeof(ehci_qtd_t));

        mouse_qh->horizontal_link_pointer = 1; // Terminate for periodic
        // Address 1, EP, EPS=High (2), Max Packet, DTC=1 for interrupt
        mouse_qh->endpoint_characteristics = 1 | (endpoint << 8) | (2 << 12) | (max_packet_size << 16) | (1 << 14);
        // S-mask = 0x01 (bit 0), C-mask = 0x00, RL=3, Multiplier=1
        mouse_qh->endpoint_capabilities = (3 << 28) | (1 << 30) | 0x01; 
        
        // Token: DATA0, Total Bytes, CERR=3, PID=IN, Active
        mouse_qtd->token = (0 << 31) | (max_packet_size << 16) | (3 << 10) | (1 << 8) | (1 << 7);
        mouse_qtd->buffer[0] = (uint32_t)VIRT_TO_PHYS(mouse_data_buf);
        mouse_qtd->next_qtd = 1;
        mouse_qtd->alt_next_qtd = 1;

        mouse_qh->next_qtd = (uint32_t)VIRT_TO_PHYS(mouse_qtd);
        mouse_qh->alt_next_qtd = 1;

        // Add to periodic schedule (every frame for now)
        for (int i = 0; i < 1024; i++) {
            periodic_frame_list[i] = (uint32_t)VIRT_TO_PHYS(mouse_qh) | 0x02; // 0x02 = QH
        }
        printf("Mouse periodic QH initialized at endpoint %d\n", endpoint);
    }

    if (!(mouse_qtd->token & (1 << 7))) { // If not active
        printf("MOUSENYA GERAK\n");
        static int64_t x = 0;
        static int64_t y = 0;
        int8_t x_movement = (int8_t)mouse_data_buf[1];
        int8_t y_movement = (int8_t)mouse_data_buf[2];
        if (!(mouse_qtd->token & (1 << 6))) { // If not halted
            x += (int8_t)x_movement;
            y += (int8_t)y_movement;

            // // Clamping (Optional but recommended)
            // if (x < -2000) x = -2000;
            // if (x > 2000) x = 2000;
            // if (y < -2000) y = -2000;
            // if (y > 2000) y = 2000;

            extern volatile uint64_t framebuffer_height;
		    extern volatile uint64_t framebuffer_width;
		    if(x <= 0) {
		    	x = 0;
		    }
		    else if(x >= framebuffer_width) {
		    	x = framebuffer_width - 1;
		    }
		    if(y <= 0) {
		    	y = 0;
		    }
		    else if(y >= framebuffer_height) {
		    	y = framebuffer_height - 1;
		    }
		    for(int y_i = 0; y_i < 8; y_i++) {
		    	for(int x_i = 0; x_i < 8; x_i++) {
		    		draw_pixel(x + x_i, y + y_i, 0xFFFFFFFF);
		    	}
		    }

            printf("Mouse data: %d %d %d %d\n", (int64_t)mouse_data_buf[0], (int64_t)x_movement, (int64_t)y_movement, (int64_t)mouse_data_buf[3]);
            printf("Mouse position: %d %d\n", x, y);
        } else {
            printf("Mouse QTD Halted! Token: %x\n", mouse_qtd->token);
        }

        // Reactivate: Clear QH overlay token first to ensure clean state
        mouse_qh->token = 0;
        
        // Toggle data toggle bit (DATA0 -> DATA1 -> DATA0 ...)
        mouse_toggle ^= 1;
        
        // Reset qTD: clear all status/error bits, set new data toggle
        mouse_qtd->next_qtd = 1;
        mouse_qtd->alt_next_qtd = 1;
        mouse_qtd->token = (mouse_toggle << 31) | (max_packet_size << 16) | (3 << 10) | (1 << 8) | (1 << 7);
        mouse_qtd->buffer[0] = (uint32_t)VIRT_TO_PHYS(mouse_data_buf);
        
        // Re-link to QH
        mouse_qh->next_qtd = (uint32_t)VIRT_TO_PHYS(mouse_qtd);
    }
}
