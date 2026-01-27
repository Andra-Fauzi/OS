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
#define OHCI_USB_OPERATIONAL (2 << 6) // value 10b for operational
#define OHCI_USB_SUSPEND (3 << 6)

volatile ohci_regs_t *ohci_regs;
volatile ohci_hcca_t *hcca;

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

	val &= ~OHCI_CTRL_HCFS; // clear bits 6-7
	val |= OHCI_USB_OPERATIONAL; // set to bits 10b
	val |= (1 << 2); // PLE: Periodic List Enable
	val |= (1 << 4); // CLE: Control List Enable
	
	ohci_regs->control = val;

	hcca = (volatile ohci_hcca_t *)malloc(sizeof(ohci_hcca_t), 256);
	ohci_regs->hcca = (uint32_t)VIRT_TO_PHYS(hcca);
	
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
	
	// --- Phase 1: SET_ADDRESS 1 ---
	setup_buffer[0] = 0x00; // Standard, Device
	setup_buffer[1] = 0x05; // SET_ADDRESS
	setup_buffer[2] = 0x01; // Address 1
	for(int i=3; i<8; i++) setup_buffer[i] = 0;

	ohci_td_t *td_dummy = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	ohci_td_t *td_setup = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	ohci_td_t *td_status = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	ohci_ed_t *ed = (ohci_ed_t *)malloc(sizeof(ohci_ed_t), 16);

	td_setup->flags = (0 << 19) | (2 << 24) | (15 << 28); // DP=SETUP, T=DATA0
	td_setup->current_buffer_ptr = (uint32_t)VIRT_TO_PHYS(setup_buffer);
	td_setup->buffer_end_ptr = td_setup->current_buffer_ptr + 7;
	td_setup->next_td = (uint32_t)VIRT_TO_PHYS(td_status);

	td_status->flags = (1 << 18) | (2 << 19) | (3 << 24) | (15 << 28); // R=1, DP=IN, T=DATA1
	td_status->current_buffer_ptr = 0;
	td_status->buffer_end_ptr = 0;
	td_status->next_td = (uint32_t)VIRT_TO_PHYS(td_dummy);

	ed->flags = (0 << 0) | (0 << 7) | (mouse_speed << 13) | (8 << 16); // Addr 0, Endp 0
	ed->head_pointer = (uint32_t)VIRT_TO_PHYS(td_setup);
	ed->tail_pointer = (uint32_t)VIRT_TO_PHYS(td_dummy);
	ed->next_ed = 0;

	ohci_regs->control_head_ed = (uint32_t)VIRT_TO_PHYS(ed);
	ohci_regs->control_current_ed = 0;
	ohci_regs->command_status |= (1 << 1); // ControlListFilled

	volatile uint32_t timeout = 50000000;
	while((td_status->flags >> 28) == 0xF && (td_setup->flags >> 28) == 0xF) {
		timeout--;
		if(timeout == 0) break;
	}
	
	if((td_status->flags >> 28) == 0x0) {
		printf("Set Address Success!\n");
	} else {
		printf("Set Address Fail: Setup CC=%x, Status CC=%x\n", td_setup->flags >> 28, td_status->flags >> 28);
		return;
	}

	// Wait 10ms for address setting to settle
	for(volatile int j = 0; j < 50000000; j++);

	// --- Phase 2: SET_CONFIGURATION 1 ---
	uint8_t *setup_buffer2 = (uint8_t *)malloc(8, 16);
	setup_buffer2[0] = 0x00;
	setup_buffer2[1] = 0x09; // SET_CONFIGURATION
	setup_buffer2[2] = 0x01; // Config 1
	for(int i=3; i<8; i++) setup_buffer2[i] = 0;

	// Use fresh TDs for second request to be safe
	ohci_td_t *td_dummy2 = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	ohci_td_t *td_setup2 = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	ohci_td_t *td_status2 = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);

	td_setup2->flags = (0 << 19) | (2 << 24) | (15 << 28);
	td_setup2->current_buffer_ptr = (uint32_t)VIRT_TO_PHYS(setup_buffer2);
	td_setup2->buffer_end_ptr = td_setup2->current_buffer_ptr + 7;
	td_setup2->next_td = (uint32_t)VIRT_TO_PHYS(td_status2);

	td_status2->flags = (1 << 18) | (2 << 19) | (3 << 24) | (15 << 28);
	td_status2->current_buffer_ptr = 0;
	td_status2->buffer_end_ptr = 0;
	td_status2->next_td = (uint32_t)VIRT_TO_PHYS(td_dummy2);

	ed->flags = (1 << 0) | (0 << 7) | (mouse_speed << 13) | (8 << 16); // Addr 1
	ed->head_pointer = (uint32_t)VIRT_TO_PHYS(td_setup2);
	ed->tail_pointer = (uint32_t)VIRT_TO_PHYS(td_dummy2);

	ohci_regs->control_current_ed = 0;
	ohci_regs->command_status |= (1 << 1);

	timeout = 20000000;
	while((td_status2->flags >> 28) == 0xF && (td_setup2->flags >> 28) == 0xF) {
		timeout--;
		if(timeout == 0) break;
	}

	if((td_status2->flags >> 28) == 0x0) {
		printf("Set Configuration Success!\n");
	} else {
		printf("Set Configuration Fail: Setup CC=%x, Status CC=%x\n", td_setup2->flags >> 28, td_status2->flags >> 28);
		printf("ED Status: Head=%x, Current=%x\n", ed->head_pointer, ohci_regs->control_current_ed);
	}
}

