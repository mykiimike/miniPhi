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
	#include <gen/pui_scale.h>

	/**
	 * Layout unit. The pages are written for a 150 ppi glass. MP_PUI_DP(n)
	 * is n of those pixels on this glass, rounded. At scale 1 it is n.
	 */
	#define MP_PUI_DP(n) ((int)(((long)(n) * MP_PUI_SCALE_NUM + MP_PUI_SCALE_DEN / 2) / MP_PUI_SCALE_DEN))

	/** glass pixels back to layout units, for the size tests the pages branch on */
	static inline int _mp_pui_units(int px) {
		return((int)(((long)px * MP_PUI_SCALE_DEN) / MP_PUI_SCALE_NUM));
	}

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
