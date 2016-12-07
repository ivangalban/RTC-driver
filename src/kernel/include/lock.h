/* Global locking.
 *
 * Some routines require to run atomically or otherwise interrupts ocurring
 * in the middle of the execution can corrupt kernel data (e.g. memory is being
 * allocated, and interrupt comes and the handler tries to allocate memory).
 * Since we are not multitasking the only situation on which a code is taken
 * out of the processor is precisely when interrupts come, thus we can rely on
 * cli and sti to avoid race conditions. Thus, critical regions are enclosed
 * by calls to cli and sti. However, this is only valid is the users of the
 * critical regions are aware of the existence of a critical region and operate
 * wisely, which is not efficient because critical regions can be small
 * sections of code inside large functions. In such scenarios the callers would
 * have to block the WHOLE execution of the function, which is not desirable.
 * It should be the responsibility of the function to protect the critical
 * region, but then the function should know whether to execute sti or not
 * after the critial region because arbitrarily issuing sti would lead to
 * undesired problems when running interrupt handlers because they all assume
 * interrupts are disabled when they are being run.
 * Thus, the solution we've come with is a little patch: we'll always cli when
 * entering the critical region, but will conditionally sti when we're done.
 * The condition will check whether we're inside an interrupt handler or not
 * and this will be known by reading a flag which will be set only before
 * entering interrupt handlers and clear after they are done. The rationale
 * behind this is:
 *
 *  If a non-interrupt code reaches the lock first interrupts will be cleared,
 *  then no interrupts will take place (let's hope no exceptions nor NMI
 *  take place). The flag will be cleared so we can issue sti after we're done.
 *
 *  If a interrupt code reaches the lock first then there was no non-interrupt
 *  code in the critical region, otherwise cli would have been called and
 *  interrupt would have not taken place. In such a case, the flag would be
 *  set before calling the handler and cleared after, so during the execution
 *  of the handler sti can not be issued, but after the interrupt is done
 *  iret will set it again.
 *
 * The two scenarios fordid each other to run, and this should be agnostic
 * to the function calling lock and unlock.
 *
 * This is actually implemented in interrupts.c, for it belongs there. This
 * header is just separated from interrupts.h because I hope in a future have
 * a better locking mechanism. */

#ifndef __LOCK_H__
#define __LOCK_H__

void lock();
void unlock();

#endif
