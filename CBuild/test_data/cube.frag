#version 430 core
in vec2 coord;
in vec4 varyingColor;
out vec4 color;

layout(binding = 0) uniform sampler2D tex;

struct Material{
  float shininess;
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
};

struct PositionalLight{
  vec3 position;
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
};

uniform Material material;
uniform PositionalLight light;
uniform vec4 gambient;

void main(){
  //vec4 px = material.shininess * light.position;
  //color = texture(tex,coord) + px;
  color = varyingColor;
  //color = vec4(light.position,1.0);
}
