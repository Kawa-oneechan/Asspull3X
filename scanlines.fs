//Simple-ass scanlines shader by Kawa
//-----------------------------------

uniform sampler2D tex0;
varying vec2 v_texCoord;

void main()
{
	vec4 color = texture2D(tex0, v_texCoord);
	float y = floor(v_texCoord.y * 480);
	bool isEven = mod(y, 2.0) == 0.0;
	gl_FragColor = isEven ? color : (color * 0.75);
}
