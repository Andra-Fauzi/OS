#include "usb.h"
// we will use OHCI or Open Host Controller Interface

uint8_t ohci_bus = 0;
uint8_t ohci_slot = 0;
uint8_t ohci_func = 0;

bool find_ohci() {
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

                		if (classcode == 0x0C && subclass  == 0x03 && progIF == 0x10) {
                    			printf("OHCI found at %d:%d:%d\n", bus, slot, func);
				ohci_bus = bus;
				ohci_slot = slot;
				ohci_func = func;
                    		return true;
                		}
            		}
        	}
    	}
    printf("OHCI not found\n");
    return false;
}

#define OHCI_ABAR_VIRT 0xFFFF8000F0000000

#define OHCI_CTRL_HCFS (3 << 6)
#define OHCI_USB_OPERATIONAL (2 << 6) // nilai 10b untuk operational
#define OHCI_USB_SUSPEND (3 << 6)

volatile ohci_regs_t *ohci_regs;
volatile ohci_hhca_t *hhca;

void init_OHCI() {
	if(!find_ohci()) return;
	uint16_t cmd = pciConfigReadWord(ohci_bus, ohci_slot, ohci_func, 0x04);
	cmd |= (1 << 1); // Mem Space
	cmd |= (1 << 2); // Bus Master
	pciConfigWriteWord(ohci_bus, ohci_slot, ohci_func, 0x04, cmd);

	uint32_t abar_phys = pciConfigReadDword(ohci_bus, ohci_slot, ohci_func, 0x10);

	uint64_t *pml4 = get_pml4();
	map_page(pml4, OHCI_ABAR_VIRT, abar_phys, PTE_PRESENT | PTE_WRITABLE | PTE_PWT | PTE_PCD);

	ohci_regs = (volatile ohci_regs_t *)OHCI_ABAR_VIRT;
	printf("revision : %d\n", ohci_regs->revision);
	printf("revision : %x\n", ohci_regs->revision);

	ohci_regs->command_status = (1 << 0); // bit 0: Host Controller Reset
	
	int timeout = 10000000;
	while(ohci_regs->command_status & (1 << 0)) {
		timeout--;
		if(timeout <= 0) {
			printf("Error: OHCI reset timeout\n");
			return;
		}
	}
	printf("Successfully OHCI reset\n");

	uint32_t val = ohci_regs->control;

	val &= ~OHCI_CTRL_HCFS; // bersihkan bit 6-7
	val |= OHCI_USB_OPERATIONAL; // set ke bit 10
	val |= (1 << 2); // CLE: Control List Enable
	val |= (1 << 4); // PLE: Periodic List Enable
	
	ohci_regs->control = val;
	
	if ((ohci_regs->control & OHCI_CTRL_HCFS) == OHCI_USB_OPERATIONAL) {
		printf("Status Successfully change to USB OPERATIONAL\n");
	} else {
		printf("Status Failed change to USB OPERATIONAL\n");
	}

}

void check_device_status() {
	uint32_t num_ports = ohci_regs->root_hub_descriptor_A & 0xFF;
	for(uint32_t i = 0; i < num_ports; i++) {
		if(ohci_regs->root_hub_port_status[i] & (1 << 0)) {
			printf("device on port %d\n", i);

			ohci_regs->root_hub_port_status[i] = (1 << 4);

			int timeout = 10000000;
			while(!(ohci_regs->root_hub_port_status[i] & (1 << 20))) {
				timeout--;
				if(timeout <= 0) {
					printf("Error: OHCI port reset timeout\n");
					return;
				}
			}
			ohci_regs->root_hub_port_status[i] = (1 << 20);
			printf("Successfully OHCI port reset\n");
		}
	}
}

uint32_t mouse_speed = 0; // 0 = full, 1 = low

