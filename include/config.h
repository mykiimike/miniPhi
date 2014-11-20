
#ifndef _HAVE_CONFIG_H
	#define _HAVE_CONFIG_H

	#define SUPPORT_DRV_LED

	#define _SUPPORT_MEM
	#define _SUPPORT_SERIAL
	#define _SUPPORT_BUTTON
	#define _SUPPORT_PINOUT /* enable pinout feature, need mem support */

	/* mem configuration */
	#ifndef MP_MEM_SIZE
		#define MP_MEM_SIZE  10240 /* total memory allowed for heap */
	#endif

	#ifndef MP_MEM_CHUNK
		#define MP_MEM_CHUNK 50    /* fixed size of a chunck */
	#endif

	/* task configuration */
	#ifndef MP_TASK_MAX
		#define MP_TASK_MAX 10 /* number of maximum task per instance */
	#endif

	/* state configuration */
	#ifndef MP_STATE_MAX
		#define MP_STATE_MAX 5 /* maximum number of machine states */
	#endif

	/* serial configuration */
	#ifndef MP_SERIAL_RX_BUFFER_SIZE
		#define MP_SERIAL_RX_BUFFER_SIZE 512
	#endif

	#ifndef MP_SERIAL_TX_BUFFER_SIZE
		#define MP_SERIAL_TX_BUFFER_SIZE 240
	#endif

	/* button configuration */


#endif
