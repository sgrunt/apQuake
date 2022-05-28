/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2016 Axel Gneiting

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef GLQUAKE_H
#define GLQUAKE_H

#include "tasks.h"
#include "atomics.h"

void     GL_WaitForDeviceIdle (void);
qboolean GL_BeginRendering (int *x, int *y, int *width, int *height);
qboolean GL_AcquireNextSwapChainImage (void);
void     GL_EndRendering (qboolean swapchain_acquired);
qboolean GL_Set2D (cb_context_t *cbx);

extern int glx, gly, glwidth, glheight;

// r_local.h -- private refresh defs

#define ALIAS_BASE_SIZE_RATIO (1.0 / 11.0)
// normalizing factor so player model works out to about
//  1 pixel per triangle
#define MAX_LBM_HEIGHT        480

#define TILE_SIZE 128 // size of textures generated by R_GenTiledSurf

#define SKYSHIFT 7
#define SKYSIZE  (1 << SKYSHIFT)
#define SKYMASK  (SKYSIZE - 1)

#define BACKFACE_EPSILON 0.01

#define MAX_GLTEXTURES       4096
#define MAX_SANITY_LIGHTMAPS 256

#define NUM_COLOR_BUFFERS              2
#define INITIAL_STAGING_BUFFER_SIZE_KB 16384

#define FAN_INDEX_BUFFER_SIZE 126

#define LIGHTMAP_BYTES 4

void       R_TimeRefresh_f (void);
void       R_ReadPointFile_f (void);
texture_t *R_TextureAnimation (texture_t *base, int frame);

typedef enum
{
	pt_static,
	pt_grav,
	pt_slowgrav,
	pt_fire,
	pt_explode,
	pt_explode2,
	pt_blob,
	pt_blob2
} ptype_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
	// driver-usable fields
	vec3_t             org;
	float              color;
	// drivers never touch the following fields
	struct particle_s *next;
	vec3_t             vel;
	float              ramp;
	float              die;
	ptype_t            type;
} particle_t;

#define P_INVALID -1
#ifdef PSET_SCRIPT
void PScript_InitParticles (void);
void PScript_Shutdown (void);
void PScript_DrawParticles (cb_context_t *cbx);
void PScript_DrawParticles_ShowTris (cb_context_t *cbx);
struct trailstate_s;
int  PScript_ParticleTrail (vec3_t startpos, vec3_t end, int type, float timeinterval, int dlkey, vec3_t axis[3], struct trailstate_s **tsk);
int  PScript_RunParticleEffectState (vec3_t org, vec3_t dir, float count, int typenum, struct trailstate_s **tsk);
void PScript_RunParticleWeather (vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, const char *efname);
void PScript_EmitSkyEffectTris (qmodel_t *mod, msurface_t *fa, int ptype);
int  PScript_FindParticleType (const char *fullname);
int  PScript_RunParticleEffectTypeString (vec3_t org, vec3_t dir, float count, const char *name);
int  PScript_EntParticleTrail (vec3_t oldorg, entity_t *ent, const char *name);
int  PScript_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void PScript_DelinkTrailstate (struct trailstate_s **tsk);
void PScript_ClearParticles (void);
void PScript_UpdateModelEffects (qmodel_t *mod);
void PScript_ClearSurfaceParticles (qmodel_t *mod); // model is being unloaded.

extern int r_trace_line_cache_counter;
#define InvalidateTraceLineCache()    \
	do                                \
	{                                 \
		++r_trace_line_cache_counter; \
	} while (0);
#else
#define PScript_RunParticleEffectState(o, d, c, t, s)   true
#define PScript_RunParticleEffectTypeString(o, d, c, n) true // just unconditionally returns an error
#define PScript_EntParticleTrail(o, e, n)               true
#define PScript_ParticleTrail(o, e, t, d, a, s)         true
#define PScript_EntParticleTrail(o, e, n)               true
#define PScript_RunParticleEffect(o, d, p, c)           true
#define PScript_RunParticleWeather(min, max, d, c, p, n)
#define PScript_ClearSurfaceParticles(m)
#define PScript_DelinkTrailstate(tsp)
#define InvalidateTraceLineCache()
#endif

