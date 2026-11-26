/* avp360: o dispositivo Xenos por baixo do Direct3D 7 do d3d.ren (ver xdev.h). Mesmo esquema do xenos_gl.c do AvP1:
 * anel de vertices de 3 quadros na memoria da GPU, cor e profundidade na eDRAM em 1280x720, resolve para a tela no
 * fim do quadro, texturas liberadas so depois que a GPU passa da fence do ultimo uso. */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <xsdk/gpu.h>
#include <xsdk/log.h>
#include <xsdk/present.h>
#include <xsdk/shader.h>
#include <xsdk/texture.h>

#include "lcs_gpu_shaders.h"
#include "xdev.h"
uint32_t xsdk_tiled_2d(uint32_t, uint32_t, uint32_t, uint32_t);

enum {
	SCREEN_W = 1280,
	SCREEN_H = 720,
	MAX_TEXTURES = 8192,
	RING_SEGMENTS = 3,
	RING_SEGMENT_BYTES = 8 << 20,
	VERTEX_BYTES = sizeof(XDevVertex),
	PENDING_FREES = 4096,
};

typedef struct {
	XsdkTexture tex;
	bool used;
} XTex;

int8_t xdev_fxaa_enabled=0;
static float fxaa_constants[64][4] __attribute__((aligned(16)));
static uint32_t fxaa_constant_count;

static struct {
	bool open, offscreen, in_frame;
	uint32_t present_w, present_h, frame_address;
	XsdkTexture offscreen_tex;
	XsdkShader vs, ps, fxaa_ps, fxaa_vs;
 XsdkTexture fxaa_tex;
 bool fxaa_loaded;
	uint32_t depth_tile;

	XTex textures[MAX_TEXTURES];
	uint32_t next_tex;
	XsdkTexture white;
	XsdkSamplerState white_sampler;

	volatile uint32_t *ring;
	uint32_t ring_address;
	uint32_t segment, segment_used;
	uint32_t segment_fence[RING_SEGMENTS];

	struct { XsdkTexture tex; uint32_t fence; } pending[PENDING_FREES];
	uint32_t pending_count;

	uint32_t frames, draws;
 uint32_t texture_bytes,texture_peak,ring_wraps,peak_segment;
 uint32_t vertex_bytes,index_bytes,vb_wraps,ib_wraps;
 uint64_t total_draws,mesh_original,mesh_submitted;
 uint32_t last_vaddr,last_iaddr,last_count,last_nindices,last_prim;
 uint32_t report_frames,present_flips,present_vblanks,present_missed;
 double fence_wait_ms;

} g;

static double gpu_clock_ms(void) {
 struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
 return (double)t.tv_sec*1000.0+(double)t.tv_nsec/1000000.0;
}
static NTSTATUS measured_wait(uint32_t fence,uint32_t timeout) {
 double start=gpu_clock_ms(); NTSTATUS result=xsdk_gpu_wait(fence,timeout);
 g.fence_wait_ms+=gpu_clock_ms()-start;return result;
}
void xdev_log(const char *fmt, ...)
{
	char line[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof line, fmt, ap);
	va_end(ap);
	xsdk_log_write("LCS XSDK render: %s", line);
	fprintf(stderr, "LCS XSDK render: %s\n", line);
}

/* ---- texturas ---- */

static void free_pending(bool all)
{
	uint32_t keep = 0;
	for (uint32_t i = 0; i < g.pending_count; i++) {
		if (all || xsdk_gpu_fence_done(g.pending[i].fence))
			{ g.texture_bytes-=g.pending[i].tex.size_bytes;
    xsdk_texture_free(&g.pending[i].tex); }
		else
			g.pending[keep++] = g.pending[i];
	}
	g.pending_count = keep;
}

static void defer_free(XsdkTexture *tex)
{
	if (g.pending_count == PENDING_FREES) {
		measured_wait(xsdk_gpu_fence(), 1000);
		free_pending(true);
	}
	g.pending[g.pending_count].tex = *tex;
	g.pending[g.pending_count].fence = xsdk_gpu_fence();
	g.pending_count++;
	memset(tex, 0, sizeof *tex);
}

