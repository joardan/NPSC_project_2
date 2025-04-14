#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in mat4 aInstanceModelMatrix;
layout(location = 7) in mat3 aInstanceNormalMatrix;
layout(std140, binding = 0) uniform Matrices {
    mat4 projection;
    mat4 view;
};

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

void main()
{
    gl_Position = projection * view * aInstanceModelMatrix * vec4(aPos, 1.0);
    FragPos = vec3(view * aInstanceModelMatrix * vec4(aPos, 1.0f));
    Normal = mat3(view) * aInstanceNormalMatrix * aNormal;
    TexCoords = aTexCoords;
}