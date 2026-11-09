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

/**
 * @defgroup mpCommonPuiPages Built-in pages
 * @ingroup mpCommonPUI
 *
 * The pages PUI ships and an app turns on: the miniPhi splash, the
 * device faces, and the CPU page. Each lives in its own file under
 * pages/. The line metrics the pages share are here.
 * @{
 */

#ifndef _HAVE_MP_PUI_PAGES_H
	#define _HAVE_MP_PUI_PAGES_H

	#include <pui/pui.h>

	/** body line height, at least one pixel */
	static inline int _mp_pui_body(void) {
		int body = mp_pui_font_default.lineHeight;

		return(body > 0 ? body : 1);
	}

	/** title line height, falling back to the body */
	static inline int _mp_pui_title_h(void) {
		int title = mp_pui_font_bold.lineHeight;

		return(title > 0 ? title : _mp_pui_body());
	}

	/** @} */

#endif