typedef struct vulkan_pipeline_layout_s
{
	VkPipelineLayout    handle;
	VkPushConstantRange push_constant_range;
} vulkan_pipeline_layout_t;

typedef struct vulkan_pipeline_s
{
	VkPipeline               handle;
	vulkan_pipeline_layout_t layout;
} vulkan_pipeline_t;

typedef struct vulkan_desc_set_layout_s
{
	VkDescriptorSetLayout handle;
	int                   num_combined_image_samplers;
	int                   num_ubos;
	int                   num_ubos_dynamic;
	int                   num_storage_buffers;
	int                   num_input_attachments;
	int                   num_storage_images;
} vulkan_desc_set_layout_t;

typedef enum
{
	VULKAN_MEMORY_TYPE_DEVICE,
	VULKAN_MEMORY_TYPE_HOST,
} vulkan_memory_type_t;

typedef struct vulkan_memory_s
{
	VkDeviceMemory       handle;
	size_t               size;
	vulkan_memory_type_t type;
} vulkan_memory_t;

#define WORLD_PIPELINE_COUNT        8
#define FTE_PARTICLE_PIPELINE_COUNT 16
#define MAX_BATCH_SIZE              65536
#define NUM_WORLD_CBX               4

typedef enum
{
	CBX_WORLD_0,
	CBX_WORLD_1,
	CBX_WORLD_2,
	CBX_WORLD_3,
	CBX_ENTITIES,
	CBX_SKY_AND_WATER,
	CBX_ALPHA_ENTITIES,
	CBX_PARTICLES,
	CBX_VIEW_MODEL,
	CBX_GUI,
	CBX_POST_PROCESS,
	CBX_NUM,
} secondary_cb_contexts_t;

typedef struct cb_context_s
{
	VkCommandBuffer   cb;
	canvastype        current_canvas;
	VkRenderPass      render_pass;
	int               render_pass_index;
	int               subpass;
	vulkan_pipeline_t current_pipeline;
	uint32_t          vbo_indices[MAX_BATCH_SIZE];
	unsigned int      num_vbo_indices;
} cb_context_t;

