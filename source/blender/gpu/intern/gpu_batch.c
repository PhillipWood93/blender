/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "GPU_batch.h"
#include "gpu_shader_private.h"

void Batch_set_builtin_program(Gwn_Batch *batch, GPUBuiltinShader shader_id)
{
	GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);
	GWN_batch_program_set(batch, shader->program, shader->interface);
}

static Gwn_Batch *sphere_high = NULL;
static Gwn_Batch *sphere_med = NULL;
static Gwn_Batch *sphere_low = NULL;
static Gwn_Batch *sphere_wire_low = NULL;
static Gwn_Batch *sphere_wire_med = NULL;

static Gwn_VertBuf *vbo;
static Gwn_VertFormat format = {0};
static unsigned int pos_id, nor_id;
static unsigned int vert;

static void batch_sphere_lat_lon_vert(float lat, float lon)
{
	float pos[3];
	pos[0] = sinf(lat) * cosf(lon);
	pos[1] = cosf(lat);
	pos[2] = sinf(lat) * sinf(lon);

	GWN_vertbuf_attr_set(vbo, nor_id, vert, pos);
	GWN_vertbuf_attr_set(vbo, pos_id, vert++, pos);
}

/* Replacement for gluSphere */
static Gwn_Batch *batch_sphere(int lat_res, int lon_res)
{
	const float lon_inc = 2 * M_PI / lon_res;
	const float lat_inc = M_PI / lat_res;
	float lon, lat;

	if (format.attrib_ct == 0) {
		pos_id = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		nor_id = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	}

	vbo = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(vbo, (lat_res - 1) * lon_res * 6);
	vert = 0;

	lon = 0.0f;
	for (int i = 0; i < lon_res; i++, lon += lon_inc) {
		lat = 0.0f;
		for (int j = 0; j < lat_res; j++, lat += lat_inc) {
			if (j != lat_res - 1) { /* Pole */
				batch_sphere_lat_lon_vert(lat + lat_inc, lon + lon_inc);
				batch_sphere_lat_lon_vert(lat + lat_inc, lon);
				batch_sphere_lat_lon_vert(lat,           lon);
			}

			if (j != 0) { /* Pole */
				batch_sphere_lat_lon_vert(lat,           lon + lon_inc);
				batch_sphere_lat_lon_vert(lat + lat_inc, lon + lon_inc);
				batch_sphere_lat_lon_vert(lat,           lon);
			}
		}
	}

	return GWN_batch_create_ex(GWN_PRIM_TRIS, vbo, NULL, GWN_BATCH_OWNS_VBO);
}

static Gwn_Batch *batch_sphere_wire(int lat_res, int lon_res)
{
	const float lon_inc = 2 * M_PI / lon_res;
	const float lat_inc = M_PI / lat_res;
	float lon, lat;

	if (format.attrib_ct == 0) {
		pos_id = GWN_vertformat_attr_add(&format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
		nor_id = GWN_vertformat_attr_add(&format, "nor", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	}

	vbo = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(vbo, (lat_res * lon_res * 2) + ((lat_res - 1) * lon_res * 2));
	vert = 0;

	lon = 0.0f;
	for (int i = 0; i < lon_res; i++, lon += lon_inc) {
		lat = 0.0f;
		for (int j = 0; j < lat_res; j++, lat += lat_inc) {
			batch_sphere_lat_lon_vert(lat + lat_inc, lon);
			batch_sphere_lat_lon_vert(lat,           lon);

			if (j != lat_res - 1) { /* Pole */
				batch_sphere_lat_lon_vert(lat + lat_inc, lon + lon_inc);
				batch_sphere_lat_lon_vert(lat + lat_inc, lon);
			}
		}
	}

	return GWN_batch_create_ex(GWN_PRIM_LINES, vbo, NULL, GWN_BATCH_OWNS_VBO);
}

Gwn_Batch *Batch_get_sphere(int lod)
{
	BLI_assert(lod >= 0 && lod <= 2);

	if (lod == 0)
		return sphere_low;
	else if (lod == 1)
		return sphere_med;
	else
		return sphere_high;
}

Gwn_Batch *Batch_get_sphere_wire(int lod)
{
	BLI_assert(lod >= 0 && lod <= 1);

	if (lod == 0)
		return sphere_wire_low;
	else
		return sphere_wire_med;
}

void gpu_batch_init(void)
{
	/* Hard coded resolution */
	sphere_low = batch_sphere(8, 16);
	sphere_med = batch_sphere(16, 10);
	sphere_high = batch_sphere(32, 24);

	sphere_wire_low = batch_sphere_wire(6, 8);
	sphere_wire_med = batch_sphere_wire(8, 16);
}

void gpu_batch_exit(void)
{
	GWN_batch_discard(sphere_low);
	GWN_batch_discard(sphere_med);
	GWN_batch_discard(sphere_high);
	GWN_batch_discard(sphere_wire_low);
	GWN_batch_discard(sphere_wire_med);
}
