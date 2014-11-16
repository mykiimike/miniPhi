#ifndef _HAVE_MP_SERIAL_H
	#define _HAVE_MP_SERIAL_H

	#ifdef _SUPPORT_SERIAL
		typedef struct mp_serial_s mp_serial_t;

		typedef void (*mp_serial_on_t)(mp_serial_t *serial);

		struct mp_serial_s {
			mp_uart_t uart;

			unsigned char rx_buffer[MP_SERIAL_RX_BUFFER_SIZE];
			int rx_size;
			int rx_pos;

			unsigned char tx_buffer[MP_SERIAL_TX_BUFFER_SIZE];
			int tx_size;
			int tx_pos;

			mp_serial_on_t onRead;
			mp_serial_on_t onWrite;
		};

		mp_ret_t mp_serial_init(mp_serial_t *serial, char *who);
		mp_ret_t mp_serial_fini(mp_serial_t *serial);

		void mp_serial_println(mp_serial_t *serial, char *text);
		void mp_serial_write(mp_serial_t *serial, char *input, int size);
	#endif

#endif
