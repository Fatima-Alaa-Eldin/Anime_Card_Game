#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <limits>
#include <cstddef>

class Model
{
private:

    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoords;
    };

    struct Mesh
    {
        GLuint VAO = 0;
        GLuint VBO = 0;
        GLuint EBO = 0;

        GLsizei indexCount = 0;

        glm::vec3 color = glm::vec3(0.08f);
    };

    std::vector<Mesh> meshes;

    glm::vec3 minBounds;
    glm::vec3 maxBounds;

public:

    Model(const std::string& path)
    {
        minBounds = glm::vec3(std::numeric_limits<float>::max());
        maxBounds = glm::vec3(std::numeric_limits<float>::lowest());

        loadModel(path);
    }

    ~Model()
    {
        deleteBuffers();
    }

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    bool IsLoaded() const
    {
        return !meshes.empty();
    }

    // Import model using Assimp
    void loadModel(const std::string& path)
    {
        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals |
            aiProcess_PreTransformVertices
        );

        if (!scene || !scene->mRootNode ||
            (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
        {
            std::cerr << "Assimp Error: "
                      << importer.GetErrorString() << '\n';
            return;
        }

        processNode(scene->mRootNode, scene);

        std::cout << "Model loaded successfully: "
                  << path << '\n';

        std::cout << "Meshes: " << meshes.size() << '\n';
    }

    // Process all model meshes
    void processNode(aiNode* node, const aiScene* scene)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

            if (mesh)
                meshes.push_back(processMesh(mesh, scene));
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }
    }

    // Convert Assimp mesh into OpenGL buffers
    Mesh processMesh(aiMesh* source, const aiScene* scene)
    {
        Mesh result;

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        for (unsigned int i = 0; i < source->mNumVertices; i++)
        {
            Vertex vertex;

            vertex.Position = glm::vec3(
                source->mVertices[i].x,
                source->mVertices[i].y,
                source->mVertices[i].z
            );

            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            if (source->HasNormals())
            {
                vertex.Normal = glm::normalize(glm::vec3(
                    source->mNormals[i].x,
                    source->mNormals[i].y,
                    source->mNormals[i].z
                ));
            }

            vertex.TexCoords = glm::vec2(0.0f);

            if (source->HasTextureCoords(0))
            {
                vertex.TexCoords = glm::vec2(
                    source->mTextureCoords[0][i].x,
                    source->mTextureCoords[0][i].y
                );
            }

            minBounds = glm::min(minBounds, vertex.Position);
            maxBounds = glm::max(maxBounds, vertex.Position);

            vertices.push_back(vertex);
        }

        for (unsigned int i = 0; i < source->mNumFaces; i++)
        {
            const aiFace& face = source->mFaces[i];

            for (unsigned int j = 0; j < face.mNumIndices; j++)
            {
                indices.push_back(face.mIndices[j]);
            }
        }

        // Read material base color
        if (source->mMaterialIndex < scene->mNumMaterials)
        {
            aiMaterial* material =
                scene->mMaterials[source->mMaterialIndex];

            aiColor4D materialColor(1.0f, 1.0f, 1.0f, 1.0f);
            aiReturn colorResult = aiGetMaterialColor(
                material, AI_MATKEY_BASE_COLOR, &materialColor
            );

            // Wavefront MTL files usually provide Kd as DIFFUSE rather than BASE_COLOR.
            if (colorResult != AI_SUCCESS)
            {
                colorResult = aiGetMaterialColor(
                    material, AI_MATKEY_COLOR_DIFFUSE, &materialColor
                );
            }

            if (colorResult == AI_SUCCESS)
            {
                result.color = glm::vec3(
                    materialColor.r,
                    materialColor.g,
                    materialColor.b
                );
            }
            else
            {
                result.color = glm::vec3(1.0f);
            }
        }

        // Create VAO / VBO / EBO
        glGenVertexArrays(1, &result.VAO);
        glGenBuffers(1, &result.VBO);
        glGenBuffers(1, &result.EBO);

        glBindVertexArray(result.VAO);

        glBindBuffer(GL_ARRAY_BUFFER, result.VBO);

        glBufferData(
            GL_ARRAY_BUFFER,
            vertices.size() * sizeof(Vertex),
            vertices.data(),
            GL_STATIC_DRAW
        );

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, result.EBO);

        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int),
            indices.data(),
            GL_STATIC_DRAW
        );

        // Position: location 0
        glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, Position)
        );
        glEnableVertexAttribArray(0);

        // Normals: location 1 (required for Diffuse and Specular lighting).
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, Normal)
        );
        glEnableVertexAttribArray(1);

        // Texture coordinates: location 2 (kept for compatibility with L5.vs).
        glVertexAttribPointer(
            2, 2, GL_FLOAT, GL_FALSE,
            sizeof(Vertex),
            (void*)offsetof(Vertex, TexCoords)
        );
        glEnableVertexAttribArray(2);

        result.indexCount = static_cast<GLsizei>(indices.size());

        glBindVertexArray(0);

        return result;
    }

    // Fit the model inside a specified box and place it in the scene
    glm::mat4 GetFitTransform(
        glm::vec3 center,
        glm::vec3 targetSize,
        float rotationY = 0.0f
    ) const
    {
        if (!IsLoaded())
            return glm::mat4(1.0f);

        glm::vec3 size = maxBounds - minBounds;

        float sx = size.x > 0.0001f ? targetSize.x / size.x : 1.0f;
        float sy = size.y > 0.0001f ? targetSize.y / size.y : 1.0f;
        float sz = size.z > 0.0001f ? targetSize.z / size.z : 1.0f;

        float uniformScale = std::min(sx, std::min(sy, sz));

        glm::vec3 modelCenter = (minBounds + maxBounds) * 0.5f;

        glm::mat4 transform(1.0f);

        transform = glm::translate(transform, center);

        transform = glm::rotate(
            transform,
            glm::radians(rotationY),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        transform = glm::scale(
            transform,
            glm::vec3(uniformScale)
        );

        transform = glm::translate(transform, -modelCenter);

        return transform;
    }

    // Draw model
    template<typename ShaderType>
    void Draw(ShaderType& shader, glm::mat4 transform)
    {
        shader.setBool("useTexture", false);
        shader.setMat4("model", transform);

        for (auto& mesh : meshes)
        {
            shader.setVec3("objectColor", mesh.color);

            glBindVertexArray(mesh.VAO);

            glDrawElements(
                GL_TRIANGLES,
                mesh.indexCount,
                GL_UNSIGNED_INT,
                nullptr
            );
        }

        glBindVertexArray(0);
    }

    // Delete OpenGL buffers
    void deleteBuffers()
    {
        for (auto& mesh : meshes)
        {
            if (mesh.VAO)
                glDeleteVertexArrays(1, &mesh.VAO);

            if (mesh.VBO)
                glDeleteBuffers(1, &mesh.VBO);

            if (mesh.EBO)
                glDeleteBuffers(1, &mesh.EBO);

            mesh.VAO = 0;
            mesh.VBO = 0;
            mesh.EBO = 0;
        }

        meshes.clear();
    }
};