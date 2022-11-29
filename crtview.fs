#version 120

varying vec2 v_texCoord;

uniform vec3 modulate = vec3(1.0);
uniform vec2 resolution = vec2(1280, 960);
uniform float iTime;
uniform sampler2D tex0;
//uniform sampler2D tex1;
//uniform sampler2D frametexture;
//uniform float use_frame;
uniform vec2 blur = vec2(32, 32);

uniform float cfg_curvature;
uniform float cfg_scanlines;
uniform float cfg_shadow_mask;
uniform float cfg_separation;
uniform float cfg_ghosting;
uniform float cfg_noise;
uniform float cfg_flicker;
uniform float cfg_vignette;
uniform float cfg_distortion;
uniform float cfg_aspect_lock;
uniform float cfg_hpos;
uniform float cfg_vpos;
uniform float cfg_hsize;
uniform float cfg_vsize;
uniform float cfg_contrast;
uniform float cfg_brightness;
uniform float cfg_saturation;
uniform float cfg_degauss;

vec4 blursample(sampler2D samp, vec2 tc)
{
	vec4 c1 = texture2D(samp, tc);
	vec4 c2 = c1 * 0.2270270270;
	c2 += texture2D(samp, vec2(tc.x - 4.0 * resolution.x, tc.y - 4.0 * resolution.y)) * 0.0162162162;
	c2 += texture2D(samp, vec2(tc.x - 3.0 * resolution.x, tc.y - 3.0 * resolution.y)) * 0.0540540541;
	c2 += texture2D(samp, vec2(tc.x - 2.0 * resolution.x, tc.y - 2.0 * resolution.y)) * 0.1216216216;
	c2 += texture2D(samp, vec2(tc.x - 1.0 * resolution.x, tc.y - 1.0 * resolution.y)) * 0.1945945946;
	c2 += texture2D(samp, vec2(tc.x + 1.0 * resolution.x, tc.y + 1.0 * resolution.y)) * 0.1945945946;
	c2 += texture2D(samp, vec2(tc.x + 2.0 * resolution.x, tc.y + 2.0 * resolution.y)) * 0.1216216216;
	c2 += texture2D(samp, vec2(tc.x + 3.0 * resolution.x, tc.y + 3.0 * resolution.y)) * 0.0540540541;
	c2 += texture2D(samp, vec2(tc.x - 4.0 * resolution.x, tc.y + 4.0 * resolution.y)) * 0.0162162162;
	return mix(c1, c2, 0.25);
}

vec3 tsample(sampler2D samp, vec2 tc, float offs, vec2 resolution)
{
	vec3 s = pow(abs(texture2D(samp, tc).rgb), vec3(2.2));
	return s * vec3(1.25);
}

vec3 filmic(vec3 LinearColor)
{
	vec3 x = max(vec3(0.0), LinearColor - vec3(0.004));
	return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
}

vec2 curve(vec2 uv)
{
	uv = (uv - 0.5) * 2.0;
	uv *= vec2(1.049, 1.042);
	uv -= vec2(-0.008, 0.008);
	uv.x *= 1.0 + pow((abs(uv.y) / 5.0), 2.0);
	uv.y *= 1.0 + pow((abs(uv.x) / 4.0), 2.0);
	uv  = (uv / 2.0) + 0.5;
	return uv;
}