static uint32_t gpu_format(uint32_t f)
{
	switch (f) {
	case XDEV_FMT_RGBA8: return XSDK_FORMAT_R8G8B8A8;
	case XDEV_FMT_DXT1: return XSDK_FORMAT_DXT1;
	case XDEV_FMT_DXT3: return XSDK_FORMAT_DXT3;
	case XDEV_FMT_DXT5: return XSDK_FORMAT_DXT5;
	}
	return XSDK_FORMAT_A8R8G8B8;
}

uint32_t xdev_tex_create(uint32_t format, uint32_t width, uint32_t height, uint32_t levels)
{
	if (!g.open || width == 0 || height == 0)
		return 0;
	uint32_t i = g.next_tex;
	for (uint32_t n = 0; n < MAX_TEXTURES - 1; n++) {
		if (++i >= MAX_TEXTURES) i = 1;
		if (g.textures[i].used)
			continue;
		NTSTATUS st = xsdk_texture_create(&g.textures[i].tex, gpu_format(format), width, height, levels ? levels : 1, true);
		if (!NT_SUCCESS(st)) {
			xdev_log("textura %ux%u (%u niveis, formato %u) falhou: 0x%08X", width, height, levels, format, (unsigned)st);
			return 0;
		}
		g.texture_bytes+=g.textures[i].tex.size_bytes;
  if(g.texture_bytes>g.texture_peak)g.texture_peak=g.texture_bytes;
  g.textures[i].used = true;
		g.next_tex = i;
		return i;
	}
	xdev_log("acabaram as texturas (%u)", MAX_TEXTURES);
	return 0;
}

void xdev_tex_upload(uint32_t tex, uint32_t level, const void *data, uint32_t row_bytes)
{
	if (tex == 0 || tex >= MAX_TEXTURES || !g.textures[tex].used || !data)
		return;
	xsdk_texture_upload(&g.textures[tex].tex, level, data, row_bytes);
	__asm__ volatile("sync");
	xsdk_gpu_invalidate(g.textures[tex].tex.address, g.textures[tex].tex.size_bytes);
}

void xdev_tex_free(uint32_t tex)
{
	if (tex == 0 || tex >= MAX_TEXTURES || !g.textures[tex].used)
		return;
	defer_free(&g.textures[tex].tex);
	g.textures[tex].used = false;
}

/* ---- quadro ---- */

static void begin_frame(void)
{
	if (g.in_frame)
		return;
	g.frame_address = g.offscreen_tex.address;
	if (!g.offscreen && !NT_SUCCESS(xsdk_present_begin(&g.frame_address, 5000))) {
		xdev_log("xsdk_present_begin expirou");
		return;
	}
	const uint32_t next_segment = (g.segment + 1) % RING_SEGMENTS;
	if (g.segment_fence[next_segment] &&
	    !NT_SUCCESS(measured_wait(g.segment_fence[next_segment], 1000))) return;
	g.segment = next_segment;
	g.segment_used = 0;
	free_pending(false);

	xsdk_gpu_set_target_at(SCREEN_W, SCREEN_H, 0, g.depth_tile);
	xsdk_gpu_set_viewport(0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, 0.0f, 1.0f);
	g.in_frame = true;
}

static volatile uint32_t *ring_take(uint32_t bytes, uint32_t *address)
{
	bytes = (bytes + 63u) & ~63u;
	if (bytes > RING_SEGMENT_BYTES)
		return NULL;
	if (g.segment_used + bytes > RING_SEGMENT_BYTES) {
		/* Keep the previous allocation alive until all queued draws finish. */
		if (!NT_SUCCESS(measured_wait(xsdk_gpu_fence(), 1000))) return NULL;
		g.segment_used = 0;
  ++g.ring_wraps;
	}
	const uint32_t offset = g.segment * RING_SEGMENT_BYTES + g.segment_used;
	g.segment_used += bytes;
 if(g.segment_used>g.peak_segment)g.peak_segment=g.segment_used;
	*address = g.ring_address + offset;
	return g.ring + offset / 4;
}

void xdev_present(void)
{
	if (!g.open)
		return;
	begin_frame();
	if (!g.in_frame)
		return;
	xsdk_gpu_resolve(g.frame_address, g.present_w, g.present_h, 0xff000000);
	if (!g.offscreen)
		xsdk_present_end();
	g.segment_fence[g.segment] = xsdk_gpu_fence();
	if (g.offscreen)
		measured_wait(g.segment_fence[g.segment], 1000);
	g.in_frame = false;
	if (++g.frames % 600 == 1)
		xdev_log("quadro %u: %u desenhos", g.frames, g.draws);
	g.draws = 0;
}

