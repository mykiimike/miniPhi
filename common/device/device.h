/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * miniPhi - RTOS                                                          *
 * Copyright (C) 2026  Michael VERGOZ                                      *
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

#ifdef SUPPORT_COMMON_DEVICE
#ifndef _HAVE_MP_COMMON_DEVICE_H
	#define _HAVE_MP_COMMON_DEVICE_H

	/**
	 * The board devices behind one interface, so an app never names a driver.
	 * A driver takes part with an mp_device_ops_t and a device entry in its
	 * _phi.js. The configurator lists the board devices of those drivers, with
	 * their storage and their options, in include/gen/devices.h. An app finds
	 * one by class and rank, opens it, and talks to its face: the common
	 * interface of the class.
	 */

	/**
	 * One frame from an MP_DEVICE_CAMERA.
	 * pixels stays put for the life of the device. Each byte is the 14-bit sample
	 * stretched across the range of the previous frame.
	 * sequence is even when pixels is a complete frame and odd while the
	 * driver is writing it. Read sequence, then pixels, then sequence again,
	 * and keep the copy only when both reads match and are even.
	 */
	typedef struct {
		const unsigned char *pixels;
		unsigned short width;
		unsigned short height;
		unsigned long sequence;
	} mp_camera_frame_t;

	typedef enum {
		/** face: mp_tui_display_t */
		MP_DEVICE_DISPLAY,
		/** face: mp_device_input_t, already listed on the kernel while open */
		MP_DEVICE_TOUCH,
		/** a measure: mp_device_measure_t. set(0) sleeps the chip */
		MP_DEVICE_ACCELEROMETER,
		/** an output: mp_device_set() */
		MP_DEVICE_LED,
		/** face: mp_camera_frame_t */
		MP_DEVICE_CAMERA,
		/** address stem temp */
		MP_DEVICE_TEMPERATURE,
		/** address stem gyro */
		MP_DEVICE_GYROSCOPE,
		/** address stem mag */
		MP_DEVICE_MAGNETOMETER,
		/** address stem volt */
		MP_DEVICE_VOLTAGE,
		/** address stem curr */
		MP_DEVICE_CURRENT,
		/** address stem cpu. Published by the arch, not by a board row. */
		MP_DEVICE_CPU,
		/** address stem button */
		MP_DEVICE_BUTTON,
		/** address stem baro. A measure: mp_device_measure_t */
		MP_DEVICE_BAROMETER,
		/** address stem alt. A measure: mp_device_measure_t */
		MP_DEVICE_ALTIMETER,
		/** address stem power. A measure: mp_device_measure_t */
		MP_DEVICE_POWER,
		/** address stem hum. A measure: mp_device_measure_t */
		MP_DEVICE_HUMIDITY,
		/** address stem lux. A measure: mp_device_measure_t */
		MP_DEVICE_ILLUMINANCE,
		/** address stem orient. A measure: mp_device_measure_t */
		MP_DEVICE_ORIENTATION,
		/** address stem clock. Instance: mp_device_time_t, a civil time */
		MP_DEVICE_CLOCK,
		/** address stem charge. Instance: mp_device_charge_t, the cycle the status lines report */
		MP_DEVICE_CHARGER,
		/** not a class: the number of classes above, and the scan that visits every driver */
		MP_DEVICE_CLASS_COUNT,
		MP_DEVICE_ANY = MP_DEVICE_CLASS_COUNT
	} mp_device_class_t;

	typedef struct mp_device_ops_s mp_device_ops_t;
	typedef struct mp_device_s mp_device_t;

	struct mp_device_ops_s {
		const char *name;
		mp_device_class_t type;
		mp_ret_t (*open)(mp_kernel_t *kernel, void *self, mp_options_t *options, char *who);
		void (*close)(void *self);
		/** optional: the class interface, see mp_device_class_t */
		void *(*face)(void *self);
		/** optional: 0 is off, anything else on */
		void (*set)(void *self, int value);
		/** catalog text. NULL when this driver has none. */
		const char *description;
		/** this driver's own version, not the chip behind it. NULL when it has none. */
		const char *version;
	};

	struct mp_device_s {
		/** the name on the board, also the owner of its pins */
		char *name;
		const mp_device_ops_t *driver;
		void *self;
		mp_options_t *options;
		mp_bool_t opened;
		/**
		 * Identity read from the chip. NULL until the driver registers it.
		 * The driver keeps the text; these point at it. label is the name for
		 * people ("FLIR Lepton 2"); a device without one is not listed.
		 */
		const char *label;
		const char *model;
		const char *serial;
		const char *version;
	};

	#ifndef MP_DEVICE_MAX
		/** devices one board can list; the table in include/gen/devices.h is checked against it */
		#define MP_DEVICE_MAX 8
	#endif

	#ifndef MP_DEVICE_ADDRESS
		/** bytes for one canonical address, including the terminating NUL */
		#define MP_DEVICE_ADDRESS 96
	#endif

	/**
	 * One field of an address. ASCII only, forced to lower case.
	 * A space or a dot becomes '-'. Empty, a control byte or a non-ASCII byte fails.
	 * dst is cleared when the call fails and cap is at least 1.
	 */
	mp_ret_t mp_device_address_field(char *dst, int cap, const char *src);

	/**
	 * Build type + index, model and serial into typeN.model.serial.
	 * The type keeps only letters after the field rules, then the index is appended
	 * with no leading zero. Model and serial are fields. The result fits in
	 * MP_DEVICE_ADDRESS.
	 */
	mp_ret_t mp_device_address(char *dst, int cap, const char *type, int num,
		const char *model, const char *serial);

	/**
	 * Recover the stem, the index, the model and the serial.
	 * Three fields, nothing else. The index text is the decimal of the number.
	 * Model and serial come back canonical.
	 */
	mp_ret_t mp_device_address_split(const char *address,
		char *type, int typeCap, int *num,
		char *model, int modelCap, char *serial, int serialCap);

	/** Rewrite an address with the same rules. FALSE when it is not three fields. */
	mp_ret_t mp_device_address_canon(char *dst, int cap, const char *address);

	#ifndef MP_DEVICE_DRIVER_EXTRA
		/** remembered channels above 0, per loaded driver */
		#define MP_DEVICE_DRIVER_EXTRA 4
	#endif

	/**
	 * A driver loaded on its ports. The caller owns the struct.
	 * The devices it registers are chained on driver->devices.
	 * classes is a mask of 1u << mp_device_class_t.
	 */
	typedef struct mp_device_driver_s mp_device_driver_t;
	typedef struct mp_device_driver_ops_s mp_device_driver_ops_t;

	struct mp_device_driver_ops_s {
		/** catalog index of this driver, slashes allowed: sensor/ina219 */
		const char *name;
		const char *description;
		/** this driver's own version, not the miniPhi version */
		const char *version;
		/** classes is a mask of 1u << mp_device_class_t. One bit per class. */
		unsigned int classes;
		/** Claim the ports. NULL when the driver has none. FALSE leaves it unregistered. */
		mp_ret_t (*load)(mp_kernel_t *kernel, mp_device_driver_t *driver, void *self, mp_options_t *options);
		/** Release the ports. Called only after a successful load. */
		void (*unload)(void *self);
		/** Probe the ports and register the devices this driver exposes. */
		void (*scan)(mp_device_driver_t *driver, void *self);
		/** First subscriber on any of its devices. */
		void (*start)(void *self);
		/** Last subscriber left. */
		void (*stop)(void *self);
	};

	struct mp_device_driver_s {
		mp_list_item_t item;
		mp_kernel_t *kernel;
		const mp_device_driver_ops_t *ops;
		void *self;
		mp_options_t *options;
		/** driver index, description and version; not the chip behind it */
		const char *name;
		const char *description;
		const char *version;
		unsigned int classes;
		/** YES after load() returned TRUE. A driver with no load() stays NO. */
		mp_bool_t loaded;
		/** devices registered by this driver */
		mp_list_t devices;
		/** subscribers on its devices; the first calls start, the last calls stop */
		int users;
		/** YES from start until stop */
		mp_bool_t running;
		/**
		 * Index of channel 0 for each class. -1 when this driver does not announce it.
		 * Reserved when the driver is registered, so a failed probe still occupies the slot.
		 */
		short index0[MP_DEVICE_CLASS_COUNT];
		/** channel > 0, remembered so a later register keeps the same index */
		struct {
			mp_bool_t used;
			unsigned char class;
			unsigned char channel;
			short index;
		} extra[MP_DEVICE_DRIVER_EXTRA];
	};

	#ifndef MP_DEVICE_DRIVER_MAX
		/** loaded drivers, board bindings plus the CPU */
		#define MP_DEVICE_DRIVER_MAX 12
	#endif

	void mp_device_driver_init(mp_kernel_t *kernel);
	void mp_device_driver_fini(mp_kernel_t *kernel);

	/**
	 * Publish a driver and load it. Zero the struct before the first call.
	 * Name, description and version come from the ops: they describe the driver.
	 * The same struct registered again returns TRUE and does not load a second time.
	 * FALSE when one of those texts is missing, the table is full, or load() refuses.
	 */
	mp_ret_t mp_device_driver_register(mp_kernel_t *kernel, mp_device_driver_t *driver,
		const mp_device_driver_ops_t *ops, void *self, mp_options_t *options);
	/**
	 * Unload a driver and drop it from this kernel.
	 * FALSE when it is missing or it still has devices.
	 */
	mp_ret_t mp_device_driver_unregister(mp_kernel_t *kernel, mp_device_driver_t *driver);

	int mp_device_driver_count(mp_kernel_t *kernel);
	mp_device_driver_t *mp_device_driver_at(mp_kernel_t *kernel, int index);
	mp_device_driver_t *mp_device_driver_named(mp_kernel_t *kernel, const char *name);

	/**
	 * Index of this channel. Channel 0 was reserved at register.
	 * A later channel is remembered on the driver. -1 when the driver does not announce the class.
	 */
	int mp_device_driver_index(mp_device_driver_t *driver, mp_device_class_t class, int channel);

	/**
	 * One registered device. The caller owns the struct and zeroes it before the first register.
	 * The driver owns the instance. The address is typeN.model.serial.
	 * Subscribers are told when the model or the serial changes, and when the device is removed.
	 */
	typedef struct mp_device_registry_s mp_device_registry_t;

	/**
	 * What a command expects in its text.
	 * NONE is an action. SELECT uses choice. INT and FLOAT use min and max,
	 * and max < min means the number is not bounded.
	 */
	typedef enum {
		MP_DEVICE_ARG_NONE = 0,   /* an action, no value */
		MP_DEVICE_ARG_BOOL,
		MP_DEVICE_ARG_INT,
		MP_DEVICE_ARG_FLOAT,
		MP_DEVICE_ARG_SELECT,
	} mp_device_arg_t;

	typedef struct {
		mp_device_arg_t arg;
		union { int on; long integer; float number; int index; } as;
	} mp_device_value_t;

	/**
	 * One command. The driver owns a static table ended by name == NULL.
	 * label NULL is shown as name. choice is NULL-terminated, for a SELECT.
	 * unit is NULL when the value has none. get is NULL when it cannot be read back.
	 */
	typedef struct mp_device_command_s {
		const char *name;      /* machine name, e.g. "ffc", "polarity" */
		const char *label;     /* for people; NULL falls back to name */
		mp_device_arg_t arg;
		float min, max;        /* INT and FLOAT; max < min means unbounded */
		const char *const *choice; /* SELECT, NULL-terminated */
		const char *unit;      /* NULL when none */
		mp_ret_t (*set)(mp_device_registry_t *device, const mp_device_value_t *value);
		mp_ret_t (*get)(mp_device_registry_t *device, mp_device_value_t *value); /* NULL when it can't be read back */
	} mp_device_command_t;

	struct mp_device_registry_s {
		mp_list_item_t item;
		/** link on driver->devices. item is the link on kernel->devices. */
		mp_list_item_t byDriver;
		mp_device_driver_t *driver;
		mp_device_class_t class;
		unsigned char channel;
		short index;
		void *instance;
		/** driver's text, not copied. NULL when the driver gave none. Not part of the address. */
		const char *version;
		/**
		 * Name of the device for people ("Bosch SMB380"), set by its driver right
		 * after register. Not copied. The address stays the machine name.
		 */
		const char *label;
		/**
		 * Commands this device accepts. The driver sets this right after
		 * register, the same way as label. Not copied. NULL when it has none.
		 * The table ends with name == NULL. A refresh leaves the pointer in place.
		 */
		const mp_device_command_t *commands;
		char address[MP_DEVICE_ADDRESS];
		/** YES while a scan of this driver has not registered it again */
		mp_bool_t stale;
		/** YES once removal has begun, so a subscriber cannot take it from on_gone */
		mp_bool_t removed;
		mp_list_t subs;
	};

	/**
	 * Register one function of a loaded driver, or refresh this struct.
	 * Zero the struct before the first call. The same struct keeps its index.
	 * Model and serial are canonical. When they change, each subscriber's on_gone runs
	 * and the address is replaced. The version may change without that.
	 * NULL when the driver is not loaded, the class is not one it announced,
	 * the instance is missing, or the address is refused.
	 */
	mp_device_registry_t *mp_device_registry_register(mp_device_driver_t *driver, mp_device_registry_t *device,
		mp_device_class_t class, int channel, void *instance,
		const char *model, const char *serial, const char *version);

	/**
	 * Probe loaded drivers. MP_DEVICE_ANY visits every driver that has a scan.
	 * A class visits the drivers that announced it. Their other devices are refreshed
	 * by the same call, because the driver reads the chip once.
	 * Devices still stale afterwards are removed and their subscribers get on_gone.
	 * A driver with no scan is left as it was: a fixed device registered from load stays.
	 */
	void mp_device_registry_scan(mp_kernel_t *kernel, mp_device_class_t class);

	int mp_device_registry_count(mp_kernel_t *kernel);
	/** position in the kernel list, not the address index */
	mp_device_registry_t *mp_device_registry_at(mp_kernel_t *kernel, int index);
	/** The address is canonicalised. NULL when it is not three fields or no device has it. */
	mp_device_registry_t *mp_device_registry_address(mp_kernel_t *kernel, const char *address);
	/** The device of that class whose address index is index. Absent when the probe failed. */
	mp_device_registry_t *mp_device_registry_find(mp_kernel_t *kernel, mp_device_class_t class, int index);

	/**
	 * Prepare the registered-device list. mp_device_init calls this.
	 */
	void mp_device_registry_init(mp_kernel_t *kernel);
	/** Drop every registered device. mp_device_fini calls this before the drivers unload. */
	void mp_device_registry_fini(mp_kernel_t *kernel);
	/** Drop this device. on_gone runs, then the struct is cleared. */
	void mp_device_registry_unregister(mp_device_registry_t *device);

	/** How many commands this device announces. 0 when it has none. */
	int mp_device_command_count(mp_device_registry_t *device);
	/** The command at that index. NULL when index is outside the table. */
	const mp_device_command_t *mp_device_command_at(mp_device_registry_t *device, int index);
	/** The command of that name, compared without regard to case. NULL when there is none. */
	const mp_device_command_t *mp_device_command_find(mp_device_registry_t *device, const char *name);
	/**
	 * Parse text for that command and call its set.
	 * NONE takes NULL or "". BOOL takes 0, 1, on, off, true, false, yes, no.
	 * INT and FLOAT are decimal, with an optional sign, inside min..max unless max < min.
	 * SELECT must be one of choice. Case does not matter.
	 * TRUE when the device accepted the request. It may apply it later.
	 * FALSE when the device is missing, removed or has no driver, the name is
	 * unknown, the text is refused, set is missing, or set returns FALSE.
	 */
	mp_ret_t mp_device_command(mp_device_registry_t *device, const char *name, const char *text);
	/** Read it back. FALSE when the command has no get, or get returns FALSE. */
	mp_ret_t mp_device_command_get(mp_device_registry_t *device, const char *name, mp_device_value_t *value);
	/**
	 * Write value as text. A bool is "1" or "0", a select is its choice token.
	 * Returns the length without the terminating NUL. A short buffer is still
	 * terminated, and the return is the full length.
	 */
	int mp_device_command_text(const mp_device_command_t *command, const mp_device_value_t *value, char *dst, int cap);

	/**
	 * One subscriber. The caller owns the struct and zeroes it before the first subscribe.
	 * on_gone runs when the model or the serial changes, and when the device is removed.
	 * The old address is still in the device. After it returns the struct is free to subscribe again.
	 * On removal the device itself is cleared once on_gone has returned.
	 */
	typedef struct mp_device_subscribe_s mp_device_subscribe_t;
	typedef void (*mp_device_subscribe_gone_t)(mp_device_registry_t *device, void *user);

	struct mp_device_subscribe_s {
		mp_list_item_t item;
		mp_device_registry_t *device;
		mp_device_subscribe_gone_t on_gone;
		void *user;
	};

	/**
	 * Take the instance. The same struct subscribed again returns it without counting twice.
	 * An input is shared. A display or an LED already taken returns NULL.
	 * The first subscriber of the driver calls start, on the driver, before this returns.
	 * NULL when the device is missing, this struct holds another device, or the output is taken.
	 */
	void *mp_device_subscribe(mp_device_registry_t *device, mp_device_subscribe_t *sub);
	/** Release this subscription. The last subscriber of the driver calls stop. No on_gone. */
	void mp_device_subscribe_end(mp_device_subscribe_t *sub);
	/**
	 * Drop every subscriber of this device and release their usage.
	 * on_gone runs before stop, so the chip is still started. The registry calls this.
	 */
	void mp_device_subscribe_notify(mp_device_registry_t *device);

	/**
	 * Class contracts. A driver fills the struct of its class with callbacks and registers
	 * a pointer to it as the device instance. An app subscribes through the class wrapper,
	 * which returns the typed struct, and calls the class API. The driver is never named.
	 */

	/** an MP_DEVICE_LED. The driver keeps state current. */
	typedef struct mp_device_led_s mp_device_led_t;

	struct mp_device_led_s {
		void (*set)(mp_device_led_t *led, int on);
		void (*turn)(mp_device_led_t *led);
		int state;
		void *user;
	};

	/** Subscribe and return the LED. NULL when the output is taken or the device is gone. */
	mp_device_led_t *mp_device_led(mp_device_registry_t *device, mp_device_subscribe_t *sub);
	/** The class API. No-op when the driver gave no callback. */
	void mp_device_led_set(mp_device_led_t *led, int on);
	void mp_device_led_turn(mp_device_led_t *led);
	int mp_device_led_state(mp_device_led_t *led);

	/**
	 * Input engine: the gesture stream a TUI reads. A driver owns an mp_device_input_t, feeds
	 * it from a task or an interrupt, and the TUI reads gestures with mp_device_input_read.
	 * It is also the instance of an MP_DEVICE_TOUCH (and a future pointer/keyboard):
	 * the device layer holds the usage right, this struct is the read path.
	 */
	#ifndef MP_DEVICE_INPUT_QUEUE
		#define MP_DEVICE_INPUT_QUEUE 16
	#endif

	#ifndef MP_DEVICE_INPUT_DRAG_THRESHOLD
		#define MP_DEVICE_INPUT_DRAG_THRESHOLD 6
	#endif

	/** an absolute pointer: touch panel, pen */
	#define MP_DEVICE_INPUT_CAP_TOUCH 0x01
	/** a relative pointer with a visible cursor */
	#define MP_DEVICE_INPUT_CAP_MOUSE 0x02
	#define MP_DEVICE_INPUT_CAP_WHEEL 0x04
	#define MP_DEVICE_INPUT_CAP_KEYS  0x08

	/** printable keys are their character; the others start at 0x100 */
	typedef enum {
		MP_DEVICE_INPUT_KEY_NONE = 0,
		MP_DEVICE_INPUT_KEY_BACKSPACE = 0x08,
		MP_DEVICE_INPUT_KEY_TAB = 0x09,
		MP_DEVICE_INPUT_KEY_ENTER = 0x0D,
		MP_DEVICE_INPUT_KEY_ESCAPE = 0x1B,
		MP_DEVICE_INPUT_KEY_UP = 0x100,
		MP_DEVICE_INPUT_KEY_DOWN,
		MP_DEVICE_INPUT_KEY_LEFT,
		MP_DEVICE_INPUT_KEY_RIGHT,
		MP_DEVICE_INPUT_KEY_HOME,
		MP_DEVICE_INPUT_KEY_END,
		MP_DEVICE_INPUT_KEY_PAGE_UP,
		MP_DEVICE_INPUT_KEY_PAGE_DOWN,
		MP_DEVICE_INPUT_KEY_DELETE,
	} mp_device_input_key_t;

	typedef enum {
		MP_DEVICE_INPUT_NONE = 0,
		/** the pointer moved without a button */
		MP_DEVICE_INPUT_MOVE,
		/** button or finger down at x, y */
		MP_DEVICE_INPUT_DOWN,
		/** released near where it went down */
		MP_DEVICE_INPUT_CLICK,
		/** moved beyond MP_DEVICE_INPUT_DRAG_THRESHOLD while down; x, y is the press point */
		MP_DEVICE_INPUT_DRAG_START,
		/** dx, dy since the previous drag event */
		MP_DEVICE_INPUT_DRAG,
		/** released after a drag; startX, startY is the press point */
		MP_DEVICE_INPUT_DROP,
		/** value is the number of steps, positive away from the user */
		MP_DEVICE_INPUT_WHEEL,
		/** value is an mp_device_input_key_t or a character */
		MP_DEVICE_INPUT_KEY,
	} mp_device_input_type_t;

	typedef struct {
		mp_device_input_type_t type;
		int x, y;
		int dx, dy;
		int startX, startY;
		int value;
	} mp_device_input_event_t;

	typedef struct mp_device_input_s mp_device_input_t;

	struct mp_device_input_s {
		mp_kernel_t *kernel;
		const char *who;
		int caps;

		/** set by mp_device_input_bind(): only that reader takes the events, NULL for any */
		void *owner;

		/** the pointer is clamped to 0..width-1, 0..height-1 when they are set */
		int width;
		int height;

		/* producer side, one producer per input */
		volatile int x, y;
		volatile int moved;
		volatile int down;
		struct {
			unsigned char type;
			unsigned char down;
			short x, y;
			int value;
		} queue[MP_DEVICE_INPUT_QUEUE];
		volatile unsigned char head;
		volatile unsigned char tail;
		volatile int dropped;

		/* reader side */
		int pressed;
		int dragging;
		int pressX, pressY;
		int lastX, lastY;
		int escape;
		int escapeValue;
		unsigned long escapeTick;
		int carriage;
		mp_device_input_event_t pending[4];
		int pendingCount;

		/** woken by every feed; the TUI puts its task there */
		struct mp_task_s *reader;

		/** the driver task, woken when the input stops dozing */
		struct mp_task_s *task;

		/** set by mp_device_input_doze(): nobody looks, the driver may sample slower */
		volatile int dozing;

		mp_list_item_t item;
	};

	/** lists the input on the kernel; caps are MP_DEVICE_INPUT_CAP_* */
	int mp_device_input_init(mp_kernel_t *kernel, mp_device_input_t *input, int caps, const char *who);
	void mp_device_input_fini(mp_device_input_t *input);
	void mp_device_input_bind(mp_device_input_t *input, void *owner);
	void mp_device_input_bounds(mp_device_input_t *input, int width, int height);

	/* feeding, from a driver task or an interrupt */
	void mp_device_input_touch(mp_device_input_t *input, int x, int y, int down);
	void mp_device_input_motion(mp_device_input_t *input, int dx, int dy);
	void mp_device_input_button(mp_device_input_t *input, int down);
	void mp_device_input_wheel(mp_device_input_t *input, int steps);
	void mp_device_input_key(mp_device_input_t *input, int key);
	void mp_device_input_terminal(mp_device_input_t *input, unsigned char c);

	/* reading */
	int mp_device_input_read(mp_device_input_t *input, mp_device_input_event_t *event);
	mp_device_input_t *mp_device_input_next(mp_kernel_t *kernel, mp_device_input_t *after, void *reader);
	unsigned long mp_device_input_waiting(mp_device_input_t *input);
	void mp_device_input_doze(mp_device_input_t *input, int dozing);

	/**
	 * an MP_DEVICE_TOUCH. The instance is the mp_device_input_t the driver feeds and the TUI reads.
	 * The device layer is the usage right (start/stop the sampling); input is the read path.
	 */
	mp_device_input_t *mp_device_touch(mp_device_registry_t *device, mp_device_subscribe_t *sub);

	/**
	 * an MP_DEVICE_BUTTON. A discrete hardware button, not a TUI gesture.
	 * The board sets active (Olimex WKUP PA0 is active high). common/button reports
	 * pin levels; the contract keeps the last state and fires on_change on each edge.
	 * Press patterns come on top of this, in a later slice.
	 */
	typedef struct mp_device_button_s mp_device_button_t;
	typedef void (*mp_device_button_on_t)(mp_device_button_t *button, int down, void *user);

	struct mp_device_button_s {
		/** 0 = active low (pressed reads 0), 1 = active high (pressed reads 1) */
		int active;
		/** last reported state: 1 pressed, 0 released. The contract keeps it. */
		int pressed;
		/** the app sets this after subscribe; fired on each edge */
		mp_device_button_on_t on_change;
		void *user;
	};

	/** Subscribe and return the button. */
	mp_device_button_t *mp_device_button(mp_device_registry_t *device, mp_device_subscribe_t *sub);
	/** The driver reports a raw pin level; the contract applies active and fires on_change. */
	void mp_device_button_level(mp_device_button_t *button, int level);
	/** The driver reports a decoded edge directly; the contract fires on_change. */
	void mp_device_button_edge(mp_device_button_t *button, int down);
	int mp_device_button_pressed(mp_device_button_t *button);

	/**
	 * A measure: the instance of every reading class (accel, temp, volt, curr, baro,
	 * alt, power, humidity, illuminance, orientation, gyro, mag). The driver owns the
	 * struct, registers it as the device instance, and publishes into it from a task
	 * or an interrupt. A reader takes a coherent copy with mp_device_measure_read.
	 * The type and the kernel link of the old mp_sensor_t are gone: the class
	 * and the address carry them. label is the caption, not the device name.
	 */
	typedef struct mp_device_measure_s mp_device_measure_t;

	struct mp_device_measure_s {
		/** YES once a value was published */
		mp_bool_t valid;
		/** clock ticks of the last publication */
		unsigned long stamp;
		/** value[0], or x y z for the 3-axis classes */
		float value[3];
	};

	/** Subscribe and return the measure. NULL when the device is gone. */
	mp_device_measure_t *mp_device_measure(mp_device_registry_t *device, mp_device_subscribe_t *sub);
	/** Publish one value; interrupt safe, NULL ignored */
	void mp_device_measure_publish(mp_device_measure_t *measure, float value);
	/** Publish x, y and z together; interrupt safe, NULL ignored */
	void mp_device_measure_publish3(mp_device_measure_t *measure, float x, float y, float z);
	/**
	 * Copy the last publication.
	 * @param[out] value mp_device_measure_axes() values
	 * @param[out] age clock ticks since it was published, NULL when not wanted
	 * @return FALSE when nothing was published yet
	 */
	mp_ret_t mp_device_measure_read(mp_device_measure_t *measure, float value[3], unsigned long *age);
	/** 3 for the 3-axis classes, 1 otherwise */
	int mp_device_measure_axes(mp_device_class_t class);
	/** The unit a class publishes in, "" for a class that is not a measure */
	const char *mp_device_measure_unit(mp_device_class_t class);
	/** YES for a class whose instance is an mp_device_measure_t */
	mp_bool_t mp_device_measure_class(mp_device_class_t class);
	/** Walk the registered measures. after is the previous one, NULL for the first. */
	mp_device_registry_t *mp_device_measure_next(mp_kernel_t *kernel, mp_device_registry_t *after);

	/**
	 * A clock: civil time. The driver owns an mp_device_time_t, registers it as
	 * the instance, and publishes into it while a subscriber holds the driver.
	 * A reader takes a coherent copy with mp_device_clock_read.
	 * set asks the driver to write that time. TRUE means the request was queued.
	 */
	typedef struct mp_device_time_s mp_device_time_t;

	struct mp_device_time_s {
		/** 2000 to 2099 */
		int year;
		/** 1 to 12, 1 to 31, and 1 to 7 as the chip stores the day of week */
		unsigned char month, day, wday;
		/** 0 to 23, 0 to 59, 0 to 59 */
		unsigned char hour, minute, second;
		/** YES when the oscillator is running */
		mp_bool_t running;
		/** YES once a time was published */
		mp_bool_t valid;
		/** clock ticks of the last publication */
		unsigned long stamp;
		/** the driver. NULL when this copy is only a reading */
		mp_ret_t (*set)(mp_device_time_t *clock, const mp_device_time_t *time);
		void *user;
	};

	/** Subscribe and return the clock. NULL when the device is gone. */
	mp_device_time_t *mp_device_clock(mp_device_registry_t *device, mp_device_subscribe_t *sub);
	/** Publish one reading; interrupt safe, NULL ignored */
	void mp_device_clock_publish(mp_device_time_t *clock, const mp_device_time_t *time);
	/**
	 * Copy the last publication.
	 * @return FALSE when nothing was published yet
	 */
	mp_ret_t mp_device_clock_read(mp_device_time_t *clock, mp_device_time_t *out);
	/** Queue a new time. FALSE when the driver gave no set, or set refused it. */
	mp_ret_t mp_device_clock_set(mp_device_time_t *clock, const mp_device_time_t *time);

	/**
	 * A charger. The driver owns an mp_device_charge_t, registers it as the
	 * instance, and samples into it while a subscriber holds the driver.
	 * The state is charging, done or waiting. STAT1 low is charging, STAT2
	 * low is done, both lines released is waiting. Both lines low leave
	 * that state as it is.
	 *
	 * The device keeps the last reading that came back twice.
	 */
	typedef enum {
		MP_DEVICE_CHARGE_CHARGING = 0,
		MP_DEVICE_CHARGE_DONE,
		MP_DEVICE_CHARGE_WAITING,
	} mp_device_charge_state_t;

	typedef struct mp_device_charge_s mp_device_charge_t;

	struct mp_device_charge_s {
		mp_device_charge_state_t state;
		/** YES once a state was published */
		mp_bool_t valid;
		/** clock ticks of the last publication */
		unsigned long stamp;
		/** last raw reading, held until it repeats */
		mp_device_charge_state_t pending;
		/** 1 when pending differs from the published state and is waiting */
		unsigned char seen;
	};

	/** Subscribe and return the charger. NULL when the device is gone. */
	mp_device_charge_t *mp_device_charge(mp_device_registry_t *device, mp_device_subscribe_t *sub);
	/** Publish one state now; interrupt safe, NULL ignored */
	void mp_device_charge_publish(mp_device_charge_t *charge, mp_device_charge_state_t state);
	/**
	 * Offer charging, done or waiting. Anything else is ignored. The state is
	 * published when the same reading is offered again. NULL ignored.
	 */
	void mp_device_charge_sample(mp_device_charge_t *charge, mp_device_charge_state_t state);
	/**
	 * Copy the last state.
	 * @return FALSE when nothing was published yet
	 */
	mp_ret_t mp_device_charge_read(mp_device_charge_t *charge, mp_device_charge_state_t *state);
	/** "charging", "done" or "waiting". "" when state is none of them. */
	const char *mp_device_charge_text(mp_device_charge_state_t state);

	/**
	 * Start the device manager, then the driver registry.
	 * The kernel calls this before the machine, so the arch can load the CPU.
	 */
	void mp_device_init(mp_kernel_t *kernel);
	/**
	 * Close every device still open, then unload the drivers.
	 * The kernel calls this while tasks and pins still exist.
	 */
	void mp_device_fini(mp_kernel_t *kernel);

	int mp_device_count(void);
	mp_device_t *mp_device_at(int index);
	/** rank counts the devices of that class in board order, from 0 */
	mp_device_t *mp_device_find(mp_device_class_t type, int rank);
	mp_device_t *mp_device_named(const char *name);

	/** TRUE when it is open, also when it already was */
	mp_ret_t mp_device_open(mp_kernel_t *kernel, mp_device_t *device);
	void mp_device_close(mp_device_t *device);
	/** NULL while closed or when the class has no face */
	void *mp_device_face(mp_device_t *device);
	void mp_device_set(mp_device_t *device, int value);

	/**
	 * One option's text. NULL when it is missing or empty.
	 * The pointer refers to the option text.
	 */
	const char *mp_device_option(mp_options_t *options, const char *key);
	/**
	 * The model and serial options a board device must give its driver.
	 * FALSE when either is missing. The pointers refer to the option text.
	 */
	mp_ret_t mp_device_options_identity(mp_options_t *options, const char **model, const char **serial);

	/**
	 * Register the name, model, serial number and software version of the chip.
	 * label is the name for people, model the address field. self is the driver
	 * object passed to open. The strings are not copied, so they must stay valid
	 * after this returns. NULL or "" leaves that field unknown.
	 * A driver calls this again as it learns each one.
	 */
	void mp_device_identify(void *self, const char *label, const char *model, const char *serial, const char *version);

#endif
#endif
