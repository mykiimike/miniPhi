/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2014  Michael VERGOZ                                      *
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

char *mp_options_get(mp_options_t *options, char *key) {
	while(options->key != NULL && options->value != NULL) {
		if(strcmp(key, options->key) == 0)
			return(options->value);
		options++;
	}
	return(NULL);

}

mp_ret_t mp_options_cmp(char *a, char *b) {
	int aL, bL, i;
	aL = strlen(a);
	bL = strlen(b);
	if(aL != bL)
		return(FALSE);
	for(i=0; i<aL; i++, a++, b++) {
		if(*a != *b)
			return(FALSE);
	}
	return(TRUE);
}
