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
 * spin3.c
 *
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
 *
 * --------------------------------------------------------------------------
 *
 * Thread A locks spin - thread B tries to unlock.
 * This should succeed, but it's undefined behaviour.
 *
 */
#include <os/config.h>

#ifdef ENABLE_PTE_TESTS
#include <pte/test.h>
#include <errno.h>

static int wasHere = 0;

static pthread_spinlock_t spin;

static void * unlocker(void * arg)
{
  int expectedResult = (int) arg;

  wasHere++;
  assert(pthread_spin_unlock(&spin) == expectedResult);
  wasHere++;
  return NULL;
}

int pthread_test_spin3()
{
  pthread_t t;

  wasHere = 0;
  assert(pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE) == 0);
  assert(pthread_spin_lock(&spin) == 0);
  assert(pthread_create(&t, NULL, unlocker, (void *) 0) == 0);
  assert(pthread_join(t, NULL) == 0);
  assert(pthread_spin_unlock(&spin) == EPERM);
  assert(pthread_spin_destroy(&spin) == 0);
  assert(wasHere == 2);

  return 0;
}
#endif