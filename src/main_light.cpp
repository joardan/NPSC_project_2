#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include <shader.h>
#include <camera.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow * window);
void updateFPS(GLFWwindow* window);

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

bool firstMouse = true;
bool cameraEnabled = true;
float lastX;
float lastY;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

glm::vec3 lightPos(1.2f, 1.0f, 2.0f);

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    unsigned int windowedWidth = mode->width;
    unsigned int windowedHeight = mode->height;

    lastX = windowedWidth / 2.0f;
    lastY = windowedHeight / 2.0f;

    GLFWwindow* window = glfwCreateWindow(windowedWidth, windowedHeight, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialise GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Shader lightingShader("../shaders/object.box.shader.vs", "../shaders/object.box.shader.fs");
    Shader lightCubeShader("../shaders/light.cube.shader.vs", "../shaders/light.cube.shader.fs");

    float vertices[] = {
        // Back face (z = -0.5)
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f,  0.0f, -1.0f, 
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  0.0f,  0.0f, -1.0f, 
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  0.0f, -1.0f, 
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  0.0f, -1.0f, 
    
        // Front face (z = +0.5)
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  0.0f,  1.0f, 
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  0.0f,  1.0f, 
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f,  0.0f,  1.0f, 
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f,  0.0f,  1.0f, 
    
        // Left face (x = -0.5)
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f, -1.0f,  0.0f,  0.0f, 
        -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, -1.0f,  0.0f,  0.0f, 
        -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, -1.0f,  0.0f,  0.0f, 
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, -1.0f,  0.0f,  0.0f, 
    
        // Right face (x = +0.5)
         0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  1.0f,  0.0f,  0.0f, 
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  1.0f,  0.0f,  0.0f, 
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  1.0f,  0.0f,  0.0f, 
         0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  1.0f,  0.0f,  0.0f, 
    
        // Bottom face (y = -0.5)
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f, -1.0f,  0.0f, 
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  0.0f, -1.0f,  0.0f, 
         0.5f, -0.5f,  0.5f,  1.0f, 1.0f,  0.0f, -1.0f,  0.0f, 
        -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,  0.0f, -1.0f,  0.0f, 
    
        // Top face (y = +0.5)
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f, 
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f, 
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f, 
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  1.0f,  0.0f  
    };
    
    unsigned int indices[] = {
        // Back face
        0,  1,  2,  2,  3,  0,
        // Front face
        4,  5,  6,  6,  7,  4,
        // Left face
        8,  9, 10, 10, 11,  8,
        // Right face
        12, 13, 14, 14, 15, 12,
        // Bottom face
        16, 17, 18, 18, 19, 16,
        // Top face
        20, 21, 22, 22, 23, 20
    };

    unsigned int VBO, cubeVAO, EBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int lightCubeVAO;
    glGenVertexArrays(1, &lightCubeVAO);
    glBindVertexArray(lightCubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    while (!glfwWindowShouldClose(window)) 
    {
        // Delta time calculations
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        updateFPS(window);
        processInput(window);
    
        // Clear screen
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
        // Calculations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)windowedWidth / (float)windowedHeight, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 LightPos = glm::vec3(view * glm::vec4(lightPos, 1.0));
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(view * model)));
        // Set tranformations
        lightingShader.use();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);
        // Set lights
        lightingShader.setVec3("light.position", LightPos);
        lightingShader.setVec3("light.ambient", 0.1f, 0.1f, 0.1f);
        lightingShader.setVec3("light.diffuse", 0.5f, 0.5f, 0.5f);
        lightingShader.setVec3("light.specular", 0.8f, 0.8f, 0.8f);
        // Set materials
        lightingShader.setVec3("material.ambient", 1.0f, 0.5f, 0.31f);
        lightingShader.setVec3("material.diffuse", 1.0f, 0.5f, 0.31f);
        lightingShader.setVec3("material.specular", 0.5f, 0.5f, 0.5f);
        lightingShader.setFloat("material.shininess", 32.0f);
        // Model & Normal Matrix
        lightingShader.setMat4("model", model);
        lightingShader.setMat3("normalMatrix", normalMatrix);
    
        // Draw Cube
        glBindVertexArray(cubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    
        // Light Cube Shader
        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);
    
        // Light Source Model Matrix
        model = glm::translate(glm::mat4(1.0f), lightPos);
        model = glm::scale(model, glm::vec3(0.2f));
        lightCubeShader.setMat4("model", model);

        // Draw Light Cube
        glBindVertexArray(lightCubeVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    
        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }    

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightCubeVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
{
    static bool backspacePressed = false;
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS) {
        if (!backspacePressed) {
            cameraEnabled = !cameraEnabled;
            glfwSetInputMode(window, GLFW_CURSOR, cameraEnabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            if (cameraEnabled) {
                firstMouse = true; // Reset mouse state to prevent jumps
            }

            backspacePressed = true;
        }
    } else {
        backspacePressed = false;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (!cameraEnabled) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}


void updateFPS(GLFWwindow* window) {
    static float elapsedTime = 0.0f;
    static int frameCount = 0;

    elapsedTime += deltaTime;
    frameCount++;

    if (elapsedTime >= 1.0f) {
        std::string title = "OpenGL - FPS: " + std::to_string(frameCount);
        glfwSetWindowTitle(window, title.c_str());

        frameCount = 0;
        elapsedTime = 0.0f;
    }
}