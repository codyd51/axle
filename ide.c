#define ATA_SR_BSY	0x80 //busy
#define ATA_SR_DRDY	0x40 //drive ready
#define ATA_SR_DF	0x20 //drive write fault
#define ATA_SR_DSC	0x10 //drive seek complete
#define ATA_SR_DRQ	0x08 //data request ready
#define ATA_SR_CORR	0x04 //corrected data
#define ATA_SR_IDX	0x02 //inlex
#define ATA_SR_ERR	0x01 //error

#define ATA_ER_BBK		0x80 //bad sector
#define ATA_ER_UNC		0x40 //uncorrectable data
#define ATA_ER_MC		0x20 //no media
#define ATA_ER_IDNF		0x10 //ID mark not found
#define ATA_ER_MCR		0x08 //no media
#define ATA_ER_ABRT		0x04 //command aborted
#define ATA_ER_TK0NF	0x02 //track 0 not found
#define ATA_ER_AMNF		0x01 //no address mark

#define ATA_CMD_READ_PIO		0x20 
#define ATA_CMD_READ_PIO_EXT	0x24
#define ATA_CMD_READ_DMA		0xC8
#define ATA_CMD_READ_DMA_EXT	0x25
#define ATA_CMD_WRITE_PIO		0x30
#define ATA_CMD_WRITE_PIO_EXT	0x34
#define ATA_CMD_WRITE_DMA		0xCA
#define ATA_CMD_WRITE_DMA_EXT	0x35
#define ATA_CMD_CACHE_FLUSH		0xE7
#define ATA_CMD_CACHE_FLUSH_EXT	0xEA
#define ATA_CMD_PACKET			0xA0
#define ATA_CMD_IDENTIFY_PACKET	0xA1
#define ATA_CMD_IDENTIFY 		0xEC

#define ATAPI_CMD_READ	0xA8
#define ATAPI_CMD_EJECT 0x1B

#define ATA_IDENT_DEVICETYPE			0
#define ATA_IDENT_DEVICETYPE_CYLINDERS	2
#define ATA_IDENT_HEADS					6
#define ATA_IDENT_SECTORS				12
#define ATA_IDENT_SERIAL				20
#define ATA_IDENT_MODEL					54
#define ATA_IDENT_CAPABILITIES			98
#define ATA_IDENT_FIELDVALID			106
#define ATA_IDENT_MAX_LBA				120
#define ATA_IDENT_COMMANDSETS			164
#define ATA_IDENT_MAX_LBA_EXT			200

#define IDE_ATA		0x00 
#define IDE_ATAPI	0x01

#define ATA_MASTER	0x00
#define ATA_SLAVE	0x01

#define ATA_REG_DATA		0x00
#define ATA_REG_ERROR		0x01
#define ATA_REG_FEATURES	0x01
#define ATA_REG_SECCOUNT0	0x02
#define ATA_REG_LBA0		0x03
#define ATA_REG_LBA1		0x04 
#define ATA_REG_LBA2		0x05 
#define ATA_REG_HDDEVSEL	0x06 
#define ATA_REG_COMMAND		0x07 
#define ATA_REG_STATUS		0x07 
#define ATA_REG_SECCOUNT1	0x08 
#define ATA_REG_LBA3		0x09
#define ATA_REG_LBA4		0x0A 
#define ATA_REG_LBA5		0x0B
#define ATA_REG_CONTROL		0x0C
#define ATA_REG_ALTSTATUS	0x0C
#define ATA_REG_DEVADDRESS	0x0D

//channels
#define ATA_PRIMARY		0x00
#define ATA_SECONDARY	0x01 

//directions
#define ATA_READ 	0x00 
#define ATA_WRITE 	0x01

#define insl(port, buffer, count) asm volatile("cld; rep; insl" :: "D" (buffer), "d" (port), "c" (count))

struct IDEChannelRegisters {
	unsigned short base; //I/O base
	unsigned short ctrl; //control base
	unsigned short bmide; //bus master IDE
	unsigned char nIEN; //nIEN (no interrupt)
} channels[2];

unsigned char ide_buf[2048] = {0};
unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

