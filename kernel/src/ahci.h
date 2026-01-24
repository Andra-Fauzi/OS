#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "pci.h"
#include "terminal.h"
#include "memory.h"

#define ALLOCATE_PRDT(pg) (pg * 256 * 1024)

#define HBA_PORT_DEV_PRESENT 0x3
#define HBA_PORT_IPM_ACTIVE 0x1
#define SATA_SIG_ATA    0x00000101
#define SATA_SIG_ATAPI  0xEB140101
#define SATA_SIG_SEMB   0xC33C0101
#define SATA_SIG_PM     0x96690101

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM   3
#define AHCI_DEV_SATAPI 4

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

#define FIS_TYPE_REG_H2D	0x27	// Register FIS - Host to Device
#define FIS_TYPE_REG_D2H	0x34	// Register FIS - Device to Host
#define FIS_TYPE_DMA_ACT	0x39	// DMA Activate FIS - Device to Host
#define FIS_TYPE_DMA_SETUP	0x41	// DMA Setup FIS - Bidirectional
#define FIS_TYPE_DATA		0x46	// Data FIS - Bidirectional
#define FIS_TYPE_BIST		0x58	// BIST Activate FIS - Bidirectional
#define FIS_TYPE_PIO_SETUP	0x5F	// PIO Setup FIS - Device to Host
#define FIS_TYPE_DEV_BITS	0xA1	// Set Device Bits FIS - Device to Host

typedef struct {
	uint8_t  fis_type;	// FIS_TYPE_REG_H2D
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:3;		// Reserved
	uint8_t  c:1;		// 1: Command, 0: Control
	uint8_t  command;	// Command register
	uint8_t  featurel;	// Feature register, 7:0
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  featureh;	// Feature register, 15:8
	uint8_t  countl;		// Count register, 7:0
	uint8_t  counth;		// Count register, 15:8
	uint8_t  icc;		// Isochronous command completion
	uint8_t  control;	// Control register
	uint8_t  rsv1[4];	// Reserved
} fis_reg_h2d_t;

typedef struct {
	uint8_t  fis_type;	// FIS_TYPE_REG_D2H
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:2;		// Reserved
	uint8_t  i:1;		// Interrupt bit
	uint8_t  rsv1:1;		// Reserved
	uint8_t  status;		// Status register
	uint8_t  error;		// Error register
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  rsv2;		// Reserved
	uint8_t  countl;		// Count register, 7:0
	uint8_t  counth;		// Count register, 15:8
	uint8_t  rsv3[2];	// Reserved
	uint8_t  rsv4[4];	// Reserved
} fis_reg_d2h_t;

typedef struct {
	uint8_t  fis_type;	// FIS_TYPE_DATA
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:4;		// Reserved
	uint8_t  rsv1[2];	// Reserved
	uint32_t data[1];	// Variable length data
} fis_data_t;

typedef struct {
	uint8_t  fis_type;	// FIS_TYPE_PIO_SETUP
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:1;		// Reserved
	uint8_t  d:1;		// Data transfer direction, 1 - device to host
	uint8_t  i:1;		// Interrupt bit
	uint8_t  rsv1:1;
	uint8_t  status;		// Status register
	uint8_t  error;		// Error register
	uint8_t  lba0;		// LBA low register, 7:0
	uint8_t  lba1;		// LBA mid register, 15:8
	uint8_t  lba2;		// LBA high register, 23:16
	uint8_t  device;		// Device register
	uint8_t  lba3;		// LBA register, 31:24
	uint8_t  lba4;		// LBA register, 39:32
	uint8_t  lba5;		// LBA register, 47:40
	uint8_t  rsv2;		// Reserved
	uint8_t  countl;		// Count register, 7:0
	uint8_t  counth;		// Count register, 15:8
	uint8_t  rsv3;		// Reserved
	uint8_t  e_status;	// New value of status register (error)
	uint8_t  tc;		// Transfer count
	uint8_t  rsv4[2];	// Reserved
} fis_pio_setup_t;

