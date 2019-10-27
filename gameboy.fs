//Simple-ass Gameboy shader by Kawa
//---------------------------------
//I made this almost ten years ago in HLSL
//and ported it just now.

uniform sampler2D tex0;
varying vec2 v_texCoord;

void main()
{
  vec4 color = texture2D(tex0, v_texCoord);
  float intensity;
  vec4 gb;

  //intensity = 0.299 * color.r + 0.587 * color.g + 0.184 * color.r;
  intensity = dot(color, vec4(0.299, 0.587, 0.184, 0));

  gb = vec4(0.60, 0.73, 0.05, color.a);
  if(intensity <= 0.7) gb = vec4(0.38, 0.54, 0.38, color.a);
  if(intensity <= 0.5) gb = vec4(0.18, 0.38, 0.18, color.a);
  if(intensity <= 0.3) gb = vec4(0.05, 0.21, 0.05, color.a);

  gl_FragColor = gb;
}
