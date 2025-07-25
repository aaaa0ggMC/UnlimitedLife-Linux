#version 430 core
out vec2 coord;
out vec4 varyingColor;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normals;

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

uniform mat4 invMV;
uniform mat4 mv_matrix;
uniform mat4 proj_matrix;

void main(){
    vec4 P = mv_matrix * vec4(position,1.0);
    vec3 N = normalize((invMV * vec4(normals,1.0)).xyz);
    //vec3 N = normals;
    vec3 L = normalize(light.position - P.xyz);
    vec3 V = normalize(-P.xyz);
    vec3 R = reflect(-L,N);

    vec3 ambient = (gambient * material.ambient + light.ambient * material.ambient).xyz;
    vec3 diffuse = light.diffuse.xyz * material.diffuse.xyz * max(dot(N,L),0);
    vec3 specular = (material.specular * light.specular).xyz * pow(max(dot(R,V),0.0f),material.shininess);

    varyingColor =  vec4(ambient + diffuse + specular,1.0);
    gl_Position = proj_matrix * P;
    coord = texCoord;
}
