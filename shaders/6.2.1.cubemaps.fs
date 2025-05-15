#version 460 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

uniform samplerCube skybox;
uniform mat4 view;

void main()
{    
    float ratio = 1.00 / 1.52;
    vec3 I = normalize(FragPos);
    vec3 R = refract(I, normalize(Normal), ratio);
    vec3 R_world = mat3(transpose(view)) * R;
    FragColor = vec4(texture(skybox, R_world).rgb, 1.0);
}