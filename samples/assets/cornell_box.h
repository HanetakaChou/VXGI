//
// Copyright (C) YuqiaoZhang(HanetakaChou)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#ifndef _CORNELL_BOX_H_
#define _CORNELL_BOX_H_ 1

#include <stdint.h>

// NOTE: The ceiling will not cast shadow since we use the backface culling.

// http://www.graphics.cornell.edu/online/box/data.html

// https://casual-effects.com/data/index.html

// Floor

static constexpr uint32_t const g_cornell_box_floor_vertex_count = 4U;

extern float const g_cornell_box_floor_vertex_position[3U * g_cornell_box_floor_vertex_count];

extern float const g_cornell_box_floor_vertex_normal[3U * g_cornell_box_floor_vertex_count];

static constexpr uint32_t const g_cornell_box_floor_index_count = 6U;

extern uint16_t const g_cornell_box_floor_index[g_cornell_box_floor_index_count];

static constexpr float const g_cornell_box_floor_base_color[] = {0.725F, 0.71F, 0.68F};

static constexpr float const g_cornell_box_floor_metallic = 0.0F;

static constexpr float const g_cornell_box_floor_roughness = 0.5F;

// Ceiling

static constexpr uint32_t const g_cornell_box_ceiling_vertex_count = 4U;

extern float const g_cornell_box_ceiling_vertex_position[3U * g_cornell_box_ceiling_vertex_count];

extern float const g_cornell_box_ceiling_vertex_normal[3U * g_cornell_box_ceiling_vertex_count];

static constexpr uint32_t const g_cornell_box_ceiling_index_count = 6U;

extern uint16_t const g_cornell_box_ceiling_index[g_cornell_box_ceiling_index_count];

static constexpr float const g_cornell_box_ceiling_base_color[] = {0.725F, 0.71F, 0.68F};

static constexpr float const g_cornell_box_ceiling_metallic = 0.0F;

static constexpr float const g_cornell_box_ceiling_roughness = 0.5F;

// Back Wall

static constexpr uint32_t const g_cornell_box_back_wall_vertex_count = 4U;

extern float const g_cornell_box_back_wall_vertex_position[3U * g_cornell_box_back_wall_vertex_count];

extern float const g_cornell_box_back_wall_vertex_normal[3U * g_cornell_box_back_wall_vertex_count];

static constexpr uint32_t const g_cornell_box_back_wall_index_count = 6U;

extern uint16_t const g_cornell_box_back_wall_index[g_cornell_box_back_wall_index_count];

static constexpr float const g_cornell_box_back_wall_base_color[] = {0.725F, 0.71F, 0.68F};

static constexpr float const g_cornell_box_back_wall_metallic = 0.0F;

static constexpr float const g_cornell_box_back_wall_roughness = 0.5F;

// Right Wall

static constexpr uint32_t const g_cornell_box_right_wall_vertex_count = 4U;

extern float const g_cornell_box_right_wall_vertex_position[3U * g_cornell_box_right_wall_vertex_count];

extern float const g_cornell_box_right_wall_vertex_normal[3U * g_cornell_box_right_wall_vertex_count];

static constexpr uint32_t const g_cornell_box_right_wall_index_count = 6U;

extern uint16_t const g_cornell_box_right_wall_index[g_cornell_box_right_wall_index_count];

static constexpr float const g_cornell_box_right_wall_base_color[] = {0.14F, 0.45F, 0.091F};

static constexpr float const g_cornell_box_right_wall_metallic = 0.0F;

static constexpr float const g_cornell_box_right_wall_roughness = 0.5F;

// Left Wall

static constexpr uint32_t const g_cornell_box_left_wall_vertex_count = 4U;

extern float const g_cornell_box_left_wall_vertex_position[3U * g_cornell_box_left_wall_vertex_count];

extern float const g_cornell_box_left_wall_vertex_normal[3U * g_cornell_box_left_wall_vertex_count];

static constexpr uint32_t const g_cornell_box_left_wall_index_count = 6U;

extern uint16_t const g_cornell_box_left_wall_index[g_cornell_box_left_wall_index_count];

static constexpr float const g_cornell_box_left_wall_base_color[] = {0.63F, 0.065F, 0.05F};

static constexpr float const g_cornell_box_left_wall_metallic = 0.0F;

static constexpr float const g_cornell_box_left_wall_roughness = 0.5F;

// Short Block

static constexpr uint32_t const g_cornell_box_short_block_vertex_count = 482U;

extern float const g_cornell_box_short_block_vertex_position[3U * g_cornell_box_short_block_vertex_count];

extern float const g_cornell_box_short_block_vertex_normal[3U * g_cornell_box_short_block_vertex_count];

static constexpr uint32_t const g_cornell_box_short_block_index_count = 2880U;

extern uint16_t const g_cornell_box_short_block_index[g_cornell_box_short_block_index_count];

static constexpr float const g_cornell_box_short_block_base_color[] = {0.552083F, 0.552083F, 0.552083F};

static constexpr float const g_cornell_box_short_block_metallic = 0.5F;

static constexpr float const g_cornell_box_short_block_roughness = 0.2F;

// Tall Block

static constexpr uint32_t const g_cornell_box_tall_block_vertex_count = 482U;

extern float const g_cornell_box_tall_block_vertex_position[3U * g_cornell_box_tall_block_vertex_count];

