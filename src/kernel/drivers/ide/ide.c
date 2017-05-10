#include "ide.h"
#include <std/common.h>
#include <std/math.h>

#define IO_WAIT_DELAY 10

struct IDEChannelRegisters {
	unsigned short base;  // I/O Base.
	unsigned short ctrl;  // Control Base
	unsigned short bmide; // Bus Master IDE
	unsigned char  nIEN;  // nIEN (No Interrupt);
} channels[2];

int package[4];

unsigned char ide_buf[2048] = {0};
unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

struct ide_device {
	unsigned char  Reserved;    // 0 (Empty) or 1 (This Drive really exists).
	unsigned char  Channel;     // 0 (Primary Channel) or 1 (Secondary Channel).
	unsigned char  Drive;       // 0 (Master Drive) or 1 (Slave Drive).
	unsigned short Type;        // 0: ATA, 1:ATAPI.
	unsigned short Signature;   // Drive Signature
	unsigned short Capabilities;// Features.
	unsigned int   CommandSets; // Command Sets Supported.
	unsigned int   Size;        // Size in Sectors.
	unsigned char  Model[41];   // Model in string.
} ide_devices[4];

#define SECTOR_SIZE 512

unsigned char ide_read(unsigned char channel, unsigned char reg) {
	unsigned char result;
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	if (reg < 0x08)
		result = inb(channels[channel].base + reg - 0x00);
	else if (reg < 0x0C)
		result = inb(channels[channel].base  + reg - 0x06);
	else if (reg < 0x0E)
		result = inb(channels[channel].ctrl  + reg - 0x0A);
	else if (reg < 0x16)
		result = inb(channels[channel].bmide + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
	return result;
}

void ide_write(unsigned char channel, unsigned char reg, unsigned char data) {
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	if (reg < 0x08)
		outb(channels[channel].base  + reg - 0x00, data);
	else if (reg < 0x0C)
		outb(channels[channel].base  + reg - 0x06, data);
	else if (reg < 0x0E)
		outb(channels[channel].ctrl  + reg - 0x0A, data);
	else if (reg < 0x16)
		outb(channels[channel].bmide + reg - 0x0E, data);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

void ide_read_buffer(unsigned char channel, unsigned char reg, unsigned int buffer,
		unsigned int quads) {
	/* WARNING: This code contains a serious bug. The inline assembly trashes ES and
	 *           ESP for all of the code the compiler generates between the inline
	 *           assembly blocks.
	 */
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	if (reg < 0x08)
		insl(channels[channel].base  + reg - 0x00, buffer, quads);
	else if (reg < 0x0C)
		insl(channels[channel].base  + reg - 0x06, buffer, quads);
	else if (reg < 0x0E)
		insl(channels[channel].ctrl  + reg - 0x0A, buffer, quads);
	else if (reg < 0x16)
		insl(channels[channel].bmide + reg - 0x0E, buffer, quads);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

unsigned char ide_polling(unsigned char channel, unsigned int advanced_check) {

	// (I) Delay 400 nanosecond for BSY to be set:
	// -------------------------------------------------
	for(int i = 0; i < 4; i++)
		ide_read(channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes 100ns; loop four times.

	// (II) Wait for BSY to be cleared:
	// -------------------------------------------------
	while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
		; // Wait for BSY to be zero.

	if (advanced_check) {
		unsigned char state = ide_read(channel, ATA_REG_STATUS); // Read Status Register.

		// (III) Check For Errors:
		// -------------------------------------------------
		if (state & ATA_SR_ERR)
			return 2; // Error.

		// (IV) Check If Device fault:
		// -------------------------------------------------
		if (state & ATA_SR_DF)
			return 1; // Device Fault.

		// (V) Check DRQ:
		// -------------------------------------------------
		// BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
		if ((state & ATA_SR_DRQ) == 0)
			return 3; // DRQ should be set

	}

	return 0; // No Error.

}

unsigned char ide_print_error(unsigned int drive, unsigned char err) {
	if (err == 0)
		return err;

	printf("IDE:");
	if (err == 1) {printf("- Device Fault\n     "); err = 19;}
	else if (err == 2) {
		unsigned char st = ide_read(ide_devices[drive].Channel, ATA_REG_ERROR);
		if (st & ATA_ER_AMNF)   {printf("- No Address Mark Found\n     ");   err = 7;}
		if (st & ATA_ER_TK0NF)   {printf("- No Media or Media Error\n     ");   err = 3;}
		if (st & ATA_ER_ABRT)   {printf("- Command Aborted\n     ");      err = 20;}
		if (st & ATA_ER_MCR)   {printf("- No Media or Media Error\n     ");   err = 3;}
		if (st & ATA_ER_IDNF)   {printf("- ID mark not Found\n     ");      err = 21;}
		if (st & ATA_ER_MC)   {printf("- No Media or Media Error\n     ");   err = 3;}
		if (st & ATA_ER_UNC)   {printf("- Uncorrectable Data Error\n     ");   err = 22;}
		if (st & ATA_ER_BBK)   {printf("- Bad Sectors\n     ");       err = 13;}
	} else  if (err == 3)           {printf("- Reads Nothing\n     "); err = 23;}
	else  if (err == 4)  {printf("- Write Protected\n     "); err = 8;}
	printf("- [%s %s] %s\n",
			(const char *[]){"Primary", "Secondary"}[ide_devices[drive].Channel], // Use the channel as an index into the array
			(const char *[]){"Master", "Slave"}[ide_devices[drive].Drive], // Same as above, using the drive
			ide_devices[drive].Model);

	return err;
}


void ide_initialize(unsigned int BAR0, unsigned int BAR1, unsigned int BAR2, unsigned int BAR3, unsigned int BAR4) {
	int i, j, k, count = 0;

	// 1- Detect I/O Ports which interface IDE Controller:
	channels[ATA_PRIMARY  ].base  = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
	channels[ATA_PRIMARY  ].ctrl  = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
	channels[ATA_SECONDARY].base  = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
	channels[ATA_SECONDARY].ctrl  = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
	channels[ATA_PRIMARY  ].bmide = (BAR4 & 0xFFFFFFFC) + 0; // Bus Master IDE
	channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8; // Bus Master IDE

	// 2- Disable IRQs:
	ide_write(ATA_PRIMARY  , ATA_REG_CONTROL, 2);
	ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);


	// 3- Detect ATA-ATAPI Devices:
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++) {

			unsigned char err = 0, type = IDE_ATA, status;
			ide_devices[count].Reserved = 0; // Assuming that no drive here.

			// (I) Select Drive:
			ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // Select Drive.
			sleep(IO_WAIT_DELAY); // Wait 1ms for drive select to work.

			// (II) Send ATA Identify Command:
			ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			sleep(IO_WAIT_DELAY); // This function should be implemented in your OS. which waits for 1 ms.
			// it is based on System Timer Device Driver.

			// (III) Polling:
			if (ide_read(i, ATA_REG_STATUS) == 0) continue; // If Status = 0, No Device.

			while(1) {
				status = ide_read(i, ATA_REG_STATUS);
				if ((status & ATA_SR_ERR)) {
					err = 1; 
					break;
				} // If Err, Device is not ATA.
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
					break; // Everything is right.
				}
			}

			// (IV) Probe for ATAPI Devices:

			if (err != 0) {
				unsigned char cl = ide_read(i, ATA_REG_LBA1);
				unsigned char ch = ide_read(i, ATA_REG_LBA2);

				if (cl == 0x14 && ch ==0xEB)
					type = IDE_ATAPI;
				else if (cl == 0x69 && ch == 0x96)
					type = IDE_ATAPI;
				else
					continue; // Unknown Type (may not be a device).

				ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
				sleep(IO_WAIT_DELAY);
			}

			// (V) Read Identification Space of the Device:
			ide_read_buffer(i, ATA_REG_DATA, (unsigned int) ide_buf, 128);

			// (VI) Read Device Parameters:
			ide_devices[count].Reserved     = 1;
			ide_devices[count].Type         = type;
			ide_devices[count].Channel      = i;
			ide_devices[count].Drive        = j;
			ide_devices[count].Signature    = *((unsigned short *)(ide_buf + ATA_IDENT_DEVICETYPE));
			ide_devices[count].Capabilities = *((unsigned short *)(ide_buf + ATA_IDENT_CAPABILITIES));
			ide_devices[count].CommandSets  = *((unsigned int *)(ide_buf + ATA_IDENT_COMMANDSETS));

			// (VII) Get Size:
			if (ide_devices[count].CommandSets & (1 << 26)) {
				// Device uses 48-Bit Addressing:
				ide_devices[count].Size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
			}
			else {
				// Device uses CHS or 28-bit Addressing:
				ide_devices[count].Size   = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA));
			}

			// (VIII) String indicates model of device (like Western Digital HDD and SONY DVD-RW...):
			for(k = 0; k < 40; k += 2) {
				ide_devices[count].Model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
				ide_devices[count].Model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];}
			ide_devices[count].Model[40] = 0; // Terminate String.

			count++;
		}
	//print summary
	for (int i = 0; i < 4; i++) {
		if (ide_devices[i].Reserved == 1) {
			printf("[%d] Found %s Drive %dGB - %s\n",
					i,
					//type
					(const char*[]){"ATA", "ATAPI"}[ide_devices[i].Type],
					//size
					ide_devices[i].Size / 1024 / 1024 / 2,
					ide_devices[i].Model);
		}
	}
}

