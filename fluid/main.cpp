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
    // Initialise GLFW
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        getchar();
        return nullptr;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Open a window and create its OpenGL context
    auto* window = glfwCreateWindow(1024, 768, "Fluid", NULL, NULL);
    if (window == NULL)
    {
        fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
        getchar();
        glfwTerminate();
        return nullptr;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize GLEW\n");
        getchar();
        glfwTerminate();
        return nullptr;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    return window;
}

GLuint quadVAO;
void prepareQuad();
void initGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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

private:
    GLuint fboTemp;
    GLuint textureTemp;

    GLuint createTexture(int width, int height, GLuint internalFormat, GLuint format, GLuint filtering)
    {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_FLOAT, 0);

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
    Shader() {}
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

    Shader disturbShader;

    Fluid(int fluidGridW, int fluidGridH, int dyeGridW, int dyeGridH) : fWidth(fluidGridW), fHeight(fluidGridH), dWidth(dyeGridW), dHeight(dyeGridH),
                                                                        velocityTarget(fluidGridW, fluidGridH, GL_RG32F, GL_RG, GL_LINEAR),
                                                                        divergenceTarget(fluidGridW, fluidGridH, GL_RG32F, GL_RG, GL_NEAREST),
                                                                        pressureTarget(fWidth, fHeight, GL_RG32F, GL_RG, GL_NEAREST),
                                                                        vorticityTarget(fWidth, fHeight, GL_RG32F, GL_RG, GL_NEAREST),
                                                                        quantityTarget(dWidth, dWidth, GL_RGBA32F, GL_RGBA, GL_LINEAR),
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

    void pipeline(float dt)
    {
        glDisable(GL_BLEND);
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

        // glUseProgram(divergenceShader.id);
        // divergenceShader.setUniform("st", st);
        // divergenceShader.setUniform("velocity", velocityTarget.bind(0));
        // stage(divergenceTarget);

        // glUseProgram(multiplyShader.id);
        // multiplyShader.setUniform("val", pressureDissipation);
        // multiplyShader.setUniform("field", pressureTarget.bind(0));
        // stage(pressureTarget);

        // glUseProgram(pressureShader.id);
        // pressureShader.setUniform("st", st);
        // pressureShader.setUniform("divergence", divergenceTarget.bind(0));
        // for (int i = 0; i < jacobiIterations; ++i)
        // {
        //     pressureShader.setUniform("pressure", pressureTarget.bind(1));
        //     stage(pressureTarget);
        // }

        // glUseProgram(pressureGradientShader.id);
        // pressureGradientShader.setUniform("st", st);
        // pressureGradientShader.setUniform("pressure", pressureTarget.bind(0));
        // pressureGradientShader.setUniform("velocity", velocityTarget.bind(1));
        // stage(velocityTarget);

        glUseProgram(advectionShader.id);
        advectionShader.setUniform("st", st);
        auto velocityID = velocityTarget.bind(0);
        advectionShader.setUniform("velocity", velocityID);
        advectionShader.setUniform("quantity", velocityID);
        advectionShader.setUniform("dt", dt);
        advectionShader.setUniform("dissipation", velocityDissipation);
        stage(velocityTarget);

        // glViewport(0, 0, dWidth, dHeight);
        // glm::vec2 dst(1.0 / dWidth, 1.0 / dHeight);

        // glUseProgram(advectionShader.id);
        // advectionShader.setUniform("st", dst);
        // advectionShader.setUniform("velocity", velocityTarget.bind(0));
        // advectionShader.setUniform("quantity", quantityTarget.bind(1));
        // advectionShader.setUniform("dt", dt);
        // advectionShader.setUniform("dissipation", quantityDissipation);
        // stage(quantityTarget);
    }

    void disturb(float x, float y, float dx, float dy, glm::vec3 color)
    {
        glDisable(GL_BLEND);
        glViewport(0, 0, fWidth, fHeight);
        glm::vec2 st(1.0 / fWidth, 1.0 / fHeight);

        glUseProgram(disturbShader.id);
        disturbShader.setUniform("quantity", velocityTarget.bind(0));
        disturbShader.setUniform("aspect", (float)gWidth / (float)gHeight);
        disturbShader.setUniform("position", glm::vec2(x / (float)gWidth, 1.0 - y / (float)gHeight));
        disturbShader.setUniform("dir", glm::vec3(dx, -dy, 1));
        disturbShader.setUniform("radius", 0.5f / 100);
        stage(velocityTarget);

        glViewport(0, 0, dWidth, dHeight);
        glm::vec2 dst(1.0 / dWidth, 1.0 / dHeight);

        glUseProgram(disturbShader.id);
        disturbShader.setUniform("quantity", quantityTarget.bind(0));
        disturbShader.setUniform("dir", color);
        stage(quantityTarget);
    }

    void randomDisturb(int amount)
    {
        for (int i = 0; i < amount; i++)
        {
            auto color = randomColor();
            color.r *= 10.0f;
            color.g *= 10.0f;
            color.b *= 10.0f;
            float x = gWidth * rand() / float(RAND_MAX);
            float y = gHeight * rand() / float(RAND_MAX);
            float dx = 1000 * (rand() / float(RAND_MAX) - 0.5f);
            float dy = 1000 * (rand() / float(RAND_MAX) - 0.5f);
            disturb(x, y, dx, dy, color);
        }
    }

    void stage(Target& target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target.targetFbo());
        drawQuad();
        target.swap();
    }

