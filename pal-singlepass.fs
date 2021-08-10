//svo's PAL single pass shader
//----------------------------
//Ported from SDLMESS to libretro
//then to Clunibus

/*
    The idea is to reproduce in GLSL shaders realistic composite-like
    artifacting by applying PAL modulation and demodulation.
    
    Digital texture, passed through the model of an analog channel,
    should suffer same effects as its analog counterpart and exhibit properties,
    such as dot crawl and colour bleeding, that may be desirable for faithful
    reproduction of look and feel of old computer games."
    
    https://github.com/svofski/CRT
    Copyright (c) 2016, Viacheslav Slavinsky
    All rights reserved.
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright notice, 
    this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice, 
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#version 150

uniform sampler2D tex0;
varying vec2 v_texCoord;
uniform float iTime;

#define TextureSize vec2(640, 480)
#define SourceSize vec4(TextureSize, 1.0 / TextureSize) //either TextureSize or InputSize
#define outsize vec4(OutputSize, 1.0 / OutputSize)

#define FIR_GAIN 1.5
#define FIR_INVGAIN 1.1
#define PHASE_NOISE 1.0

/* Subcarrier frequency */
#define FSC          4433618.75

/* Line frequency */
#define FLINE        15625.

#define VISIBLELINES 312.

#define PI           3.14159265358

#define RGB_to_YIQ  mat3( 0.299,    0.595716,  0.211456,\
                            0.587,   -0.274453, -0.522591,\
                            0.114,   -0.321263,  0.311135)

#define YIQ_to_RGB  mat3( 1.0   ,   1.0,       1.0,\
                            0.9563,  -0.2721,   -1.1070,\
                            0.6210,  -0.6474,    1.7046)

#define RGB_to_YUV  mat3( 0.299,   -0.14713,   0.615,\
                            0.587,   -0.28886,  -0.514991,\
                            0.114,    0.436,    -0.10001)

#define YUV_to_RGB  mat3( 1.0,      1.0,       1.0,\
                            0.0,     -0.39465,   2.03211,\
                            1.13983, -0.58060,   0.0)
                            
#define fetch(ofs,center,invx) texture2D(tex0, vec2((ofs) * (invx) + center.x, center.y))

#define FIRTAPS 20

float FIR[FIRTAPS] = float[FIRTAPS] (
   -0.008030271,
    0.003107906,
    0.016841352,
    0.032545161,
    0.049360136,
    0.066256720,
    0.082120150,
    0.095848433,
    0.106453014,
    0.113151423,
    0.115441842,
    0.113151423,
    0.106453014,
    0.095848433,
    0.082120150,
    0.066256720,
    0.049360136,
    0.032545161,
    0.016841352,
    0.003107906
);

/* subcarrier counts per scan line = FSC/FLINE = 283.7516 */
/* We save the reciprocal of this only to optimize it */
float counts_per_scanline_reciprocal = 1.0 / (FSC/FLINE);

float width_ratio;
float height_ratio;
float altv;
float invx;

/* http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/ */
float rand(vec2 co)
{
    float a  = 12.9898;
    float b  = 78.233;
    float c  = 43758.5453;
    float dt = dot(co.xy, vec2(a, b));
    float sn = mod(dt,3.14);

    return fract(sin(sn) * c);
}

float modulated(vec2 xy, float sinwt, float coswt) 
{
    vec3 rgb = fetch(0., xy, invx).xyz;
    vec3 yuv = RGB_to_YUV * rgb;

    return clamp(yuv.x + yuv.y * sinwt + yuv.z * coswt, 0.0, 1.0);    
}

vec2 modem_uv(vec2 xy, float ofs) {
    float t  = (xy.x + ofs * invx) * SourceSize.x;
    float wt = t * 2. * PI / width_ratio;

    float sinwt = sin(wt);
    float coswt = cos(wt + altv);

    vec3 rgb = fetch(ofs, xy, invx).xyz;
    vec3 yuv = RGB_to_YUV * rgb;
    float signal = clamp(yuv.x + yuv.y * sinwt + yuv.z * coswt, 0.0, 1.0);
    
    if (PHASE_NOISE != 0.)
    {
        /* .yy is horizontal noise, .xx looks bad, .xy is classic noise */
        vec2 seed = xy.yy * (iTime * 0.02); //float(FrameCount);
        wt        = wt + PHASE_NOISE * (rand(seed) - 0.5);
        sinwt     = sin(wt);
        coswt     = cos(wt + altv);
    }

    return vec2(signal * sinwt, signal * coswt);
}

void main()
{
    vec2 xy      = v_texCoord;
    width_ratio  = SourceSize.x * (counts_per_scanline_reciprocal);
    height_ratio = SourceSize.y / VISIBLELINES;
    altv         = mod(floor(xy.y * VISIBLELINES + 0.5), 2.0) * PI;
    invx         = 0.25 * (counts_per_scanline_reciprocal); // equals 4 samples per Fsc period

    // lowpass U/V at baseband
    vec2 filtered = vec2(0.0, 0.0);
#if __VERSION__ < 130 //unroll the loop
	vec2 uv;
	
	#define macro_loopz(c)	uv = modem_uv(xy, float(c) - FIRTAPS*0.5); \
        filtered += FIR_GAIN * uv * FIR##c;
		
	macro_loopz(1)
	macro_loopz(2)
	macro_loopz(3)
	macro_loopz(4)
	macro_loopz(5)
	macro_loopz(6)
	macro_loopz(7)
	macro_loopz(8)
	macro_loopz(9)
	macro_loopz(10)
	macro_loopz(11)
	macro_loopz(12)
	macro_loopz(13)
	macro_loopz(14)
	macro_loopz(15)
	macro_loopz(16)
	macro_loopz(17)
	macro_loopz(18)
	macro_loopz(19)
	macro_loopz(20)
#else
    for (int i = 0; i < FIRTAPS; i++) {
        vec2 uv   = modem_uv(xy, i - FIRTAPS*0.5);
        filtered += FIR_GAIN * uv * FIR[i];
    }
#endif
    float t  = xy.x * SourceSize.x;
    float wt = t * 2. * PI / width_ratio;

    float sinwt = sin(wt);
    float coswt = cos(wt + altv);

    float luma = modulated(xy, sinwt, coswt) - FIR_INVGAIN * (filtered.x * sinwt + filtered.y * coswt);
    vec3 yuv_result = vec3(luma, filtered.x, filtered.y);

    gl_FragColor = vec4(YUV_to_RGB * yuv_result, 1.0);
} 
