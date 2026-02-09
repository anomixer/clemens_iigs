// GLSL ES 1.00 shaders for WebGL 1/2 (Emscripten)

const char *VS_VERTEX_SOURCE = "precision mediump float;\n"
                               "uniform vec2 render_dims;\n"
                               "uniform vec2 display_ratio;\n"
                               "uniform vec2 virtual_dims;\n"
                               "uniform vec2 offsets;\n"
                               "\n"
                               "attribute vec2 pos;\n"
                               "attribute vec2 uv1;\n"
                               "attribute vec4 color1;\n"
                               "varying vec2 uv;\n"
                               "varying vec4 color;\n"
                               "void main() {\n"
                               "  vec2 t_pos = (pos * display_ratio + offsets) / render_dims;\n"
                               "  t_pos = (t_pos - 0.5) * vec2(2.0, -2.0);\n"
                               "  gl_Position = vec4(t_pos, 0.5, 1.0);\n"
                               "  uv = uv1;\n"
                               "  color = color1;\n"
                               "}\n";

const char *FS_TEXT_SOURCE = "precision mediump float;\n"
                             "uniform sampler2D tex;\n"
                             "varying vec4 color;\n"
                             "varying vec2 uv;\n"
                             "void main() {\n"
                             "  gl_FragColor = texture2D(tex, uv).xxxx * color;\n"
                             "}\n";

const char *FS_HIRES_SOURCE = "precision mediump float;\n"
                              "uniform sampler2D hgr_tex;\n"
                              "uniform sampler2D hcolor_tex;\n"
                              "varying vec4 color;\n"
                              "varying vec2 uv;\n"
                              "void main() {\n"
                              "  vec4 texl_hgr = texture2D(hgr_tex, uv);\n"
                              "  float cx = texl_hgr.x;\n"
                              "  gl_FragColor = texture2D(hcolor_tex, vec2(cx, 0.0));\n"
                              "}\n";

const char *FS_SUPER_SOURCE = R"glsl(
precision mediump float;

uniform sampler2D screen_tex;
uniform sampler2D color_tex;
uniform vec4 screen_params;
uniform vec4 color_params;

varying vec4 color;
varying vec2 uv;

void main() {
    float color_index = floor(texture2D(screen_tex, uv).x * 255.0 / 16.0 + 0.5);
    float scanline = floor((1.0 - uv.t) * screen_params.y / screen_params.w + 0.5);
    vec2 color_uv = vec2(
      (color_index + 0.5) * color_params.z / color_params.x,
      1.0 - (scanline + 0.5) * color_params.w / color_params.y);
    gl_FragColor = texture2D(color_tex, color_uv);
}
)glsl";