void setup_mouse() {
	uint32_t num_ports = ohci_regs->root_hub_descriptor_A & 0xFF;
	for(uint32_t i = 0; i < num_ports; i++) {
		if(ohci_regs->root_hub_port_status[i] & (1 << 0)) {
			if(ohci_regs->root_hub_port_status[i] & (1 << 9)) {
				mouse_speed = 1; // Low Speed
				printf("Detected Low Speed device on port %d\n", i);
			} else {
				mouse_speed = 0; // Full Speed
				printf("Detected Full Speed device on port %d\n", i);
			}
		}
	}

	uint8_t *setup_buffer = (uint8_t *)malloc(8, 16);
	setup_buffer[0] = 0x00; // Type: Standard, Recipient: Device
	setup_buffer[1] = 0x05; // Request: SET_ADDRESS
	setup_buffer[2] = 0x01; // Address = 1
	setup_buffer[3] = 0x00;
	setup_buffer[4] = 0x00;
	setup_buffer[5] = 0x00;
	setup_buffer[6] = 0x00;
	setup_buffer[7] = 0x00;

	ohci_td_t *td_dummy = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
    	td_dummy->next_td = 0;

	ohci_td_t *td_setup = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	td_setup->flags = (0 << 19) | (2 << 24) | (15 << 28); // DP=SETUP (00b), T=DATA0 (10b)
	td_setup->current_buffer_ptr = (uint32_t)VIRT_TO_PHYS(setup_buffer);
	td_setup->buffer_end_ptr = td_setup->current_buffer_ptr + 7;

	ohci_td_t *td_status = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);	
	td_status->flags = (1 << 18) | (2 << 19) | (3 << 24) | (15 << 28); // R=1, DP=IN (10b), T=DATA1 (11b)
	td_status->current_buffer_ptr = 0;
	td_status->buffer_end_ptr = 0;
	td_status->next_td = (uint32_t)VIRT_TO_PHYS(td_dummy);

	td_setup->next_td = (uint32_t)VIRT_TO_PHYS(td_status);

	ohci_ed_t *ed = (ohci_ed_t *)malloc(sizeof(ohci_ed_t), 16);
	ed->flags = 	(0 << 0) | // Address 0
			(0 << 7) | // Endpoint 0
			(mouse_speed << 13) | // Speed
			(8 << 16); // MaxPacketSize 8
	ed->head_pointer = (uint32_t)VIRT_TO_PHYS(td_setup);
	ed->tail_pointer = (uint32_t)VIRT_TO_PHYS(td_dummy);
	ed->next_ed = 0;

	ohci_regs->control_head_ed = (uint32_t)VIRT_TO_PHYS(ed);
	ohci_regs->command_status |= (1 << 1); // ControlListFilled

	int timeout = 10000000;
	while((td_status->flags >> 28) == 0xF) {
		timeout--;
		if(timeout <= 0) {
			printf("SET_ADDRESS timeout\n");
			break;
		}
	}
	
	if((td_status->flags >> 28) == 0x0) {
		printf("Set Address Success! Device now in Address 1\n");
	} else {
		printf("Set Address Error: CC = %x\n", (td_status->flags >> 28));
		return;
	}

	// Wait a bit for the device to settle on the new address
	for(volatile int j = 0; j < 1000000; j++);

	// SET_CONFIGURATION 1
	uint8_t *setup_buffer1 = (uint8_t *)malloc(8, 16);
	setup_buffer1[0] = 0x00;
	setup_buffer1[1] = 0x09; // SET_CONFIGURATION
	setup_buffer1[2] = 0x01; // Configuration 1
	setup_buffer1[3] = 0x00;

	td_setup->flags = (0 << 19) | (2 << 24) | (15 << 28); // DP=SETUP, T=DATA0
	td_setup->current_buffer_ptr = (uint32_t)VIRT_TO_PHYS(setup_buffer1);
	td_setup->buffer_end_ptr = td_setup->current_buffer_ptr + 7;

	td_status->flags = (1 << 18) | (2 << 19) | (3 << 24) | (15 << 28); // R=1, DP=IN, T=DATA1
	td_status->current_buffer_ptr = 0;
	td_status->buffer_end_ptr = 0;

	ed->flags = (1 << 0) | (0 << 7) | (mouse_speed << 13) | (8 << 16);
	ed->head_pointer = (uint32_t)VIRT_TO_PHYS(td_setup);

	ohci_regs->control_current_ed = 0; // Force start from head
	ohci_regs->command_status |= (1 << 1);

	timeout = 100000000;
	while((td_status->flags >> 28) == 0xF) {
		timeout--;
		if(timeout <= 0) {
			printf("SET_CONFIGURATION timeout (ED Head: %x, ED Flags: %x)\n", ed->head_pointer, ed->flags);
			break;
		}
	}

	if((td_status->flags >> 28) == 0x0) {
		printf("Set Configuration Success!\n");
	} else {
		printf("Set Configuration Error: CC = %x\n", (td_status->flags >> 28));
	}
}

