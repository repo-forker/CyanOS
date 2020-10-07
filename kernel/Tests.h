#pragma once

#include "Devices/DebugPort/Logger.h"
#include "Devices/Timer/Pit.h"
#include "Filesystem/FileDescription.h"
#include "Filesystem/VirtualFilesystem.h"
#include "Tasking/Loader/PE.h"
#include "Tasking/Scheduler.h"
#include "Tasking/Semaphore.h"
#include "VirtualMemory/Memory.h"
#include <ArgumentParser.h>
#include <Bitmap.h>
#include <PathParser2.h>

Semaphore* sem_lock;
void test_semaphore_thread2(uintptr_t arg)
{
	UNUSED(arg);
	warn() << "Thread2:";
	sem_lock->acquire();
	warn() << "Semaphore acquired by thread2";
	Thread::sleep(1000);
	sem_lock->release();
	warn() << "Semaphore released by thread2";
	while (1) {
		HLT();
	}
}

void test_semaphore(uintptr_t arg)
{
	UNUSED(arg);
	warn() << "Thread1:";
	sem_lock = new Semaphore(1);
	sem_lock->acquire();
	Thread::create_thread(Thread::current->parent_process(), test_semaphore_thread2, 0, ThreadPrivilege::Kernel);
	warn() << "Semaphore acquired by thread1";
	Thread::sleep(3000);
	warn() << "wakeup thread1";
	sem_lock->release();
	warn() << "Semaphore released by thread1";
	while (1) {
		HLT();
	}
}

void thread_test(uintptr_t arg)
{
	warn() << "Thread " << arg;
	for (size_t i = 0; i < 3; i++) {
		warn() << "Thread " << arg << ": Hello " << i;
	}
	while (true) {
		// HLT();
	}
}
void test_threading(uintptr_t arg)
{
	UNUSED(arg);
	// printf("Main thread: creating other threads\n");
	for (size_t i = 0; i < 200; i++) {
		warn() << "Main thread: Creating thread " << i;
		Thread::create_thread(Thread::current->parent_process(), thread_test, i, ThreadPrivilege::Kernel);
	}
	while (true) {
		/* code */
	}
}

void test_pipe1(uintptr_t arg)
{
	UNUSED(arg);

	auto fd = FileDescription::open("/fs/my_pipe2", OpenMode::OM_READ, OpenFlags::OF_CREATE_NEW);
	if (fd.is_error()) {
		warn() << "error opening the file, error: " << fd.error();
		HLT();

		return;
	}
	char* buff = (char*)valloc(0xc00, PAGE_READWRITE);
	memset(buff, 0, 4096);
	auto result = fd.value()->read(buff, 12);
	dbg() << "got it, read";
	dbg() << buff;
	if (result.is_error())
		warn() << "error reading the file " << result.error();
	HLT();
}

void test_pipe2(uintptr_t arg)
{
	UNUSED(arg);
	Thread::sleep(1000);
	auto fd = FileDescription::open("/fs/my_pipe", OpenMode::OM_WRITE, OpenFlags::OF_OPEN_EXISTING);
	if (fd.is_error()) {
		warn() << "error opening the file, error: " << fd.error();
		HLT();
		return;
	}
	char* buff = (char*)valloc(0xc00, PAGE_READWRITE);
	memset(buff, 0, 4096);
	auto result = fd.value()->write(static_cast<const void*>("Hello there"), 12);
	dbg() << "got it, write";
	dbg() << buff;
	if (result.is_error())
		warn() << "error writing the file " << result.error();
	HLT();
}

void test_keyboard(uintptr_t arg)
{
	UNUSED(arg);

	auto fd = FileDescription::open("/Devices/keyboard", OpenMode::OM_WRITE, OpenFlags::OF_OPEN_EXISTING);
	if (fd.is_error()) {
		warn() << "error opening the file, error: " << fd.error();
		HLT();
		return;
	}
	char buff[1];
	while (true) {
		auto result = fd.value()->read(buff, 1);
		Logger(DebugColor::Cyan) << buff;
	}
	HLT();
}

void test_keyboard2(uintptr_t arg)
{
	UNUSED(arg);

	auto fd = FileDescription::open("/Devices/keyboard", OpenMode::OM_WRITE, OpenFlags::OF_OPEN_EXISTING);
	if (fd.is_error()) {
		warn() << "error opening the file, error: " << fd.error();
		HLT();
		return;
	}
	char buff[1];
	while (true) {
		auto result = fd.value()->read(buff, 1);
		Logger(DebugColor::Red) << buff;
	}
	HLT();
}

