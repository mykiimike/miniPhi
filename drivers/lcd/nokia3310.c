#include <mp.h>



typedef struct mp_drv_nokia3310_s mp_drv_nokia3310_t;

struct mp_drv_nokia3310_s {
	mp_kernel_t *kernel;
	mp_spi_t *spi;
};

/*
 * SOMI=<port>
 * SIMO=<port>
 * STE=<port>
 * CLK=<port>
 * DC=<port>
 * CONTRATS=<port>
 * GATE=<gate name>
 */
mp_drv_nokia3310_t *mp_drv_nokia3310_init(mp_kernel_t *kernel, mp_options_t **options) {


}

mp_ret_t mp_drv_nokia3310_fini(mp_drv_nokia3310_t *drv) {


}
