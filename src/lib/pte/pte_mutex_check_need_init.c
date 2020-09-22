/* Copyright (C) 2017, Ward Jaradat and Jonathan Lewis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * pte_mutex_check_need_init.c
 *
 * Description:
 * This translation unit implements mutual exclusion (mutex) primitives.
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads-embedded (PTE) - POSIX Threads Library for embedded systems
 *      Copyright(C) 2008 Jason Schmidlapp
 *
 *      Contact Email: jschmidlapp@users.sourceforge.net
 *
 *
 *      Based upon Pthreads-win32 - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999,2005 Pthreads-win32 contributors
 *
 *      Contact Email: rpj@callisto.canberra.edu.au
 *
 *      The original list of contributors to the Pthreads-win32 project
 *      is contained in the file CONTRIBUTORS.ptw32 included with the
 *      source code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      http://sources.redhat.com/pthreads-win32/contributors.html
 *
 *      This library is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU Lesser General Public
 *      License as published by the Free Software Foundation; either
 *      version 2 of the License, or (at your option) any later version.
 *
 *      This library is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *      Lesser General Public License for more details.
 *
 *      You should have received a copy of the GNU Lesser General Public
 *      License along with this library in the file COPYING.LIB;
 *      if not, write to the Free Software Foundation, Inc.,
 *      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <os/config.h>

#ifdef ENABLE_PTE

#include <pte/pthread.h>
#include <pte/implement.h>
#include <pte/pte_generic_osal.h>
#include <errno.h>

static struct pthread_mutexattr_t_ pte_recursive_mutexattr_s =
{
		PTHREAD_PROCESS_PRIVATE, PTHREAD_MUTEX_RECURSIVE
};
static struct pthread_mutexattr_t_ pte_errorcheck_mutexattr_s =
{
		PTHREAD_PROCESS_PRIVATE, PTHREAD_MUTEX_ERRORCHECK
};
static pthread_mutexattr_t pte_recursive_mutexattr = &pte_recursive_mutexattr_s;
static pthread_mutexattr_t pte_errorcheck_mutexattr = &pte_errorcheck_mutexattr_s;


int
pte_mutex_check_need_init (pthread_mutex_t * mutex)
{
	register int result = 0;
	register pthread_mutex_t mtx;

	/*
	 * The following guarded test is specifically for statically
	 * initialised mutexes (via PTHREAD_MUTEX_INITIALIZER).
	 *
	 * Note that by not providing this synchronisation we risk
	 * introducing race conditions into applications which are
	 * correctly written.
	 *
	 * Approach
	 * --------
	 * We know that static mutexes will not be PROCESS_SHARED
	 * so we can serialise access to internal state using
	 * critical sections rather than mutexes.
	 *
	 * If using a single global lock slows applications down too much,
	 * multiple global locks could be created and hashed on some random
	 * value associated with each mutex, the pointer perhaps. At a guess,
	 * a good value for the optimal number of global locks might be
	 * the number of processors + 1.
	 *
	 */


	pte_osMutexLock (pte_mutex_test_init_lock);

	/*
	 * We got here possibly under race
	 * conditions. Check again inside the critical section
	 * and only initialise if the mutex is valid (not been destroyed).
	 * If a static mutex has been destroyed, the application can
	 * re-initialise it only by calling pthread_mutex_init()
	 * explicitly.
	 */
	mtx = *mutex;

	if (mtx == PTHREAD_MUTEX_INITIALIZER)
	{
		result = pthread_mutex_init (mutex, NULL);
	}
	else if (mtx == PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
	{
		result = pthread_mutex_init (mutex, &pte_recursive_mutexattr);
	}
	else if (mtx == PTHREAD_ERRORCHECK_MUTEX_INITIALIZER)
	{
		result = pthread_mutex_init (mutex, &pte_errorcheck_mutexattr);
	}
	else if (mtx == NULL)
	{
		/*
		 * The mutex has been destroyed while we were waiting to
		 * initialise it, so the operation that caused the
		 * auto-initialisation should fail.
		 */
		result = EINVAL;
	}

	pte_osMutexUnlock(pte_mutex_test_init_lock);

	return (result);
}

#endif