extern float const g_cornell_box_tall_block_vertex_normal[3U * g_cornell_box_tall_block_vertex_count];

static constexpr uint32_t const g_cornell_box_tall_block_index_count = 2880U;

extern uint16_t const g_cornell_box_tall_block_index[g_cornell_box_tall_block_index_count];

static constexpr float const g_cornell_box_tall_block_base_color[] = {0.552083F, 0.552083F, 0.552083F};

static constexpr float const g_cornell_box_tall_block_metallic = 0.5F;

static constexpr float const g_cornell_box_tall_block_roughness = 0.2F;

// Summary

constexpr uint32_t const g_cornell_box_mesh_count = 7U;

constexpr uint32_t const g_cornell_box_mesh_vertex_count[] = {
    g_cornell_box_floor_vertex_count,
    g_cornell_box_ceiling_vertex_count,
    g_cornell_box_back_wall_vertex_count,
    g_cornell_box_right_wall_vertex_count,
    g_cornell_box_left_wall_vertex_count,
    g_cornell_box_short_block_vertex_count,
    g_cornell_box_tall_block_vertex_count};
static_assert(g_cornell_box_mesh_count == (sizeof(g_cornell_box_mesh_vertex_count) / sizeof(g_cornell_box_mesh_vertex_count[0])), "");

constexpr float const *const g_cornell_box_mesh_vertex_position[] = {
    g_cornell_box_floor_vertex_position,
    g_cornell_box_ceiling_vertex_position,
    g_cornell_box_back_wall_vertex_position,
    g_cornell_box_right_wall_vertex_position,
    g_cornell_box_left_wall_vertex_position,
    g_cornell_box_short_block_vertex_position,
    g_cornell_box_tall_block_vertex_position};
static_assert(g_cornell_box_mesh_count == (sizeof(g_cornell_box_mesh_vertex_position) / sizeof(g_cornell_box_mesh_vertex_position[0])), "");

constexpr float const *const g_cornell_box_mesh_vertex_normal[] = {
    g_cornell_box_floor_vertex_normal,
    g_cornell_box_ceiling_vertex_normal,
    g_cornell_box_back_wall_vertex_normal,
    g_cornell_box_right_wall_vertex_normal,
    g_cornell_box_left_wall_vertex_normal,
    g_cornell_box_short_block_vertex_normal,
    g_cornell_box_tall_block_vertex_normal};
static_assert(g_cornell_box_mesh_count == (sizeof(g_cornell_box_mesh_vertex_normal) / sizeof(g_cornell_box_mesh_vertex_normal[0])), "");

constexpr uint32_t const g_cornell_box_mesh_index_count[] = {
    g_cornell_box_floor_index_count,
    g_cornell_box_ceiling_index_count,
    g_cornell_box_back_wall_index_count,
    g_cornell_box_right_wall_index_count,
    g_cornell_box_left_wall_index_count,
    g_cornell_box_short_block_index_count,
    g_cornell_box_tall_block_index_count};
static_assert(g_cornell_box_mesh_count == (sizeof(g_cornell_box_mesh_index_count) / sizeof(g_cornell_box_mesh_index_count[0])), "");

constexpr uint16_t const *const g_cornell_box_mesh_index[] = {
    g_cornell_box_floor_index,
    g_cornell_box_ceiling_index,
    g_cornell_box_back_wall_index,
    g_cornell_box_right_wall_index,
    g_cornell_box_left_wall_index,
    g_cornell_box_short_block_index,
    g_cornell_box_tall_block_index};
static_assert(g_cornell_box_mesh_count == (sizeof(g_cornell_box_mesh_index) / sizeof(g_cornell_box_mesh_index[0])), "");

static constexpr float const *const g_cornell_box_mesh_base_color[] = {
    g_cornell_box_floor_base_color,
    g_cornell_box_ceiling_base_color,
    g_cornell_box_back_wall_base_color,
    g_cornell_box_right_wall_base_color,
    g_cornell_box_left_wall_base_color,
    g_cornell_box_short_block_base_color,
    g_cornell_box_tall_block_base_color};
static_assert(g_cornell_box_mesh_count == (sizeof(g_cornell_box_mesh_base_color) / sizeof(g_cornell_box_mesh_base_color[0])), "");

static constexpr float const g_cornell_box_mesh_metallic[] = {
    g_cornell_box_floor_metallic,
    g_cornell_box_ceiling_metallic,
    g_cornell_box_back_wall_metallic,
    g_cornell_box_right_wall_metallic,
    g_cornell_box_left_wall_metallic,
    g_cornell_box_short_block_metallic,
    g_cornell_box_tall_block_metallic};
static_assert(g_cornell_box_mesh_count == (sizeof(g_cornell_box_mesh_metallic) / sizeof(g_cornell_box_mesh_metallic[0])), "");

static constexpr float const g_cornell_box_mesh_roughness[] = {
    g_cornell_box_floor_roughness,
    g_cornell_box_ceiling_roughness,
    g_cornell_box_back_wall_roughness,
    g_cornell_box_right_wall_roughness,
    g_cornell_box_left_wall_roughness,
    g_cornell_box_short_block_roughness,
    g_cornell_box_tall_block_roughness};
static_assert(g_cornell_box_mesh_count == (sizeof(g_cornell_box_mesh_roughness) / sizeof(g_cornell_box_mesh_roughness[0])), "");

#endif