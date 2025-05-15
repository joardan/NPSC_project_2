#ifndef SPHERE_CREATOR_H
#define SPHERE_CREATOR_H

#include <mesh.h>
#include <vector>
#include <glm.hpp>
#include <gtc/constants.hpp>

class SphereCreator
{
public:
    static Mesh CreateSphere(float radius, unsigned int sectorCount, unsigned int stackCount)
    {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        for (unsigned int i = 0; i <= stackCount; ++i)
        {
            float stackAngle = glm::half_pi<float>() - i * (glm::pi<float>() / stackCount);
            float xy = radius * glm::cos(stackAngle);
            float z = radius * glm::sin(stackAngle);

            for (unsigned int j = 0; j <= sectorCount; ++j)
            {
                float sectorAngle = j * (glm::two_pi<float>() / sectorCount);
                float x = xy * glm::cos(sectorAngle);
                float y = xy * glm::sin(sectorAngle);

                Vertex vertex;
                vertex.Position = glm::vec3(x, y, z);
                vertex.Normal = glm::normalize(glm::vec3(x, y, z));
                vertex.TexCoords = glm::vec2((float)j / sectorCount, (float)i / stackCount);
                vertex.Tangent = glm::vec3(0.0f, 0.0f, 0.0f);
                vertex.Bitangent = glm::vec3(0.0f, 0.0f, 0.0f);
                std::fill(std::begin(vertex.m_BoneIDs), std::end(vertex.m_BoneIDs), -1);
                std::fill(std::begin(vertex.m_Weights), std::end(vertex.m_Weights), 0.0f);
                vertices.push_back(vertex);
            }
        }

        for (unsigned int i = 0; i < stackCount; ++i)
        {
            for (unsigned int j = 0; j < sectorCount; ++j)
            {
                unsigned int first = i * (sectorCount + 1) + j;
                unsigned int second = first + sectorCount + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        return Mesh(vertices, indices, {});
    }
};

#endif