void xdev_mesh_span(uint32_t original,uint32_t submitted) {
 g.mesh_original+=original;g.mesh_submitted+=submitted;
}
void xdev_perf_report(void) {
 if(!g.offscreen){XsdkPresentStats p;xsdk_present_stats(&p);
  xdev_log("PRESENT flips=%u vblanks=%u missed=%u longest_wait_us=%u fxaa=%d",p.flips-g.present_flips,p.vblanks-g.present_vblanks,p.missed-g.present_missed,p.longest_wait_us,xdev_fxaa_enabled);
  g.present_flips=p.flips;g.present_vblanks=p.vblanks;g.present_missed=p.missed;
 }
 xdev_log("POSTFX fxaa=%d target_kib=%u",xdev_fxaa_enabled,g.fxaa_tex.size_bytes/1024);
 xdev_log("PERF gpu frames=%u draws=%u vb_kib=%u ib_kib=%u mesh_original=%u mesh_submitted=%u ring_wraps=%u ring_peak_kib=%u texture_live_kib=%u texture_peak_kib=%u pending=%u ring_reserved_kib=%u offscreen=%u fence_wait_ms=%.3f vb_4g_wraps=%u ib_4g_wraps=%u",
  g.frames-g.report_frames,(unsigned)g.total_draws,(unsigned)(g.vertex_bytes/1024),(unsigned)(g.index_bytes/1024),
  (unsigned)g.mesh_original,(unsigned)g.mesh_submitted,g.ring_wraps,g.peak_segment/1024,
  g.texture_bytes/1024,g.texture_peak/1024,g.pending_count,RING_SEGMENTS*RING_SEGMENT_BYTES/1024,g.offscreen,g.fence_wait_ms,g.vb_wraps,g.ib_wraps);
 g.report_frames=g.frames;g.total_draws=g.vertex_bytes=g.index_bytes=g.mesh_original=g.mesh_submitted=0;g.ring_wraps=0;g.peak_segment=0;g.fence_wait_ms=0;g.vb_wraps=g.ib_wraps=0;
}

void xdev_clear(uint32_t argb, int color, int depth)
{
	if (!g.open || (!color && !depth)) return;
	begin_frame();
	if (!g.in_frame) return;
	/* Resolve first, then clear only the requested eDRAM attachments. */
	xsdk_gpu_set_resolve_clears(color != 0, depth != 0);
	xsdk_gpu_resolve(g.frame_address, g.present_w, g.present_h, argb);
	xsdk_gpu_set_resolve_clears(true, true);
	/* A resolve unbinds the eDRAM target in XSDK. Rebind before drawing. */
	xsdk_gpu_set_target_at(SCREEN_W, SCREEN_H, 0, g.depth_tile);
	xsdk_gpu_set_viewport(0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, 0.0f, 1.0f);
}

int xdev_capture_ppm(const char *path)
{
	/* Diagnostic readback only; presentation buffers are owned by present. */
	if (!g.open || !g.offscreen || g.in_frame) return 0;
	if (!NT_SUCCESS(measured_wait(xsdk_gpu_fence(), 5000))) return 0;
	FILE *f = fopen(path, "wb");
	if (!f) return 0;
	fprintf(f, "P6\n%u %u\n255\n", SCREEN_W, SCREEN_H);
	unsigned char row[SCREEN_W * 3];
	for (uint32_t y = 0; y < SCREEN_H; ++y) {
		for (uint32_t x = 0; x < SCREEN_W; ++x) {
			uint32_t v = g.offscreen_tex.texels[xsdk_tiled_2d(x, y, SCREEN_W, 2) / 4];
			row[x*3] = v >> 8; row[x*3+1] = v >> 16; row[x*3+2] = v >> 24;
		}
		if (fwrite(row, 1, sizeof row, f) != sizeof row) { fclose(f); return 0; }
	}
	return fclose(f) == 0;
}

