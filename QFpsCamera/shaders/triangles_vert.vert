#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

layout(push_constant) uniform PushConstant{
    mat4 mvp;
}pushConstant;

layout(location = 0) out vec3 v_color;

out gl_PerVertex { vec4 gl_Position; };

void main(){
    v_color = color;
    gl_Position = pushConstant.mvp * vec4(position,0,1);
}