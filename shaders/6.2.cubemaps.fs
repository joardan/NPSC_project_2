#version 460 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

uniform mat4 view;
uniform samplerCube skybox;

void main()
{    
    vec3 I = normalize(FragPos);
    vec3 R = reflect(I, normalize(Normal));
    vec3 R_world = mat3(transpose(view)) * R;
    FragColor = vec4(texture(skybox, R_world).rgb, 1.0);
}