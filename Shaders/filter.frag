#version 330 core 

//point light

uniform sampler2D tex0;
in vec2 texCoord;

in vec3 normCoord;
in vec3 fragPos;

out vec4 FragColor;


void main()
{
  vec4 pixelColor = texture(tex0, texCoord);
  if (pixelColor.a < 0.1f){discard;}

  FragColor = texture(tex0, texCoord);
}