void test_console(uintptr_t arg)
{
	UNUSED(arg);

	auto fd = FileDescription::open("/Devices/console", OpenMode::OM_WRITE, OpenFlags::OF_OPEN_EXISTING);
	if (fd.is_error()) {
		warn() << "error opening the file, error: " << fd.error();
		HLT();
		return;
	}
	auto result = fd.value()->write("Hello there", 12);
	HLT();
}

void test_server(uintptr_t arg)
{
	UNUSED(arg);

	auto fd = FileDescription::open("/Sockets/test", OpenMode::OM_SERVER, OpenFlags::OF_CREATE_NEW);
	if (fd.is_error()) {
		warn() << "error opening the file, error: " << fd.error();
		HLT();
		return;
	}

	auto connection_fd = fd.value()->accept();
	if (connection_fd.is_error()) {
		warn() << "error accepting the connection, error: " << fd.error();
		HLT();
		return;
	}

	auto result = connection_fd.value()->write("I'm the server", 15);

	char buff[0x20];
	auto result2 = connection_fd.value()->read(buff, 15);
	Logger(DebugColor::Bright_Magenta) << "I'm the server, message from server : " << buff;
	while (true) {
		HLT();
	}
}

void test_client(uintptr_t arg)
{
	UNUSED(arg);
	Thread::current->sleep(100);
	auto fd = FileDescription::open("/Sockets/test", OpenMode::OM_CLIENT, OpenFlags::OF_OPEN_EXISTING);
	if (fd.is_error()) {
		warn() << "error opening the file, error: " << fd.error();
		HLT();
		return;
	}

	auto connection_fd = fd.value()->connect();
	if (connection_fd.is_error()) {
		warn() << "error accepting the connection, error: " << fd.error();
		HLT();
		return;
	}

	auto result = connection_fd.value()->write("I'm the client", 15);

	char buff[0x20];
	auto result2 = connection_fd.value()->read(buff, 15);
	Logger(DebugColor::Bright_Magenta) << "I'm the client, message from server : " << buff;
	while (true) {
		HLT();
	}
}

void test_ls(uintptr_t arg)
{
	UNUSED(arg);

	auto fd = FileDescription::open("/Tar/UserBinary", OpenMode::OM_READ, OpenFlags::OF_OPEN_EXISTING);
	if (fd.is_error()) {
		warn() << "error opening the file, error: " << fd.error();
		HLT();
		return;
	}
	DirectoryInfo dir;
	memset(&dir, 0, sizeof(DirectoryInfo));

	Result<void> result = fd.value()->dir_query(&dir);
	if (result.is_error())
		err() << result.error();
	while (!result.is_error()) {
		dbg() << dir.file_name;
		result = fd.value()->dir_query(&dir);
	}
	while (true) {
		HLT();
	}
}

void test_tid1(uintptr_t arg)
{
	UNUSED(arg);
	uint32_t tid, pid;
	char* exe_arg;
	asm("movl %%fs:0x0,%0" : "=r"(tid));
	asm("movl %%gs:0x0,%0" : "=r"(pid));
	asm("movl %%gs:0x8,%0" : "=r"(exe_arg));
	warn() << "PID:" << pid << " TID:" << tid << " Arg: " << exe_arg;
}

void test_tid2(uintptr_t arg)
{
	UNUSED(arg);
	uint32_t tid, pid;
	char* exe_arg;
	asm("movl %%fs:0x0,%0" : "=r"(tid));
	asm("movl %%gs:0x0,%0" : "=r"(pid));
	asm("movl %%gs:0x8,%0" : "=r"(exe_arg));
	warn() << "PID:" << pid << " TID:" << tid << "Arg: " << exe_arg;
}

void pathparser_test(uintptr_t arg)
{
	PathView parser("/drivers/test", "bin/test2");
	warn() << parser.count();
	warn() << parser[-1];
	warn() << parser[0];
	warn() << parser[1];
	warn() << parser[2];
	warn() << parser[3];

	PathView new_path = parser.sub_path(1, 3);
	warn() << new_path.count();
	warn() << new_path[0];
	warn() << new_path[1];
	warn() << new_path[2];
}