float rand(vec2 co)
{
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

void main(void)
{
	/* Curve */
	vec2 curved_uv = mix(curve(v_texCoord), v_texCoord, 0.4);
	float scale = 0.04;
	vec2 scuv = curved_uv * (1.0 - scale ) + scale / 2.0 + vec2(0.003, -0.001);
	scuv = curved_uv;

	/* Main color, Bleed */
	vec3 col;
	float x = sin(0.1 * iTime + curved_uv.y * 13.0) * sin(0.23 * iTime + curved_uv.y * 19.0) * sin(0.3 + 0.11 * iTime + curved_uv.y * 23.0) * 0.0012;
	float o = sin(gl_FragCoord.y / 1.5) / resolution.x;
	x += o * 0.25;
	x *= 0.2f;
	col.r = tsample(tex0,vec2(x + scuv.x + 0.0009, scuv.y + 0.0009), resolution.y / 800.0, resolution).x + 0.02;
	col.g = tsample(tex0,vec2(x + scuv.x + 0.0000, scuv.y - 0.0011), resolution.y / 800.0, resolution).y + 0.02;
	col.b = tsample(tex0,vec2(x + scuv.x - 0.0015, scuv.y + 0.0000), resolution.y / 800.0, resolution).z + 0.02;

	float i = clamp(col.r * 0.299 + col.g * 0.587 + col.b * 0.114, 0.0, 1.0);
	i = pow(1.0 - pow(i, 2.0), 1.0);
	i = (1.0 - i) * 0.85 + 0.15;

	/* Ghosting */
/*
	float ghs = 0.15;
	vec3 r = tsample(tex1, vec2(x - 0.014 * 1.0, -0.027) * 0.85 + 0.007 * vec2(0.35 * sin(1.0 / 7.0 + 15.0 * curved_uv.y + 0.9 * iTime), 0.35 * sin(2.0 / 7.0 + 10.0 * curved_uv.y + 1.37 * iTime)) + vec2(scuv.x + 0.001, scuv.y + 0.001), 5.5 + 1.3 * sin(3.0 / 9.0 + 31.0 * curved_uv.x + 1.70 * iTime), resolution).xyz * vec3(0.5, 0.25, 0.25);
	vec3 g = tsample(tex1, vec2(x - 0.019 * 1.0, -0.020) * 0.85 + 0.007 * vec2(0.35 * cos(1.0 / 9.0 + 15.0 * curved_uv.y + 0.5 * iTime), 0.35 * sin(2.0 / 9.0 + 10.0 * curved_uv.y + 1.50 * iTime)) + vec2(scuv.x + 0.000, scuv.y - 0.002), 5.4 + 1.3 * sin(3.0 / 3.0 + 71.0 * curved_uv.x + 1.90 * iTime), resolution).xyz * vec3(0.25, 0.5, 0.25);
	vec3 b = tsample(tex1, vec2(x - 0.017 * 1.0, -0.003) * 0.85 + 0.007 * vec2(0.35 * sin(2.0 / 3.0 + 15.0 * curved_uv.y + 0.7 * iTime), 0.35 * cos(2.0 / 3.0 + 10.0 * curved_uv.y + 1.63 * iTime)) + vec2(scuv.x - 0.002, scuv.y + 0.000), 5.3 + 1.3 * sin(3.0 / 7.0 + 91.0 * curved_uv.x + 1.65 * iTime), resolution).xyz * vec3(0.25, 0.25, 0.5);

	col += vec3(ghs * (1.0 - 0.299)) * pow(clamp(vec3(3.0) * r, vec3(0.0), vec3(1.0)), vec3(2.0)) * vec3(i);
	col += vec3(ghs * (1.0 - 0.587)) * pow(clamp(vec3(3.0) * g, vec3(0.0), vec3(1.0)), vec3(2.0)) * vec3(i);
	col += vec3(ghs * (1.0 - 0.114)) * pow(clamp(vec3(3.0) * b, vec3(0.0), vec3(1.0)), vec3(2.0)) * vec3(i);
*/

	/* Level adjustment (curves) */
	col *= vec3(0.95, 1.05, 0.95);
	col = clamp(col * 1.3 + 0.75 * col * col + 1.25 * col * col * col * col * col, vec3(0.0), vec3(10.0));

	/* Vignette */
//	float vig = (0.1 + 1.0 * 16.0 * curved_uv.x * curved_uv.y * (1.0 - curved_uv.x) * (1.0 - curved_uv.y));
//	vig = 1.3 * pow(vig, 0.5);
//	col *= vig;

	/* Scanlines */
	float scans = clamp(0.35 + 0.18 * sin(6.0 + curved_uv.y * resolution.y * 1.5), 0.0, 1.0);
	float s = pow(scans, 0.9);
	col = col * vec3(s);

	/* Vertical lines (shadow mask) */
	col *= 1.0 - 0.23 * (clamp((mod(gl_FragCoord.xy.x, 3.0)) / 2.0, 0.0, 1.0));

	/* Tone map */
	col = filmic(col);

	/* Noise */
	/* vec2 seed = floor(curved_uv * resolution.xy * vec2(0.5)) / resolution.xy; */
	vec2 seed = curved_uv * resolution.xy;;
	/* seed = curved_uv; */
	col -= 0.015 * pow(vec3(rand(seed + iTime), rand(seed + iTime * 2.0), rand(seed + iTime * 3.0)), vec3(1.5));

	/* Flicker */
	col *= (1.0 - 0.004 * (sin(50.0 * iTime + curved_uv.y * 2.0) * 0.5 + 0.5));

	/* Clamp */
	if (curved_uv.x < 0.0 || curved_uv.x > 1.0)
		col *= 0.0;
	if (curved_uv.y < 0.0 || curved_uv.y > 1.0)
		col *= 0.0;

	/* Frame */
/*
	vec2 fscale = vec2(-0.019, -0.018);
	vec4 f = texture2D(frametexture, v_texCoord * ((1.0) + 2.0 * fscale) - fscale - vec2(-0.0, 0.005));
	f.xyz = mix(f.xyz, vec3(0.5), 0.5);
	float fvig = clamp(-0.00 + 512.0 * v_texCoord.x * v_texCoord.y * (1.0 - v_texCoord.x) * (1.0 - v_texCoord.y), 0.2, 0.8);
	col = mix(col, mix(max(col, 0.0), pow(abs(f.xyz), vec3(1.4)) * fvig, f.w * f.w), vec3(use_frame));
*/

	gl_FragColor = vec4(col, 1.0) * vec4(modulate, 1.0);
}
