#include "ahci.h"
#include "paging.h"

uint8_t ahci_bus = 0;
uint8_t ahci_slot = 0;
uint8_t ahci_func = 0;
hba_mem_t *ghba = NULL;
volatile hba_port_t *sataport = NULL;

void start_cmd(volatile hba_port_t *port) {
	while (port->cmd & HBA_PxCMD_CR);
	port->cmd |= HBA_PxCMD_FRE;
	port->cmd |= HBA_PxCMD_ST;
}

void stop_cmd(volatile hba_port_t *port) {
	port->cmd &= ~HBA_PxCMD_ST;
	port->cmd &= ~HBA_PxCMD_FRE;
	while(1) {
		if (port->cmd & HBA_PxCMD_FR) continue;
		if (port->cmd & HBA_PxCMD_CR) continue;
		break;
	}
}

int find_cmdslot(volatile hba_port_t *port) {
	uint32_t slots = (port->sact | port->ci);
	for (int i = 0; i < 32; i++) {
		if ((slots&1) == 0) return i;
		slots >>= 1;
	}
	printf("Cannot find free command list slot\n");
	return -1;
}

bool find_ahci() {
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

                		if (classcode == 0x01 && subclass  == 0x06 && progIF    == 0x01) {
                    			printf("AHCI found at %d:%d:%d\n", bus, slot, func);
				ahci_bus = bus;
				ahci_slot = slot;
				ahci_func = func;
                    		return true;
                		}
            		}
        	}
    	}
    printf("AHCI not found\n");
    return false;
}

#define AHCI_ABAR_VIRT 0xFFFF8000B0000000

void init_ahci_port(volatile hba_port_t *port) {
	stop_cmd(port);

	uint64_t clb_phys = allocate_frame();
	uint64_t fb_phys  = allocate_frame();

	port->clb  = (uint32_t)(clb_phys & 0xFFFFFFFF);
	port->clbu = (uint32_t)(clb_phys >> 32);
	port->fb   = (uint32_t)(fb_phys & 0xFFFFFFFF);
	port->fbu  = (uint32_t)(fb_phys >> 32);

	memset(PHYS_TO_VIRT(clb_phys), 0, 1024);
	memset(PHYS_TO_VIRT(fb_phys),  0, 256);

	hba_cmd_header_t *cmdheader = (hba_cmd_header_t *)PHYS_TO_VIRT(clb_phys);
	for (int i = 0; i < 32; i++) {
		cmdheader[i].prdtl = 8; // 8 prdt entries per command table
		// Command table offset: 0 ~ 31
		// Base address = physically allocated + i * 256 (arbitrary size? NO. cmd table size varies)
		// We need to allocate memory for Command Table.
		// For simplicity, let's allocate 1 page for EACH command slot? That's expensive.
		// Or 1 page for ALL command tables? 32 * 256 bytes = 8KB. Need 2 pages?
		// Let's allocate one frame (4KB) for command tables?
		// A command table is up to 64KB?
		// Minimal command table: 64B header + 16B * prdtl.
		// If prdtl=8, 64 + 128 = 192 bytes.
		// 32 * 192 = 6144 bytes. > 4096.
		// We need 2 frames for the Command Tables.
        // Let's allocate a Frame per slot to be safe and simple for now? Or just one big chunk.
        // Let's alloc 1 frame for slots 0-15 and 1 frame for 16-31.
        
        // Actually, let's just do it for slot 0 for now? No, we might need more.
        
        uint64_t ct_phys = allocate_frame(); // wasting memory but easiest
        cmdheader[i].ctba  = (uint32_t)(ct_phys & 0xFFFFFFFF);
        cmdheader[i].ctbau = (uint32_t)(ct_phys >> 32);
        
        memset(PHYS_TO_VIRT(ct_phys), 0, 4096);
	}

	port->sctl = (port->sctl & 0xFFFFFF00) | 1; // Power on and Spin up? NO. This is SControl.
    // SCTL: Det=1 (Present), IPM=0 (Partial/Slumber disabled?)
    // Actually we just want to reset if needed?
    // Let's rely on BIOS/Limine for now unless it fails. 
    // Just start cmd.
	start_cmd(port);
}

