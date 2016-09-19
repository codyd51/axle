#include "pci_detect.h"

#define MAX_DEVICES 32

static array_m* devices;

uint16_t pci_config_readw(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
	uint32_t long_bus = (uint32_t)bus;
	uint32_t long_slot = (uint32_t)slot;
	uint32_t long_func = (uint32_t)function;
	
	//create config address
	//bit layout:
	//0-1 		00
	//2-7		register number
	//8-10		function number
	//11-15		device number
	//16-23		bus number
	//24-30		reserved
	//31		enable bit
	uint32_t address = (uint32_t)((long_bus << 16) | (long_slot << 11) | (long_func << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));

	//write out address
	outl(0xCF8, address);

	//read in data
	//(offset & 2) * 8) == 0 will choose first word of 32b register
	uint16_t in = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xFFFF);
	return (in);
}

uint16_t pci_vendor_id(uint8_t bus, uint8_t slot, uint8_t function) {
	//try and read first config register
	uint16_t vendor = pci_config_readw(bus, slot, function, 0);
	//since there are no vendors equal to 0xFFFF, it must be a non-existent device
	if (vendor != 0xFFFF) {
		uint16_t device = pci_config_readw(bus, slot, function, 2);
	}
	return (vendor);
}

//TODO implement real function
uint8_t pci_get_header_type(uint8_t bus, uint8_t device, uint8_t function) {
	uint16_t word = pci_config_readw(bus, device, function, 0xE);
	//high byte is BIST
	//low byte is header type
	//uint8_t header_type = (word & 0x00FF);
	uint8_t header_type = (uint8_t)word;
	printf_info("header type %x", header_type);
	return header_type;
}

//TODO implement real function
uint8_t pci_baseclass(uint8_t bus, uint8_t device, uint8_t function) {
	//return 0x06;
	return pci_config_readw(bus, device, function, 24);
}

//TODO implement real function
uint8_t pci_subclass(uint8_t bus, uint8_t device, uint8_t function) {
	//return 0x04;
	return pci_config_readw(bus, device, function, 16);
}

//TODO implement real function
uint8_t pci_secondary_bus(uint8_t bus, uint8_t device, uint8_t function) {
	//return 0x00;
	return pci_config_readw(bus, device, function, 8);
}

void pci_check_function(uint8_t bus, uint8_t device, uint8_t function) {
	uint8_t baseclass = pci_baseclass(bus, device, function);
	uint8_t subclass = pci_subclass(bus, device, function);
	printf_info("baseclass %d subclass %d", baseclass, subclass);
	if ((baseclass == 0x06) && (subclass = 0x04)) {
		//uint8_t secondary_bus = pci_secondary_bus(bus, device, function);
		//pci_check_bus(secondary_bus);
	}
}

uint16_t pci_device_id(uint16_t bus, uint16_t device, uint16_t function) {
	return pci_config_readw(bus, device, function, 2);
}

void pci_traverse_buses(void) {
	for (uint32_t bus = 0; bus < 256; bus++) {
		for (uint32_t slot = 0; slot < 32; slot++) {
			for (uint32_t func = 0; func < 8; func++) {
				uint16_t vendor = pci_vendor_id(bus, slot, func);
				if (vendor == 0xFFFF) continue;

				uint16_t device_id = pci_device_id(bus, slot, func);
				printf_info("PCI vendor %x device %x", vendor, device_id);
				pci_device* device = kmalloc(sizeof(pci_device));
				device->vendor = vendor;
				device->device = device_id;
				device->func = func;
				array_m_insert(devices, device);
			}
		}
	}
}

void pci_install() {
	printf_info("Registering PCI devices...");

	devices = array_m_create(MAX_DEVICES);
	pci_traverse_buses();
}