void xdev_close(void)
{
	if (!g.open) return;
	measured_wait(xsdk_gpu_fence(), 5000);
	for (uint32_t i = 1; i < MAX_TEXTURES; ++i)
		if (g.textures[i].used) xsdk_texture_free(&g.textures[i].tex);
	free_pending(true);
	xsdk_texture_free(&g.white);
	if (g.offscreen) xsdk_texture_free(&g.offscreen_tex);
	else xsdk_present_close();
	xsdk_shader_free(&g.vs); xsdk_shader_free(&g.ps);
 if(g.fxaa_loaded){xsdk_shader_free(&g.fxaa_ps);xsdk_shader_free(&g.fxaa_vs);}
 if(g.fxaa_tex.address)xsdk_texture_free(&g.fxaa_tex);
	if (g.ring) xsdk_gpu_free(g.ring);
	xsdk_gpu_close();
	memset(&g, 0, sizeof g);
}

/* ---- estados ---- */

/* D3DBLEND_ZERO..D3DBLEND_SRCALPHASAT (1..11) */
static uint32_t blend_factor(uint32_t b)
{
	static const uint32_t map[12] = {
		XSDK_BLEND_ONE, XSDK_BLEND_ZERO, XSDK_BLEND_ONE, XSDK_BLEND_SRC_COLOR, XSDK_BLEND_ONE_MINUS_SRC_COLOR,
		XSDK_BLEND_SRC_ALPHA, XSDK_BLEND_ONE_MINUS_SRC_ALPHA, XSDK_BLEND_DST_ALPHA, XSDK_BLEND_ONE_MINUS_DST_ALPHA,
		XSDK_BLEND_DST_COLOR, XSDK_BLEND_ONE_MINUS_DST_COLOR, XSDK_BLEND_SRC_ALPHA_SATURATE,
	};
	return b < 12 ? map[b] : XSDK_BLEND_ONE;
}

/* D3DCMP_NEVER..D3DCMP_ALWAYS (1..8) e D3DSTENCILOP_KEEP..DECR (1..8) estao na ordem do xsdk, a partir de 1. */
static uint32_t from_one(uint32_t v, uint32_t fallback) { return v >= 1 && v <= 8 ? v - 1 : fallback; }

static void apply_states(const XDevState *s)
{
	XsdkBlend b = {
		.color_source = XSDK_BLEND_ONE, .color_dest = XSDK_BLEND_ZERO, .color_op = XSDK_BLEND_OP_ADD,
		.alpha_source = XSDK_BLEND_ONE, .alpha_dest = XSDK_BLEND_ZERO, .alpha_op = XSDK_BLEND_OP_ADD,
		.write_mask = s->color_write ? XSDK_WRITE_ALL : 0,
	};
	if (s->blend) {
		b.color_source = b.alpha_source = blend_factor(s->src_blend);
		b.color_dest = b.alpha_dest = blend_factor(s->dst_blend);
	}
	XsdkBlendState bs;
	xsdk_gpu_blend_state(&bs, &b);
	xsdk_gpu_set_blend(&bs);

	XsdkDepthStencil d = {
		.depth_test = s->depth_test || s->depth_write,
		.depth_write = s->depth_write,
		.depth_compare = s->depth_test ? from_one(s->depth_func, XSDK_COMPARE_LESS_EQUAL) : XSDK_COMPARE_ALWAYS,
		.stencil_test = s->stencil,
		.stencil_compare = from_one(s->stencil_func, XSDK_COMPARE_ALWAYS),
		.stencil_fail = from_one(s->stencil_fail, XSDK_STENCIL_KEEP),
		.stencil_depth_fail = from_one(s->stencil_zfail, XSDK_STENCIL_KEEP),
		.stencil_pass = from_one(s->stencil_pass, XSDK_STENCIL_KEEP),
		.stencil_reference = s->stencil_ref,
		.stencil_read_mask = s->stencil_mask,
		.stencil_write_mask = s->stencil_write_mask,
	};
	XsdkDepthStencilState ds;
	xsdk_gpu_depth_stencil_state(&ds, &d);
	xsdk_gpu_set_depth_stencil(&ds);

	/* D3DCULL_CCW (o padrao) descarta os triangulos anti-horarios na tela: a frente e a horaria. O vertice volta para o
	 * NDC com y invertido e o viewport do Xenos o desinverte, entao o sentido na tela e o mesmo. */
	XsdkRaster r = {
		.cull = s->cull == 2 || s->cull == 3 ? XSDK_CULL_BACK : XSDK_CULL_NONE,
		.front_clockwise = s->cull == 3,
		.depth_bias = s->depth_bias,
		.depth_bias_slope = 0.0f,
		.fill = s->wireframe ? XSDK_FILL_WIREFRAME : XSDK_FILL_SOLID,
	};
	XsdkRasterState rs;
	xsdk_gpu_raster_state(&rs, &r);
	xsdk_gpu_set_raster(&rs);

	int32_t x0 = s->scissor_x, y0 = s->scissor_y, x1 = s->scissor_x + s->scissor_w, y1 = s->scissor_y + s->scissor_h;
	if (s->scissor_w <= 0 || s->scissor_h <= 0) {
		x0 = y0 = 0;
		x1 = SCREEN_W;
		y1 = SCREEN_H;
	}
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > SCREEN_W) x1 = SCREEN_W;
	if (y1 > SCREEN_H) y1 = SCREEN_H;
	if (x1 < x0) x1 = x0;
	if (y1 < y0) y1 = y0;
	xsdk_gpu_set_scissor((uint32_t)x0, (uint32_t)y0, (uint32_t)(x1 - x0), (uint32_t)(y1 - y0));
}

