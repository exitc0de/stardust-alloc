/* Copyright (C) 2017, Ward Jaradat
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

#include <os/config.h>
#include <os/startup.h>
#include <os/console.h>
#include <os/sched.h>
#include <os/smalloc.h>
#include <os/smalloc_test.h>
#include <os/smalloc_eval.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#ifdef ENABLE_FS
#include <fs/osal.h>
#include <fs/fs-tests.h>
#endif

#ifdef ENABLE_PTE_TESTS
#include <pte/test.h>
#endif

#ifdef ENABLE_PTE_TESTS
void pte_tests_thread(void *p)
{
	pthread_t r;
	pthread_create(&r, NULL, pte_test_main, NULL);
}
#endif

#ifdef ENABLE_FS
void fs_test_thread(void *p)
{
    if(init_disk())
    {
		int file_len;
		fs_read_test(&file_len);
	}
}
#endif



void* smalloc_tests_thread(void *args) {
   printk("Thread 1!\n");

   //char* my_array = smalloc(100, 1);
   //my_array[10] = 't';
   //printk("my_array[10] = %c\n", my_array[10]);
   // you cn create any othreads you want under here
   //sched_print_threads();
   tests();
   tests();
   tests();
   //tests();
   //printk("DONE\n");
   return NULL;
}

void* smalloc_tests_thread_two(void *args) {
	printk("Thread 2!\n");
	tests();
	tests();
	tests();

	return NULL;
}

void three_smalloc_test_threads(void *args) {
	do_test(5, 10000);
	// pthread_t first;
	// pthread_t second;
	// pthread_t third;
	// pthread_create(&first, NULL, smalloc_tests_thread, NULL);
	// pthread_create(&second, NULL, smalloc_tests_thread_two, NULL);
	// pthread_create(&third, NULL, smalloc_tests_thread, NULL);

	// pthread_join(first, NULL);
	// pthread_join(second, NULL);
	// pthread_join(third, NULL);
	return;
}


__attribute__((weak)) int app_main(struct app_main_args *aargs)
{

/* This function can be overwritten to permit the kernel to be linked with
an external application that defines the same function. Currently, in this
function we provide a list of default tests.
*/

	printf("Hello: Startdust is running?\n");
	create_thread("three_smalloc_test_threads", three_smalloc_test_threads, UKERNEL_FLAG, NULL);

#ifdef ENABLE_PTE_TESTS
	/* Example for running the pthread tests. Please note that these tests 
	have been ported and adapted from PTE for experimentation purposes and
	in order to run them, for now, you need to:
	
	Enable the commented out OBJS+= directive for lib/pte/tests/*.c in the
	makefile, and define ENABLE_PTE_TESTS in os/config.h header file.
	
	Kindly note that these tests need to be revisited as long as the work
	on the optimisation of the scheduler continues.
	*/
	create_thread("pte_tests_thread", pte_tests_thread, UKERNEL_FLAG, NULL);
#endif

#ifdef ENABLE_FS
	create_thread("fs_test_thread", fs_test_thread, UKERNEL_FLAG, NULL);
#endif 
	return 0;
}
