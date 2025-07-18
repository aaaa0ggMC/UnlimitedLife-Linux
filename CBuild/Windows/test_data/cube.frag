#version 430 core

out vec4 color;
in vec2 coord;

layout(binding = 0) uniform sampler2D tex;

void main(){
  color = texture2D(tex,coord);
}
