#include <mp.h>

#ifdef SUPPORT_DRV_LSM9DS0

mp_ret_t mp_drv_LSM9DS0_init(mp_kernel_t *kernel, mp_drv_LSM9DS0_t *LSM9DS0, mp_options_t *options, char *who) {
	mp_ret_t ret;

	/* open spi */
	ret = mp_spi_open(kernel, &LSM9DS0->spi, options, "LSM9DS0");
	if(ret == FALSE)
		return(FALSE);

	mp_options_t setup[] = {
			{ "frequency", "10000000" },
			{ "phase", "capture" },
			{ "polarity", "high" },
			{ "first", "MSB" },
			{ "role", "master" },
			{ "bit", "8" },
			{ "flow", "sync" },
			{ NULL, NULL }
	};
	ret = mp_spi_setup(&LSM9DS0->spi, setup);
	if(ret == FALSE) {
		mp_spi_close(&LSM9DS0->spi);
		return(FALSE);
	}

}


mp_ret_t mp_drv_LSM9DS0_fini(mp_drv_LSM9DS0_t *LSM9DS0) {


}
#endif

