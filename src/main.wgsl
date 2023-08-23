struct uniforms {
    draw_start_timestamp: vec2u,
    draw_end_timestamp  : vec2u,

     view_size     : vec2u,
     cell_size     : vec2u,
    glyph_size     : vec2u,
    margin         : u32,
    line_view_width: u32,

    _pad0      : vec3u,
    inlay_color: u32,

    palette    : array<vec4u, 64>
}

@group(0) @binding(0) var<uniform> uni: uniforms;
@group(0) @binding(1) var wrap_sampler: sampler;
@group(0) @binding(2) var glyph_tex     : texture_2d<f32>;
@group(0) @binding(3) var line_glyph_tex: texture_2d<u32>;
@group(0) @binding(4) var code_glyph_tex: texture_2d<u32>;
@group(0) @binding(5) var code_color_tex: texture_2d<u32>;
@group(0) @binding(6) var code_inlay_tex: texture_2d<u32>;
@group(0) @binding(7) var<storage, read> diag_glyph_buf: array<u32, 64>;

@vertex   fn vs_main(@builtin(vertex_index) index : u32      ) -> @builtin (position) vec4f { return fullscreen_triangle_vertex_at(index); }
@fragment fn fs_main(@builtin(position) screen_pos: vec4<f32>) -> @location(0)        vec4f { return view_pixel_color_at(vec2u(screen_pos.xy)); }

fn view_pixel_color_at(pos: vec2u) -> vec4f {
    let lines_rect = rect(0, 0, uni.line_view_width * uni.cell_size.x + 2, uni.view_size.y - uni.cell_size.y);
    if (is_within(pos, lines_rect)) { return lines_pixel_color_at(within(pos, lines_rect), lines_rect); }

    let code_rect = rect(lines_rect.right + uni.margin, 0, uni.view_size.x, uni.view_size.y - uni.cell_size.y);
    if (is_within(pos, code_rect)) { return code_view_pixel_color_at(within(pos, code_rect)); }

    let diag_rect = rect(0, uni.view_size.y - uni.cell_size.y, uni.view_size.x, uni.view_size.y);
    if (is_within(pos, diag_rect)) { return diag_view_pixel_color_at(within(pos, diag_rect)); }

    return vec4f(0,0,0,0);
}

fn diag_view_pixel_color_at(pos: vec2u) -> vec4f {
    let cell_index = pos / uni.cell_size;
    let cell_px    = pos % uni.cell_size;

    let glyph_coverage = diag_glyph_coverage_at(cell_index.x, cell_px);
    return mix(unpack_color(0x111122ff), vec4f(1,1,1,1), glyph_coverage);
}

fn lines_pixel_color_at(pos: vec2u, rect: rect) -> vec4f {
    if (pos.x >= rect.right - 2) {
        return to_premul_alpha(palette_color(8)); // inlay color
    }

    return mix(vec4f(0,0,0,0), to_premul_alpha(palette_color(8)), glyph_coverage_at(line_glyph_tex, pos));
}

fn code_view_pixel_color_at(pos: vec2u) -> vec4f {
    let cell_index = pos / uni.cell_size;
    let cell_px    = pos % uni.cell_size;

    let text_size  = textureDimensions(code_glyph_tex);
    //TODO: ensure texture always covers the view
    if (true
    && cell_index.x < text_size.x
    && cell_index.y < text_size.y
    ) {} else {
        discard;
    }

    //NOTE: convert to premul alpha, so that we can iteratively mix the colors
    let inlay_color = to_premul_alpha(palette_color(uni.inlay_color)) * inlay_coverage_at(cell_index, cell_px);
    let glyph_color = to_premul_alpha(text_color_at(cell_index     )) *  text_coverage_at(cell_index, cell_px);

    return premul_alpha_blend(inlay_color, glyph_color);
}

fn text_glyph_at   (cell_index: vec2u) -> u32   { return load(code_glyph_tex, cell_index); }
fn text_color_at   (cell_index: vec2u) -> vec4f { return palette_color(load(code_color_tex, cell_index)); }
fn text_coverage_at(cell_index: vec2u, cell_px: vec2u) -> f32 { return glyph_coverage_of(text_glyph_at(cell_index), cell_px); }

fn glyph_coverage_at(glyphs_tex: texture_2d<u32>, pos: vec2u) -> f32 {
    let cell_index = pos / uni.cell_size;
    let cell_px    = pos % uni.cell_size;

    return glyph_coverage_at_cell(glyphs_tex, cell_index, cell_px);
}

fn glyph_coverage_at_cell(glyphs_tex: texture_2d<u32>, cell_index: vec2u, cell_px: vec2u) -> f32 {
    return glyph_coverage_of(load(glyphs_tex, cell_index), cell_px);
}

fn glyph_coverage_of(glyph: u32, cell_px: vec2u) -> f32 {
    if (glyph != 0) {} else { return 0; }
    let glyph_px     = clamp(cell_px - (uni.cell_size - uni.glyph_size) / 2, vec2u(0, 0), uni.glyph_size);
    let glyph_tex_px = vec2u(glyph * uni.glyph_size.x, 0) + glyph_px;
    let glyph_tex_uv = vec2f(glyph_tex_px) / vec2f(textureDimensions(glyph_tex));
    return textureSampleLevel(glyph_tex, wrap_sampler, glyph_tex_uv, 0).r;
}