typedef struct
{
	VkDevice                         device;
	qboolean                         device_idle;
	qboolean                         validation;
	qboolean                         debug_utils;
	VkQueue                          queue;
	cb_context_t                     primary_cb_context;
	cb_context_t                     secondary_cb_contexts[CBX_NUM];
	VkClearValue                     color_clear_value;
	VkFormat                         swap_chain_format;
	qboolean                         want_full_screen_exclusive;
	qboolean                         swap_chain_full_screen_exclusive;
	qboolean                         swap_chain_full_screen_acquired;
	VkPhysicalDeviceProperties       device_properties;
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t                         gfx_queue_family_index;
	VkFormat                         color_format;
	VkFormat                         depth_format;
	VkSampleCountFlagBits            sample_count;
	qboolean                         supersampling;
	qboolean                         non_solid_fill;
	qboolean                         screen_effects_sops;

	// Instance extensions
	qboolean get_surface_capabilities_2;
	qboolean get_physical_device_properties_2;
	qboolean vulkan_1_1_available;

	// Device extensions
	qboolean dedicated_allocation;
	qboolean full_screen_exclusive;

	// Buffers
	VkImage color_buffers[NUM_COLOR_BUFFERS];

	// Index buffers
	VkBuffer fan_index_buffer;

	// Staging buffers
	int staging_buffer_size;

	// Render passes
	VkRenderPass warp_render_pass;

	// Pipelines
	vulkan_pipeline_t        basic_alphatest_pipeline[2];
	vulkan_pipeline_t        basic_blend_pipeline[2];
	vulkan_pipeline_t        basic_notex_blend_pipeline[2];
	vulkan_pipeline_t        basic_poly_blend_pipeline;
	vulkan_pipeline_layout_t basic_pipeline_layout;
	vulkan_pipeline_t        world_pipelines[WORLD_PIPELINE_COUNT];
	vulkan_pipeline_layout_t world_pipeline_layout;
	vulkan_pipeline_t        raster_tex_warp_pipeline;
	vulkan_pipeline_t        particle_pipeline;
	vulkan_pipeline_t        sprite_pipeline;
	vulkan_pipeline_t        sky_stencil_pipeline;
	vulkan_pipeline_t        sky_color_pipeline;
	vulkan_pipeline_t        sky_box_pipeline;
	vulkan_pipeline_t        sky_layer_pipeline;
	vulkan_pipeline_t        alias_pipeline;
	vulkan_pipeline_t        alias_blend_pipeline;
	vulkan_pipeline_t        alias_alphatest_pipeline;
	vulkan_pipeline_t        alias_alphatest_blend_pipeline;
	vulkan_pipeline_t        postprocess_pipeline;
	vulkan_pipeline_t        screen_effects_pipeline;
	vulkan_pipeline_t        screen_effects_scale_pipeline;
	vulkan_pipeline_t        screen_effects_scale_sops_pipeline;
	vulkan_pipeline_t        cs_tex_warp_pipeline;
	vulkan_pipeline_t        showtris_pipeline;
	vulkan_pipeline_t        showtris_depth_test_pipeline;
	vulkan_pipeline_t        showbboxes_pipeline;
	vulkan_pipeline_t        alias_showtris_pipeline;
	vulkan_pipeline_t        alias_showtris_depth_test_pipeline;
	vulkan_pipeline_t        update_lightmap_pipeline;
#ifdef PSET_SCRIPT
	vulkan_pipeline_t fte_particle_pipelines[FTE_PARTICLE_PIPELINE_COUNT];
#endif

	// Descriptors
	VkDescriptorPool         descriptor_pool;
	vulkan_desc_set_layout_t ubo_set_layout;
	vulkan_desc_set_layout_t single_texture_set_layout;
	vulkan_desc_set_layout_t input_attachment_set_layout;
	VkDescriptorSet          screen_warp_desc_set;
	vulkan_desc_set_layout_t screen_warp_set_layout;
	vulkan_desc_set_layout_t single_texture_cs_write_set_layout;
	vulkan_desc_set_layout_t lightmap_compute_set_layout;

	// Samplers
	VkSampler point_sampler;
	VkSampler linear_sampler;
	VkSampler point_aniso_sampler;
	VkSampler linear_aniso_sampler;
	VkSampler point_sampler_lod_bias;
	VkSampler linear_sampler_lod_bias;
	VkSampler point_aniso_sampler_lod_bias;
	VkSampler linear_aniso_sampler_lod_bias;

	// Matrices
	float projection_matrix[16];
	float view_matrix[16];
	float view_projection_matrix[16];

	// Dispatch table
	PFN_vkCmdBindPipeline       vk_cmd_bind_pipeline;
	PFN_vkCmdPushConstants      vk_cmd_push_constants;
	PFN_vkCmdBindDescriptorSets vk_cmd_bind_descriptor_sets;
	PFN_vkCmdBindIndexBuffer    vk_cmd_bind_index_buffer;
	PFN_vkCmdBindVertexBuffers  vk_cmd_bind_vertex_buffers;
	PFN_vkCmdDraw               vk_cmd_draw;
	PFN_vkCmdDrawIndexed        vk_cmd_draw_indexed;
	PFN_vkCmdPipelineBarrier    vk_cmd_pipeline_barrier;
	PFN_vkCmdCopyBufferToImage  vk_cmd_copy_buffer_to_image;

#ifdef _DEBUG
	PFN_vkCmdBeginDebugUtilsLabelEXT vk_cmd_begin_debug_utils_label;
	PFN_vkCmdEndDebugUtilsLabelEXT   vk_cmd_end_debug_utils_label;
#endif
} vulkanglobals_t;

extern vulkanglobals_t vulkan_globals;

//====================================================

extern qboolean r_cache_thrash;  // compatability
extern int      r_visframecount; // ??? what difs?
extern int      r_framecount;
extern mplane_t frustum[4];
extern qboolean render_warp;
extern qboolean in_update_screen;
extern qboolean use_simd;
extern int      render_scale;