private:
    glm::vec3 HSVtoRGB(float h, float s, float v)
    {
        float r, g, b, i, f, p, q, t;
        i = std::floor(h * 6);
        f = h * 6 - i;
        p = v * (1 - s);
        q = v * (1 - f * s);
        t = v * (1 - (1 - f) * s);

        switch ((int)i % 6)
        {
        case 0:
            r = v, g = t, b = p;
            break;
        case 1:
            r = q, g = v, b = p;
            break;
        case 2:
            r = p, g = v, b = t;
            break;
        case 3:
            r = p, g = q, b = v;
            break;
        case 4:
            r = t, g = p, b = v;
            break;
        case 5:
            r = v, g = p, b = q;
            break;
        }
        return {r, g, b};
    }

    glm::vec3 randomColor()
    {
        auto c = HSVtoRGB(rand() / float(RAND_MAX), 1.0f, 1.0f);
        c.r *= 0.15f;
        c.g *= 0.15f;
        c.b *= 0.15f;
        return c;
    }
};

Shader renderTextureShader;
void renderTexture(GLuint textureID)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_BLEND);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, gWidth, gHeight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glUseProgram(renderTextureShader.id);
    renderTextureShader.setUniform("renderedTexture", (GLuint)0);
    drawQuad();
}

int main(void)
{
    int seed = 131;
    srand(seed);

    auto* window = initWindow(gWidth, gHeight);
    if (window == nullptr)
        return -1;

    initGL();

    renderTextureShader = Shader("shaders/vector.vs", "shaders/screen.fs");
    auto bg = loadDDS("data/bg.dds");

    Fluid fluid{(int)(fluidGrid * (float)gWidth / (float)gHeight), fluidGrid, (int)(dyeGrid * (float)gWidth / (float)gHeight), dyeGrid};
    fluid.randomDisturb(15);

    auto lastTime = glfwGetTime();
    int nbFrames = 0;
    do
    {
        auto currentTime = glfwGetTime();
        nbFrames++;
        if (currentTime - lastTime >= 1.0)
        {
            printf("%f ms/frame\n", 1000.0 / double(nbFrames));
            nbFrames = 0;
            lastTime += 1.0;
        }

        fluid.pipeline(0.0016f);
        renderTexture(fluid.velocityTarget.texture);
        //renderTexture(bg);

        glfwSwapInterval(1);
        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
             glfwWindowShouldClose(window) == 0);
    glfwTerminate();

    return 0;
}
