#version 430 core

layout(location = 0) in vec3 position;

uniform mat4 mvp_matrix;

out vec3 pc;

void main(){
 gl_Position = mvp_matrix * vec4(position,1.0);
 pc = position * 0.4 + 0.6;
}