//
// view origin
//
extern vec3_t vup;
extern vec3_t vpn;
extern vec3_t vright;
extern vec3_t r_origin;

//
// screen size info
//
extern refdef_t r_refdef;
extern mleaf_t *r_viewleaf, *r_oldviewleaf;
extern int      d_lightstylevalue[256]; // 8.8 fraction of base light value

extern cvar_t r_drawentities;
extern cvar_t r_drawworld;
extern cvar_t r_drawviewmodel;
extern cvar_t r_speeds;
extern cvar_t r_pos;
extern cvar_t r_waterwarp;
extern cvar_t r_fullbright;
extern cvar_t r_lightmap;
extern cvar_t r_wateralpha;
extern cvar_t r_lavaalpha;
extern cvar_t r_telealpha;
extern cvar_t r_slimealpha;
extern cvar_t r_dynamic;
extern cvar_t r_novis;
extern cvar_t r_scale;

extern cvar_t gl_polyblend;
extern cvar_t gl_nocolors;

extern cvar_t gl_subdivide_size;

// johnfitz -- polygon offset
#define OFFSET_NONE  0
#define OFFSET_DECAL 1

// johnfitz -- rendering statistics
extern atomic_uint32_t rs_brushpolys, rs_aliaspolys, rs_skypolys, rs_particles, rs_fogpolys;
extern atomic_uint32_t rs_dynamiclightmaps, rs_brushpasses, rs_aliaspasses, rs_skypasses;

extern size_t total_device_vulkan_allocation_size;
extern size_t total_host_vulkan_allocation_size;

// johnfitz -- track developer statistics that vary every frame
extern cvar_t devstats;
typedef struct
{
	int packetsize;
	int edicts;
	int visedicts;
	int efrags;
	int tempents;
	int beams;
	int dlights;
} devstats_t;
extern devstats_t dev_stats, dev_peakstats;

// ohnfitz -- reduce overflow warning spam
typedef struct
{
	double packetsize;
	double efrags;
	double beams;
	double varstring;
} overflowtimes_t;
extern overflowtimes_t dev_overflows; // this stores the last time overflow messages were displayed, not the last time overflows occured
#define CONSOLE_RESPAM_TIME 3         // seconds between repeated warning messages

typedef struct
{
	float position[3];
	float texcoord[2];
	byte  color[4];
} basicvertex_t;

// johnfitz -- moved here from r_brush.c
extern int gl_lightmap_format;

#define LMBLOCK_WIDTH \
	1024 // FIXME: make dynamic. if we have a decent card there's no real reason not to use 4k or 16k (assuming there's no lightstyles/dynamics that need
	     // uploading...)
#define LMBLOCK_HEIGHT 1024 // Alternatively, use texture arrays, which would avoid the need to switch textures as often.

typedef struct lm_compute_workgroup_bounds_s
{
	float mins[3];
	float maxs[3];
} lm_compute_workgroup_bounds_t;
COMPILE_TIME_ASSERT (lm_compute_workgroup_bounds_t, sizeof (lm_compute_workgroup_bounds_t) == 24);

typedef struct glRect_s
{
	unsigned short l, t, w, h;
} glRect_t;
struct lightmap_s
{
	gltexture_t    *texture;
	gltexture_t    *surface_indices_texture;
	gltexture_t    *lightstyle_textures[MAXLIGHTMAPS];
	VkDescriptorSet descriptor_set;
	qboolean        modified;
	glRect_t        rectchange;
	VkBuffer        workgroup_bounds_buffer;

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	byte						  *data;               //[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
	byte						  *lightstyle_data[4]; //[4*LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
	uint32_t					  *surface_indices;    //[LMBLOCK_WIDTH*LMBLOCK_HEIGHT];
	lm_compute_workgroup_bounds_t *workgroup_bounds;   //[(LMBLOCK_WIDTH/8)*(LMBLOCK_HEIGHT/8)];
};
extern struct lightmap_s *lightmaps;
extern int                lightmap_count; // allocated lightmaps

extern qboolean r_fullbright_cheatsafe, r_lightmap_cheatsafe, r_drawworld_cheatsafe; // johnfitz