struct ide_device {
	unsigned char Reserved; //0 (empty) or 1 (this drive exists)
	unsigned char Channel; //0 (primary channel) or 1 (secondary channel)
	unsigned char Drive; //0 (master drive) or 1 (slave drive)
	unsigned short Type; //0 (ATA) or 1 (ATAPI)
	unsigned short Signature; //drive signature
	unsigned short Capabilities; //features
	unsigned int CommandSets; //command sets supported
	unsigned int Size; //size (in sectors)
	unsigned char Model[41]; //model string
} ide_devices[4];

unsigned char ide_read(unsigned char channel, unsigned char reg) {
	unsigned char result;
	if (reg > 0x07 && reg < 0x0C) {
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	}
	if (reg < 0x08) {
		result = inb(channels[channel].base + reg - 0x00);
	}
	else if (reg < 0x0C) {
		result = inb(channels[channel].base + reg - 0x06);
	}
	else if (reg < 0x0E) {
		result = inb(channels[channel].ctrl + reg - 0x0A);
	}
	else if (reg < 0x16) {
		result = inb(channels[channel].bmide + reg - 0x0E);
	}
	if (reg > 0x07 && reg < 0x0C) {
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
	}
	return result;
}

void ide_write(unsigned char channel, unsigned char reg, unsigned char data) {
	if (reg > 0x07 && reg < 0x0C) {
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	}
	if (reg < 0x08) {
		outb(channels[channel].base + reg - 0x00, data);
	}
	else if (reg < 0x0C) {
		outb(channels[channel].base + reg - 0x06, data);
	}
	else if (reg < 0x0E) {
		outb(channels[channel].ctrl + reg - 0x0A, data);
	}
	else if (reg < 0x16) {
		outb(channels[channel].bmide + reg - 0x0E, data);
	}
	if (reg > 0x07 && reg < 0x0C) {
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
	}
}

void ide_read_buffer(unsigned char channel, unsigned char reg, unsigned int buffer, unsigned int quads) {
	//TODO fix bug in this!
	//inline asm trashes ES and ESP for all of the code the
	//compiler generates between the inline asm blocks
	if (reg > 0x07 && reg < 0x0C) {
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	}
	asm("pushw %es; movw %ds, %ax; movw %ax, %es");
	if (reg < 0x08) {
		insl(channels[channel].base + reg - 0x00, buffer, quads);
	}
	else if (reg < 0x0C) {
		insl(channels[channel].base + reg - 0x06, buffer, quads);
	}
	else if (reg < 0x0E) {
		insl(channels[channel].ctrl + reg - 0x0A, buffer, quads);
	}
	else if (reg < 0x16) {
		insl(channels[channel].bmide + reg - 0x0E, buffer, quads);
	}
	asm("popw %es;");
	if (reg > 0x07 && reg < 0x0C) {
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
	}
}

unsigned char ide_polling(unsigned char channel, unsigned int advanced_check) {
	//delay 400ns for BSY to be set
	for (int i = 0; i < 4; i++) {
		//reading alternate status port wastes 100ns
		ide_read(channel, ATA_REG_ALTSTATUS);
	}

	//wait for BSY to be cleared
	while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY)
		;

	if (advanced_check) {
		//read status register
		unsigned char state = ide_read(channel, ATA_REG_STATUS);

		//check for errors
		if (state & ATA_SR_ERR) {
			//error
			return 2;
		}

		//check if device faults
		if (state & ATA_SR_DF) {
			//device fault
			return 1;
		}

		//check DRQ
		//BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now
		if ((state & ATA_SR_DRQ) == 0) {
			//DRQ should be set
			return 3;
		}
	}

	//no error
	return 0;
}

