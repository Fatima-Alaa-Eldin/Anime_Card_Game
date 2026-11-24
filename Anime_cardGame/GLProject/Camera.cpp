#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

enum Camera_Movement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

class Camera
{
public:

    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Right;
    glm::vec3 Up;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    Camera(glm::vec3 position)
    {
        Position = position;

        Front = glm::vec3(0.0f, 0.0f, -1.0f);
        WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);

        Yaw = -90.0f;
        Pitch = 0.0f;

        MovementSpeed = 2.5f;
        MouseSensitivity = 0.1f;
        Zoom = 45.0f;

        updateCameraVectors();
    }

    glm::mat4 GetViewMatrix()
    {
        return glm::lookAt(Position, Position + Front, Up);
    }

    void ProcessKeyboard(Camera_Movement direction, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;

        if (direction == FORWARD)
            Position += Front * velocity;

        if (direction == BACKWARD)
            Position -= Front * velocity;

        if (direction == LEFT)
            Position -= Right * velocity;

        if (direction == RIGHT)
            Position += Right * velocity;
    }

    void ProcessMouseMovement(float xoffset, float yoffset)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (Pitch > 89.0f)
            Pitch = 89.0f;

        if (Pitch < -89.0f)
            Pitch = -89.0f;

        updateCameraVectors();
    }

    void ProcessMouseScroll(float yoffset)
    {
        Zoom -= yoffset;

        if (Zoom < 1.0f)
            Zoom = 1.0f;

        if (Zoom > 45.0f)
            Zoom = 45.0f;
    }

    void SetPose(
    const glm::vec3& position,
    float yaw,
    float pitch,
    float zoom
)
{
    Position = position;
    Yaw = yaw;
    Pitch = pitch;
    Zoom = zoom;

    updateCameraVectors();
}
private:

    void updateCameraVectors()
    {
        glm::vec3 front;

        front.x = std::cos(glm::radians(Yaw)) *
                  std::cos(glm::radians(Pitch));

        front.y = std::sin(glm::radians(Pitch));

        front.z = std::sin(glm::radians(Yaw)) *
                  std::cos(glm::radians(Pitch));

        Front = glm::normalize(front);

        Right = glm::normalize(glm::cross(Front, WorldUp));

        Up = glm::normalize(glm::cross(Right, Front));
    }
};