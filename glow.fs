//Glow shader
//-----------
//Based on https://www.shadertoy.com/view/4tlBWH

#version 140

uniform sampler2D tex0;
varying vec2 v_texCoord;

uniform vec2 iResolution = vec2(640, 480);

#define px (vec2(1.0) / iResolution.xy)

vec4 GetBloom ( in vec2 uv, in vec4 inColor )
{
	float numSamples = 1.0;
    vec4 color = inColor;

	for (float x = -8.0; x <= 8.0; x += 1.0)
	{
		for (float y = -8.0; y <= 8.0; y += 1.0)
		{
			vec4 addColor = texture(tex0, uv + (vec2(x, y) * px));
			if (max(addColor.r, max(addColor.g, addColor.b)) > 0.3)
			{
				float dist = length(vec2(x,y))+1.0;
				vec4 glowColor = max((addColor * 128.0) / pow(dist, 2.0), vec4(0.0));
				if (max(glowColor.r, max(glowColor.g, glowColor.b)) > 0.0)
				{
					color += glowColor;
					numSamples += 1.0;
				}
			}
		}
	}
    
	return color / numSamples;
}

void main()
{
	vec2 uv = v_texCoord.xy;
	vec4 color =  texture(tex0, uv);
	gl_FragColor = mix(color, GetBloom(uv, color), 0.05);
}
