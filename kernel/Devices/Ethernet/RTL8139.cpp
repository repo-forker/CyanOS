#include "RTL8139.h"
#include "Arch/x86/Asm.h"
#include "Arch/x86/Isr.h"
#include "Arch/x86/Pic.h"
#include "Devices/DebugPort/Logger.h"
#include "Tasking/Thread.h"
#include "VirtualMemory/Memory.h"

static void irq_handler(ISRContextFrame& context);
RTL8139* instance = nullptr;

RTL8139::RTL8139(GenericPCIDevice&& device) : m_ports{device.BAR0().io_address()}
{
	m_mac = read_MAC();
	instance = this;

	device.enable_bus_mastering();
	device.enable_interrupts();

	turn_on();
	software_rest();
	start();
	setup_tx();
	setup_rx();

	ISR::register_hardware_interrupt_handler(irq_handler, device.interrupt_line());
}

void RTL8139::turn_on()
{
	write_register8(RTL8139_PORT_CONFIG1, 0);
}

void RTL8139::software_rest()
{
	write_register8(RTL8139_PORT_CHIP_CMD, RTL8139_COMMAND_RESET);
	while (read_register8(RTL8139_PORT_CHIP_CMD) & RTL8139_COMMAND_RESET) {
	}
}

void RTL8139::setup_tx()
{
	for (size_t i = 0; i < NUMBER_TX_BUFFERS; i++) {
		m_tx_buffers[i] = valloc(0, MAX_TX_BUFFER_SIZE, PAGE_READWRITE | PAGE_CONTAGIOUS);
		write_register32(TSAD_array[i], virtual_to_physical_address(m_tx_buffers[i]));
	}
}

void RTL8139::setup_rx()
{
	// CAPR: keeps the address of data that driver had read. 0x0038 (16bit) R/W
	// CBA: keeps the current address of data moved to buffer. 0x003A (16bit) R

	m_rx_buffer = reinterpret_cast<uint8_t*>(valloc(0, MAX_RX_BUFFER_SIZE, PAGE_READWRITE | PAGE_CONTAGIOUS));
	write_register32(RTL8139_PORT_RX_BUF, virtual_to_physical_address(m_rx_buffer));
	write_register32(RTL8139_PORT_RX_CONFIG, RTL8139_RX_CONFIG_ACCEPT_PHYSICAL_MATCH_PACKETS |
	                                             RTL8139_RX_CONFIG_ACCEPT_PHYSICAL_ADDRESS_PACKETS |
	                                             RTL8139_RX_CONFIG_ACCEPT_MULTICAST_PACKETS |
	                                             RTL8139_RX_CONFIG_ACCEPT_BROADCAST_PACKETS | RTL8139_RX_CONFIG_WRAP |
	                                             RTL8139_RX_CONFIG_MAX_DMA_BURST_SIZE_256);
	// write_register16(RTL8139_PORT_RX_BUF_PTR, 0);
}

void RTL8139::start()
{
	write_register8(RTL8139_PORT_CHIP_CMD, RTL8139_COMMAND_RX_ENABLE | RTL8139_COMMAND_TX_ENABLE);

	write_register16(RTL8139_PORT_INTR_MASK,
	                 RTL8139_INTERRUPT_MASK_RX_OK | RTL8139_INTERRUPT_MASK_TX_OK | RTL8139_INTERRUPT_MASK_TX_ERROR |
	                     RTL8139_INTERRUPT_MASK_RX_ERROR | RTL8139_INTERRUPT_MASK_BUFFER_OVERFLOW |
	                     RTL8139_INTERRUPT_MASK_PACKET_UNDERRUN | RTL8139_INTERRUPT_MASK_RX_FIFO_OVERFLOW |
	                     RTL8139_INTERRUPT_MASK_DESCRIPTOR_UNAVAILABLE | RTL8139_INTERRUPT_MASK_CABLE_LEN_CHANGE |
	                     RTL8139_INTERRUPT_MASK_TIMEOUT | RTL8139_INTERRUPT_MASK_SYSTEM_ERROR);
}

