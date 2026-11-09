/* The export calls stay compilable when COMMANDER is off, and do nothing. */

#include <stdio.h>

typedef signed char mp_ret_t;

#include "../commander.h"

int main(void) {
	if(mp_command_register(0, &not_declared, "mod", "name", 0, 0, 0, 0, not_a_function, 0) != 0)
		return(1);
	mp_command_unregister(0, &not_declared);
	if(mp_command_call(0, 0, "name", 0, 0, 0, 0) != 0)
		return(1);
	if(mp_command_count(0) != 0)
		return(1);
	if(mp_command_find(0, "name") != 0)
		return(1);
	if(mp_command_at(0, 0) != 0)
		return(1);
	if(mp_command_value_text(0, 0, 0, 0) != 0)
		return(1);
	mp_commander_init(0);
	mp_commander_fini(0);
	printf("ok\n");
	return(0);
}
