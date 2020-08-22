#include "SystemCall.h"
#include "Arch/x86/isr.h"
#include "Devices/Console/console.h"
#include "Devices/DeviceManager.h"
#include "Filesystem/VirtualFilesystem.h"
#include "Tasking/Process.h"
#include "Tasking/Thread.h"
#include "utils/stl.h"

#pragma GCC diagnostic ignored "-Wcast-function-type"

void SystemCall::setup()
{
	ISR::register_isr_handler(systemcall_handler, SYSCALL_IRQ);
}

generic_syscall SystemCall::get_syscall_routine(unsigned syscall_num)
{
	if (syscall_num < syscalls_count) {
		return systemcalls_routines[syscall_num];
	}
	return nullptr;
}

void SystemCall::systemcall_handler(ISRContextFrame* frame)
{

	printf("System Call happened: %x (%x, %x, %x, %x, %x)!\n", Context::syscall_num(frame),
	       Context::syscall_param1(frame), Context::syscall_param2(frame), Context::syscall_param3(frame),
	       Context::syscall_param4(frame), Context::syscall_param5(frame));

	generic_syscall syscall = get_syscall_routine(Context::syscall_num(frame));
	if (!syscall) {
		PANIC("undefined systemcall invoked");
	}
	auto ret = syscall(Context::syscall_param1(frame), Context::syscall_param2(frame), Context::syscall_param3(frame),
	                   Context::syscall_param4(frame), Context::syscall_param5(frame));

	Context::set_return_value(frame, ret.error());
	if (ret.is_error()) {
		Context::set_return_arg1(frame, 0);
	} else {
		Context::set_return_arg1(frame, ret.value());
	}
}

generic_syscall SystemCall::systemcalls_routines[] = {reinterpret_cast<generic_syscall>(OpenFile),
                                                      reinterpret_cast<generic_syscall>(ReadFile),
                                                      reinterpret_cast<generic_syscall>(WriteFile),
                                                      reinterpret_cast<generic_syscall>(CloseFile),

                                                      reinterpret_cast<generic_syscall>(OpenDevice),
                                                      reinterpret_cast<generic_syscall>(ReadDevice),
                                                      reinterpret_cast<generic_syscall>(WriteDevice),
                                                      reinterpret_cast<generic_syscall>(CloseDevice),

                                                      reinterpret_cast<generic_syscall>(CreateThread),
                                                      reinterpret_cast<generic_syscall>(CreateRemoteThread), //
                                                      reinterpret_cast<generic_syscall>(CreateProcess),
                                                      reinterpret_cast<generic_syscall>(Sleep),
                                                      reinterpret_cast<generic_syscall>(Yield)};

unsigned SystemCall::syscalls_count = sizeof(systemcalls_routines) / sizeof(generic_syscall);

Result<int> OpenFile(char* path, int mode, int flags)
{
	auto file_description = VFS::open(path, static_cast<OpenMode>(mode), static_cast<OpenFlags>(flags));
	if (file_description.is_error()) {
		return ResultError(file_description.error());
	}

	unsigned fd = Thread::current->parent_process().m_file_descriptors.add_descriptor(move(file_description.value()));
	return fd;
}

Result<int> ReadFile(unsigned descriptor, void* buff, size_t size)
{
	auto& description = Thread::current->parent_process().m_file_descriptors.get_description(descriptor);
	auto result = description.read(buff, size);
	if (result.is_error()) {
		return ResultError(result.error());
	}
	return 0;
}

Result<int> WriteFile(unsigned descriptor, void* buff, size_t size)
{
	auto& description = Thread::current->parent_process().m_file_descriptors.get_description(descriptor);
	auto result = description.read(buff, size);
	if (result.is_error()) {
		return ResultError(result.error());
	}
	return 0;
}

Result<int> CloseFile(unsigned descriptor)
{
	auto& description = Thread::current->parent_process().m_file_descriptors.get_description(descriptor);
	auto result = description.close();
	if (result.is_error()) {
		return ResultError(result.error());
	}
	Thread::current->parent_process().m_file_descriptors.remove_descriptor(descriptor);
	return 0;
}

Result<int> OpenDevice(char* path, int mode, int flags)
{
	auto description = DeviceManager::open(path, mode, flags);
	if (description.is_error()) {
		return ResultError(description.error());
	}

	unsigned fd = Thread::current->parent_process().m_device_descriptors.add_descriptor(move(description.value()));
	return fd;
}

Result<int> ReadDevice(unsigned descriptor, void* buff, size_t size)
{
	auto& description = Thread::current->parent_process().m_device_descriptors.get_description(descriptor);
	auto result = description.receive(buff, size);
	if (result.is_error()) {
		return ResultError(result.error());
	}
	return 0;
}

Result<int> WriteDevice(unsigned descriptor, void* buff, size_t size)
{
	auto& description = Thread::current->parent_process().m_device_descriptors.get_description(descriptor);
	auto result = description.send(buff, size);
	if (result.is_error()) {
		return ResultError(result.error());
	}
	return 0;
}

Result<int> CloseDevice(unsigned descriptor)
{
	auto& description = Thread::current->parent_process().m_device_descriptors.get_description(descriptor);
	auto result = description.close();
	if (result.is_error()) {
		return ResultError(result.error());
	}
	Thread::current->parent_process().m_device_descriptors.remove_descriptor(descriptor);
	return 0;
}

Result<int> CreateThread(void* address, int arg)
{
	return 0;
}

Result<int> CreateRemoteThread(int process, void* address, int arg)
{
	return 0;
}

Result<int> CreateProcess(char* name, char* path, int flags)
{
	UNUSED(flags);
	Process::create_new_process(name, path);
	return 0;
}

Result<int> Sleep(size_t size)
{
	Thread::sleep(size);
	return 0;
}

Result<int> Yield()
{
	Thread::yield();
	return 0;
}
