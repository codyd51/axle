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

