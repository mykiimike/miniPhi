
#ifndef _HAVE_CONFIG_H
#define _HAVE_CONFIG_H

	/* task configuration */
	#ifndef MP_TASK_MAX
		#define MP_TASK_MAX 10 /* number of maximum task per instance */
	#endif

	/* state configuration */
	#ifndef MP_STATE_MAX
		#define MP_STATE_MAX 5 /* maximum number of machine states */
	#endif

	/* serial configuration */
	#define _SUPPORT_SERIAL
	#ifndef MP_SERIAL_RX_BUFFER_SIZE
		#define MP_SERIAL_RX_BUFFER_SIZE 512
	#endif

	#ifndef MP_SERIAL_TX_BUFFER_SIZE
		#define MP_SERIAL_TX_BUFFER_SIZE 240
	#endif



#endif
