#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <gtc/quaternion.hpp>

#include <shader.h>
#include <camera.h>
#include <model.h>    // Should include mesh.h
#include <sphere.h>   // For SphereCreator

#include <iostream>
#include <vector>
#include <string>
#include <random>     // For C++11 random number generation
#include <iomanip>    // For std::fixed and std::setprecision in updateFPS

// ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define NR_POINT_LIGHTS 1 // Sun will be the point light

void framebuffer_size_callback([[maybe_unused]] GLFWwindow* window, int width, int height);
void mouse_callback([[maybe_unused]] GLFWwindow* window, double xpos, double ypos);
void scroll_callback([[maybe_unused]] GLFWwindow* window, [[maybe_unused]] double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void updateFPS(GLFWwindow* window);
unsigned int loadTexture(char const* path, bool gammaCorrection);
unsigned int loadCubemap(std::vector<std::string> faces);
void resetSimulation(); // Forward declaration

// Camera
Camera camera(glm::vec3(0.0f, 20.0f, 150.0f));
float lastX;
float lastY;
bool firstMouse = true;
bool cameraEnabled = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float simulationSpeed = 1.0f;

// Physics & Scene Objects
const float GRAVITATIONAL_CONSTANT_BASE = 6.674e-11f; // Not directly used, G_scaled is used
float G_scaled = 1000.0f;

struct CelestialBody {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    float mass;
    float radiusScale;
    Model* modelPtr;
    Mesh* meshPtr;
    glm::quat orientation;
    glm::mat4 modelMatrix;
    bool isStatic;
    bool isAsteroid;
    int type; // 0: Sun, 1: Planet, 2: Asteroid

    CelestialBody(glm::vec3 pos, glm::vec3 vel, float m, float rScale,
                  Model* mod = nullptr, Mesh* mesh = nullptr,
                  glm::quat orient = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                  bool stat = false, bool asteroid = false, int t = 2)
        : position(pos),
          velocity(vel),
          acceleration(0.0f), // Initialize here
          mass(m),
          radiusScale(rScale),
          modelPtr(mod),
          meshPtr(mesh),
          orientation(orient),
          // modelMatrix initialized by updateModelMatrix()
          isStatic(stat),
          isAsteroid(asteroid),
          type(t)
    {
        updateModelMatrix();
    }

    void applyForce(glm::vec3 force) {
        if (isStatic || mass == 0.0f) return;
        acceleration += force / mass;
    }

    void update(float dt) {
        if (isStatic) {
            acceleration = glm::vec3(0.0f);
            updateModelMatrix(); // Still update matrix if scale changes via ImGui (not yet implemented for static objects)
            return;
        }
        velocity += acceleration * dt;
        position += velocity * dt;
        acceleration = glm::vec3(0.0f);
        updateModelMatrix();
    }

    void updateModelMatrix() {
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rot = glm::mat4_cast(orientation);
        glm::mat4 scale_mat = glm::scale(glm::mat4(1.0f), glm::vec3(radiusScale)); // Renamed to avoid conflict
        modelMatrix = trans * rot * scale_mat;
    }
};

std::vector<CelestialBody> celestialBodies;
Model* planetModelPtr = nullptr;
Model* rockModelPtr = nullptr;
Mesh sphereMesh; // For the sun - REQUIRES Mesh TO HAVE A DEFAULT CONSTRUCTOR (see notes above)

unsigned int asteroidAmount = 0;
glm::mat4* asteroidModelMatrices = nullptr;
glm::mat3* asteroidNormalMatrices = nullptr;
unsigned int asteroidInstanceVBO = 0;
unsigned int asteroidNormalInstanceVBO = 0;

float sunMass = 20000.0f;
float sunRadiusScale = 15.0f;

float planetMass = 200.0f;
float planetRadiusScale = 4.0f;
float planetOrbitRadius = 70.0f;
float planetInitialAngle = 0.0f;

float avgAsteroidMass = 0.1f;
float minAsteroidScale = 0.05f;
float maxAsteroidScale = 0.25f;
float asteroidBeltInnerRadius = 100.0f;
float asteroidBeltOuterRadius = 180.0f;
float asteroidBeltHeight = 10.0f;

bool pauseSimulation = false;


void initializeCelestialBodies() {
    celestialBodies.clear();

    // Sun
    glm::vec3 sunPos(0.0f, 0.0f, 0.0f);
    glm::vec3 sunVel(0.0f, 0.0f, 0.0f);
    celestialBodies.emplace_back(sunPos, sunVel, sunMass, sunRadiusScale, nullptr, &sphereMesh, glm::quat(1.0f,0,0,0), false, false, 0);

    // Planet
    float angleRad = glm::radians(planetInitialAngle);
    glm::vec3 planetPos(planetOrbitRadius * cos(angleRad), 0.0f, planetOrbitRadius * sin(angleRad));
    float orbitalVelMag = (sunMass > 0 && planetOrbitRadius > 0) ? sqrt((G_scaled * sunMass) / planetOrbitRadius) : 0.0f;
    glm::vec3 planetVel(-orbitalVelMag * sin(angleRad), 0.0f, orbitalVelMag * cos(angleRad));
    celestialBodies.emplace_back(planetPos, planetVel, planetMass, planetRadiusScale, planetModelPtr, nullptr, glm::angleAxis(glm::radians(0.0f), glm::vec3(0,1,0)), false, false, 1);

    if (asteroidModelMatrices) delete[] asteroidModelMatrices;
    if (asteroidNormalMatrices) delete[] asteroidNormalMatrices;
    if (asteroidAmount > 0) {
        asteroidModelMatrices = new glm::mat4[asteroidAmount];
        asteroidNormalMatrices = new glm::mat3[asteroidAmount];
    } else {
        asteroidModelMatrices = nullptr;
        asteroidNormalMatrices = nullptr;
    }


    std::mt19937 rng(static_cast<unsigned int>(glfwGetTime()));
    std::uniform_real_distribution<float> distribRadius(asteroidBeltInnerRadius, asteroidBeltOuterRadius);
    std::uniform_real_distribution<float> distribAngle(0.0f, 2.0f * glm::pi<float>());
    std::uniform_real_distribution<float> distribHeight(-asteroidBeltHeight / 2.0f, asteroidBeltHeight / 2.0f);
    std::uniform_real_distribution<float> distribScale(minAsteroidScale, maxAsteroidScale);
    std::uniform_real_distribution<float> distribRot(0.0f, 360.0f);
    std::uniform_real_distribution<float> distribMassFactor(0.5f, 1.5f);

    for (unsigned int i = 0; i < asteroidAmount; i++) {
        float r = distribRadius(rng);
        float angle = distribAngle(rng);
        float y = distribHeight(rng);
        glm::vec3 pos(r * cos(angle), y, r * sin(angle));

        float velMag = (sunMass > 0 && r > 0) ? sqrt((G_scaled * sunMass) / r) : 0.0f;
        glm::vec3 vel(-velMag * sin(angle), 0.0f, velMag * cos(angle));

        std::uniform_real_distribution<float> distribVelPerturb(-velMag*0.1f, velMag*0.1f);
        vel.x += distribVelPerturb(rng);
        vel.y += distribVelPerturb(rng) * 0.1f;
        vel.z += distribVelPerturb(rng);

        float currentAsteroidMass = avgAsteroidMass * distribMassFactor(rng);
        float currentAsteroidScale = distribScale(rng);
        
        glm::vec3 randomAxis = glm::normalize(glm::vec3(distribRot(rng) + 0.1f, distribRot(rng) + 0.1f, distribRot(rng) + 0.1f)); // Ensure not zero vector
        glm::quat orientation = glm::angleAxis(glm::radians(distribRot(rng)), randomAxis);

        celestialBodies.emplace_back(pos, vel, currentAsteroidMass, currentAsteroidScale, rockModelPtr, nullptr, orientation, false, true, 2);
    }
}

void updatePhysics(float dt) {
    if (pauseSimulation) return;

    dt *= simulationSpeed;
    if (dt == 0.0f) return; // No time passed, no update

    for (size_t i = 0; i < celestialBodies.size(); ++i) {
        if (celestialBodies[i].isStatic) continue;

        for (size_t j = 0; j < celestialBodies.size(); ++j) {
            if (i == j) continue;
            if (celestialBodies[i].isAsteroid && celestialBodies[j].isAsteroid) continue;

            glm::vec3 r_vec = celestialBodies[j].position - celestialBodies[i].position;
            float r_mag_sq = glm::dot(r_vec, r_vec);
            
            const float epsilon_sq = 1e-4f; // Softening factor squared
            if (r_mag_sq < epsilon_sq) {
                 r_mag_sq = epsilon_sq;
            }
            
            float r_mag = sqrt(r_mag_sq);
            glm::vec3 r_hat = (r_mag > 0.0f) ? (r_vec / r_mag) : glm::vec3(0.0f);

            float force_mag = (G_scaled * celestialBodies[i].mass * celestialBodies[j].mass) / r_mag_sq;
            glm::vec3 force_vec = force_mag * r_hat;

            celestialBodies[i].applyForce(force_vec);
        }
    }

    unsigned int asteroidInstanceIdx = 0; // Changed to unsigned int
    for (size_t i = 0; i < celestialBodies.size(); ++i) {
        celestialBodies[i].update(dt);
        if (celestialBodies[i].isAsteroid && asteroidInstanceIdx < asteroidAmount) {
            asteroidModelMatrices[asteroidInstanceIdx] = celestialBodies[i].modelMatrix;
            asteroidNormalMatrices[asteroidInstanceIdx] = glm::transpose(glm::inverse(glm::mat3(celestialBodies[i].modelMatrix)));
            asteroidInstanceIdx++;
        }
    }
    
    if (asteroidAmount > 0 && asteroidInstanceIdx > 0 && asteroidInstanceVBO != 0 && asteroidNormalInstanceVBO != 0) {
        glBindBuffer(GL_ARRAY_BUFFER, asteroidInstanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, asteroidInstanceIdx * sizeof(glm::mat4), asteroidModelMatrices);
        glBindBuffer(GL_ARRAY_BUFFER, asteroidNormalInstanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, asteroidInstanceIdx * sizeof(glm::mat3), asteroidNormalMatrices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void setupAsteroidInstanceBuffers() {
    if (asteroidInstanceVBO != 0) { glDeleteBuffers(1, &asteroidInstanceVBO); asteroidInstanceVBO = 0; }
    if (asteroidNormalInstanceVBO != 0) { glDeleteBuffers(1, &asteroidNormalInstanceVBO); asteroidNormalInstanceVBO = 0; }
    
    if (asteroidAmount == 0 || !rockModelPtr) return;

    glGenBuffers(1, &asteroidInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, asteroidInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, asteroidAmount * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &asteroidNormalInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, asteroidNormalInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, asteroidAmount * sizeof(glm::mat3), nullptr, GL_DYNAMIC_DRAW);

    for (unsigned int i = 0; i < rockModelPtr->meshes.size(); i++) {
        unsigned int VAO = rockModelPtr->meshes[i].VAO;
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, asteroidInstanceVBO);
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
        glEnableVertexAttribArray(4); glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
        glEnableVertexAttribArray(5); glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
        glEnableVertexAttribArray(6); glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));
        glVertexAttribDivisor(3, 1); glVertexAttribDivisor(4, 1); glVertexAttribDivisor(5, 1); glVertexAttribDivisor(6, 1);

        glBindBuffer(GL_ARRAY_BUFFER, asteroidNormalInstanceVBO);
        glEnableVertexAttribArray(7); glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(glm::mat3), (void*)0);
        glEnableVertexAttribArray(8); glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(glm::mat3), (void*)(sizeof(glm::vec3)));
        glEnableVertexAttribArray(9); glVertexAttribPointer(9, 3, GL_FLOAT, GL_FALSE, sizeof(glm::mat3), (void*)(2 * sizeof(glm::vec3)));
        glVertexAttribDivisor(7, 1); glVertexAttribDivisor(8, 1); glVertexAttribDivisor(9, 1);
        
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void resetSimulation() {
    initializeCelestialBodies();
    setupAsteroidInstanceBuffers();
}


int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE); // Can cause issues on some systems / WM
    glfwWindowHint(GLFW_SAMPLES, 4);

    // Get primary monitor video mode for window size
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    unsigned int windowedWidth = mode->width * 0.9;  // Use 90% of screen for non-maximized
    unsigned int windowedHeight = mode->height * 0.9;
    if (windowedWidth == 0 || windowedHeight == 0) { // Fallback
        windowedWidth = 1280; windowedHeight = 720;
    }


    lastX = windowedWidth / 2.0f;
    lastY = windowedHeight / 2.0f;

    GLFWwindow* window = glfwCreateWindow(windowedWidth, windowedHeight, "Solar System Sim", NULL, NULL);
    if (window == NULL) { std::cout << "Failed to create GLFW window" << std::endl; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    
    glfwSetInputMode(window, GLFW_CURSOR, cameraEnabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cout << "Failed to initialize GLAD" << std::endl; return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);  
    camera.MovementSpeed = 50.0f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460"); // Ensure this matches your shader capabilities

    // Shaders (Paths from original, VS then FS)
    Shader lightSourceShader("../shaders.2/structured.light.cube.shader.vs", "../shaders.2/light.cube.shader.fs");
    Shader skyboxShader("../shaders.2/structured.skybox.vs", "../shaders.2/6.1.skybox.fs");
    Shader objectShader("../shaders.2/structured.object.model.shader.vs", "../shaders.2/2.instanced.object.model.shader.fs");
    Shader asteroidShader("../shaders.2/instanced.object.model.shader.vs", "../shaders.2/2.instanced.object.model.shader.fs");

    // Skybox
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
    };
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO); glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO); glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
    std::vector<std::string> faces {
        "../textures/space_skybox/GalaxyTex_PositiveX.png", "../textures/space_skybox/GalaxyTex_NegativeX.png",
        "../textures/space_skybox/GalaxyTex_PositiveY.png", "../textures/space_skybox/GalaxyTex_NegativeY.png",
        "../textures/space_skybox/GalaxyTex_PositiveZ.png", "../textures/space_skybox/GalaxyTex_NegativeZ.png"
    };
    unsigned int cubemapTexture = loadCubemap(faces);
    stbi_set_flip_vertically_on_load(true); // For model textures if they need it (often they do)
    planetModelPtr = new Model("../resources/objects/planet/planet.obj", true);
    rockModelPtr = new Model("../resources/objects/rock/rock.obj", true);
    stbi_set_flip_vertically_on_load(false); // Reset if other images don't need it

    sphereMesh = SphereCreator::CreateSphere(1.0f, 36, 18);
    resetSimulation();

    unsigned int uboMatrices;
    glGenBuffers(1, &uboMatrices);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboMatrices);

    // Projection matrix update will happen in the loop or framebuffer_size_callback
    // For now, set initial projection (will be updated if window resizes or zoom changes)
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)windowedWidth / (float)windowedHeight, 0.1f, 3000.0f);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
    glBindBuffer(GL_UNIFORM_BUFFER, 0);


    struct Material { float shininess; float padding[3]; };
    struct DirLight { glm::vec3 direction; float padding1; glm::vec3 ambient; float padding2; glm::vec3 diffuse; float padding3; glm::vec3 specular; float padding4; };
    struct PointLight { glm::vec4 position; float constant, linear, quadratic, padding0; glm::vec3 ambient; float padding1; glm::vec3 diffuse; float padding2; glm::vec3 specular; float padding3; };
    struct SpotLight { glm::vec3 position_spot; float padding0_spot; glm::vec3 direction_spot; float cutOff; float outerCutOff; float constant_spot; float linear_spot; float quadratic_spot; glm::vec3 ambient_spot; float padding4_spot; glm::vec3 diffuse_spot; float padding5_spot; glm::vec3 specular_spot; float padding6_spot;}; // Adjusted Spotlight for vec4 alignment
    struct LightData { Material material; DirLight dirLight; PointLight pointLights[NR_POINT_LIGHTS]; SpotLight spotLight; };
    
    GLuint uboLightData;
    glGenBuffers(1, &uboLightData);
    glBindBuffer(GL_UNIFORM_BUFFER, uboLightData);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(LightData), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, uboLightData);

    LightData lighting;
    lighting.material.shininess = 32.0f;
    lighting.dirLight.direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    lighting.dirLight.ambient = glm::vec3(0.02f);
    lighting.dirLight.diffuse = glm::vec3(0.1f);
    lighting.dirLight.specular = glm::vec3(0.1f);

    lighting.pointLights[0].constant = 1.0f;
    lighting.pointLights[0].linear = 0.0007f; // Adjusted for larger scales, might need tuning
    lighting.pointLights[0].quadratic = 0.000002f; // Adjusted
    lighting.pointLights[0].ambient = glm::vec3(0.1f, 0.1f, 0.1f);
    lighting.pointLights[0].diffuse = glm::vec3(1.0f, 0.95f, 0.8f); // Sun-like color
    lighting.pointLights[0].specular = glm::vec3(1.0f);
    
    // Initialize Spotlight (e.g. camera flashlight)
    lighting.spotLight.position_spot = camera.Position; // Will be updated
    lighting.spotLight.direction_spot = camera.Front;   // Will be updated
    lighting.spotLight.cutOff = glm::cos(glm::radians(12.5f));
    lighting.spotLight.outerCutOff = glm::cos(glm::radians(15.0f));
    lighting.spotLight.constant_spot = 1.0f;
    lighting.spotLight.linear_spot = 0.022f;
    lighting.spotLight.quadratic_spot = 0.0019f;
    lighting.spotLight.ambient_spot = glm::vec3(0.0f);
    lighting.spotLight.diffuse_spot = glm::vec3(0.8f);
    lighting.spotLight.specular_spot = glm::vec3(0.5f);


    skyboxShader.use(); skyboxShader.setInt("skybox", 0);
    objectShader.use(); objectShader.setBool("gamma", true); // Assuming shaders handle gamma
    asteroidShader.use(); asteroidShader.setBool("gamma", true); asteroidShader.setInt("texture_diffuse1", 0);


    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents(); // Poll events early
        processInput(window); // Process input after polling

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Simulation Controls");
        ImGui::Text("FPS: %.1f (%.3f ms/frame)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Checkbox("Pause Simulation", &pauseSimulation);
        ImGui::SliderFloat("Sim Speed", &simulationSpeed, 0.0f, 10.0f);
        ImGui::Separator();
        ImGui::Text("Physics:");
        ImGui::SliderFloat("G Scaled", &G_scaled, 0.0f, 20000.0f, "%.0f");
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Sun Properties")) {
            bool sunChanged = ImGui::SliderFloat("Sun Mass", &sunMass, 1000.0f, 100000.0f, "%.0f");
            sunChanged |= ImGui::SliderFloat("Sun Radius Scale", &sunRadiusScale, 1.0f, 50.0f);
            if (sunChanged && !celestialBodies.empty()) {
                celestialBodies[0].mass = sunMass;
                celestialBodies[0].radiusScale = sunRadiusScale;
                // No full reset needed for sun mass/scale only, but orbits will be affected.
            }
        }
        if (ImGui::CollapsingHeader("Planet Properties")) {
             ImGui::SliderFloat("Planet Mass", &planetMass, 1.0f, 1000.0f);
             ImGui::SliderFloat("Planet Radius Scale", &planetRadiusScale, 0.1f, 10.0f);
             ImGui::SliderFloat("Planet Orbit Radius", &planetOrbitRadius, 10.0f, 300.0f);
             ImGui::SliderFloat("Planet Initial Angle", &planetInitialAngle, 0.0f, 360.0f);
        }
        if (ImGui::CollapsingHeader("Asteroid Properties")) {
            bool asteroidAmountChanged = ImGui::SliderInt("Asteroid Count", (int*)&asteroidAmount, 0, 20000); // Reduced max for stability
            ImGui::SliderFloat("Avg. Asteroid Mass", &avgAsteroidMass, 0.001f, 1.0f, "%.3f");
            ImGui::SliderFloat("Min Asteroid Scale", &minAsteroidScale, 0.01f, 0.5f);
            ImGui::SliderFloat("Max Asteroid Scale", &maxAsteroidScale, 0.05f, 1.0f);
            ImGui::SliderFloat("Belt Inner Radius", &asteroidBeltInnerRadius, 20.0f, 500.0f);
            ImGui::SliderFloat("Belt Outer Radius", &asteroidBeltOuterRadius, 50.0f, 600.0f);
            ImGui::SliderFloat("Belt Height", &asteroidBeltHeight, 1.0f, 50.0f);
            if (asteroidAmountChanged) resetSimulation(); // Reset if count changes
        }
        if (ImGui::Button("Reset Simulation Full")) {
            resetSimulation();
        }
        ImGui::End();


        if (!celestialBodies.empty()) {
            updatePhysics(deltaTime);
            lighting.pointLights[0].position = glm::vec4(celestialBodies[0].position, 1.0f);
        }
        lighting.spotLight.position_spot = camera.Position;
        lighting.spotLight.direction_spot = camera.Front;
        glBindBuffer(GL_UNIFORM_BUFFER, uboLightData);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightData), &lighting); // Update all light data
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        glClearColor(0.01f, 0.01f, 0.01f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        projection = glm::perspective(glm::radians(camera.Zoom), (float)display_w / (float)display_h, 0.1f, 3000.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        if (celestialBodies.empty()) {
            ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
            continue;
        }

        // Sun
        lightSourceShader.use();
        lightSourceShader.setMat4("projection", projection); // Ensure these shaders take P and V
        lightSourceShader.setMat4("view", view);
        lightSourceShader.setMat4("model", celestialBodies[0].modelMatrix);
        sphereMesh.Draw(lightSourceShader);

        // Planet
        if (celestialBodies.size() > 1 && planetModelPtr) {
             objectShader.use();
             objectShader.setMat4("viewMat", view); // Assuming shader takes separate view for non-UBO path
             objectShader.setVec3("viewPos", camera.Position);
             objectShader.setMat4("model", celestialBodies[1].modelMatrix);
             objectShader.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(celestialBodies[1].modelMatrix))));
             planetModelPtr->Draw(objectShader);
        }

        // Asteroids
        if (asteroidAmount > 0 && rockModelPtr && asteroidInstanceVBO != 0) {
            asteroidShader.use();
            asteroidShader.setMat4("viewMat", view);
            asteroidShader.setVec3("viewPos", camera.Position);
            if (!rockModelPtr->textures_loaded.empty()) {
                 glActiveTexture(GL_TEXTURE0); // Ensure texture unit 0 for diffuse
                 glBindTexture(GL_TEXTURE_2D, rockModelPtr->textures_loaded[0].id);
                 // asteroidShader.setInt("texture_diffuse1", 0); // Set once at init or ensure it's set
            }
            for (unsigned int i = 0; i < rockModelPtr->meshes.size(); i++) {
                glBindVertexArray(rockModelPtr->meshes[i].VAO);
                glDrawElementsInstanced(GL_TRIANGLES, static_cast<unsigned int>(rockModelPtr->meshes[i].indices.size()), GL_UNSIGNED_INT, 0, asteroidAmount);
                glBindVertexArray(0);
            }
        }
        
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        glm::mat4 skyboxView = glm::mat4(glm::mat3(camera.GetViewMatrix()));
        skyboxShader.setMat4("view", skyboxView);
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        updateFPS(window); // Call after swap potentially, or before, depends on what it measures
    }

    if (asteroidModelMatrices) delete[] asteroidModelMatrices;
    if (asteroidNormalMatrices) delete[] asteroidNormalMatrices;
    if (asteroidInstanceVBO != 0) glDeleteBuffers(1, &asteroidInstanceVBO);
    if (asteroidNormalInstanceVBO != 0) glDeleteBuffers(1, &asteroidNormalInstanceVBO);

    delete planetModelPtr;
    delete rockModelPtr;
    // sphereMesh is not dynamically allocated, so no delete needed if it's an object.
    // Its internal GL resources (VAO/VBO) should be cleaned up if Mesh dtor doesn't do it.
    // For simplicity, assuming Mesh doesn't auto-cleanup its GL buffers on destruction.
    // If it does, then SphereCreator created mesh's buffers would be cleaned when sphereMesh goes out of scope.

    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteBuffers(1, &uboMatrices);
    glDeleteBuffers(1, &uboLightData);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window) {
    ImGuiIO& io = ImGui::GetIO();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) // Escape always works
        glfwSetWindowShouldClose(window, true);

    if (io.WantCaptureKeyboard && !cameraEnabled) return; // If ImGui wants keyboard and camera is off, let ImGui have it.
                                                          // If camera is on, special handling for backspace.
    
    static bool backspacePressed = false; // Toggle camera/UI mode
    if (glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS) {
        if (!backspacePressed) {
            cameraEnabled = !cameraEnabled;
            glfwSetInputMode(window, GLFW_CURSOR, cameraEnabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            if (cameraEnabled) firstMouse = true;
            backspacePressed = true;
        }
    } else {
        backspacePressed = false;
    }
    
    if (io.WantCaptureKeyboard && !cameraEnabled) return; // Re-check after backspace logic for general keys

    // Camera movement keys are processed only if camera is enabled OR ImGui doesn't want keyboard input
    if (cameraEnabled || !io.WantCaptureKeyboard) {
        float actualDeltaTime = deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) actualDeltaTime *= 3.5f;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, actualDeltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, actualDeltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, actualDeltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, actualDeltaTime);
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera.ProcessKeyboard(UP, actualDeltaTime);
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) camera.ProcessKeyboard(DOWN, actualDeltaTime);
    }
}

