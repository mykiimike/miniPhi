#ifndef _HAVE_LIST_H
	#define _HAVE_LIST_H

	typedef struct mp_list_s mp_list_t;
	typedef struct mp_list_item_s mp_list_item_t;

	struct mp_list_s {
		mp_list_item_t *first;
		mp_list_item_t *last;

	};

	struct mp_list_item_s {
		mp_list_item_t *next;
		mp_list_item_t *prev;
		void *user;
	};

	static inline void mp_list_init(mp_list_t *list) {
		list->first = NULL;
		list->last = NULL;
	}

	static inline void mp_list_add_last(mp_list_t *list, mp_list_item_t *item, void *user) {
		item->user = user;
		if(list->last == NULL) {
			list->first = item;
			list->last = item;
		}
		else {
			list->last->next = item;
			item->prev = list->last;
		}
		list->last = item;
	}

	static inline void mp_list_add_first(mp_list_t *list, mp_list_item_t *item, void *user) {
		item->user = user;
		if(list->first == NULL) {
			list->first = item;
			list->last = item;
		}
		else {
			list->first->prev = item;
			item->next = list->first;
		}
		list->first = item;
	}

	static inline void mp_list_remove(mp_list_t *list, mp_list_item_t *item) {
		/* remove links */
		if(item->next != NULL) {
			if(item->prev != NULL)
				item->next->prev = item->prev;
			else
				item->next->prev = NULL;
		}

		if(item->prev != NULL) {
			if(item->next != NULL)
				item->prev->next = item->next;
			else
				item->prev->next = NULL;
		}

		if(list->last == item) {
			if(item->prev != NULL)
				list->last = item->prev;
			else
				list->last = NULL;
		}

		if(list->first == item) {
			if(item->next != NULL)
				list->first = item->next;
			else
				list->first = NULL;
		}

		item->prev = NULL;
		item->next = NULL;
	}


#endif
