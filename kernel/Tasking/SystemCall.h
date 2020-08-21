#pragma once
#include "Arch/x86/context.h"
#include "utils/Result.h"
#include "utils/types.h"

typedef Result<int> (*generic_syscall)(int arg0, int arg1, int arg2, int arg3, int arg4);

class SystemCall
{
  private:
	static generic_syscall systemcalls_routines[];
	static unsigned syscalls_count;

	static generic_syscall get_syscall_routine(unsigned syscall_num);
	static void systemcall_handler(ISRContextFrame* frame);

  public:
	static void setup();
};

Result<int> OpenFile(char* path, int mode, int flags);
Result<int> ReadFile(int discriptor, void* buff, size_t size);
Result<int> WriteFile(int discriptor, void* buff, size_t size);
Result<int> CloseFile(int discriptor);

Result<int> OpenDevice(char* path, int mode, int flags);
Result<int> ReadDevice(int discriptor, void* buff, size_t size);
Result<int> WriteDevice(int discriptor, void* buff, size_t size);
Result<int> CloseDevice(int discriptor);

Result<int> CreateThread(void* address, int arg);
Result<int> CreateRemoteThread(int process, void* address, int arg);
Result<int> CreateProcess(char* name, char* path, int flags);
Result<int> Sleep(size_t size);
Result<int> Yield();