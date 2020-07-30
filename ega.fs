//EGA shader
//----------
//Based on https://www.shadertoy.com/view/4dXSzl

uniform sampler2D tex0;
varying vec2 v_texCoord;

#define colorDetail 16.

#define _ 0.0
#define o (1./3.)
#define b (2./3.)
#define B 1.0

#define check(r,g,b) color=vec4(r,g,b,0.); dist = distance(s,color); if (dist < bestDistance) {bestDistance = dist; bestColor = color;}

void main()
{
	float dist;
	float bestDistance = 1000.;
	vec4 color;
	vec4 bestColor;		

	vec4 s = texture2D(tex0, v_texCoord);
	s = floor(s*colorDetail+0.5)/colorDetail;	

	check(_,_,_);
	check(_,_,b);
	check(_,b,_);
	check(_,b,b);
	check(b,_,_);
	check(b,_,b);
	check(b,o,_);
	check(b,b,b);
	
	check(o,o,o);
	check(o,o,B);
	check(o,B,o);
	check(o,B,B);
	check(B,o,o);
	check(B,o,B);
	check(B,B,o);
	check(B,B,B);	
	
	gl_FragColor = bestColor;	
}

