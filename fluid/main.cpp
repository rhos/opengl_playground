#include <iostream>
#include <map>
#include <utility>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#pragma comment(lib, "opengl32.lib")
#include <windows.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <common/shader.hpp>
#include <common/texture.hpp>

const int gWidth = 1024;
const int gHeight = 768;
const int fluidGrid = 128;
const int dyeGrid = 512;

GLFWwindow* initWindow(int width, int height)
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return nullptr;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, "Fluid", NULL, NULL);
    if (window == nullptr)
    {
        std::cerr << "Failed to open GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = true;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        getchar();
        glfwTerminate();
        return nullptr;
    }
    return window;
}

GLuint quadVAO;

void prepareQuad();
void initGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);

    prepareQuad();
}

void prepareQuad()
{
    static const GLfloat quadData[] =
        {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            1.0f, 1.0f,
            -1.0f, 1.0f,

            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f};

    glGenVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);

    GLuint quadVBO;
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadData), quadData, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)(8 * sizeof(float)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
}

void drawQuad()
{
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void renderScreen(GLuint texture)
{
}

struct Target
{
    GLuint fbo;
    GLuint texture;

    Target(int width, int height, GLuint internal, GLuint format, GLuint filtering)
    {
        texture = createTexture(width, height, internal, format, filtering);
        fbo = createFbo(width, height, texture);

        textureTemp = createTexture(width, height, internal, format, filtering);
        fboTemp = createFbo(width, height, textureTemp);
    }

    GLuint bind(GLuint id)
    {
        glActiveTexture(GL_TEXTURE0 + id);
        glBindTexture(GL_TEXTURE_2D, texture);
        return id;
    }

    void swap()
    {
        std::swap(fbo, fboTemp);
        std::swap(texture, textureTemp);
    }

    GLuint targetFbo()
    {
        return fboTemp;
    }

    GLuint fboTemp;
    GLuint textureTemp;

private:
    GLuint createTexture(int width, int height, GLuint format, GLuint internalFormat, GLuint filtering)
    {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_FLOAT, 0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        return textureID;
    }

    GLuint createFbo(int width, int height, GLuint textureID)
    {
        GLuint bufferID;
        glGenFramebuffers(1, &bufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, bufferID);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureID, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);

        return bufferID;
    }
};

struct Shader
{
    GLuint id;
    Shader(std::string vertex, std::string fragment)
    {
        id = LoadShaders(vertex.c_str(), fragment.c_str());
        collectUniforms();
    }

    void setUniform(const std::string& name, glm::vec2 vec)
    {
        glUniform2f(uniforms[name], vec.x, vec.y);
    }

    void setUniform(const std::string& name, glm::vec3 vec)
    {
        glUniform3f(uniforms[name], vec.x, vec.y, vec.z);
    }

    void setUniform(const std::string& name, GLuint val)
    {
        glUniform1i(uniforms[name], val);
    }

    void setUniform(const std::string& name, float val)
    {
        glUniform1f(uniforms[name], val);
    }

    void setUniform(const std::string& name, double val)
    {
        glUniform1f(uniforms[name], (float)val);
    }

private:
    void collectUniforms()
    {
        int count;
        glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &count);
        for (int i = 0; i < count; i++)
        {
            int length, size;
            GLenum type = GL_ZERO;
            char name[100];
            glGetActiveUniform(id, (GLuint)i, sizeof(name) - 1, &length, &size, &type, name);
            name[length] = 0;
            uniforms[name] = glGetUniformLocation(id, name);
        }
    }
    std::map<std::string, GLuint> uniforms;
};

struct Fluid
{
    int fWidth;
    int fHeight;

    int dWidth;
    int dHeight;

    float dxscale = 30.0f;
    float quantityDissipation = 1.0f;
    float velocityDissipation = 0.97f;
    float pressureDissipation = 0.8f;
    int jacobiIterations = 20;

    Target divergenceTarget;
    Target pressureTarget;
    Target velocityTarget;
    Target vorticityTarget;
    Target quantityTarget;

    Shader advectionShader;
    Shader divergenceShader;
    Shader vorticityShader;
    Shader vorticityForceShader;
    Shader pressureShader;
    Shader pressureGradientShader;
    Shader multiplyShader;

    Shader disturbShader;

    GLuint bg;

    Fluid(int fWidth, int fHeight, int dWidth, int dHeight) : fWidth(fWidth), fHeight(fHeight), dWidth(dWidth), dHeight(dHeight),
                                                              divergenceTarget(fWidth, fHeight, GL_R16F, GL_R16, GL_NEAREST),
                                                              pressureTarget(fWidth, fHeight, GL_R16F, GL_R16, GL_NEAREST),
                                                              velocityTarget(fWidth, fHeight, GL_RG16F, GL_RG16, GL_LINEAR),
                                                              vorticityTarget(fWidth, fHeight, GL_R16F, GL_R16, GL_NEAREST),
                                                              quantityTarget(dWidth, dWidth, GL_RGBA16F, GL_RGBA, GL_LINEAR),
                                                              advectionShader("shaders/vector.vs", "shaders/advection.fs"),
                                                              divergenceShader("shaders/field.vs", "shaders/divergence.fs"),
                                                              vorticityShader("shaders/field.vs", "shaders/vorticity.fs"),
                                                              vorticityForceShader("shaders/field.vs", "shaders/vorticityForce.fs"),
                                                              pressureShader("shaders/field.vs", "shaders/pressure.fs"),
                                                              pressureGradientShader("shaders/field.vs", "shaders/pressureGradient.fs"),
                                                              multiplyShader("shaders/vector.vs", "shaders/multiply.fs"),
                                                              disturbShader("shaders/vector.vs", "shaders/disturb.fs")
    {
    }
    void process(double dt)
    {
        glDisable(GL_BLEND);
        pipeline(dt);
    }

