//Simple-ass scanlines shader by Kawa
//-----------------------------------

uniform sampler2D tex0;
varying vec2 v_texCoord;

void main()
{
	vec4 color = texture2D(tex0, v_texCoord);
	int y = floor(v_texCoord.y * 480);
	if(y % 2 == 0)
		color *= 0.75;
	gl_FragColor = color;
}
