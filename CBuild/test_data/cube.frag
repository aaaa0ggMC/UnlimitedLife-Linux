#version 430 core

out vec4 color;
layout(binding = 0) uniform sampler2D tex;

in vec2 coord;

void main(){
  color = texture2D(tex,coord);
}
