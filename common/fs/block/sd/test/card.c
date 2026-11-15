/* Host test of the SD SPI protocol. Not linked into the firmware. */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define SUPPORT_COMMON_FS
#define SUPPORT_COMMON_FS_BLOCK_SD

typedef struct mp_gpio_port_s mp_gpio_port_t;

struct card_s;

struct mp_gpio_port_s {
	int level;
	struct card_s *card;
};

typedef enum {
	MP_GPIO_MODE_INPUT = 1,
	MP_GPIO_MODE_INPUT_PULL = 2,
	MP_GPIO_MODE_OUTPUT = 3,
} mp_gpio_mode_t;

#include "../../../fs.h"
#include "../sd.h"

enum {
	CARD_WAIT = 0,
	CARD_CMD,
	CARD_RESP,
	CARD_PAYLOAD,
	CARD_WRITE,
	CARD_WRESP
};

typedef struct card_s {
	mp_spi_bus_t *bus;
	int selected;
	int dead;
	int v1;
	int hc;
	int ready;
	int idle_left;
	int app;
	int phase;
	int n;
	int then;
	unsigned char frame[6];
	unsigned char resp[8];
	int resp_n;
	int resp_i;
	unsigned char csd[16];
	unsigned char cid[16];
	unsigned char mem[4][512];
	const unsigned char *src;
	int len;
	unsigned short paycrc;
	int sector;
	unsigned long read_arg;
	unsigned long write_arg;
	unsigned long acmd_arg;
	unsigned char saw_cmd0;
	unsigned char saw_cmd8;
	int crc_ok;
} card_t;

static card_t cards[2];
static unsigned char card_zero[512];
static int releases;
static unsigned long micros;

unsigned long mp_clock_us(void) {
	return(micros += 5);
}

