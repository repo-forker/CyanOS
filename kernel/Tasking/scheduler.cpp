#include "scheduler.h"
#include "Arch/x86/asm.h"
#include "Arch/x86/gdt.h"
#include "Arch/x86/isr.h"
#include "Arch/x86/panic.h"
#include "Devices/Console/console.h"
#include "Devices/Timer/pit.h"
#include "VirtualMemory/heap.h"
#include "VirtualMemory/memory.h"
#include "utils/assert.h"

CircularQueue<ThreadControlBlock>* Scheduler::ready_threads;
CircularQueue<ThreadControlBlock>* Scheduler::sleeping_threads;
ThreadControlBlock* current_thread;
SpinLock Scheduler::scheduler_lock;
Bitmap* Scheduler::m_id_bitmap;

void idle()
{
	while (1) {
		HLT();
	}
}
void Scheduler::setup()
{
	spinlock_init(&scheduler_lock);
	ready_threads = new CircularQueue<ThreadControlBlock>;
	sleeping_threads = new CircularQueue<ThreadControlBlock>;
	m_id_bitmap = new Bitmap(MAX_BITMAP_SIZE);
	current_thread = nullptr;
	ISR::register_isr_handler(schedule_handler, SCHEDULE_IRQ);
	create_new_thread((void*)idle);
}

void Scheduler::schedule_handler(ContextFrame* frame)
{
	schedule(frame, ScheduleType::FORCED);
}

// Select next process, save and switch context.
void Scheduler::schedule(ContextFrame* current_context, ScheduleType type)
{
	spinlock_acquire(&scheduler_lock);
	if (type == ScheduleType::TIMED)
		wake_up_sleepers();

	if (current_thread) {
		save_context(current_context, current_thread);
		current_thread->state = ThreadState::READY;
	}
	// FIXME: schedule idle if there is no ready thread
	select_next_thread();
	ready_threads->head().state = ThreadState::RUNNING;
	load_context(current_context, &ready_threads->head());
	current_thread = &ready_threads->head();
	spinlock_release(&scheduler_lock);
}

// Round Robinson Scheduling Algorithm.
void Scheduler::select_next_thread()
{
	ready_threads->increment_head();
}

// Decrease sleep_ticks of each thread and wake up whose value is zero.
void Scheduler::wake_up_sleepers()
{
	for (CircularQueue<ThreadControlBlock>::Iterator thread = sleeping_threads->begin();
	     thread != sleeping_threads->end(); ++thread) {
		if (thread->sleep_ticks <= PIT::ticks) {
			thread->sleep_ticks = 0;
			sleeping_threads->move_to_other_list(ready_threads, thread);
			wake_up_sleepers();
			break;
		}
	}
}

// Put the current thread into sleep for ms.
void Scheduler::sleep(unsigned ms)
{

	spinlock_acquire(&scheduler_lock);
	ThreadControlBlock& current = ready_threads->head();
	current.sleep_ticks = PIT::ticks + ms;
	current.state = ThreadState::BLOCKED_SLEEP;
	ready_threads->move_head_to_other_list(sleeping_threads);
	spinlock_release(&scheduler_lock);
	yield();
}

void Scheduler::block_current_thread(ThreadState reason, CircularQueue<ThreadControlBlock>* waiting_list)
{
	spinlock_acquire(&scheduler_lock);
	ThreadControlBlock& current = ready_threads->head();
	current.state = ThreadState::BLOCKED_LOCK;
	ready_threads->move_head_to_other_list(waiting_list);
	spinlock_release(&scheduler_lock);
}

void Scheduler::unblock_thread(CircularQueue<ThreadControlBlock>* waiting_list)
{
	spinlock_acquire(&scheduler_lock);
	waiting_list->move_head_to_other_list(ready_threads);
	spinlock_release(&scheduler_lock);
}

// schedule another thread.
void Scheduler::yield()
{
	asm volatile("int $0x81");
}

// Create thread structure of a new thread
void Scheduler::create_new_thread(void* address)
{
	spinlock_acquire(&scheduler_lock);
	ThreadControlBlock new_thread;
	memset((char*)&new_thread, 0, sizeof(ThreadControlBlock));
	void* thread_stack = Memory::alloc(STACK_SIZE, MEMORY_TYPE::KERNEL | MEMORY_TYPE::WRITABLE);
	ContextFrame* frame = (ContextFrame*)((unsigned)thread_stack + STACK_SIZE - sizeof(ContextFrame));
	frame->eip = (uint32_t)address;
	frame->cs = KCS_SELECTOR;
	frame->eflags = 0x202;
	new_thread.tid = Scheduler::reserve_thread_id();
	new_thread.task_stack = (intptr_t)thread_stack;
	new_thread.context.esp = (unsigned)frame + 4;
	new_thread.state = ThreadState::READY;
	ready_threads->push_back(new_thread);
	spinlock_release(&scheduler_lock);
}

unsigned Scheduler::reserve_thread_id()
{
	unsigned id = m_id_bitmap->find_first_unused();
	m_id_bitmap->set_used(id);
	return id;
}
// Switch the returned context of the current IRQ.
void Scheduler::load_context(ContextFrame* current_context, const ThreadControlBlock* thread)
{
	current_context->esp = thread->context.esp;
	GDT::set_tss_stack(thread->task_stack);
}

// Save current context into its TCB.
void Scheduler::save_context(const ContextFrame* current_context, ThreadControlBlock* thread)
{
	thread->context.esp = current_context->esp;
}

// Switch to page directory
void Scheduler::switch_page_directory(uintptr_t page_directory)
{
	Memory::switch_page_directory(page_directory);
}
