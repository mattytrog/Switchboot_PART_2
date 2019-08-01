/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _FE_TOOLS_H_
#define _FE_TOOLS_H_

void dump_packages12();
void fix_sd_all_attr();
void fix_sd_nin_attr();
void fix_battery_desync();
void menu_autorcm();
void flipVertically(unsigned char* pixels_buffer, const unsigned int width, const unsigned int height, const int bytes_per_pixel);
void screenshot();
void fix_fuel_gauge_configuration();
void reset_pmic_fuel_gauge_charger_config();
#endif