void test_printk(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

char *mp_options_get(mp_options_t *options, char *key) {
	if(!options || !key)
		return(NULL);
	while(options->key && options->value) {
		if(strcmp(options->key, key) == 0)
			return(options->value);
		options++;
	}
	return(NULL);
}

static mp_gpio_port_t pins[16];
static char pin_name[16][16];
static int pin_n;
/** 0 keeps the socket switch low: a card is in. 1 is an empty socket. */
static int cd_level;

mp_gpio_port_t *mp_gpio_text_handle(char *text, char *who) {
	int i;

	(void)who;
	if(!text || !text[0])
		return(NULL);
	for(i = 0; i < pin_n; i++) {
		if(strcmp(pin_name[i], text) == 0)
			return(&pins[i]);
	}
	if(pin_n >= 16)
		return(NULL);
	memset(&pins[pin_n], 0, sizeof(pins[pin_n]));
	memcpy(pin_name[pin_n], text, 15);
	pins[pin_n].level = strcmp(text, "cd-a") == 0 ? cd_level : 1;
	if(strcmp(text, "cs-a") == 0)
		pins[pin_n].card = &cards[0];
	if(strcmp(text, "cs-b") == 0)
		pins[pin_n].card = &cards[1];
	return(&pins[pin_n++]);
}

mp_ret_t mp_gpio_release(mp_gpio_port_t *port) {
	(void)port;
	releases++;
	return(TRUE);
}

void mp_gpio_mode(mp_gpio_port_t *port, mp_gpio_mode_t mode) {
	(void)port;
	(void)mode;
}

void mp_gpio_pull_up(mp_gpio_port_t *port) {
	(void)port;
}

mp_bool_t mp_gpio_read(mp_gpio_port_t *port) {
	if(!port)
		return(NO);
	return(port->level ? YES : NO);
}

static void card_cs(mp_gpio_port_t *port, int level) {
	port->level = level;
	if(!port->card)
		return;
	port->card->selected = level ? 0 : 1;
	port->card->phase = CARD_WAIT;
	port->card->n = 0;
}

void mp_gpio_set(mp_gpio_port_t *port) {
	card_cs(port, 1);
}

void mp_gpio_unset(mp_gpio_port_t *port) {
	card_cs(port, 0);
}

mp_ret_t mp_spi_open(mp_kernel_t *kernel, mp_spi_bus_t *bus, mp_options_t *options, char *who) {
	char *sck = mp_options_get(options, "sck");
	card_t *card = (sck && strcmp(sck, "sck-b") == 0) ? &cards[1] : &cards[0];

	memset(bus, 0, sizeof(*bus));
	bus->sclk = mp_gpio_text_handle(sck, who);
	bus->mosi = mp_gpio_text_handle(mp_options_get(options, "mosi"), who);
	bus->miso = mp_gpio_text_handle(mp_options_get(options, "miso"), who);
	if(!bus->sclk || !bus->mosi || !bus->miso)
		return(FALSE);
	bus->kernel = kernel;
	card->bus = bus;
	return(TRUE);
}

void mp_spi_close(mp_spi_bus_t *bus) {
	int i;

	if(!bus || !bus->kernel)
		return;
	if(cards[0].bus == bus)
		cards[0].bus = NULL;
	if(cards[1].bus == bus)
		cards[1].bus = NULL;
	for(i = 0; i < 3; i++) {
		mp_gpio_port_t *pin = i == 0 ? bus->sclk : i == 1 ? bus->mosi : bus->miso;

		if(pin)
			mp_gpio_release(pin);
	}
	bus->kernel = NULL;
}

static unsigned char crc7(const unsigned char *data, int len) {
	unsigned char crc = 0;
	int i, bit;

	for(i = 0; i < len; i++) {
		unsigned char byte = data[i];

		for(bit = 0; bit < 8; bit++) {
			crc = (unsigned char)(crc << 1);
			if((byte ^ crc) & 0x80)
				crc ^= 0x09;
			byte = (unsigned char)(byte << 1);
		}
	}
	return((unsigned char)((crc << 1) | 1));
}

static unsigned short crc16(const unsigned char *data, int len) {
	unsigned short crc = 0;
	int i, bit;

	for(i = 0; i < len; i++) {
		crc ^= (unsigned short)data[i] << 8;
		for(bit = 0; bit < 8; bit++) {
			if(crc & 0x8000)
				crc = (unsigned short)((crc << 1) ^ 0x1021);
			else
				crc = (unsigned short)(crc << 1);
		}
	}
	return(crc);
}

static void bits_put(unsigned char *reg, int hi, int lo, unsigned long value) {
	int bit;

	for(bit = lo; bit <= hi; bit++) {
		int byte = (127 - bit) / 8;
		int pos = bit % 8;

		if((value >> (bit - lo)) & 1UL)
			reg[byte] |= (unsigned char)(1u << pos);
		else
			reg[byte] &= (unsigned char)~(1u << pos);
	}
}

static void seal(unsigned char *reg) {
	reg[15] = crc7(reg, 15);
}

static void card_queue(card_t *card, const unsigned char *bytes, int n, int then) {
	memcpy(card->resp, bytes, (size_t)n);
	card->resp_n = n;
	card->resp_i = 0;
	card->then = then;
	card->phase = CARD_RESP;
	card->n = 0;
}

static void card_begin_payload(card_t *card, const unsigned char *src, int len) {
	unsigned char r1 = 0x00;

	card->src = src;
	card->len = len;
	card->paycrc = crc16(src, len);
	card_queue(card, &r1, 1, CARD_PAYLOAD);
}

static unsigned long card_arg(card_t *card) {
	return((unsigned long)card->frame[1] << 24) | ((unsigned long)card->frame[2] << 16) |
		((unsigned long)card->frame[3] << 8) | card->frame[4];
}

static void card_command(card_t *card) {
	unsigned char cmd = (unsigned char)(card->frame[0] & 0x3F);
	unsigned long arg = card_arg(card);
	unsigned char r1;

	if(cmd == 0) {
		card->ready = 0;
		card->idle_left = 1;
		card->app = 0;
		card->saw_cmd0 = card->frame[5];
		r1 = 0x01;
		card_queue(card, &r1, 1, CARD_WAIT);
		return;
	}
	if(cmd == 8) {
		card->saw_cmd8 = card->frame[5];
		if(card->v1) {
			r1 = 0x05;
			card_queue(card, &r1, 1, CARD_WAIT);
		}
		else {
			unsigned char tail[5] = { 0x01, 0x00, 0x00, 0x01, 0xAA };

			card_queue(card, tail, 5, CARD_WAIT);
		}
		return;
	}
	if(cmd == 55) {
		card->app = 1;
		r1 = card->ready ? 0x00 : 0x01;
		card_queue(card, &r1, 1, CARD_WAIT);
		return;
	}
	if(cmd == 41 && card->app) {
		card->app = 0;
		card->acmd_arg = arg;
		if(card->idle_left) {
			card->idle_left--;
			r1 = 0x01;
		}
		else {
			card->ready = 1;
			r1 = 0x00;
		}
		card_queue(card, &r1, 1, CARD_WAIT);
		return;
	}
	if(cmd == 58) {
		unsigned char ocr[5];

		ocr[0] = 0x00;
		ocr[1] = (unsigned char)(card->hc ? 0xC0 : 0x80);
		ocr[2] = 0xFF;
		ocr[3] = 0x80;
		ocr[4] = 0x00;
		card_queue(card, ocr, 5, CARD_WAIT);
		return;
	}
	if(cmd == 16) {
		r1 = arg == 512 ? 0x00 : 0x40;
		card_queue(card, &r1, 1, CARD_WAIT);
		return;
	}
	if(cmd == 9) {
		card_begin_payload(card, card->csd, 16);
		return;
	}
	if(cmd == 10) {
		card_begin_payload(card, card->cid, 16);
		return;
	}
	if(cmd == 17 || cmd == 24) {
		unsigned long sector = card->hc ? arg : arg / 512UL;

		card->sector = sector < 4 ? (int)sector : 0;
		if(cmd == 17) {
			card->read_arg = arg;
			card_begin_payload(card, sector < 4 ? card->mem[sector] : card_zero, 512);
		}
		else {
			card->write_arg = arg;
			r1 = 0x00;
			card->crc_ok = 0;
			card_queue(card, &r1, 1, CARD_WRITE);
		}
		return;
	}
	r1 = 0x04;
	card_queue(card, &r1, 1, CARD_WAIT);
}

static unsigned char card_payload(card_t *card) {
	if(card->n == 0) {
		card->n = 1;
		return(0xFE);
	}
	if(card->n <= card->len) {
		unsigned char byte = card->src[card->n - 1];

		card->n++;
		return(byte);
	}
	if(card->n == card->len + 1) {
		card->n++;
		return((unsigned char)(card->paycrc >> 8));
	}
	card->phase = CARD_WAIT;
	return((unsigned char)card->paycrc);
}

static unsigned char card_write(card_t *card, unsigned char byte) {
	if(card->n == 0) {
		if(byte == 0xFF)
			return(0xFF);
		if(byte != 0xFE) {
			card->phase = CARD_WAIT;
			return(0xFF);
		}
		card->n = 1;
		return(0xFF);
	}
	if(card->n <= 512) {
		card->mem[card->sector][card->n - 1] = byte;
		card->n++;
		return(0xFF);
	}
	if(card->n == 513) {
		card->paycrc = (unsigned short)(byte << 8);
		card->n = 514;
		return(0xFF);
	}
	card->paycrc |= byte;
	card->crc_ok = card->paycrc == crc16(card->mem[card->sector], 512);
	card->phase = CARD_WRESP;
	card->n = 0;
	return(0xFF);
}

static unsigned char card_wresp(card_t *card) {
	if(card->n == 0) {
		card->n = 1;
		return(card->crc_ok ? 0x05 : 0x0B);
	}
	if(card->n == 1) {
		card->n = 2;
		return(0x00);
	}
	card->phase = CARD_WAIT;
	return(0xFF);
}

static unsigned char card_byte(card_t *card, unsigned char byte) {
	if(!card->selected || card->dead)
		return(0xFF);
	if(card->phase == CARD_RESP) {
		if(card->resp_i < card->resp_n)
			return(card->resp[card->resp_i++]);
		card->phase = card->then;
		card->n = 0;
	}
	if(card->phase == CARD_WAIT) {
		if(byte == 0xFF)
			return(0xFF);
		card->frame[0] = byte;
		card->n = 1;
		card->phase = CARD_CMD;
		return(0xFF);
	}
	if(card->phase == CARD_CMD) {
		card->frame[card->n++] = byte;
		if(card->n < 6)
			return(0xFF);
		card_command(card);
		return(0xFF);
	}
	if(card->phase == CARD_PAYLOAD)
		return(card_payload(card));
	if(card->phase == CARD_WRITE)
		return(card_write(card, byte));
	if(card->phase == CARD_WRESP)
		return(card_wresp(card));
	return(0xFF);
}

unsigned char mp_spi_xfer(mp_spi_bus_t *bus, unsigned char byte) {
	card_t *card = cards[1].bus == bus ? &cards[1] : &cards[0];

	return(card_byte(card, byte));
}

#include "../../../fs.c"
#include "../sd.c"

static int failed;

static void expect(int cond, const char *msg) {
	if(cond)
		return;
	failed++;
	fprintf(stderr, "fail: %s\n", msg);
}

static mp_options_t opt_a[] = {
	{ "sck", "sck-a" }, { "mosi", "mosi-a" }, { "miso", "miso-a" }, { "cs", "cs-a" }, { NULL, NULL }
};
static mp_options_t opt_b[] = {
	{ "sck", "sck-b" }, { "mosi", "mosi-b" }, { "miso", "miso-b" }, { "cs", "cs-b" }, { NULL, NULL }
};
static mp_options_t opt_short[] = {
	{ "sck", "sck-a" }, { "mosi", "mosi-a" }, { "miso", "miso-a" }, { NULL, NULL }
};
static mp_options_t opt_cd[] = {
	{ "sck", "sck-a" }, { "mosi", "mosi-a" }, { "miso", "miso-a" }, { "cs", "cs-a" }, { "cd", "cd-a" }, { NULL, NULL }
};

static void name_cid(card_t *card, const char *name, unsigned long serial) {
	int i;

	memset(card->cid, 0, sizeof(card->cid));
	for(i = 0; i < 5; i++)
		bits_put(card->cid, 103 - i * 8, 96 - i * 8, (unsigned char)name[i]);
	bits_put(card->cid, 55, 24, serial);
	seal(card->cid);
}

static void prepare(void) {
	memset(cards, 0, sizeof(cards));
	memset(pins, 0, sizeof(pins));
	pin_n = 0;
	releases = 0;
	cd_level = 0;
	cards[0].hc = 1;
	cards[0].idle_left = 1;
	memset(cards[0].csd, 0, 16);
	bits_put(cards[0].csd, 127, 126, 1);
	bits_put(cards[0].csd, 69, 48, 15);
	seal(cards[0].csd);
	name_cid(&cards[0], "SDLX1", 0x1A2B3C4DUL);

	cards[1].v1 = 1;
	cards[1].idle_left = 1;
	memset(cards[1].csd, 0, 16);
	bits_put(cards[1].csd, 83, 80, 9);
	bits_put(cards[1].csd, 73, 62, 255);
	bits_put(cards[1].csd, 49, 47, 0);
	seal(cards[1].csd);
	name_cid(&cards[1], "CARD2", 0x00000010UL);
}

int main(void) {
	mp_kernel_t kernel;
	mp_fs_sd_t a, b;
	unsigned char sector[512];
	unsigned char cmd0[5] = { 0x40, 0, 0, 0, 0 };
	unsigned char cmd8[5] = { 0x48, 0, 0, 1, 0xAA };
	int before;

	memset(&kernel, 0, sizeof(kernel));
	expect(crc7(cmd0, 5) == 0x95, "cmd0 crc");
	expect(crc7(cmd8, 5) == 0x87, "cmd8 crc");
	expect(crc16((const unsigned char *)"123456789", 9) == 0x31C3, "crc16 vector");

	prepare();
	expect(mp_fs_sd_init(&kernel, &a, opt_short, "SD") == FALSE, "cs is required");

	prepare();
	cards[0].dead = 1;
	before = releases;
	expect(mp_fs_sd_init(&kernel, &a, opt_a, "SD") == TRUE, "a missing card keeps the socket");
	expect(mp_fs_sd_present(&a) == NO, "nothing answered");
	expect(releases == before, "the pins stay claimed");
	cards[0].dead = 0;
	expect(mp_fs_sd_probe(&a) == TRUE, "probe finds the card");
	expect(mp_fs_sd_present(&a) == YES && a.ready == YES, "the probe identifies it");
	mp_fs_sd_fini(&a);

	prepare();
	cd_level = 1;
	expect(mp_fs_sd_init(&kernel, &a, opt_cd, "SD") == TRUE, "an empty socket still claims the bus");
	expect(mp_fs_sd_present(&a) == NO && cards[0].saw_cmd0 == 0, "a high detect pin is not clocked");
	cd_level = 0;
	{
		int i;

		for(i = 0; i < pin_n; i++) {
			if(strcmp(pin_name[i], "cd-a") == 0)
				pins[i].level = 0;
		}
	}
	expect(mp_fs_sd_probe(&a) == TRUE && mp_fs_sd_present(&a) == YES, "the pin going low starts a probe");
	{
		int i;

		for(i = 0; i < pin_n; i++) {
			if(strcmp(pin_name[i], "cd-a") == 0)
				pins[i].level = 1;
		}
	}
	expect(mp_fs_sd_present(&a) == NO, "the pin going high means the card is out");
	expect(mp_fs_block_read(&a.block, 0, sector, 1) == FALSE && a.ready == NO, "a read notices the pin");
	mp_fs_sd_fini(&a);

	prepare();
	expect(mp_fs_sd_init(&kernel, &a, opt_a, "A") == TRUE, "init block-address card");
	expect(mp_fs_sd_init(&kernel, &b, opt_b, "B") == TRUE, "init byte-address card");
	expect(a.block.sectors == 16384 && a.block_address == 1, "sdhc capacity");
	expect(strcmp(a.product, "SDLX1") == 0 && a.serial == 0x1A2B3C4DUL, "sdhc identity");
	expect(b.block.sectors == 1024 && b.block_address == 0, "sdsc capacity");
	expect(strcmp(b.product, "CARD2") == 0, "sdsc name");
	expect(cards[0].saw_cmd0 == 0x95 && cards[0].saw_cmd8 == 0x87, "command crc on the wire");
	expect(cards[0].acmd_arg == 0x40000000UL, "v2 init asks for high capacity");
	expect(cards[1].acmd_arg == 0, "v1 init stays in byte addressing");

	memset(sector, 0, sizeof(sector));
	sector[0] = 0x10;
	sector[1] = 0x20;
	sector[2] = 0x30;
	expect(mp_fs_block_write(&a.block, 1, sector, 1) == TRUE, "write one sector");
	expect(cards[0].write_arg == 1 && cards[0].crc_ok, "block address and data crc");
	memset(sector, 0, sizeof(sector));
	expect(mp_fs_block_read(&a.block, 1, sector, 1) == TRUE, "read one sector");
	expect(sector[0] == 0x10 && sector[1] == 0x20 && sector[2] == 0x30, "sector came back");
	expect(cards[0].read_arg == 1, "read uses the block address");
	memset(sector, 0, sizeof(sector));
	expect(mp_fs_block_read(&b.block, 1, sector, 1) == TRUE, "second card reads its own medium");
	expect(sector[0] == 0 && sector[1] == 0 && sector[2] == 0, "the write did not cross cards");
	expect(cards[1].read_arg == 512, "byte address is the sector times 512");
	expect(mp_fs_block_read(&a.block, 16384, sector, 1) == FALSE, "past the end of the card");
	expect(mp_fs_sd_present(&a) == YES, "a range error leaves the card present");
	cards[0].dead = 1;
	expect(mp_fs_block_read(&a.block, 1, sector, 1) == FALSE, "a silent bus fails the read");
	expect(mp_fs_sd_present(&a) == NO && a.ready == NO, "no answer clears the identification");
	cards[0].dead = 0;
	expect(mp_fs_sd_probe(&a) == TRUE && mp_fs_sd_present(&a) == YES, "probe brings it back");

	mp_fs_sd_fini(&a);
	mp_fs_sd_fini(&b);
	expect(a.kernel == NULL && b.kernel == NULL, "fini clears the card");

	if(failed) {
		fprintf(stderr, "%d failed\n", failed);
		return(1);
	}
	printf("ok\n");
	return(0);
}