extern uint32_t mouse_speed;

static ohci_td_t *td_dummy = NULL;
static ohci_td_t *td = NULL;
static ohci_ed_t *ed = NULL;
static uint8_t *buffer = NULL;

// 00 = from ED, 10 = DATA0, 11 = DATA1
static int data_toogle = 0;

void input_mouse() {
	if(buffer == NULL) {
		buffer = (uint8_t *)malloc(8, 16);
	}

	if(td_dummy == NULL) {
		td_dummy = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	}
	td_dummy->next_td = 0;

	if(td == NULL) {
		td = (ohci_td_t *)malloc(sizeof(ohci_td_t), 16);
	}
	
	// According to OHCI spec Table 4-3:
	// Bits 18-18: R (Buffer Rounding)
	// Bits 19-20: DP (Direction PID)
	// Bits 21-23: DI (Delay Interrupt)
	// Bits 24-25: T (Data Toggle)
	// Bits 26-27: EC (Error Count)
	// Bits 28-31: CC (Condition Code)
	
	// R: bit 18
	// DP: bits 19-20 (00=SETUP, 01=OUT, 10=IN)
	// DI: bits 21-23
	// T: bits 24-25 (00=from ED, 01=Reserved, 10=DATA0, 11=DATA1)
	
	td->flags = (1 << 18) | // R = 1
			(2 << 19) | // DP = IN (10b)
			(0 << 21) | // DI = 0
			(data_toogle << 24) | // T = DATA0 (10b)
			(15 << 28); // CC = Not Accessed
	td->current_buffer_ptr = (uint32_t)VIRT_TO_PHYS(buffer);
	td->buffer_end_ptr = td->current_buffer_ptr + 3; // Mouse usually sends 4 bytes
	td->next_td = (uint32_t)VIRT_TO_PHYS(td_dummy);

	if(ed == NULL) {
		ed = (ohci_ed_t *)malloc(sizeof(ohci_ed_t), 16);
	}
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

	// Use the virtual pointer we saved in init_OHCI!
	ohci_regs->period_current_ed = 0;
	ohci_regs->control |= (1 << 2); // PLE: Periodic List Enable
	ohci_regs->control |= (1 << 4); // CLE: Control List Enable
	for(int i = 0; i < 32; i++) {
		hcca->interrupt_table[i] = (uint32_t)VIRT_TO_PHYS(ed);
	}
	
	// printf("Waiting for mouse movement...\n");
	int timeout = 10000000;
	while((*(volatile uint32_t*)&td->flags >> 28) == 0xF) {
		timeout--;
		if(timeout <= 0) break;
	}
	
	if((td->flags >> 28) == 0x0) {
		static uint32_t x_d = 0;
		static uint32_t y_d = 0;
		int8_t x = buffer[1];
		int8_t y = buffer[2];
		// for(int y_i = 0; y_i < 8; y_i++) {
		// 	for(int x_i = 0; x_i < 8; x_i++) {
		// 		draw_pixel(x_d + x_i, y_d + y_i, 0x0);
		// 	}
		// }
		x_d += x;
		y_d += y;
		extern volatile uint64_t framebuffer_height;
		extern volatile uint64_t framebuffer_width;
		if(x_d <= 0) {
			x_d = 0;
		}
		else if(x_d >= framebuffer_width) {
			x_d = framebuffer_width - 1;
		}
		if(y_d <= 0) {
			y_d = 0;
		}
		else if(y_d >= framebuffer_height) {
			y_d = framebuffer_height - 1;
		}
		for(int y_i = 0; y_i < 8; y_i++) {
			for(int x_i = 0; x_i < 8; x_i++) {
				draw_pixel(x_d + x_i, y_d + y_i, 0xFFFFFFFF);
			}
		}
		uint8_t buttons = buffer[0];
		printf("Mouse moved! Data: X=%d, Y=%d, Buttons=%x\n", x, y, buttons);
		// data_toogle = (data_toogle == 2) ? 3 : 2;
	} else {
		// printf("Input Mouse Error: CC = %x\n", (td->flags >> 28));
	}
}