void framebuffer_size_callback([[maybe_unused]] GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    // Projection matrix should be updated here or in the main loop when camera.Zoom changes
    // If camera.Zoom can change, or aspect ratio needs to be strictly tied to resize:
    // if (height > 0 && uboMatrices != 0) { // uboMatrices needs to be accessible or passed
    //    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)width / (float)height, 0.1f, 3000.0f);
    //    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    //    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
    //    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    // }
}

void mouse_callback([[maybe_unused]] GLFWwindow* window, double xposIn, double yposIn) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || !cameraEnabled) {
        return;
    }
    
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos; lastY = ypos; firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos; lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback([[maybe_unused]] GLFWwindow* window, [[maybe_unused]] double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
     if (io.WantCaptureMouse || !cameraEnabled) {
        return;
    }
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void updateFPS(GLFWwindow* window) {
    static double lastTime = glfwGetTime();
    static int nbFrames = 0;
    static double lastFPSTitleUpdateTime = 0.0;

    double currentTime = glfwGetTime();
    nbFrames++;
    if (currentTime - lastTime >= 1.0) { // If last prinf() was more than 1 sec ago
        // Convert nbFrames to FPS
        double fps = double(nbFrames) / (currentTime - lastTime);
        
        // Update title only every 0.25 seconds or so to avoid excessive updates
        if (currentTime - lastFPSTitleUpdateTime >= 0.25) {
            std::string title = "Solar System Sim - FPS: " + std::to_string(static_cast<int>(fps));
            glfwSetWindowTitle(window, title.c_str());
            lastFPSTitleUpdateTime = currentTime;
        }

        nbFrames = 0;
        lastTime = currentTime;
    }
}

unsigned int loadTexture(char const * path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat = GL_RGB; // Default
        GLenum dataFormat = GL_RGB;     // Default

        if (nrComponents == 1)
        {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrComponents == 3) // GL_RGB
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4) // GL_RGBA
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data); // stbi_load sets data to NULL on failure, safe to call free.
        textureID = 0; // Return 0 on failure
    }

    return textureID;
}

unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false); // Cubemaps generally don't need flipping
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format = GL_RGB;
            if (nrChannels == 1) format = GL_RED;
            else if (nrChannels == 4) format = GL_RGBA;
            
            // For cubemaps, often SRGB is not used for internal format unless specifically needed
            // and handled by shader. Standard is GL_RGB/GL_RGBA.
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
            // Consider returning 0 or handling error more robustly
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    // stbi_set_flip_vertically_on_load(true); // Set back if models need it - done in main where models are loaded

    return textureID;
}