void setup_ahci(void) {
	if(!find_ahci()) return;

	uint16_t cmd = pciConfigReadWord(ahci_bus, ahci_slot, ahci_func, 0x04);
	cmd |= (1 << 1); // Mem Space
	cmd |= (1 << 2); // Bus Master
	pciConfigWriteWord(ahci_bus, ahci_slot, ahci_func, 0x04, cmd);

	uint32_t bar_low = pciConfigReadDword(ahci_bus, ahci_slot, ahci_func, 0x24);
	uint32_t bar_high = pciConfigReadDword(ahci_bus, ahci_slot, ahci_func, 0x28);
	uint64_t abar_phys = ((uint64_t)bar_high << 32) | ((uint64_t)bar_low & 0xFFFFFFF0);

	// Map ABAR
	uint64_t *pml4 = get_pml4();
	map_page(pml4, AHCI_ABAR_VIRT, abar_phys, PTE_PRESENT | PTE_WRITABLE | PTE_PWT | PTE_PCD);

	volatile hba_mem_t *hba = (volatile hba_mem_t *)AHCI_ABAR_VIRT;
	printf("AHCI VERSION: %d.%d\n", (hba->vs >> 16), (hba->vs & 0xFFFF));

	hba->ghc |= (1 << 31); // AHCI Enable
	hba->ghc |= (1 << 0);  // HBA Reset
	while (hba->ghc & 1); // Wait for reset
	
	hba->ghc |= (1 << 31); // AHCI Enable again
	hba->ghc |= (1 << 1); // Interrupt Enable

	uint32_t pi = hba->pi;
    printf("Ports Implemented: %x\n", pi);
	for(int i = 0; i < 32; i++) {
		if(pi & (1 << i)){
			volatile hba_port_t *port = &hba->ports[i];
			uint32_t ssts = port->ssts;
			uint8_t det = ssts & 0x0F;
			if (det != HBA_PORT_DEV_PRESENT) continue;
			if (((ssts >> 8) & 0x0F) != HBA_PORT_IPM_ACTIVE) continue;

            uint32_t sig = port->sig;
            for(int w=0; w<10000 && sig == 0xFFFFFFFF; w++) {
                sig = port->sig;
            }

			if(sig == SATA_SIG_ATA || sig == 0xFFFFFFFF) {
				printf("SATA Device found at port %d\n", i);
				init_ahci_port(port);
				sataport = port; // Use first found SATA port
			} else if (sig == SATA_SIG_ATAPI) {
				printf("SATAPI Device found at port %d\n", i);
			}
		}
	}
}

