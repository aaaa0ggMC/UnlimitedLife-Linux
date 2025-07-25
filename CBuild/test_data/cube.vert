#version 430 core
out vec2 coord;
out vec3 varyingNormal;
out vec3 varyingLightDir;
out vec3 varyingVertPos;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normals;

struct PositionalLight{
  vec3 position;
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
};

uniform PositionalLight light;
uniform mat4 invMV;
uniform mat4 mv_matrix;
uniform mat4 proj_matrix;

void main(){
  vec4 P = mv_matrix * vec4(position,1.0);
  varyingVertPos = P.xyz;
  varyingLightDir = light.position - varyingVertPos;
  varyingNormal = (invMV * vec4(normals,1.0)).xyz;

  gl_Position = proj_matrix * mv_matrix * vec4(position,1.0);
}