static void bind_sampler(uint32_t slot, const XDevSampler *s)
{
	if (s->tex == 0 || s->tex >= MAX_TEXTURES || !g.textures[s->tex].used) {
		xsdk_gpu_set_texture(slot, &g.white, &g.white_sampler);
		return;
	}
	XsdkSampler sm = {
		.mag_filter = s->mag_linear ? XSDK_FILTER_LINEAR : XSDK_FILTER_POINT,
		.min_filter = s->min_linear ? XSDK_FILTER_LINEAR : XSDK_FILTER_POINT,
		.mip_filter = s->mip_linear ? XSDK_FILTER_LINEAR : XSDK_FILTER_POINT,
		.address_u = s->clamp_u ? XSDK_ADDRESS_CLAMP : XSDK_ADDRESS_WRAP,
		.address_v = s->clamp_v ? XSDK_ADDRESS_CLAMP : XSDK_ADDRESS_WRAP,
		.max_anisotropy = s->max_aniso > 1 ? s->max_aniso : 0,
		.lod_bias = s->lod_bias,
		.level_count = s->mip_on ? 0 : 1,
	};
	XsdkSamplerState ss;
	xsdk_texture_sampler_state(&ss, &sm);
	xsdk_gpu_set_texture(slot, &g.textures[s->tex].tex, &ss);
}

static void submit_bound(const XDevState *state,uint32_t prim,uint32_t vaddr,uint32_t count,uint32_t iaddr,uint32_t nindices)
{
	xsdk_gpu_set_shaders(&g.vs, &g.ps);
	xsdk_built_shader_set_literals(&tl_vert);
	xsdk_built_shader_set_literals(&tl_frag);
	xsdk_gpu_set_vertex_constants(TL_VERT_PUSH, &state->vs[0][0], TL_VERT_PUSH_REGISTERS);
	xsdk_gpu_set_pixel_constants(TL_FRAG_PUSH, &state->ps[0][0], TL_FRAG_PUSH_REGISTERS);
	apply_states(state);
	bind_sampler(TL_FRAG_SAMPLER_0_0, &state->samplers[0]);
	bind_sampler(TL_FRAG_SAMPLER_0_1, &state->samplers[1]);
	xsdk_gpu_set_vertices(0, vaddr, count * VERTEX_BYTES);
	if (nindices)
		xsdk_gpu_draw_indexed(prim, iaddr, nindices, false);
	else
		xsdk_gpu_draw(prim, count);
	g.draws++;
 ++g.total_draws;
}

