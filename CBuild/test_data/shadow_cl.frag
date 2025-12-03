#version 430 core
in vec2 TexCoords;
out vec4 color;

layout(binding = 1) uniform sampler2D shTex;

void main(){
    float val = texture(shTex,TexCoords).r;
    val = pow(val,20);
    vec3 mid = vec3(val,val,val);
    color = vec4(mid,1.0);
}