#include <AGE/Shader.h>

using namespace age;

CreateShaderInfo::CreateShaderInfo(){
    vertex = fragment = geometry = compute = "";
    sid = "";
}

Shader::Shader(){
    reset();
}

void Shader::reset(){
    pid= 0;
    computeShader = false;
}

Shader Shader::null(){
    return Shader();
}