extern float map_wateralpha, map_lavaalpha, map_telealpha, map_slimealpha; // ericw
extern float map_fallbackalpha; // spike -- because we might want r_wateralpha to apply to teleporters while water itself wasn't watervised

// johnfitz -- fog functions called from outside gl_fog.c
void   Fog_ParseServerMessage (void);
float *Fog_GetColor (void);
float  Fog_GetDensity (void);
void   Fog_EnableGFog (cb_context_t *cbx);
void   Fog_DisableGFog (cb_context_t *cbx);
void   Fog_SetupFrame (cb_context_t *cbx);
void   Fog_NewMap (void);
void   Fog_Init (void);

void R_NewGame (void);

void R_AnimateLight (void);
void R_UpdateLightmaps (cb_context_t *cbx);
void R_MarkSurfaces (qboolean use_tasks, task_handle_t before_mark, task_handle_t *store_efrags, task_handle_t *cull_surfaces, task_handle_t *chain_surfaces);
qboolean R_CullBox (vec3_t emins, vec3_t emaxs);
void     R_StoreEfrags (efrag_t **ppefrag);
qboolean R_CullModelForEntity (entity_t *e);
void     R_RotateForEntity (float matrix[16], vec3_t origin, vec3_t angles);
void     R_MarkLights (dlight_t *light, int num, mnode_t *node);

void R_InitParticles (void);
void R_DrawParticles (cb_context_t *cbx);
void CL_RunParticles (void);
void R_ClearParticles (void);

void R_TranslatePlayerSkin (int playernum);
void R_TranslateNewPlayerSkin (int playernum); // johnfitz -- this handles cases when the actual texture changes
void R_UpdateWarpTextures (cb_context_t **cbx_ptr);

void R_DrawWorld (cb_context_t *cbx, int texstart, int texend);
void R_DrawAliasModel (cb_context_t *cbx, entity_t *e);
void R_DrawBrushModel (cb_context_t *cbx, entity_t *e);
void R_DrawSpriteModel (cb_context_t *cbx, entity_t *e);

void R_DrawTextureChains_Water (cb_context_t *cbx, qmodel_t *model, entity_t *ent, texchain_t chain);

void GL_BuildLightmaps (void);
void GL_DeleteBModelVertexBuffer (void);
void GL_BuildBModelVertexBuffer (void);
void GL_PrepareSIMDData (void);
void GLMesh_LoadVertexBuffers (void);
void GLMesh_DeleteVertexBuffers (void);

int R_LightPoint (vec3_t p, lightcache_t *cache);

void GL_SubdivideSurface (msurface_t *fa);
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride);
void R_RenderDynamicLightmaps (msurface_t *fa);
void R_UploadLightmaps (void);

void R_DrawWorld_ShowTris (cb_context_t *cbx);
void R_DrawBrushModel_ShowTris (cb_context_t *cbx, entity_t *e);
void R_DrawAliasModel_ShowTris (cb_context_t *cbx, entity_t *e);
void R_DrawParticles_ShowTris (cb_context_t *cbx);
void R_DrawSpriteModel_ShowTris (cb_context_t *cbx, entity_t *e);

void DrawGLPoly (cb_context_t *cbx, glpoly_t *p, float color[3], float alpha);
void GL_MakeAliasModelDisplayLists (qmodel_t *m, aliashdr_t *hdr);

void Sky_Init (void);
void Sky_ClearAll (void);
void Sky_DrawSky (cb_context_t *cbx);
void Sky_NewMap (void);
void Sky_LoadTexture (texture_t *mt);
void Sky_LoadTextureQ64 (texture_t *mt);
void Sky_LoadSkyBox (const char *name);

void R_ClearTextureChains (qmodel_t *mod, texchain_t chain);
void R_ChainSurface (msurface_t *surf, texchain_t chain);
void R_DrawTextureChains (cb_context_t *cbx, qmodel_t *model, entity_t *ent, texchain_t chain);
void R_DrawWorld_Water (cb_context_t *cbx);

float GL_WaterAlphaForSurface (msurface_t *fa);

int GL_MemoryTypeFromProperties (uint32_t type_bits, VkFlags requirements_mask, VkFlags preferred_mask);