void RTL8139::handle_rx()
{
	// while (!(read_register8(RTL8139_PORT_CHIP_CMD) & RTL8139_COMMAND_BUFFER_EMPTY)) {
	RxPacket* received_packet = reinterpret_cast<RxPacket*>(m_rx_buffer + m_current_rx_pointer);
	info() << "Status : " << received_packet->status;

	if (is_packet_ok(received_packet->status)) {
		info() << "Size   : " << received_packet->size;
		handle_received_ethernet_frame(received_packet->data, received_packet->size);
	} else {
		info() << "Invalid Packet.";
	}

	if ((m_current_rx_pointer + received_packet->size + RTL8139_RX_PACKET_HEADER_SIZE) < MAX_RX_BUFFER_SIZE) {
		m_current_rx_pointer =
		    (received_packet->size + RTL8139_RX_PACKET_HEADER_SIZE + 3) & RTL8139_RX_READ_POINTER_MASK;
	} else {
		m_current_rx_pointer = 0; // FIXME: not sure about this.
	}

	write_register16(RTL8139_PORT_RX_BUF_PTR, m_current_rx_pointer - RTL8139_RX_PAD);
	//}
}

void RTL8139::handle_tx()
{
	// info() << "TX descriptors: " << Hex(read_register16(RTL8139_PORT_TX_SUMMARY));
	// FIXME: release waitqueues for descriptors here.
}

void RTL8139::rx_tx_handler()
{
	info() << "---------------------------------------";

	uint16_t status = read_register16(RTL8139_PORT_INTR_STATUS);
	warn() << "Network Interupt:";
	info() << "ISR status : " << status;
	if (status & RTL8139_INTERRUPT_STATUS_TX_OK) {
		// info() << "Interrupt: Data has been sent!";
		handle_tx();
	}
	if (status & RTL8139_INTERRUPT_STATUS_RX_OK) {
		info() << "Interrupt: Data has been received!";
		handle_rx();
	}

	write_register16(RTL8139_PORT_INTR_STATUS, status);
}

void RTL8139::send_ethernet_frame(const void* data, size_t len)
{
	if (len > MAX_TX_BUFFER_SIZE) {
		warn() << "Data too large to be sent!";
		return;
	}

	memcpy(m_tx_buffers[m_current_tx_buffer], data, len);
	/*if (len < 60) {
	    memset(reinterpret_cast<uint8_t*>(m_tx_buffers[m_current_tx_buffer]) + len, 0, 60 - len);
	    len = 60;
	}*/

	info() << "Sending frame with size: " << len;
	write_register32(TSD_array[m_current_tx_buffer], len);
	while (!(read_register32(TSD_array[m_current_tx_buffer]) & RTL8139_TX_STATUS_TOK)) {
	}
	m_current_tx_buffer = (m_current_tx_buffer + 1) % NUMBER_TX_BUFFERS;
}

bool RTL8139::is_packet_ok(uint16_t status)
{
	if (status & RTL8139_RX_PACKET_STATUS_ROK) {
		return true;
	} else {
		return false;
	}
}

MACAddress RTL8139::read_MAC()
{
	uint32_t mac1 = read_register32(0);
	uint32_t mac2 = read_register32(4);
	MACAddress mac{};
	mac[0] = (mac1 & 0xFF);
	mac[1] = (mac1 >> 8) & 0xFF;
	mac[2] = (mac1 >> 16) & 0xFF;
	mac[3] = (mac1 >> 24) & 0xFF;
	mac[4] = (mac2 & 0xFF);
	mac[5] = (mac2 >> 8) & 0xFF;
	return mac;
}

void RTL8139::write_register8(uint16_t address, uint8_t value)
{
	out8(m_ports + address, value);
}

void RTL8139::write_register16(uint16_t address, uint16_t value)
{
	out16(m_ports + address, value);
}

void RTL8139::write_register32(uint16_t address, uint32_t value)
{
	out32(m_ports + address, value);
}

uint8_t RTL8139::read_register8(uint16_t address)
{
	return in8(m_ports + address);
}

uint16_t RTL8139::read_register16(uint16_t address)
{
	return in16(m_ports + address);
}

uint32_t RTL8139::read_register32(uint16_t address)
{
	return in32(m_ports + address);
}

static void irq_handler(ISRContextFrame& context)
{
	UNUSED(context);
	ASSERT(instance);
	instance->rx_tx_handler();
}