unsigned char ide_ata_access(unsigned char direction, unsigned char drive, unsigned int lba, unsigned int edi, unsigned int byte_count) {
	unsigned char lba_mode /* 0: CHS, 1:LBA28, 2: LBA48 */, dma /* 0: No DMA, 1: DMA */, cmd;
	unsigned char lba_io[6];
	unsigned int  channel      = ide_devices[drive].Channel; // Read the Channel.
	unsigned int  slavebit      = ide_devices[drive].Drive; // Read the Drive [Master/Slave]
	unsigned int  bus = channels[channel].base; // Bus Base, like 0x1F0 which is also data port.
	unsigned short cyl, i;
	unsigned char head, sect, err;
	unsigned int numsects = sectors_from_bytes(byte_count);

	ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = (ide_irq_invoked = 0x0) + 0x02);

	//select one from LBA28, LBA48, or CHS
	if (lba >= 0x10000000) {
		//LBA48:
		lba_mode  = 2;
		lba_io[0] = (lba & 0x000000FF) >> 0;
		lba_io[1] = (lba & 0x0000FF00) >> 8;
		lba_io[2] = (lba & 0x00FF0000) >> 16;
		lba_io[3] = (lba & 0xFF000000) >> 24;
		//LBA28 is integer, so 32bits are enough
		lba_io[4] = 0;
		lba_io[5] = 0;
		//lower 4 bits of HDDEVSEL are not used here
		head 	  = 0;
	}
	//does drive support LBA?
	else if (ide_devices[drive].Capabilities & 0x200) {
		//LBA28:
		lba_mode  = 1;
		lba_io[0] = (lba & 0x00000FF) >> 0;
		lba_io[1] = (lba & 0x000FF00) >> 8;
		lba_io[2] = (lba & 0x0FF0000) >> 16;
		//these registers are not used here
		lba_io[3] = 0;
		lba_io[4] = 0;
		lba_io[5] = 0;
		head 	  = (lba & 0xF000000) >> 24;
	}
	else {
		//CHS
		lba_mode  = 0;
		sect 	  = (lba % 63) + 1;
		cyl 	  = (lba + 1 - sect) / (16 * 63);
		lba_io[0] = sect;
		lba_io[1] = (cyl >> 0) & 0xFF;
		lba_io[2] = (cyl >> 8) & 0xFF;
		lba_io[3] = 0;
		lba_io[4] = 0;
		lba_io[5] = 0;
		//head number is written to HDDEVSEL lower 4 bits
		head 	  = (lba + 1 - sect) % (16 * 63) / (63);
	}

	//see if drive supports DMA or not
	dma = 0;

	//wait if drive is busy
	while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) {
		;
	}

	//select drive from controller
	if (lba_mode == 0) {
		//drive & CHS
		ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head);
	}
	else {
		//drive & LBA
		ide_write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head);
	}

	//write parameters
	if (lba_mode == 2) {
		ide_write(channel, ATA_REG_SECCOUNT1, 0);
		ide_write(channel, ATA_REG_LBA3, lba_io[3]);
		ide_write(channel, ATA_REG_LBA4, lba_io[4]);
		ide_write(channel, ATA_REG_LBA5, lba_io[5]);
	}
	ide_write(channel, ATA_REG_SECCOUNT0, numsects);
	ide_write(channel, ATA_REG_LBA0, lba_io[0]);
	ide_write(channel, ATA_REG_LBA1, lba_io[1]);
	ide_write(channel, ATA_REG_LBA2, lba_io[2]);

	if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
	if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
	if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;
	if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
	if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
	if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
	if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
	if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
	if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
	if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
	if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
	if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
	//send command
	ide_write(channel, ATA_REG_COMMAND, cmd);

	if (dma) {
		printf_err("DMA is not supported.\n");
		return -1;
	}
	else {
		if (direction == 0) {
			//PIO read
			for (i = 0; i < numsects; i++) {
				if (err = ide_polling(channel, 1)) {
					//polling, set error and exit if there is
					return err;
				}

				int words = SECTOR_SIZE / 2;
				insw(bus, edi, words);
				edi += (words * 2);
			}
		}
		else {
			//PIO write
			for (int i = 0; i < numsects; i++) {
				//polling
				ide_polling(channel, 0);

				int words = SECTOR_SIZE / 2;
				asm("rep outsw"::"c"(words), "d"(bus), "S"(edi)); //send data
				edi += (words * 2);
			}
			ide_write(channel, ATA_REG_COMMAND, (char[]){
					ATA_CMD_CACHE_FLUSH,
					ATA_CMD_CACHE_FLUSH,
					ATA_CMD_CACHE_FLUSH_EXT}[lba_mode]);
			//polling
			ide_polling(channel, 0);
		}
	}

	return 0;
}