void R_CreateDescriptorPool ();
void R_CreateDescriptorSetLayouts ();
void R_InitSamplers ();
void R_CreatePipelineLayouts ();
void R_CreatePipelines ();
void R_DestroyPipelines ();

#define MAX_PUSH_CONSTANT_SIZE 128 // Vulkan guaranteed minimum maxPushConstantsSize

static inline void R_BindPipeline (cb_context_t *cbx, VkPipelineBindPoint bind_point, vulkan_pipeline_t pipeline)
{
	static byte zeroes[MAX_PUSH_CONSTANT_SIZE];
	assert (pipeline.handle != VK_NULL_HANDLE);
	assert (pipeline.layout.handle != VK_NULL_HANDLE);
	assert (cbx->current_pipeline.layout.push_constant_range.size <= MAX_PUSH_CONSTANT_SIZE);
	if (cbx->current_pipeline.handle != pipeline.handle)
	{
		vulkan_globals.vk_cmd_bind_pipeline (cbx->cb, bind_point, pipeline.handle);
		if ((pipeline.layout.push_constant_range.size > 0) &&
		    ((cbx->current_pipeline.layout.push_constant_range.stageFlags != pipeline.layout.push_constant_range.stageFlags) ||
		     (cbx->current_pipeline.layout.push_constant_range.size != pipeline.layout.push_constant_range.size)))
			vulkan_globals.vk_cmd_push_constants (
				cbx->cb, pipeline.layout.handle, pipeline.layout.push_constant_range.stageFlags, 0, pipeline.layout.push_constant_range.size, zeroes);
		cbx->current_pipeline = pipeline;
	}
}

static inline void R_PushConstants (cb_context_t *cbx, VkShaderStageFlags stage_flags, int offset, int size, const void *data)
{
	vulkan_globals.vk_cmd_push_constants (cbx->cb, cbx->current_pipeline.layout.handle, stage_flags, offset, size, data);
}

static inline void R_BeginDebugUtilsLabel (cb_context_t *cbx, const char *name)
{
#ifdef _DEBUG
	VkDebugUtilsLabelEXT label;
	memset (&label, 0, sizeof (label));
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = name;
	if (vulkan_globals.vk_cmd_begin_debug_utils_label)
		vulkan_globals.vk_cmd_begin_debug_utils_label (cbx->cb, &label);
#endif
}

static inline void R_EndDebugUtilsLabel (cb_context_t *cbx)
{
#ifdef _DEBUG
	if (vulkan_globals.vk_cmd_end_debug_utils_label)
		vulkan_globals.vk_cmd_end_debug_utils_label (cbx->cb);
#endif
}

void R_AllocateVulkanMemory (vulkan_memory_t *, VkMemoryAllocateInfo *, vulkan_memory_type_t);
void R_FreeVulkanMemory (vulkan_memory_t *);

VkDescriptorSet R_AllocateDescriptorSet (vulkan_desc_set_layout_t *layout);
void            R_FreeDescriptorSet (VkDescriptorSet desc_set, vulkan_desc_set_layout_t *layout);

void  R_InitStagingBuffers ();
void  R_SubmitStagingBuffers ();
byte *R_StagingAllocate (int size, int alignment, VkCommandBuffer *cb_context, VkBuffer *buffer, int *buffer_offset);

void  R_InitGPUBuffers ();
void  R_SwapDynamicBuffers ();
void  R_FlushDynamicBuffers ();
void  R_CollectDynamicBufferGarbage ();
void  R_CollectMeshBufferGarbage ();
byte *R_VertexAllocate (int size, VkBuffer *buffer, VkDeviceSize *buffer_offset);
byte *R_IndexAllocate (int size, VkBuffer *buffer, VkDeviceSize *buffer_offset);
byte *R_UniformAllocate (int size, VkBuffer *buffer, uint32_t *buffer_offset, VkDescriptorSet *descriptor_set);

void R_AllocateLightmapComputeBuffers ();

void GL_SetObjectName (uint64_t object, VkObjectType object_type, const char *name);

#endif /* GLQUAKE_H */
