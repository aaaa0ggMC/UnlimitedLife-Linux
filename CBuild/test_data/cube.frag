#version 430 core
in vec2 coord;
in vec3 varyingNormal;
in vec3 varyingLightDir;
in vec3 varyingVertPos;
in vec3 varyingHalfVector;
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
  vec3 L = normalize(varyingLightDir);
  vec3 N = normalize(varyingNormal);
  vec3 V = normalize(-varyingVertPos);
  vec3 H = normalize(varyingHalfVector);
  float cosTheta = dot(L,N);
  float cosPhi = dot(H,N);

  vec3 ambient = (gambient * material.ambient + light.ambient * material.ambient).xyz;
  vec3 diffuse = light.diffuse.xyz * material.diffuse.xyz * max(cosTheta,0.0);
  vec3 specular = light.specular.xyz * material.specular.xyz * pow(max(cosPhi,0.0),material.shininess);

  vec4 lightColor = vec4(ambient + diffuse + specular,1.0);

  color = lightColor * 0.5 + texture(tex,coord) * 0.5;
}
