#version 430 core

out vec4 color;

in vec3 pc;

void main(){
  color = vec4(pc,1.0);
}
