#include "memory.h"
#include "Arch/x86/isr.h"
#include "Arch/x86/paging.h"
#include "Arch/x86/panic.h"
#include "console.h"
#include "physical.h"
#include "virtual.h"

void setup_virtual_memory()
{
	setup_paging(get_kernel_pages());
	initialize_physical_memory();
	// Reserve Low 1MB Pages.
	set_used_physical_pages(0, GET_FRAME(KERNEL_PHYSICAL_ADDRESS));
	// Set kernel memory as used.
	set_used_physical_pages(GET_FRAME(KERNEL_PHYSICAL_ADDRESS), get_kernel_pages());
}

void setup_page_fault_handler()
{
	register_isr_handler(page_fault_handler, IRQ_NUMBER::PF);
}

void page_fault_handler(ISR_INFO isr_info)
{
	printf("Page= %X EIP=%X\t CS=%X\t ESP=%X  SS=%X\t\n", isr_info.cr2, isr_info.eip, isr_info.cs, isr_info.esp,
	       isr_info.ss);
	if (!PF_PRESENT(isr_info.error_code)) {
		PANIC("Page fault due accessing non-present page.");
	} else if (PF_US(isr_info.error_code)) {
		PANIC("Page fault due accessing kernel page from user mode.");
	} else if (PF_WR(isr_info.error_code)) {
		PANIC("Page fault due writing to read only page.");
	}
}
uintptr_t memory_alloc(uint32_t size, uint32_t flags)
{
	uint32_t vAdd;
	uint32_t pages_num = GET_PAGES(size, PAGE_4K);

	if (flags & MEMORY::KERNEL) {
		vAdd = virtual_find_pages(GET_FRAME(KERNEL_VIRTUAL_ADDRESS), 1024 * 1024,
		                          pages_num); // TODO: do 1024 * 1024 dynamicly
	} else {
		vAdd = virtual_find_pages(1, GET_FRAME(KERNEL_VIRTUAL_ADDRESS),
		                          pages_num); // skip first page to detect null pointer
	}

	for (size_t i = 0; i < pages_num; i++) {

		uint32_t pAdd = alloc_physical_page();
		map_virtual_pages(vAdd + PAGE_4K * i, pAdd, 1);
	}
	return vAdd;
}

uintptr_t memory_free(uintptr_t address, uint32_t size, uint32_t flags)
{
	free_physical_pages(get_physical_page(address), GET_PAGES(size, PAGE_4K));
	unmap_virtual_pages(address, GET_PAGES(size, PAGE_4K));
}

uintptr_t memory_map(uint32_t virtual_address, uint32_t physical_address, uint32_t size, uint32_t flags)
{
	map_virtual_pages(virtual_address, physical_address, GET_PAGES(size, PAGE_4K));
}

void memory_unmap(uint32_t virtual_address, uint32_t physical_address, uint32_t size, uint32_t flags)
{
	unmap_virtual_pages(virtual_address, GET_PAGES(size, PAGE_4K));
}

uint32_t virtual_memory_size()
{
}

uint32_t physical_memory_size()
{
	return get_physical_memory_size();
}

static uint32_t get_kernel_pages()
{
	uint32_t kernel_size = (uint32_t)&KERNEL_END - KERNEL_VIRTUAL_ADDRESS;
	uint32_t pages = kernel_size / PAGE_4K + ((kernel_size % PAGE_4K == 0) ? 0 : 1);
	return pages;
}
