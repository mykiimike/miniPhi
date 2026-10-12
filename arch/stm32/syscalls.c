/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program; if not, write to the Free Software Foundation, *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA       *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mp.h>
#include <errno.h>
#include <sys/stat.h>

/**
@defgroup mpArchSTM32Syscalls newlib hooks

@ingroup mpArchSTM32

No file system and no console behind stdio: miniPhi prints through
mp_printk(). _sbrk() hands out the HEAP_SIZE block the linker script
reserves between .bss and the stack.

@{
*/

#undef errno
extern int errno;

/* from the linker script */
extern char end;
extern char __heap_size;

void *_sbrk(ptrdiff_t increment) {
	static char *brk = &end;
	char *previous = brk;

	if(brk + increment > &end + (uintptr_t)&__heap_size || brk + increment < &end) {
		errno = ENOMEM;
		return((void *)-1);
	}

	brk += increment;
	return(previous);
}

int _write(int fd, const char *buffer, int size) {
	return(size);
}

int _read(int fd, char *buffer, int size) {
	return(0);
}

int _close(int fd) {
	return(-1);
}

int _lseek(int fd, int offset, int whence) {
	return(0);
}

int _fstat(int fd, struct stat *st) {
	st->st_mode = S_IFCHR;
	return(0);
}

int _isatty(int fd) {
	return(1);
}

int _getpid(void) {
	return(1);
}

int _kill(int pid, int sig) {
	errno = EINVAL;
	return(-1);
}

void _exit(int status) {
	mp_machine_reset();
}

/**@}*/
