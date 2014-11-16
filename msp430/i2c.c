#include "mp.h"

typedef struct mp_i2c_context_s mp_i2c_context_t;
typedef struct mp_i2c_regs_s mp_i2c_regs_t;
typedef struct mp_i2c_bus_s mp_i2c_bus_t;

typedef void (*mp_i2c_cb_t)(mp_i2c_context_t *);


struct mp_i2c_bus_s {
	unsigned char busId;
	unsigned char *busDevice;
	unsigned long baseAddress;
};

/*
struct mp_i2c_context_s {
	unsigned char busId;

	mp_i2c_cb_t result;
};

*/

struct mp_i2c_slave_s {

	mp_i2c_cb_t onStart;
	mp_i2c_cb_t onStop;
	mp_i2c_cb_t onRestart;
};

struct mp_i2c_master_s {
	mp_i2c_cb_t onDone;
};


static mp_i2c_bus_t _i2c_bus[10];
static unsigned char _i2c_bus_count = 0;


static void __mp_i2c_bus_init(mp_i2c_bus_t *bus) {

}

/**
 * @brief Global I2C initialization
 * @return TRUE or FALSE
 */
mp_ret_t mp_i2c_init() {
	mp_i2c_bus_t *bus;

#ifdef USCI_B0_BASE
	bus = &_i2c_bus[_i2c_bus_count];
	bus->busId = _i2c_bus_count;
	bus->busDevice = "USCI_B0";
	bus->baseAddress = USCI_B0_BASE;
	__mp_i2c_bus_init(bus);
	_i2c_bus_count++;
#endif

#ifdef USCI_B1_BASE
	bus = &_i2c_bus[_i2c_bus_count];
	bus->busId = _i2c_bus_count;
	bus->busDevice = "USCI_B1";
	bus->baseAddress = USCI_B1_BASE;
	__mp_i2c_bus_init(bus);
	_i2c_bus_count++;
#endif

#ifdef USCI_B2_BASE
	bus = &_i2c_bus[_i2c_bus_count];
	bus->busId = _i2c_bus_count;
	bus->busDevice = "USCI_B2";
	bus->baseAddress = USCI_B2_BASE;
	__mp_i2c_bus_init(bus);
	_i2c_bus_count++;
#endif

#ifdef USCI_B3_BASE
	bus = &_i2c_bus[_i2c_bus_count];
	bus->busId = _i2c_bus_count;
	bus->busDevice = "USCI_B3";
	bus->baseAddress = USCI_B3_BASE;
	__mp_i2c_bus_init(bus);
	_i2c_bus_count++;
#endif

	return(TRUE);
}

mp_ret_t mp_i2c_fini() {





}


mp_ret_t mp_i2c_open(mp_i2c_context_t *context) {


}

mp_ret_t mp_i2c_close(mp_i2c_context_t *context) {

}