unsigned char ide_print_error(unsigned int drive, unsigned char err) {
	if (err == 0)
		return err;

	printf("IDE: ");
	if (err == 1) {
		printf("- Device Fault\n");
		err = 19;
	}
	else if (err == 2) {
		unsigned char st = ide_read(ide_devices[drive].Channel, ATA_REG_ERROR);
		if (st & ATA_ER_AMNF) {
			printf("- No Address Mark Found\n");
			err = 7;
		}
		if (st & ATA_ER_TK0NF) {
			printf("- No Media or Media Error\n");
			err = 3;
		}
		if (st & ATA_ER_ABRT) {
			printf("- Command Aborted\n");
			err = 20;
		}
		if (st & ATA_ER_MCR) {
			printf("- No Media or Media Error\n");
			err = 3;
		}
		if (st & ATA_ER_IDNF) {
			printf("- ID Mark not Found\n");
			err = 21;
		}
		if (st & ATA_ER_MC) {
			printf("- No Media or Media Error\n");
			err = 3;
		}
		if (st & ATA_ER_UNC) {
			printf("- Uncorrectable Data Error\n");
			err = 22;
		}
		if (st & ATA_ER_BBK) {
			printf("- Bad Sectors\n");
			err = 13;
		}
	}
	else if (err == 3) {
		printf("- Reads Nothing\n");
		err = 23;
	}
	else if (err == 4) {
		printf("- Write Protected\n");
		err = 8;
	}
	printf("- [%s %s] %s\n", 
		//use the channel as an index into array
		(const char*[]){"Primary", "Secondary"}[ide_devices[drive].Channel],
		//same as above, using drive
		(const char*[]){"Master", "Slave"}[ide_devices[drive].Drive],
		ide_devices[drive].Model);

	return err;
}

void ide_initialize(unsigned int BAR0, unsigned int BAR1, unsigned int BAR2, unsigned int BAR3, unsigned int BAR4) {
	int j, k, count = 0;

	//detect I/O ports which interface IDE controller
	channels[ATA_PRIMARY	].base 	= (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
	channels[ATA_PRIMARY	].ctrl 	= (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
	channels[ATA_SECONDARY	].base 	= (BAR0 & 0xFFFFFFFC) + 0x170 * (!BAR2);
	channels[ATA_PRIMARY	].ctrl 	= (BAR1 & 0xFFFFFFFC) + 0x376 * (!BAR3);
	channels[ATA_PRIMARY	].bmide	= (BAR0 & 0xFFFFFFFC) + 0; //bus master IDE
	channels[ATA_SECONDARY	].bmide = (BAR1 & 0xFFFFFFFC) + 8; //bus master IDE

	//disable IRQs
	ide_write(ATA_PRIMARY	, ATA_REG_CONTROL, 2);
	ide_write(ATA_SECONDARY	, ATA_REG_CONTROL, 2);

	//detect ATA-ATAPI devices
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			unsigned char err = 0, type = IDE_ATA, status;
			//assuming no drive here
			ide_devices[count].Reserved = 0;

			//select drive
			ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4));
			//sleep 1ms for drive select to work
			sleep(1);

			//sent ATA identify command
			ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			sleep(1);

			//polling
			//if status == 0, no device
			if (ide_read(i, ATA_REG_STATUS) == 0) continue;

			while (1) {
				status = ide_read(i, ATA_REG_STATUS);
				//if err, device is not ATA
				if ((status & ATA_SR_ERR)) {
					err = 1;
					break;
				}
				//everything is okay
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) break;
			}

			//probe for ATAPI devices

			if (err != 0) {
				unsigned char cl = ide_read(i, ATA_REG_LBA1);
				unsigned char ch = ide_read(i, ATA_REG_LBA2);

				if (cl == 0x14 && ch == 0xEB) {
					type = IDE_ATAPI;
				}
				else if (cl == 0x69 && ch == 0x96) {
					type = IDE_ATAPI;
				}
				else {
					//unknown type (may not be a device)
					continue;
				}

				ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
				sleep(1);
			}

			//read identification space of device
			ide_read_buffer(i, ATA_REG_DATA, (unsigned int)ide_buf, 128);

			//read device parameters
			ide_devices[count].Reserved 	= 1;
			ide_devices[count].Type 		= type;
			ide_devices[count].Channel 		= i;
			ide_devices[count].Drive 		= j;
			ide_devices[count].Signature 	= *((unsigned short*)(ide_buf + ATA_IDENT_DEVICETYPE));
			ide_devices[count].Capabilities = *((unsigned short*)(ide_buf + ATA_IDENT_CAPABILITIES));
			ide_devices[count].CommandSets 	= *((unsigned int*)(ide_buf + ATA_IDENT_COMMANDSETS));

			//get size
			if (ide_devices[count].CommandSets & (1 << 26)) {
				//device uses 48-bit addressing
				ide_devices[count].Size = *((unsigned int*)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
			}
			else {
				//device uses CHS or 28-bit addressing
				ide_devices[count].Size = *((unsigned int*)(ide_buf + ATA_IDENT_MAX_LBA));
			}

			//string indicates model of device
			for (k = 0; k < 40; k += 2) {
				ide_devices[count].Model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
				ide_devices[count].Model[k+1] = ide_buf[ATA_IDENT_MODEL + k];
			}
			//terminate string
			ide_devices[count].Model[40] = 0;

			count++;
		}
	}

	//print summary
	for (int i = 0; i < 4; i++) {
		if (ide_devices[i].Reserved == 1) {
			printf(" Found %s Drive %dGB - %s\n",
				//type
				(const char*[]){"ATA", "ATAPI"}[ide_devices[i].Type],
				//size
				ide_devices[i].Size / 1024 / 1024 / 2,
				ide_devices[i].Model);
		}
	}
}

