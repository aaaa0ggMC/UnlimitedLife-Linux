#version 430 core
in vec2 coord;
in vec4 shadowCoord;
in vec3 varyingNormal;
in vec3 varyingLightDir;
in vec3 varyingVertPos;
in vec3 varyingHalfVector;
in float varyingLightDistance;
out vec4 color;

layout(binding = 0) uniform sampler2D tex;
layout(binding = 1) uniform sampler2DShadow shadowTex;

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

#define FACTOR 1
#define FACTOR_DIV 9
#define STEP 0.001

float sampleShadow(in vec4 tex_coord){
    float val = 0;
    for(int i = -FACTOR;i <= FACTOR;++i){
        for(int j = -FACTOR;j <= FACTOR;++j){
            val += textureProj(shadowTex,tex_coord + vec4(i * STEP * tex_coord.w, j * STEP * tex_coord.w, -0.001 ,0));
        }
    }
    val /= FACTOR_DIV;
    return val;
}

void main(){
  vec3 L = normalize(varyingLightDir);
  vec3 N = normalize(varyingNormal);
  vec3 V = normalize(-varyingVertPos);
  vec3 H = normalize(varyingHalfVector);
  float cosTheta = dot(L,N);
  float cosPhi = dot(H,N);

  vec4 texColor = texture(tex,coord);

  vec3 ambient = 2* (gambient * material.ambient + light.ambient * material.ambient).xyz * texColor.xyz;
  vec3 diffuse = light.diffuse.xyz * material.diffuse.xyz * texColor.xyz * max(cosTheta,0.0);
  vec3 specular = light.specular.xyz * material.specular.xyz * pow(max(cosPhi,0.0),material.shininess);

  color = vec4(ambient + sampleShadow(shadowCoord) * (diffuse + specular),texColor.a);
}