extern uint32_t mouse_speed;

void input_mouse() {
	uint8_t *buffer = (uint8_t *)malloc(8, 16);

	ohci_td_t *td_dummy = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	td_dummy->next_td = 0;

	ohci_td_t *td = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	td->flags = (1 << 18) | // DP = OUT? NO, wait.
			(1 << 18) | (1 << 21) | (15 << 28); // Wait, I need to be careful.
	// Let's rewrite flags clearly.
	td->flags = (2 << 18) | // DP = IN (10b)
			(0 << 21) | // DI = 0
			(2 << 24) | // DT = DATA0 (10b)
			(1 << 18) | // Buffer Rounding (bit 18 is also DP[0]?)
			(15 << 28); // CC = Not Accessed

	// Buffer rounding is bit 18. DP is bits 18-19.
	// Bit 18: R (Buffer Rounding)
	// Bits 19-20: DP (Direction PID)
	// Oh! I might have misread the bit positions.
	
	// According to OHCI spec Table 4-3:
	// Bits 18-18: R (Buffer Rounding)
	// Bits 19-20: DP (Direction PID)
	// Bits 21-23: DI (Delay Interrupt)
	// Bits 24-25: T (Data Toggle)
	// Bits 26-27: EC (Error Count)
	// Bits 28-31: CC (Condition Code)
	
	// FIXED OFFSETS based on Table 4-3:
	// R: bit 18
	// DP: bits 19-20 (00=SETUP, 01=OUT, 10=IN)
	// DI: bits 21-23
	// T: bits 24-25 (00=from ED, 01=Reserved, 10=DATA0, 11=DATA1)
	
	td->flags = (1 << 18) | // R = 1
			(2 << 19) | // DP = IN (10b)
			(0 << 21) | // DI = 0
			(2 << 24) | // T = DATA0 (10b)
			(15 << 28); // CC = Not Accessed
	td->current_buffer_ptr = (uint32_t)VIRT_TO_PHYS(buffer);
	td->buffer_end_ptr = td->current_buffer_ptr + 3; // Mouse usually sends 4 bytes
	td->next_td = (uint32_t)VIRT_TO_PHYS(td_dummy);

	ohci_ed_t *ed = (ohci_ed_t *)malloc(sizeof(ohci_ed_t), 16);
	uint32_t addr = 1;
	uint32_t en = 1; // Assuming endpoint 1 for mouse HID
	uint32_t dir = 0; // Use TD direction
	uint32_t speed = mouse_speed;
	uint32_t mps = 8;
	ed->flags = (addr << 0) |
			(en << 7) |
			(dir << 11) |
			(speed << 13) |
			(mps << 16);
	ed->next_ed = 0;
	ed->tail_pointer = (uint32_t)VIRT_TO_PHYS(td_dummy);
	ed->head_pointer = (uint32_t)VIRT_TO_PHYS(td);

	volatile ohci_hhca_t * _hhca = (ohci_hhca_t *)malloc(sizeof(ohci_hhca_t), 256);

	ohci_regs->hhca = (uint32_t)VIRT_TO_PHYS(_hhca);
	ohci_regs->period_current_ed = 0;
	ohci_regs->control |= (1 << 2); // PLE: Periodic List Enable
	ohci_regs->control |= (1 << 4); // PLE = Periodic List Enable
	for(int i = 0; i < 32; i++) {
		_hhca->interrupt_table[i] = (uint32_t)VIRT_TO_PHYS(ed);
	}
	
	printf("Menunggu gerakan mouse...\n");
	while((*(volatile uint32_t*)&td->flags >> 28) == 0xF) {
		// Tunggu interupsi/transfer selesai
	}
	
	if((td->flags >> 28) == 0x0) {
		printf("Mouse digerakkan! Data: X=%d, Y=%d, Buttons=%x\n", (int8_t)buffer[1], (int8_t)buffer[2], buffer[0]);
	} else {
		printf("Input Mouse Error: CC = %x\n", (td->flags >> 28));
	}
}