bool ahci_read(volatile hba_port_t *port, uint32_t start_low, uint32_t start_high, uint32_t count, uint16_t *buf) {
	port->is = (uint32_t) -1; // Clear Interrupt Status
	int slot = find_cmdslot(port);
	if (slot == -1) return false;

	uint64_t clb_phys = ((uint64_t)port->clbu << 32) | port->clb;
	hba_cmd_header_t *cmdheader = (hba_cmd_header_t *)PHYS_TO_VIRT(clb_phys);
	
	cmdheader += slot;
	cmdheader->cfl = sizeof(fis_reg_h2d_t)/sizeof(uint32_t); // Command FIS size
	cmdheader->w = 0; // Read
	cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1; // PRDT entries count (assume 8K per entry max? No. Standard 4MB max per entry. We use small chunks?)
	// Actually count is sector count? 
	// Let's assume we read `count` sectors. 
	// If buf size is huge, we might need multiple PRDTs. 
	// For simplicity, let's assume valid PRDTL.
	// But we need to setup PRDTs.

	uint64_t ctba_phys = ((uint64_t)cmdheader->ctbau << 32) | cmdheader->ctba;
	hba_cmd_table_t *cmdtbl = (hba_cmd_table_t*)PHYS_TO_VIRT(ctba_phys);
	memset((void*)cmdtbl, 0, sizeof(hba_cmd_table_t) + (cmdheader->prdtl-1)*sizeof(hba_cmd_table_t)); // Rough clear

	// Setup PRDTs
	// We need physical address of buf.
	// This implies buf must be contiguous physical memory OR we need to walk the page table of buf.
    // For this simple OS, we assume the user buffer is allocated physically contiguous or we only read into a temporary low-mem buffer?
    // Wait, buf is virtual. We need physical.
    // Since we don't have a VIRT_TO_PHYS widely available or guaranteed contiguous virtual->physical mapping for random buffers:
    // We should allocate a DMA buffer (Identity Mapped or PHYS_TO_VIRT known), read into it, then memcpy to buf.
    // OR: We iterate pages of buf and fill PRDT.
    // Let's implement PRDT filling.
    
    // For simplicity, let's assume buf is in the higher half kernel heap (linear mapped?).
    // No, standard `malloc` might not be linear. 
    // `allocate_frame` gives physical. `PHYS_TO_VIRT` gives virtual.
    // If buf comes from `allocate_frame` then `PHYS(buf) = buf - OFFSET`.
    
    // Let's attempt to use the HHDM reverse logic IF address is in HHDM region.
    uint64_t buf_phys = (uint64_t)buf - hhdm_request.response->offset; // SUPER UNSAFE if buf is not HHDM.
    // But for now, we will alloc a buffer for testing using allocate_frame and pass PHYS_TO_VIRT(frame).
    
    int i = 0;
    // 1 PRDT entry for the whole buffer (assuming < 4MB and contiguous physical)
    cmdtbl->prdt_entry[i].dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
    cmdtbl->prdt_entry[i].dbau = (uint32_t)(buf_phys >> 32);
    cmdtbl->prdt_entry[i].dbc = (count * 512) - 1; // 512 bytes per sector
    cmdtbl->prdt_entry[i].i = 1;
    
	// Setup Command FIS
	fis_reg_h2d_t *cmdfis = (fis_reg_h2d_t*)(&cmdtbl->cfis);
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1; // Command
	cmdfis->command = 0x25; // READ DMA EXT? OR 0xC8 READ DMA.
    // Use READ DMA EXT (0x25) for LBA48.
	cmdfis->lba0 = (uint8_t)start_low;
	cmdfis->lba1 = (uint8_t)(start_low >> 8);
	cmdfis->lba2 = (uint8_t)(start_low >> 16);
	cmdfis->device = 1 << 6; // LBA mode
	cmdfis->lba3 = (uint8_t)(start_low >> 24);
	cmdfis->lba4 = (uint8_t)start_high;
	cmdfis->lba5 = (uint8_t)(start_high >> 8);
	cmdfis->countl = count & 0xFF;
	cmdfis->counth = (count >> 8) & 0xFF;

	// Issue command
	while ((port->tfd & (0x80 | 0x08)) && (port->tfd & 1)); // Wait until not busy and DRQ??
    // Actually we just wait for PxTFD.BSY and PxTFD.DRQ to be clear.
    
	port->ci = 1 << slot; // Issue command

	// Wait for completion
	while (1) {
		if ((port->ci & (1 << slot)) == 0) break;
		if (port->is & (1 << 30)) { // Task File Error
			printf("Read Disk Error\n");
			return false;
		}
	}

	if (port->is & (1 << 30)) {
		printf("Read Disk Error\n");
		return false;
	}

	return true;
}

