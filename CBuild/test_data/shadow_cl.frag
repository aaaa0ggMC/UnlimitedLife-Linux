#version 430 core
in vec2 TexCoords;
out vec4 color;

layout(binding = 1) uniform sampler2D shTex;

void main(){
    float val = texture(shTex,TexCoords).r;
    vec3 mid = vec3(1 - val,1 - val,1 - val);
    color = vec4(10 * mid,1.0);
}