void ide_wait_irq() {
	while (ide_irq_invoked)
		;
	ide_irq_invoked = 0;
}

void ide_irq() {
	ide_irq_invoked = 1;
}

unsigned char ide_atapi_read(unsigned char drive, unsigned int lba, unsigned char numsects, unsigned int edi) {
	unsigned int 	channel  = ide_devices[drive].Channel;
	unsigned int 	slavebit = ide_devices[drive].Drive;
	unsigned int 	bus 	 = channels[channel].base;
	//sector size. ATAPI drives have sector size of 2048 bytes
	unsigned int 	words 	 = 1024;
	unsigned char 	err;
	int i;

	//enable IRQs
	ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = ide_irq_invoked = 0x0);

	//setup SCSI packet
	atapi_packet[ 0] = ATAPI_CMD_READ;
	atapi_packet[ 1] = 0x00;
	atapi_packet[ 2] = (lba >> 24) & 0xFF;
	atapi_packet[ 3] = (lba >> 16) & 0xFF;
	atapi_packet[ 4] = (lba >> 8) & 0xFF;
	atapi_packet[ 5] = (lba >> 0) & 0xFF;
	atapi_packet[ 6] = 0x0;
	atapi_packet[ 7] = 0x0;
	atapi_packet[ 8] = 0x0;
	atapi_packet[ 9] = numsects;
	atapi_packet[10] = 0x0;
	atapi_packet[11] = 0x0;

	//select the drive
	ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);

	//delay 400ns for select to complete
	for (int i = 0; i < 4; i++) {
		//reading alternate status port wastes 100ns
		ide_read(channel, ATA_REG_ALTSTATUS);
	}

	//inform controller that we use PIO mode
	ide_write(channel, ATA_REG_FEATURES, 0);

	//tell controller size of buffer
	//lower byte of sector size
	ide_write(channel, ATA_REG_LBA1, (words*2) & 0xFF);
	//upper byte of sector size
	ide_write(channel, ATA_REG_LBA2, (words*2) >> 8);

	//send packet command
	ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

	//waiting for driver to finish or return error code
	if (err = ide_polling(channel, 1)) return err;

	//sending packet data
	asm("rep outsw" :: "c"(6), "d"(bus), "S"(atapi_packet));

	//receiving data
	for (i = 0; i < numsects; i++) {
		//wait for IRQ
		ide_wait_irq();
		if (err = ide_polling(channel, 1)) {
			return err;
		}
		//receive data
		insm(bus, edi, words);
		edi += (words * 2);
	}

	//wait for IRQ
	ide_wait_irq();

	//wait for BSY & DRQ to clear
	while (ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ))
		;

	return 0;
}

