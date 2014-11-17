
#ifndef _HAVE_MEM_H
	#define _HAVE_MEM_H

	#ifdef _SUPPORT_MEM


		typedef struct mp_mem_chunk_s mp_mem_chunk_t;

		struct mp_mem_chunk_s {
			unsigned char data[MP_MEM_CHUNK];
		};

		mp_ret_t mp_mem_erase(mp_kernel_t *kernel);
		void *mp_mem_alloc(mp_kernel_t *kernel, int size);
		void mp_mem_free(mp_kernel_t *kernel, void *ptr);
	#endif

#endif