typedef struct {
	uint8_t  fis_type;	// FIS_TYPE_DMA_SETUP
	uint8_t  pmport:4;	// Port multiplier
	uint8_t  rsv0:1;		// Reserved
	uint8_t  d:1;		// Data transfer direction, 1 - device to host
	uint8_t  i:1;		// Interrupt bit
	uint8_t  a:1;		// Auto-activate. Specifies if DMA Activate FIS is needed
    uint8_t  rsv1[2];       // Reserved
    uint64_t DMAbufferID;   // DMA Buffer Identifier. Used to Identify DMA buffer in host memory. SATA Spec says host specific and not in Spec. Generic Host Control Spec says 64 bit address.
    uint32_t rsv2;          // More reserved
    uint32_t DMAbufOffset;  // Byte offset into buffer. First 2 bits must be 0
    uint32_t TransferCount; // Number of bytes to transfer. Bit 0 must be 0
    uint32_t rsv3;          // Reserved
} fis_dma_setup_t;

typedef volatile struct {
	// 0x00
	fis_dma_setup_t	dsfis;		// DMA Setup FIS
	uint8_t		pad0[4];

	// 0x20
	fis_pio_setup_t	psfis;		// PIO Setup FIS
	uint8_t		pad1[12];

	// 0x40
	fis_reg_d2h_t	rfis;		// Register – Device to Host FIS
	uint8_t		pad2[4];

	// 0x58
	uint8_t	sdbfis[8];		// Set Device Bit FIS // TODO: Struct?
	// 0x60
	uint8_t	ufis[64];
	// 0xA0
	uint8_t	rsv[0x100-0xA0];
} hba_fis_t;

typedef volatile struct {
    uint32_t clb;       // 0x00, Command List Base Address
    uint32_t clbu;      // 0x04, Command List Base Address Upper
    uint32_t fb;        // 0x08, FIS Base Address
    uint32_t fbu;       // 0x0C, FIS Base Address Upper
    uint32_t is;        // 0x10, Interrupt Status
    uint32_t ie;        // 0x14, Interrupt Enable
    uint32_t cmd;       // 0x18, Command and Status
    uint32_t rsv0;      // 0x1C, Reserved
    uint32_t tfd;       // 0x20, Task File Data
    uint32_t sig;       // 0x24, Signature
    uint32_t ssts;      // 0x28, SATA Status (SStatus)
    uint32_t sctl;      // 0x2C, SATA Control (SControl)
    uint32_t serr;      // 0x30, SATA Error (SError)
    uint32_t sact;      // 0x34, SATA Active (SActive)
    uint32_t ci;        // 0x38, Command Issue
    uint32_t sntf;      // 0x3C, SATA Notification (SNotification)
    uint32_t fbs;       // 0x40, FIS-based Switching Control
    uint32_t rsv1[11];  // 0x44 ~ 0x6F, Reserved
    uint32_t vendor[4]; // 0x70 ~ 0x7F, Vendor specific
} hba_port_t;

typedef volatile struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  rsv[0x100 - 0x2C];   // padding hingga offset port 0x100
    hba_port_t ports[32];        // maksimal 32 port
} hba_mem_t;

typedef volatile struct {
    uint16_t cfl:5;     // Command FIS length in DWORDS
    uint16_t a:1;       // ATAPI
    uint16_t w:1;       // Write flag
    uint16_t p:1;       // Prefetchable
    uint16_t r:1;       // Reset
    uint16_t b:1;       // BIST
    uint16_t c:1;       // Clear busy upon R_OK
    uint16_t rsv0:1;
    uint16_t pmp:4;     // Port multiplier
    uint16_t prdtl;     // Number of PRDT entries
    uint32_t prdbc;     // PRD byte count transferred
    uint32_t ctba;      // Command Table Base Address
    uint32_t ctbau;     // Command Table Base Address Upper
    uint32_t rsv1[4];
} hba_cmd_header_t;

typedef volatile struct {
    uint8_t cfis[64];        // Command FIS
    uint8_t acmd[16];        // ATAPI command
    uint8_t rsv[48];
    struct {
        uint32_t dba;        // Data Base Address
        uint32_t dbau;       // Upper 32-bit
        uint32_t rsv0;
        uint32_t dbc:22;     // Byte count
        uint32_t rsv1:9;
        uint32_t i:1;        // Interrupt on completion
    } prdt_entry[1];         // Bisa lebih dari 1
} hba_cmd_table_t;

extern volatile hba_port_t *sataport;
void setup_ahci(void);
bool ahci_read(volatile hba_port_t *port, uint32_t start_low, uint32_t start_high, uint32_t count, uint16_t *buf);
void ahci_write(volatile hba_port_t *port, uint32_t start_low, uint32_t start_high, uint32_t count, uint16_t *buf);