void xdev_draw(const XDevState *state, uint32_t prim, const XDevVertex *verts, uint32_t count, const uint16_t *indices,
	uint32_t nindices)
{
	if (!g.open || !verts || count == 0 || (indices && nindices == 0))
		return;
	/* Xenos fan/strip numbers are reversed relative to D3DPT. */
	if (prim == XDEV_PRIM_TRIANGLE_STRIP) prim = XSDK_PRIMITIVE_TRIANGLE_STRIP;
	else if (prim == XDEV_PRIM_TRIANGLE_FAN) prim = XSDK_PRIMITIVE_TRIANGLE_FAN;
	begin_frame();
	if (!g.in_frame)
		return;

	uint32_t vaddr;
	/* Reserve vertices and indices together: a separate index reservation
	 * could wrap the ring and overwrite vertices before this draw is queued. */
	if (count > RING_SEGMENT_BYTES / VERTEX_BYTES ||
	    nindices > RING_SEGMENT_BYTES / 2) return;
	const uint32_t vertex_bytes = (count * VERTEX_BYTES + 63u) & ~63u;
	const uint32_t index_bytes = indices ? nindices * 2 + 4 : 0;
	volatile uint32_t *v = ring_take(vertex_bytes + index_bytes, &vaddr);
	if (!v) {
		xdev_log("desenho grande demais: %u vertices", count);
		return;
	}
	const uint32_t *src = (const uint32_t *)verts;
	for (uint32_t i = 0; i < count * (VERTEX_BYTES / 4); i++)
		v[i] = src[i];
	xsdk_gpu_invalidate(vaddr, count * VERTEX_BYTES);

	uint32_t iaddr = 0;
	if (indices) {
		volatile uint32_t *ix = v + vertex_bytes / 4;
		iaddr = vaddr + vertex_bytes;
		for (uint32_t i = 0; i + 1 < nindices; i += 2)
			ix[i / 2] = (uint32_t)indices[i] << 16 | indices[i + 1];
		if (nindices & 1)
			ix[nindices / 2] = (uint32_t)indices[nindices - 1] << 16;
		xsdk_gpu_invalidate(iaddr, nindices * 2 + 4);
	}

 g.last_vaddr=vaddr;g.last_iaddr=iaddr;g.last_count=count;g.last_nindices=indices?nindices:0;g.last_prim=prim;
 submit_bound(state,prim,vaddr,count,iaddr,indices?nindices:0);
 uint32_t vb=count*VERTEX_BYTES,ib=indices?nindices*2u:0;
 uint32_t prev=g.vertex_bytes;g.vertex_bytes+=vb;if(g.vertex_bytes<prev)++g.vb_wraps;
 prev=g.index_bytes;g.index_bytes+=ib;if(g.index_bytes<prev)++g.ib_wraps;
}

// Explicit paired alpha passes share immutable uploaded vertices/indices.
// No cross-draw cache: the second submission immediately follows the first.
void xdev_draw_dual(const XDevState *first,const XDevState *second,uint32_t prim,const XDevVertex *verts,uint32_t count,const uint16_t *indices,uint32_t nindices) {
 uint32_t before=g.draws;xdev_draw(first,prim,verts,count,indices,nindices);
 if(g.draws!=before)submit_bound(second,g.last_prim,g.last_vaddr,g.last_count,g.last_iaddr,g.last_nindices);
}

static const XsdkVertexElement tl_layout[] = {
	{ .usage = XSDK_USAGE_POSITION, .format = XSDK_VERTEX_FLOAT4, .offset_bytes = 0 },
	{ .usage = XSDK_USAGE_COLOR, .format = XSDK_VERTEX_UBYTE4N, .offset_bytes = 16 },
	{ .usage = XSDK_USAGE_TEXCOORD, .usage_index = 0, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 24 },
	{ .usage = XSDK_USAGE_TEXCOORD, .usage_index = 1, .format = XSDK_VERTEX_FLOAT2, .offset_bytes = 32 },
	{ .usage = XSDK_USAGE_TEXCOORD, .usage_index = 2, .format = XSDK_VERTEX_UBYTE4N, .offset_bytes = 20 },
};

