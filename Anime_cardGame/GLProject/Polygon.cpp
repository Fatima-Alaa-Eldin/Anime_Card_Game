#pragma once

#include <glad/glad.h>
#include <vector>
#include <string>
#include <utility>
#include <glm/glm.hpp>
#include <learnopengl/shader_m.h>

class Polygon {
public:
    std::string name;

    Polygon(std::vector<glm::vec3> v, glm::vec3 c, std::string shapeName = "Polygon")
        : name(std::move(shapeName)), vertices(std::move(v)), color(c), model(1.0f) {

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        if (!vertices.empty()) {
            glBufferData(
                GL_ARRAY_BUFFER,
                vertices.size() * sizeof(glm::vec3),
                vertices.data(),
                GL_STATIC_DRAW
            );
        }

        // location = 0  <->  layout (location = 0) in vec3 aPos;
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    // منع النسخ (لأن النسخ كان يخلي كذا كائن يشاركوا نفس الـ VAO/VBO)
    Polygon(const Polygon&) = delete;
    Polygon& operator=(const Polygon&) = delete;

    // السماح بالنقل (ضروري لـ std::vector)
    Polygon(Polygon&& o) noexcept
        : name(std::move(o.name)),
          vertices(std::move(o.vertices)),
          color(o.color),
          model(o.model),
          VAO(o.VAO),
          VBO(o.VBO) {
        o.VAO = 0;
        o.VBO = 0;
    }

    Polygon& operator=(Polygon&& o) noexcept {
        if (this != &o) {
            deleteBuffers();
            name = std::move(o.name);
            vertices = std::move(o.vertices);
            color = o.color;
            model = o.model;
            VAO = o.VAO;
            VBO = o.VBO;
            o.VAO = 0;
            o.VBO = 0;
        }
        return *this;
    }

    // تطبيق مصفوفة التحويل (Translation / Rotation / Scale)
    void transformation(glm::mat4 t) { model = t; }

    glm::mat4 getModel() const { return model; }

    const std::vector<glm::vec3>& getVertices() const { return vertices; }

    void draw(Shader& shader) {
        shader.setVec3("objectColor", color);
        shader.setMat4("model", model);
        glBindVertexArray(VAO);
        // الرؤوس لازم تكون مرتبة بشكل متتالٍ حول محيط المضلع
        glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(vertices.size()));
        glBindVertexArray(0);
    }

    void deleteBuffers() {
        if (VBO != 0) {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }
        if (VAO != 0) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
    }

private:
    std::vector<glm::vec3> vertices;
    glm::vec3 color;
    glm::mat4 model;
    GLuint VAO = 0;
    GLuint VBO = 0;
};
