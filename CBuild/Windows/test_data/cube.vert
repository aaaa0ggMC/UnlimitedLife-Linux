#version 430 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 in_coord;

uniform mat4 mvp_matrix;
out vec2 coord;

void main(){
 gl_Position = mvp_matrix * vec4(position,1.0);
 coord = in_coord;
 //pc = position * 0.4 + 0.6;
}