void xdev_postprocess(void) {
 if(!g.open || !g.in_frame || !xdev_fxaa_enabled)return;
 if(!g.fxaa_loaded) {
  NTSTATUS st=xsdk_shader_load(&g.fxaa_ps,fxaa_frag.container,fxaa_frag.container_bytes);
  if(!NT_SUCCESS(st)){xdev_log("FXAA shader failed: %08X",(unsigned)st);xdev_fxaa_enabled=0;return;}
  st=xsdk_shader_load(&g.fxaa_vs,tl_vert.container,tl_vert.container_bytes);
  if(!NT_SUCCESS(st)){xsdk_shader_free(&g.fxaa_ps);xdev_fxaa_enabled=0;return;}
  st=xsdk_shader_set_vertex_layout(&g.fxaa_vs,tl_layout,5,(const uint32_t[]){VERTEX_BYTES});
  if(!NT_SUCCESS(st)){for(unsigned i=0;i<g.fxaa_vs.fetch_count;++i)xdev_log("FXAA fetch usage=%u index=%u",(g.fxaa_vs.fetches[i]>>12)&15,(g.fxaa_vs.fetches[i]>>16)&15);xdev_log("FXAA vertex layout failed: %08X",(unsigned)st);xsdk_shader_free(&g.fxaa_vs);xsdk_shader_free(&g.fxaa_ps);xdev_fxaa_enabled=0;return;}
  st=xsdk_shader_link(&g.fxaa_vs,&g.fxaa_ps);
  if(!NT_SUCCESS(st)){xdev_log("FXAA shader link failed: %08x",(unsigned)st);xsdk_shader_free(&g.fxaa_vs);xsdk_shader_free(&g.fxaa_ps);xdev_fxaa_enabled=0;return;}
  memset(fxaa_constants,0,sizeof fxaa_constants);fxaa_constant_count=32;
  for(unsigned i=0;i<fxaa_frag.literal_count;++i){const XsdkShaderLiteral *l=&fxaa_frag.literals[i];
   if(l->reg>=64){xdev_log("FXAA constants exceed budget");xsdk_shader_free(&g.fxaa_vs);xsdk_shader_free(&g.fxaa_ps);xdev_fxaa_enabled=0;return;}
   memcpy(fxaa_constants[l->reg],l->values,sizeof l->values);if(l->reg>=fxaa_constant_count)fxaa_constant_count=l->reg+1;
  }
  fxaa_constant_count=(fxaa_constant_count+3)&~3u;
  fxaa_constants[0][0]=1.0f/SCREEN_W;fxaa_constants[0][1]=1.0f/SCREEN_H;
  g.fxaa_loaded=true;
 }
 if(!g.fxaa_tex.address) {
  NTSTATUS st=xsdk_texture_create(&g.fxaa_tex,XSDK_FORMAT_A8R8G8B8,SCREEN_W,SCREEN_H,1,true);
  if(!NT_SUCCESS(st)){xdev_log("FXAA target failed: %08X",(unsigned)st);xdev_fxaa_enabled=0;return;}
  xsdk_texture_base_at_corner(&g.fxaa_tex);
 }
 static const XDevVertex quad[4]={
  {.x=.5f,.y=.5f,.rhw=1,.diffuse=0xffffffff,.u0=0,.v0=0},
  {.x=SCREEN_W+.5f,.y=.5f,.rhw=1,.diffuse=0xffffffff,.u0=1,.v0=0},
  {.x=.5f,.y=SCREEN_H+.5f,.rhw=1,.diffuse=0xffffffff,.u0=0,.v0=1},
  {.x=SCREEN_W+.5f,.y=SCREEN_H+.5f,.rhw=1,.diffuse=0xffffffff,.u0=1,.v0=1}};
 uint32_t address;volatile uint32_t *dst=ring_take(sizeof quad,&address);if(!dst)return;
 const uint32_t *src=(const uint32_t*)quad;for(unsigned i=0;i<sizeof quad/4;++i)dst[i]=src[i];
 xsdk_gpu_invalidate(address,sizeof quad);
 xsdk_gpu_resolve_texture(&g.fxaa_tex,0xff000000);
 // Offscreen Xenia validation needs resolved guest memory visible before
 // texture fetch. The native resolve already queues its GPU idle/coherency barrier.
 if(g.offscreen && !NT_SUCCESS(measured_wait(xsdk_gpu_fence(),1000)))return;
 xsdk_gpu_invalidate(g.fxaa_tex.address,g.fxaa_tex.size_bytes);
 xsdk_gpu_set_target_at(SCREEN_W,SCREEN_H,0,g.depth_tile);
 xsdk_gpu_set_viewport(0,0,SCREEN_W,SCREEN_H,0,1);
 XDevState state={0};state.color_write=1;state.depth_func=4;
 state.vs[0][0]=2.0f/SCREEN_W;state.vs[0][1]=-2.0f/SCREEN_H;
 state.vs[0][2]=-1.0f-1.0f/SCREEN_W;state.vs[0][3]=1.0f+1.0f/SCREEN_H;
 apply_states(&state);
 xsdk_gpu_set_shaders(&g.fxaa_vs,&g.fxaa_ps);xsdk_built_shader_set_literals(&tl_vert);
 xsdk_gpu_set_vertex_constants(TL_VERT_PUSH,&state.vs[0][0],TL_VERT_PUSH_REGISTERS);
 xsdk_gpu_set_pixel_constants(0,&fxaa_constants[0][0],fxaa_constant_count);
 static const XsdkSampler settings={.mag_filter=XSDK_FILTER_LINEAR,.min_filter=XSDK_FILTER_LINEAR,.mip_filter=XSDK_FILTER_POINT,.address_u=XSDK_ADDRESS_CLAMP,.address_v=XSDK_ADDRESS_CLAMP};
 XsdkSamplerState sampler;xsdk_texture_sampler_state(&sampler,&settings);
 xsdk_gpu_set_texture(0,&g.fxaa_tex,&sampler);
 xsdk_gpu_set_vertices(0,address,sizeof quad);xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_STRIP,4);
 ++g.draws;++g.total_draws;
}