fn inlay_coverage_at(cell_index: vec2u, cell_px: vec2u) -> f32 {
    const bracket_size = 0.5;
    let inlay_id = load(code_inlay_tex, cell_index);
    if ((inlay_id & 0xff) != 0) {} else { return 0; }
    let l_px = vec2u(cell_px.x, uni.cell_size.y - cell_px.y);
    let r_px = uni.cell_size - cell_px;
    return f32((inlay_id & 0x01)     ) * saturate(sdf_corner(vec2f(l_px), bracket_size))
         + f32((inlay_id & 0x02) >> 1) * saturate(sdf_corner(vec2f(r_px), bracket_size));
}

fn diag_glyph_coverage_at(index: u32, cell_px: vec2u) -> f32 { return glyph_coverage_of(diag_glyph_at(index), cell_px); }

fn diag_glyph_at(index: u32) -> u32 { return (diag_glyph_buf[index / 4] >> (8 * (index % 4))) & 0x000000ffu; }

fn set_diag_glyph_at(index: u32, value: u32) {
    let shift = (index % 4) * 8;

    var stored = cs_diag_glyph_buf[index / 4];

    stored &= ~(0x000000ffu << shift);
    stored |= (value & 0x000000ffu) << shift;

    cs_diag_glyph_buf[index / 4] = stored;
}

fn palette_color(index: u32) -> vec4f { return unpack_color(uni.palette[index / 4][index % 4]); }

//region compute
@group(0) @binding(1) var<storage, read_write> cs_diag_glyph_buf: array<u32, 64>;
@compute
@workgroup_size(1)
fn cs_update_timing_text() {
    let elapsed_mc = (uni.draw_end_timestamp.x - uni.draw_start_timestamp.x) / 1000;
    let elapsed_ms = elapsed_mc / 1000;

    var cell_i = 1u;
    var trailing = true;

    // max 99 seconds
    // ms
    for (var power = 100000u; power >= 10; power /= 10) {
        let digit = (elapsed_ms % power) / (power / 10);
        if (!trailing || digit != 0 || power == 10) {
            trailing = false;
            set_diag_glyph_at(cell_i, 16 + digit); cell_i++;
        }
    }

    set_diag_glyph_at(cell_i, 14); cell_i++; // '.'

    // mc
    for (var power = 1000u; power >= 10; power /= 10) {
        let digit = (elapsed_mc % power) / (power / 10);
        set_diag_glyph_at(cell_i, 16 + digit); cell_i++;
    }

    set_diag_glyph_at(cell_i, 77); cell_i++; // 'm'
    set_diag_glyph_at(cell_i, 83); cell_i++; // 's'
}
//endregion

//region panels
struct rect {
      left: u32,
      top : u32,
    right : u32,
    bottom: u32,
}

fn top_left_of(r: rect) -> vec2u { return vec2u(r.left, r.top); }

fn within(v: vec2u, r: rect) -> vec2u {
    return v - top_left_of(r);
}

fn is_within(v: vec2u, r: rect) -> bool {
    return v.x >= r.left && v.x < r.right && v.y >= r.top && v.y < r.bottom;
}
//endregion

//region textures
fn load(tex: texture_2d<u32>, pos: vec2u) -> u32 {
    return textureLoad(tex, pos, 0).r;
}
//endregion

//region packing
fn unpack_color(c: u32) -> vec4f {
    let r = (c >> 24)       ;
    let g = (c >> 16) & 0xff;
    let b = (c >>  8) & 0xff;
    let a =  c        & 0xff;

    return vec4f(vec4u(r, g, b, a)) / 255.0;
}
//endregion

//region blending
fn to_premul_alpha(c: vec4f) -> vec4f {
    return vec4f(c.rgb * c.a, c.a);
}

fn to_normal_alpha(c: vec4f) -> vec4f {
    return vec4f(c.rgb / c.a, c.a);
}

fn premul_alpha_blend(c0: vec4f, c1: vec4f) -> vec4f {
    return c0 * (1.0 - c1.a) + c1;
}
//endregion

//region SDF
fn sdf_thick_segment(p: vec2f, a: vec2f, b: vec2f, r: f32) -> f32 {
    return sdf_segment(p, a, b) - r;
}

fn sdf_segment(p: vec2f, a: vec2f, b: vec2f) -> f32
{
    let pa = p-a;
    let ba = b-a;
    let h = saturate(dot(pa,ba)/dot(ba,ba));
    return length(pa - ba*h);
}

fn sdf_corner(p: vec2f, r: f32) -> f32 {
    let a0 = (1.0 - sdf_thick_segment(p, vec2f(2.0, 5.0), vec2f(6.0, 5.0), r));
    let a1 = (1.0 - sdf_thick_segment(p, vec2f(2.0, 5.0), vec2f(2.0, 8.0), r));

    return max(a0, a1);
}
//endregion

//region transforms
struct gpu_transform2 {
    m: mat2x4f
};

fn apply_to_point(t: gpu_transform2, p: vec2f) -> vec2f { return vec2f(
    t.m[0].x * p.x + t.m[0].y * p.y + t.m[0].z,
    t.m[1].x * p.x + t.m[1].y * p.y + t.m[1].z
);}
//endregion

//region utility
fn step(a: u32, b: u32) -> u32 { return u32(a < b); }

fn fullscreen_triangle_vertex_at(index: u32) -> vec4f { return vec4f(4 * f32(index & 1) - 1, -4 * f32(index>>1) + 1, 0, 1); }
//endregion