    void disturb(float x, float y, float dx, float dy, glm::vec3 color)
    {
        glDisable(GL_BLEND);
        glViewport(0, 0, fWidth, fHeight);
        glm::vec2 st(1.0 / fWidth, 1.0 / fHeight);

        glUseProgram(disturbShader.id);
        disturbShader.setUniform("quantity", velocityTarget.bind(0));
        disturbShader.setUniform("aspect", (float)gWidth / gHeight);
        disturbShader.setUniform("position", glm::vec2(x, y));
        disturbShader.setUniform("dir", glm::vec3(dx, dy, 1));
        disturbShader.setUniform("radius", 0.5f / 100);
        stage(velocityTarget);

        glViewport(0, 0, dWidth, dHeight);
        glm::vec2 dst(1.0 / dWidth, 1.0 / dHeight);

        glUseProgram(disturbShader.id);
        disturbShader.setUniform("quantity", quantityTarget.bind(0));
        disturbShader.setUniform("dir", color);
        stage(quantityTarget);
    }

private:
    void pipeline(double dt)
    {
        glViewport(0, 0, fWidth, fHeight);
        glm::vec2 st(1.0f / fWidth, 1.0f / fHeight);

        // glUseProgram(vorticityShader.id);
        // vorticityShader.setUniform("st", st);
        // vorticityShader.setUniform("velocity", velocityTarget.bind(0));
        // stage(vorticityTarget);

        // glUseProgram(vorticityForceShader.id);
        // vorticityForceShader.setUniform("st", st);
        // vorticityForceShader.setUniform("velocity", velocityTarget.bind(0));
        // vorticityForceShader.setUniform("vorticity", vorticityTarget.bind(1));
        // vorticityForceShader.setUniform("dxscale", dxscale);
        // vorticityForceShader.setUniform("dt", dt);
        // stage(velocityTarget);

        glUseProgram(divergenceShader.id);
        divergenceShader.setUniform("st", st);
        divergenceShader.setUniform("velocity", velocityTarget.bind(0));
        stage(divergenceTarget);

        // glUseProgram(multiplyShader.id);
        // multiplyShader.setUniform("val", pressureDissipation);
        // multiplyShader.setUniform("field", pressureTarget.bind(0));
        // stage(pressureTarget);

        glUseProgram(pressureShader.id);
        pressureShader.setUniform("st", st);
        pressureShader.setUniform("divergence", divergenceTarget.bind(0));
        for (int i = 0; i < jacobiIterations; ++i)
        {
            pressureShader.setUniform("pressure", pressureTarget.bind(1));
            stage(pressureTarget);
        }

        glUseProgram(pressureGradientShader.id);
        pressureGradientShader.setUniform("st", st);
        pressureGradientShader.setUniform("pressure", pressureTarget.bind(0));
        pressureGradientShader.setUniform("velocity", velocityTarget.bind(1));
        stage(velocityTarget);

        glUseProgram(advectionShader.id);
        advectionShader.setUniform("st", st);
        velocityTarget.bind(0);
        advectionShader.setUniform("velocity", (GLuint)0);
        advectionShader.setUniform("quantity", (GLuint)0);
        advectionShader.setUniform("dt", dt);
        advectionShader.setUniform("dissipation", velocityDissipation);
        stage(velocityTarget);

        glViewport(0, 0, dWidth, dHeight);
        glm::vec2 dst(1.0 / dWidth, 1.0 / dHeight);

        glUseProgram(advectionShader.id);
        advectionShader.setUniform("st", dst);
        advectionShader.setUniform("velocity", velocityTarget.bind(0));
        advectionShader.setUniform("quantity", quantityTarget.bind(1));
        advectionShader.setUniform("dt", dt);
        advectionShader.setUniform("dissipation", quantityDissipation);
        stage(quantityTarget);
    }

    void stage(Target& target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target.targetFbo());
        drawQuad();
        target.swap();
    }
};

int main(void)
{
    int width = 1024;
    int height = 768;
    auto* window = initWindow(width, height);
    if (window == nullptr)
        return -1;

    initGL();

    GLuint screenProgramID = LoadShaders("shaders/vector.vs", "shaders/screen.fs");
    GLuint screenTextureID = glGetUniformLocation(screenProgramID, "renderedTexture");

    Fluid fluid(fluidGrid, fluidGrid, dyeGrid, dyeGrid);
    fluid.disturb(0.5f, 0.5f, 0, 400, glm::vec3(1.0, 1.0, 0));
    fluid.disturb(0.2f, 0.3f, 200, -400, glm::vec3(1.0, 1.0, 0));
    fluid.disturb(0.7f, 0.8f, 300, 0, glm::vec3(1.0, 1.0, 0));
    fluid.disturb(0.6f, 0.8f, 350, 0, glm::vec3(1.0, 1.0, 0));
    fluid.disturb(0.5f, 0.8f, 300, 20, glm::vec3(1.0, 1.0, 0));
    auto lastTime = glfwGetTime();
    int nbFrames = 0;
    do
    {
        auto currentTime = glfwGetTime();
        nbFrames++;
        if (currentTime - lastTime >= 1.0)
        { // If last prinf() was more than 1 sec ago
            // printf and reset timer
            printf("%f ms/frame\n", 1000.0 / double(nbFrames));
            nbFrames = 0;
            lastTime += 1.0;
        }

        fluid.process(0.0016);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(screenProgramID);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fluid.quantityTarget.texture);
        glUniform1i(screenTextureID, 0);
        drawQuad();

        glfwSwapInterval(1);
        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
             glfwWindowShouldClose(window) == 0);
    glfwTerminate();

    return 0;
}
