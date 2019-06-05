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
const int fluidGrid = 200;
const int dyeGrid = 600;

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
            1.0f, -1.0f,
            -1.0f, -1.0f,
            -1.0f, 1.0f,
            1.0f, 1.0f,

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

    Target(int width, int height)
    {
        texture = createTexture(width, height);
        fbo = createFbo(width, height, texture);

        textureTemp = createTexture(width, height);
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

private:
    GLuint fboTemp;
    GLuint textureTemp;

    GLuint createTexture(int width, int height)
    {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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
    float quantityDissipation = 0.97f;
    float velocityDissipation = 0.98f;
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

    Fluid(int fWidth, int fHeight, int dWidth, int dHeight) : fWidth(fWidth), fHeight(fHeight), dWidth(dWidth), dHeight(dHeight),
                                                              divergenceTarget(fWidth, fHeight),
                                                              pressureTarget(fWidth, fHeight),
                                                              velocityTarget(fWidth, fHeight),
                                                              vorticityTarget(fWidth, fHeight),
                                                              quantityTarget(dWidth, dWidth),
                                                              advectionShader("shaders/vector.vs", "shaders/advection.fs"),
                                                              divergenceShader("shaders/field.vs", "shaders/divergence.fs"),
                                                              vorticityShader("shaders/field.vs", "shaders/vorticity.fs"),
                                                              vorticityForceShader("shaders/field.vs", "shaders/vorticityForce.fs"),
                                                              pressureShader("shaders/field.vs", "shaders/pressure.fs"),
                                                              pressureGradientShader("shaders/field.vs", "shaders/pressureGradient.fs"),
                                                              multiplyShader("shaders/vector.vs", "shaders/multiplyShader.fs")
    {
    }
    void process(double dt)
    {
        glDisable(GL_BLEND);
        pipeline(dt);
    }

private:
    void pipeline(double dt)
    {
        glViewport(0, 0, fWidth, fHeight);

        glm::vec2 st(1.0 / fWidth, 1.0 / fHeight);

        vorticityShader.setUniform("st", st);
        vorticityShader.setUniform("velocity", velocityTarget.bind(0));
        stage(vorticityTarget, vorticityShader);

        vorticityForceShader.setUniform("st", st);
        vorticityForceShader.setUniform("velocity", velocityTarget.bind(0));
        vorticityForceShader.setUniform("vorticity", vorticityTarget.bind(1));
        vorticityForceShader.setUniform("dxscale", dxscale);
        vorticityForceShader.setUniform("dt", dt);
        stage(velocityTarget, vorticityForceShader);

        divergenceShader.setUniform("st", st);
        divergenceShader.setUniform("velocity", velocityTarget.bind(0));
        stage(divergenceTarget, divergenceShader);

        multiplyShader.setUniform("val", pressureDissipation);
        multiplyShader.setUniform("field", pressureTarget.bind(0));
        stage(pressureTarget, multiplyShader);

        pressureShader.setUniform("st", st);
        pressureShader.setUniform("divergence", divergenceTarget.bind(0));
        for (int i = 0; i < jacobiIterations; ++i)
        {
            pressureShader.setUniform("pressure", pressureTarget.bind(1));
            stage(pressureTarget, pressureShader);
        }

        pressureGradientShader.setUniform("st", st);
        pressureGradientShader.setUniform("pressure", pressureTarget.bind(0));
        pressureGradientShader.setUniform("velocity", velocityTarget.bind(1));
        stage(velocityTarget, pressureGradientShader);

        advectionShader.setUniform("st", st);
        auto velocityID = velocityTarget.bind(0);
        advectionShader.setUniform("velocity", velocityID);
        advectionShader.setUniform("quantity", velocityID);
        advectionShader.setUniform("dt", dt);
        advectionShader.setUniform("dissipation", velocityDissipation);
        stage(velocityTarget, advectionShader);


        glViewport(0, 0, dWidth, dHeight);

        glm::vec2 dst(1.0 / dWidth, 1.0 / dHeight);
        advectionShader.setUniform("st", dst);
        advectionShader.setUniform("velocity", velocityTarget.bind(0));
        advectionShader.setUniform("quantity", quantityTarget.bind(1));
        advectionShader.setUniform("dt", dt);
        advectionShader.setUniform("dissipation", quantityDissipation);
        stage(quantityTarget, advectionShader);
    }

    void stage(Target& target, Shader& shader)
    {
        glUseProgram(shader.id);
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

    auto last = glfwGetTime();
    do
    {
        auto current = glfwGetTime();
        fluid.process(current - last);
        last = current;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_CULL_FACE);

        glUseProgram(screenProgramID);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fluid.quantityTarget.texture);
        glUniform1i(screenTextureID, 0);
        drawQuad();

        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
             glfwWindowShouldClose(window) == 0);
    glfwTerminate();

    return 0;
}