void ide_ata_read(unsigned char drive, unsigned int lba, unsigned int edi, unsigned int byte_count, unsigned int offset) {
	//if offset is greater than a sector, offset lba so we're starting at the correct sector
	int sector_offset = offset / SECTOR_SIZE;
	lba += sector_offset;
	offset -= (sector_offset * SECTOR_SIZE);

	unsigned int numsects = sectors_from_bytes(byte_count);
	//check if drive present
	if (drive > 3 || ide_devices[drive].Reserved == 0) {
		//drive not found
		package[0] = 0x1;
	}

	//check if inputs are valid
	else if (((lba + numsects) > ide_devices[drive].Size) && (ide_devices[drive].Type == IDE_ATA)) {
		//seeking to invalid position
		package[0] = 0x2;
	}

	//read in PIO mode through polling and IRQs
	else {
		unsigned char err;
		if (ide_devices[drive].Type == IDE_ATA) {
			for (int i = 0; i < numsects; i++) {
				int sector = lba + i;
				char sector_buf[512];
				err = ide_ata_access(ATA_READ, drive, sector, sector_buf, SECTOR_SIZE);

				//only copy a sector at at time!
				int copy_count = MIN(byte_count, SECTOR_SIZE);

				memcpy(edi, &sector_buf[offset], copy_count);
				//if we've just accounted for the requested offset, remove the offset
				offset = 0;
				//we've copied 'copy_count' bytes, decrement from amount left to copy
				byte_count -= copy_count;
			}
		}
		else if (ide_devices[drive].Type == IDE_ATAPI) {
			for (int i = 0; i < numsects; i++) {
				err = ide_atapi_read(drive, lba + i, 1, edi + (i*2048));
			}
		}
		package[0] = ide_print_error(drive, err);
	}
	ide_print_error(drive, package[0]);
}