/* ---- abertura ---- */


int xdev_open(uint32_t *width, uint32_t *height)
{
	if (g.open) {
		*width = SCREEN_W;
		*height = SCREEN_H;
		return 1;
	}
	NTSTATUS st = xsdk_gpu_open();
	if (!NT_SUCCESS(st)) {
		xdev_log("xsdk_gpu_open falhou: 0x%08X", (unsigned)st);
		return 0;
	}
	st = xsdk_present_open(XSDK_DISPLAY_720P, 3, 1, &g.present_w, &g.present_h);
	if (!NT_SUCCESS(st)) {
		xdev_log("sem display (0x%08X): desenhando fora da tela", (unsigned)st);
		g.present_w = SCREEN_W;
		g.present_h = SCREEN_H;
		st = xsdk_texture_create(&g.offscreen_tex, XSDK_FORMAT_A8R8G8B8, SCREEN_W, SCREEN_H, 1, true);
		if (!NT_SUCCESS(st))
			return 0;
		xsdk_texture_base_at_corner(&g.offscreen_tex);
		g.offscreen = true;
	}
	st = xsdk_shader_load(&g.vs, tl_vert.container, tl_vert.container_bytes);
	if (NT_SUCCESS(st))
		st = xsdk_shader_load(&g.ps, tl_frag.container, tl_frag.container_bytes);
	if (NT_SUCCESS(st))
		st = xsdk_shader_set_vertex_layout(&g.vs, tl_layout, sizeof tl_layout / sizeof tl_layout[0],
			(const uint32_t[]){ VERTEX_BYTES });
	if (!NT_SUCCESS(st)) {
		xdev_log("shaders falharam: 0x%08X", (unsigned)st);
		return 0;
	}

	g.ring = xsdk_gpu_alloc(RING_SEGMENTS * RING_SEGMENT_BYTES);
	if (!g.ring) {
		xdev_log("sem memoria para o anel de vertices");
		return 0;
	}
	g.ring_address = xsdk_gpu_address(g.ring);

	st = xsdk_texture_create(&g.white, XSDK_FORMAT_A8R8G8B8, 1, 1, 1, false);
	if (!NT_SUCCESS(st))
		return 0;
	g.white.texels[0] = 0xffffffffu;
	xsdk_gpu_invalidate(g.white.address, g.white.size_bytes);
	XsdkSampler s = { .mag_filter = XSDK_FILTER_POINT, .min_filter = XSDK_FILTER_POINT, .mip_filter = XSDK_FILTER_POINT,
		.address_u = XSDK_ADDRESS_CLAMP, .address_v = XSDK_ADDRESS_CLAMP };
	xsdk_texture_sampler_state(&g.white_sampler, &s);

	g.depth_tile = xsdk_gpu_target_tiles(SCREEN_W, SCREEN_H);
	xsdk_gpu_set_depth_clear(1.0f, 0);
	g.open = true;
	*width = SCREEN_W;
	*height = SCREEN_H;
	xdev_log("Xenos pronto: %ux%u%s", g.present_w, g.present_h, g.offscreen ? " (fora da tela)" : "");
	return 1;
}
