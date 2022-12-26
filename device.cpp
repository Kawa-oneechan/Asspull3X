#include "asspull.h"

Device* devices[MAXDEVS] = { 0 };

Device::Device(void) { }
Device::~Device(void) { }
unsigned int Device::Read(unsigned int address) { address; return (unsigned int)-1; }
void Device::Write(unsigned int address, unsigned int value) { address, value; }
int Device::GetID() { return 0; }
void Device::HBlank() {}
void Device::VBlank() {}
