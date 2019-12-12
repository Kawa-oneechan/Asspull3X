//Dead-ass PSX dither shader by Kawa
//----------------------------------
//Based on https://stackoverflow.com/questions/4694608
//Inspired by https://www.chrismcovell.com/psxdither.html

uniform sampler2D tex0;
varying vec2 v_texCoord;

void main()
{
	vec4 color = texture2D(tex0, v_texCoord);
	float total = floor(v_texCoord.x * 640) + floor(v_texCoord.y * 480);
	bool isEven = mod(total, 2.0) == 0.0;
	gl_FragColor = isEven ? color : (color * 0.90);
}
