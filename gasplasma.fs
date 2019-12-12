//Simple-ass Gas Plasma shader by Kawa
//------------------------------------
//Based on my Gameboy shader.

uniform sampler2D tex0;
varying vec2 v_texCoord;

void main()
{
	vec4 color = texture2D(tex0, v_texCoord);
	float intensity = dot(color, vec4(0.299, 0.587, 0.184, 0));
	if(intensity <= 0.5) gl_FragColor = mix(vec4(0), vec4(1, 0.32, 0.19, 1), intensity * 1.5);
	else gl_FragColor = mix(vec4(1, 0.32, 0.19, 1), vec4(1, 0.9, 0.1, 1), (intensity - 0.5) * 1.5);
}
