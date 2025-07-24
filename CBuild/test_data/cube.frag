#version 430 core

out vec4 color;
layout(binding = 0) uniform sampler2D tex;

uniform vec4 dcolor;

in vec2 coord;

void main(){
  //color = texture(tex,coord);
  color = dcolor;
}