void ide_ata_write(unsigned char drive, unsigned int lba, unsigned int edi, unsigned int byte_count, unsigned int offset) {
	//if offset is greater than a sector, offset lba so we're starting at the correct sector
	int sector_offset = offset / SECTOR_SIZE;
	lba += sector_offset;
	offset -= (sector_offset * SECTOR_SIZE);

	unsigned int numsects = sectors_from_bytes(byte_count);

	//check if drive is present
	if (drive > 3 || ide_devices[drive].Reserved == 0) {
		//drive not found!
		package[0] = 0x1;
	}
	//check if inputs are valid
	else if (((lba + numsects) > ide_devices[drive].Size) && (ide_devices[drive].Type == IDE_ATA)) {
		//seeking to invalid position
		package[0] = 0x2;
	}
	//read in PIO mode through polling and IRQs
	else {
		unsigned char err;
		if (ide_devices[drive].Type == IDE_ATA) {
			for (int i = 0; i < numsects; i++) {
				int sector = lba + i;
				char sector_buf[512];
				memset(sector_buf, 0, sizeof(sector_buf));
				ide_ata_read(drive, sector, sector_buf, SECTOR_SIZE, 0);

				//copy over data to write
				//only copy a sector at at time!
				int copy_count = MIN(byte_count, SECTOR_SIZE);

				memcpy(&sector_buf[offset], edi, copy_count);
				//if we've just accounted for the requested offset, remove the offset
				offset = 0;
				//we've copied 'copy_count' bytes, decrement from amount left to copy
				byte_count -= copy_count;

				//copy back to actual sector
				err = ide_ata_access(ATA_WRITE, drive, sector, sector_buf, SECTOR_SIZE);
			}
		}
		else if (ide_devices[drive].Type == IDE_ATAPI) {
			//write protected
			err = 4;
		}
		package[0] = ide_print_error(drive, err);
	}
}

void ide_atapi_eject(unsigned char drive) {
	unsigned int 	channel  = ide_devices[drive].Channel;
	unsigned int 	slavebit = ide_devices[drive].Drive;
	unsigned int 	bus 	 = channels[channel].base;
	//sector size in words
	unsigned int 	words 	 = 2048 / 2;
	unsigned char   err 	 = 0;
	ide_irq_invoked = 0;

	//check if drive present
	if (drive > 3 || ide_devices[drive].Reserved == 0) {
		//drive not found
		package[0] = 0x1;
	}
	//check if drive isn't ATAPI
	else if (ide_devices[drive].Type == IDE_ATA) {
		//command aborted
		package[0] = 20;
	}
	//eject ATAPI driver
	else {
		//enable IRQs
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN = ide_irq_invoked = 0x0);

		//setup SCSI packet
		atapi_packet[ 0] = ATAPI_CMD_EJECT;
		atapi_packet[ 1] = 0x00;
		atapi_packet[ 2] = 0x00;
		atapi_packet[ 3] = 0x00;
		atapi_packet[ 4] = 0x02;
		atapi_packet[ 5] = 0x00;
		atapi_packet[ 6] = 0x00;
		atapi_packet[ 7] = 0x00;
		atapi_packet[ 8] = 0x00;
		atapi_packet[ 9] = 0x00;
		atapi_packet[10] = 0x00;
		atapi_packet[11] = 0x00;

		//select the drive
		ide_write(channel, ATA_REG_HDDEVSEL, slavebit << 4);

		//delay 400ns for select to complete
		for (int i = 0; i < 4; i++) {
			//reading alternate status port wastes 100ns
			ide_read(channel, ATA_REG_ALTSTATUS);
		}

		//send packet command
		ide_write(channel, ATA_REG_COMMAND, ATA_CMD_PACKET);

		//waiting for the driver to finish/invoke error
		if (err = ide_polling(channel, 1)) return err;
		//sending data packet
		else {
			asm("rep outsw"::"c"(6), "d"(bus), "S"(atapi_packet));
			//wait for IRQ
			ide_wait_irq();
			//polling and get error code
			err = ide_polling(channel, 1);
			//DRQ is not needed here
			if (err == 3) err = 0;
		}
		package[0] = ide_print_error(drive, err);
	}
}