unsigned char ide_ata_access(unsigned char direction, unsigned char drive, unsigned int lba, unsigned char numsects, unsigned short selector, unsigned int edi) {
	unsigned char lba_mode /*0: CHS, 1: LBA28, 2: LBA48 */, dma /* 0: No DMA, 1: DMA */, cmd;
	unsigned char lba_io[6];
	//read the channel
	unsigned int channel 		= ide_devices[drive].Channel;
	//read the drive (master/slave)
	unsigned int slavebit 		= ide_devices[drive].Drive;
	//bus base, like 0x1F0 which is also a data port
	unsigned int bus 			= channels[channel].base;
	//almost every ATA drive has a sector size of 512 bytes
	unsigned int words 			= 256;
	unsigned short cyl, i;
	unsigned char head, sect, err;

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
		if (direction == 0) {
			//DMA read
		}
		else {
			//DMA write
		}
	}
	else {
		if (direction == 0) {
			//PIO read
			for (i = 0; i < numsects; i++) {
				if (err = ide_polling(channel, 1)) {
					//polling, set error and exit if there is
					return err;
				}
				asm("pushw %es");
				asm("mov %%ax, %%es" : : "a"(selector));
				asm("rep insw" : : "c"(words), "d"(bus), "D"(edi)); //receive data
				asm("popw %es");
				edi += (words * 2);
			}
		}
		else {
			//PIO write
			for (int i = 0; i < numsects; i++) {
				//polling
				ide_polling(channel, 0);
				asm("pushw %ds");
				asm("mov %%ax, %%ds" :: "a"(selector));
				asm("rep outsw"::"c"(words), "d"(bus), "S"(edi)); //send data
				asm("popw %ds");
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

unsigned char ide_atapi_read(unsigned char drive, unsigned int lba, unsigned char numsects, unsigned short selector, unsigned int edi) {
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
		asm("pushw %es");
		asm("mov %%ax, %%es"::"a"(selector));
		//receive data
		asm("rep insw"::"c"(words), "d"(bus), "D"(edi));
		asm("popw %es");
		edi += (words * 2);
	}

	//wait for IRQ
	ide_wait_irq();

	//wait for BSY & DRQ to clear
	while (ide_read(channel, ATA_REG_STATUS) & (ATA_SR_BSY | ATA_SR_DRQ))
		;

	return 0;
}

void ide_read_sectors(unsigned char drive, unsigned char numsects, unsigned int lba, unsigned short es, unsigned int edi) {
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
			err = ide_ata_access(ATA_READ, drive, lba, numsects, es, edi);
		}
		else if (ide_devices[drive].Type == IDE_ATAPI) {
			for (i = 0; i < numsects; i++) {
				err = ide_atapi_read(drive, lba + i, 1, es, edi + (i*2048));
			}
		}
		package[0] = ide_print_error(drive, err);
	}
}

void ide_write_sectors(unsigned char drive, unsigned char numsects, unsigned int lba, unsigned short es, unsigned int edi) {
	//check if drive is present
	if (drive > 3 || ide_devices[drive].Reserved == 0) {
		//drive not found!
		pacakge[0] = 0x1;
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
			err = ide_ata_access(ADA_WRITE, drive, lba, numsects, es, edi);
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
	unsigned int 	bus 	 = channels[channel].Base;
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
	else if (ide_devices[drive].type == IDE_ATA) {
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










//ide_initialize(0x1F0, 0x36, 0x170, 0x376, 0x000);