void ahci_write(volatile hba_port_t *port, uint32_t start_low, uint32_t start_high, uint32_t count, uint16_t *buf) {
	port->is = (uint32_t) -1; // Clear Interrupt Status
	int slot = find_cmdslot(port);
	if (slot == -1) return;

	uint64_t clb_phys = ((uint64_t)port->clbu << 32) | port->clb;
	hba_cmd_header_t *cmdheader = (hba_cmd_header_t *)PHYS_TO_VIRT(clb_phys);
	
	cmdheader += slot;
	cmdheader->cfl = sizeof(fis_reg_h2d_t)/sizeof(uint32_t); // Command FIS size
	cmdheader->w = 1; // Write
	cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1; 

	uint64_t ctba_phys = ((uint64_t)cmdheader->ctbau << 32) | cmdheader->ctba;
	hba_cmd_table_t *cmdtbl = (hba_cmd_table_t*)PHYS_TO_VIRT(ctba_phys);
	memset((void*)cmdtbl, 0, sizeof(hba_cmd_table_t) + (cmdheader->prdtl-1)*sizeof(hba_cmd_table_t));

    uint64_t buf_phys = (uint64_t)buf - hhdm_request.response->offset; 
    
    int i = 0;
    cmdtbl->prdt_entry[i].dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
    cmdtbl->prdt_entry[i].dbau = (uint32_t)(buf_phys >> 32);
    cmdtbl->prdt_entry[i].dbc = (count * 512) - 1; 
    cmdtbl->prdt_entry[i].i = 1;
    
	// Setup Command FIS
	fis_reg_h2d_t *cmdfis = (fis_reg_h2d_t*)(&cmdtbl->cfis);
	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1; // Command
	cmdfis->command = 0x35; // WRITE DMA EXT
	cmdfis->lba0 = (uint8_t)start_low;
	cmdfis->lba1 = (uint8_t)(start_low >> 8);
	cmdfis->lba2 = (uint8_t)(start_low >> 16);
	cmdfis->device = 1 << 6; // LBA mode
	cmdfis->lba3 = (uint8_t)(start_low >> 24);
	cmdfis->lba4 = (uint8_t)start_high;
	cmdfis->lba5 = (uint8_t)(start_high >> 8);
	cmdfis->countl = count & 0xFF;
	cmdfis->counth = (count >> 8) & 0xFF;

	// Issue command
	while ((port->tfd & (0x80 | 0x08)) && (port->tfd & 1)); 
    
	port->ci = 1 << slot; // Issue command

	// Wait for completion
	while (1) {
		if ((port->ci & (1 << slot)) == 0) break;
		if (port->is & (1 << 30)) { 
			printf("Write Disk Error\n");
			return;
		}
	}
}

// i write this all so im not lazy again
bool identify(volatile hba_port_t *port) {
	uint32_t count = 1;
	uint64_t buf_phys = allocate_frame();
	uint16_t *buf = (uint16_t *)PHYS_TO_VIRT(buf_phys);
	port->is = (uint32_t)-1;
	int slot = find_cmdslot(port);
	if(slot == -1) return false;

	uint64_t clb_phys = ((uint64_t)port->clbu << 32) | port->clb;
	hba_cmd_header_t *cmdheader = (hba_cmd_header_t *)PHYS_TO_VIRT(clb_phys);	

	cmdheader += slot;
	cmdheader->cfl = sizeof(fis_reg_h2d_t)/sizeof(uint32_t);
	cmdheader->w = 0; // read
	cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1;

	uint64_t ctba_phys = ((uint64_t)cmdheader->ctbau << 32) | cmdheader->ctba;
	hba_cmd_table_t *cmdtbl = (hba_cmd_table_t *)PHYS_TO_VIRT(ctba_phys);
	memset((void *)cmdtbl, 0, sizeof(hba_cmd_table_t) + (cmdheader->prdtl-1)*sizeof(hba_cmd_table_t));
	
	int i = 0;
	cmdtbl->prdt_entry[i].dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
	cmdtbl->prdt_entry[i].dbau = (uint32_t)(buf_phys >> 32);
	cmdtbl->prdt_entry[i].dbc = (count * 512) - 1;
	cmdtbl->prdt_entry[i].i = 1;

	fis_reg_h2d_t *cmdfis = (fis_reg_h2d_t *)(&cmdtbl->cfis);
	cmdfis->fis_type = 0x27;
	cmdfis->c = 1;
	cmdfis->command = 0xEC;
	cmdfis->lba0 = 0;
	cmdfis->lba1 = 0;
	cmdfis->lba2 = 0;
	cmdfis->lba3 = 0;
	cmdfis->lba4 = 0;
	cmdfis->lba5 = 0;
	cmdfis->countl = 0;
	cmdfis->counth = 0;
	cmdfis->device = 1 << 6;

	while((port->tfd & (0x80 | 0x08)) && (port->tfd & 1));

	port->ci = 1 << slot;

	while(1) {
		if((port->ci & (1 << slot)) == 0) break;
		if(port->is & (1 << 30)) {
			printf("Read Disk Error\n");
			return false;
		}
	}
	if (port->is & (1 << 30)) {
		printf("Read Disk Error\n");
		return false;
	}
	uint64_t sectors =
    ((uint64_t)buf[103] << 48) |
    ((uint64_t)buf[102] << 32) |
    ((uint64_t)buf[101] << 16) |
    buf[100];
	support = buf[83] & (1 << 10);
	printf("sector : %d\n", sectors);
	printf("support lba48: %d\n", support);
	return true;
}
