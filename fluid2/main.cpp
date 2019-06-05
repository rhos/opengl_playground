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

int width = 1024;
int height = 768;

struct Config
{
    float SIM_RESOLUTION = 128;
    float DYE_RESOLUTION = 512;
    float DENSITY_DISSIPATION = 0.97;
    float VELOCITY_DISSIPATION = 0.98;
    float PRESSURE_DISSIPATION = 0.8;
    float PRESSURE_ITERATIONS = 20;
    float CURL = 30;
    float SPLAT_RADIUS = 0.5;
} config;

struct GLProgram
{
    GLuint id;
    std::map<std::string, GLuint> uniforms;
    GLProgram() {}
    GLProgram(const std::string& vertexShader, const std::string& fragmentShader)
    {
        id = LoadShadersCode(vertexShader, fragmentShader);

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

    void bind()
    {
        glUseProgram(id);
    }
};

const std::string baseVertexShader = R"(
    #version 410
    precision highp float;
    layout(location = 0) in vec2 aPosition;
    out vec2 vUv;
    out vec2 vL;
    out vec2 vR;
    out vec2 vT;
    out vec2 vB;
    uniform vec2 texelSize;
    void main () {
        vUv = aPosition * 0.5 + 0.5;
        vL = vUv - vec2(texelSize.x, 0.0);
        vR = vUv + vec2(texelSize.x, 0.0);
        vT = vUv + vec2(0.0, texelSize.y);
        vB = vUv - vec2(0.0, texelSize.y);
        gl_Position = vec4(aPosition, 0.0, 1.0);
    }
)";

const std::string clearShader = R"(
    #version 410
    layout (location = 0) out vec4 color;
    precision mediump float;
    precision mediump sampler2D;
    in highp vec2 vUv;
    uniform sampler2D uTexture;
    uniform float value;
    void main () {
        color = value * texture(uTexture, vUv);
    }
)";

const std::string splatShader = R"(
    #version 410
    layout (location = 0) out vec4 color_out;
    precision highp float;
    precision highp sampler2D;
    in vec2 vUv;
    uniform sampler2D uTarget;
    uniform float aspectRatio;
    uniform vec3 color;
    uniform vec2 point;
    uniform float radius;
    void main () {
        vec2 p = vUv - point.xy;
        p.x *= aspectRatio;
        vec3 splat = exp(-dot(p, p) / radius) * color;
        vec3 base = texture(uTarget, vUv).xyz;
        color_out = vec4(base + splat, 1.0);
    }
)";

const std::string advectionShader = R"(
    #version 410
    layout (location = 0) out vec4 color;
    precision highp float;
    precision highp sampler2D;
    in vec2 vUv;
    uniform sampler2D uVelocity;
    uniform sampler2D uSource;
    uniform vec2 texelSize;
    uniform float dt;
    uniform float dissipation;
    void main () {
        vec2 coord = vUv - dt * texture(uVelocity, vUv).xy * texelSize;
        color = dissipation * texture(uSource, coord);
        color.a = 1.0;
    }
)";

const std::string divergenceShader = R"(
    #version 410
    layout (location = 0) out vec4 color;
    precision mediump float;
    precision mediump sampler2D;
    in highp vec2 vUv;
    in highp vec2 vL;
    in highp vec2 vR;
    in highp vec2 vT;
    in highp vec2 vB;
    uniform sampler2D uVelocity;
    void main () {
        float L = texture(uVelocity, vL).x;
        float R = texture(uVelocity, vR).x;
        float T = texture(uVelocity, vT).y;
        float B = texture(uVelocity, vB).y;
        vec2 C = texture(uVelocity, vUv).xy;
        if (vL.x < 0.0) { L = -C.x; }
        if (vR.x > 1.0) { R = -C.x; }
        if (vT.y > 1.0) { T = -C.y; }
        if (vB.y < 0.0) { B = -C.y; }
        float div = 0.5 * (R - L + T - B);
        color = vec4(div, 0.0, 0.0, 1.0);
        //color = vec4(vUv, 0, 1);
    }
)";

const std::string curlShader = R"(
    #version 410
    layout (location = 0) out vec4 color;
    precision mediump float;
    precision mediump sampler2D;
    in highp vec2 vUv;
    in highp vec2 vL;
    in highp vec2 vR;
    in highp vec2 vT;
    in highp vec2 vB;
    uniform sampler2D uVelocity;
    void main () {
        float L = texture(uVelocity, vL).y;
        float R = texture(uVelocity, vR).y;
        float T = texture(uVelocity, vT).x;
        float B = texture(uVelocity, vB).x;
        float vorticity = R - L - T + B;
        color = vec4(0.5 * vorticity, 0.0, 0.0, 1.0);
    }
)";

const std::string vorticityShader = R"(
    #version 410
    layout (location = 0) out vec4 color;
    precision highp float;
    precision highp sampler2D;
    in vec2 vUv;
    in vec2 vL;
    in vec2 vR;
    in vec2 vT;
    in vec2 vB;
    uniform sampler2D uVelocity;
    uniform sampler2D uCurl;
    uniform float curl;
    uniform float dt;
    void main () {
        float L = texture(uCurl, vL).x;
        float R = texture(uCurl, vR).x;
        float T = texture(uCurl, vT).x;
        float B = texture(uCurl, vB).x;
        float C = texture(uCurl, vUv).x;
        vec2 force = 0.5 * vec2(abs(T) - abs(B), abs(R) - abs(L));
        force /= length(force) + 0.0001;
        force *= curl * C;
        force.y *= -1.0;
        vec2 vel = texture(uVelocity, vUv).xy;
        color = vec4(vel + force * dt, 0.0, 1.0);
    }
)";

const std::string pressureShader = R"(
    #version 410
    layout (location = 0) out vec4 color;
    precision mediump float;
    precision mediump sampler2D;
    in highp vec2 vUv;
    in highp vec2 vL;
    in highp vec2 vR;
    in highp vec2 vT;
    in highp vec2 vB;
    uniform sampler2D uPressure;
    uniform sampler2D uDivergence;
    vec2 boundary (vec2 uv) {
        return uv;
        // uncomment if you use wrap or repeat texture mode
        // uv = min(max(uv, 0.0), 1.0);
        // return uv;
    }
    void main () {
        float L = texture(uPressure, boundary(vL)).x;
        float R = texture(uPressure, boundary(vR)).x;
        float T = texture(uPressure, boundary(vT)).x;
        float B = texture(uPressure, boundary(vB)).x;
        float C = texture(uPressure, vUv).x;
        float divergence = texture(uDivergence, vUv).x;
        float pressure = (L + R + B + T - divergence) * 0.25;
        color = vec4(pressure, 0.0, 0.0, 1.0);
    }
)";

const std::string gradientSubtractShader = R"(
    #version 410
    layout (location = 0) out vec4 color;
    precision mediump float;
    precision mediump sampler2D;
    in highp vec2 vUv;
    in highp vec2 vL;
    in highp vec2 vR;
    in highp vec2 vT;
    in highp vec2 vB;
    uniform sampler2D uPressure;
    uniform sampler2D uVelocity;
    vec2 boundary (vec2 uv) {
        return uv;
        // uv = min(max(uv, 0.0), 1.0);
        // return uv;
    }
    void main () {
        float L = texture(uPressure, boundary(vL)).x;
        float R = texture(uPressure, boundary(vR)).x;
        float T = texture(uPressure, boundary(vT)).x;
        float B = texture(uPressure, boundary(vB)).x;
        vec2 velocity = texture(uVelocity, vUv).xy;
        velocity.xy -= vec2(R - L, T - B);
        color = vec4(velocity, 0.0, 1.0);
    }
)";

const std::string displayShader = R"(
    #version 410
    layout (location = 0) out vec4 color;
    precision highp float;
    precision highp sampler2D;
    in vec2 vUv;
    uniform sampler2D uTexture;
    void main () {
        vec3 C = texture(uTexture, vUv).rgb;
        float a = max(C.r, max(C.g, C.b));
        color = vec4(C, a);
        //color = vec4(C, 1);
        //color = vec4(vUv, 0, 1);
        //color = vec4(C, 0, 0, 1);
    }
)";

struct Blit
{
    GLuint quadVAO;
    Blit()
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

    void operator()(GLuint destination)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, destination);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
};

void blit(GLuint destination)
{
    static Blit bl;
    bl(destination);
}

struct FBO
{
    GLuint textureID;
    GLuint bufferID;
    int w;
    int h;
    FBO() {}
    FBO(GLuint textureID, GLuint bufferID, int w, int h) : textureID(textureID), bufferID(bufferID), w(w), h(h)
    {
    }

    GLuint attach(GLuint dest)
    {
        glActiveTexture(GL_TEXTURE0 + dest);
        glBindTexture(GL_TEXTURE_2D, textureID);
        return dest;
    }
};

struct DoubleFBO
{
    FBO fbo1;
    FBO fbo2;

    FBO getRead()
    {
        return fbo1;
    }
    void setRead(FBO fbo)
    {
        fbo1 = fbo;
    }

    FBO getWrite()
    {
        return fbo2;
    }
    void setWrite(FBO fbo)
    {
        fbo2 = fbo;
    }
    void swap()
    {
        auto temp = fbo1;
        fbo1 = fbo2;
        fbo2 = temp;
    }
};
float simWidth;
float simHeight;
float dyeWidth;
float dyeHeight;
DoubleFBO density;
DoubleFBO velocity;
FBO divergence;
FBO curl;
DoubleFBO pressure;

GLProgram clearProgram;
GLProgram splatProgram;
GLProgram advectionProgram;
GLProgram divergenceProgram;
GLProgram curlProgram;
GLProgram vorticityProgram;
GLProgram pressureProgram;
GLProgram gradienSubtractProgram;
GLProgram displayProgram;

std::pair<float, float> getResolution(float resolution)
{
    float aspectRatio = (float)width / height;
    if (aspectRatio < 1)
        aspectRatio = 1.0 / aspectRatio;

    float max = resolution * aspectRatio;
    float min = resolution;

    if (width > height)
        return {max, min};
    else
        return {min, max};
}

FBO createFBO(int w, int h, GLuint internalFormat, GLuint format, GLuint type, GLuint param)
{
    glActiveTexture(GL_TEXTURE0);
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, param);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, param);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, nullptr);

    GLuint bufferID;
    glGenFramebuffers(1, &bufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, bufferID);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureID, 0);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    return FBO(textureID, bufferID, w, h);
}

DoubleFBO createDoubleFBO(int w, int h, GLuint internalFormat, GLuint format, GLuint type, GLuint param)
{
    auto fbo1 = createFBO(w, h, internalFormat, format, type, param);
    auto fbo2 = createFBO(w, h, internalFormat, format, type, param);

    DoubleFBO fbo;
    fbo.fbo1 = fbo1;
    fbo.fbo2 = fbo2;
    return fbo;
}

void initFramebuffers()
{
    auto [asimWidth, asimHeight] = getResolution(config.SIM_RESOLUTION);
    auto [adyeWidth, adyeHeight] = getResolution(config.DYE_RESOLUTION);

    simWidth = asimWidth;
    simHeight = asimHeight;
    dyeWidth = adyeWidth;
    dyeHeight = adyeHeight;

    auto texType = GL_HALF_FLOAT;

    density = createDoubleFBO(dyeWidth, dyeHeight, GL_RGBA16F, GL_RGBA, texType, GL_LINEAR);
    velocity = createDoubleFBO(simWidth, simHeight, GL_RG16F, GL_RG, texType, GL_LINEAR);
    divergence = createFBO(simWidth, simHeight, GL_RG16F, GL_RG, texType, GL_NEAREST);
    curl = createFBO(simWidth, simHeight, GL_RG16F, GL_RG, texType, GL_NEAREST);
    pressure = createDoubleFBO(simWidth, simHeight, GL_RG16F, GL_RG, texType, GL_NEAREST);
}

void step(float dt)
{
    glDisable(GL_BLEND);
    glViewport(0, 0, simWidth, simHeight);

    curlProgram.bind();
    glUniform2f(curlProgram.uniforms["texelSize"], 1.0 / simWidth, 1.0 / simHeight);
    glUniform1i(curlProgram.uniforms["uVelocity"], velocity.getRead().attach(0));
    blit(curl.bufferID);

    vorticityProgram.bind();
    glUniform2f(vorticityProgram.uniforms["texelSize"], 1.0 / simWidth, 1.0 / simHeight);
    glUniform1i(vorticityProgram.uniforms["uVelocity"], velocity.getRead().attach(0));
    glUniform1i(vorticityProgram.uniforms["uCurl"], curl.attach(1));
    glUniform1f(vorticityProgram.uniforms["curl"], config.CURL);
    glUniform1f(vorticityProgram.uniforms["dt"], dt);
    blit(velocity.getWrite().bufferID);
    velocity.swap();

    divergenceProgram.bind();
    glUniform2f(divergenceProgram.uniforms["texelSize"], 1.0 / simWidth, 1.0 / simHeight);
    glUniform1i(divergenceProgram.uniforms["uVelocity"], velocity.getRead().attach(0));
    blit(divergence.bufferID);
    
    clearProgram.bind();
    glUniform1i(clearProgram.uniforms["uTexture"], pressure.getRead().attach(0));
    glUniform1f(clearProgram.uniforms["value"], config.PRESSURE_DISSIPATION);
    blit(pressure.getWrite().bufferID);
    pressure.swap();

    pressureProgram.bind();
    glUniform2f(pressureProgram.uniforms["texelSize"], 1.0 / simWidth, 1.0 / simHeight);
    glUniform1i(pressureProgram.uniforms["uDivergence"], divergence.attach(0));
    for (int i = 0; i < config.PRESSURE_ITERATIONS; i++) {
        glUniform1i(pressureProgram.uniforms["uPressure"], pressure.getRead().attach(1));
        blit(pressure.getWrite().bufferID);
        pressure.swap();
    }

    gradienSubtractProgram.bind();
    glUniform2f(gradienSubtractProgram.uniforms["texelSize"], 1.0 / simWidth, 1.0 / simHeight);
    glUniform1i(gradienSubtractProgram.uniforms["uPressure"], pressure.getRead().attach(0));
    glUniform1i(gradienSubtractProgram.uniforms["uVelocity"], velocity.getRead().attach(1));
    blit(velocity.getWrite().bufferID);
    velocity.swap();

    advectionProgram.bind();
    glUniform2f(advectionProgram.uniforms["texelSize"], 1.0 / simWidth, 1.0 / simHeight);
    auto velocityId = velocity.getRead().attach(0);
    glUniform1i(advectionProgram.uniforms["uVelocity"], velocityId);
    glUniform1i(advectionProgram.uniforms["uSource"], velocityId);
    glUniform1f(advectionProgram.uniforms["dt"], dt);
    glUniform1f(advectionProgram.uniforms["dissipation"], config.VELOCITY_DISSIPATION);
    blit(velocity.getWrite().bufferID);
    velocity.swap();

    glViewport(0, 0, dyeWidth, dyeHeight);

    glUniform1i(advectionProgram.uniforms["uVelocity"], velocity.getRead().attach(0));
    glUniform1i(advectionProgram.uniforms["uSource"], density.getRead().attach(1));
    glUniform1f(advectionProgram.uniforms["dissipation"], config.DENSITY_DISSIPATION);
    blit(density.getWrite().bufferID);
    density.swap();
}
void render()
{
    glDisable(GL_BLEND);

    glViewport(0, 0, width, height);
    displayProgram.bind();
    glUniform1f(displayProgram.uniforms["uTexture"], density.getRead().attach(0));
    //glUniform1f(displayProgram.uniforms["uTexture"], velocity.getRead().attach(0));
    //glUniform1f(displayProgram.uniforms["uTexture"], divergence.attach(0));
    blit(0);
}

void update()
{
    step(0.016);
    render();
}

void splat(int x, int y, float dx, float dy, glm::vec3 color)
{
    glViewport(0, 0, simWidth, simHeight);
    splatProgram.bind();
    glUniform1i(splatProgram.uniforms["uTarget"], velocity.getRead().attach(0));
    glUniform1f(splatProgram.uniforms["aspectRatio"], (float)width / (float)height);
    glUniform2f(splatProgram.uniforms["point"], x / (float)width, 1.0 - y / (float)height);
    glUniform3f(splatProgram.uniforms["color"], dx, -dy, 1.0);
    glUniform1f(splatProgram.uniforms["radius"], config.SPLAT_RADIUS / 100.0);
    blit(velocity.getWrite().bufferID);
    velocity.swap();

    glViewport(0, 0, dyeWidth, dyeHeight);
    glUniform1i(splatProgram.uniforms["uTarget"], density.getRead().attach(0));
    glUniform3f(splatProgram.uniforms["color"], color.r, color.g, color.b);
    blit(density.getWrite().bufferID);
    density.swap();
}

glm::vec3 HSVtoRGB(float h, float s, float v)
{
    int r, g, b, i, f, p, q, t;
    i = std::floor(h * 6);
    f = h * 6 - i;
    p = v * (1 - s);
    q = v * (1 - f * s);
    t = v * (1 - (1 - f) * s);

    switch (i % 6)
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

    return {
        r,
        g,
        b};
}

glm::vec3 generateColor()
{
    auto c = HSVtoRGB(rand() / double(RAND_MAX), 1.0, 1.0);
    c.r *= 0.15;
    c.g *= 0.15;
    c.b *= 0.15;
    return c;
}

void multipleSplats(int amount)
{
    for (int i = 0; i < amount; i++)
    {
        auto color = generateColor();
        color.r *= 10.0;
        color.g *= 10.0;
        color.b *= 10.0;
        int x = width * rand() / double(RAND_MAX);
        int y = height * rand() / double(RAND_MAX);
        float dx = 1000 * (rand() / double(RAND_MAX) - 0.5);
        float dy = 1000 * (rand() / double(RAND_MAX) - 0.5);
        splat(x, y, dx, dy, color);
    }
}

int main(void)
{

    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        getchar();
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Open a window and create its OpenGL context
    auto* window = glfwCreateWindow(1024, 768, "Tutorial 02 - Red triangle", NULL, NULL);
    if (window == NULL)
    {
        fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
        getchar();
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize GLEW\n");
        getchar();
        glfwTerminate();
        return -1;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    // Dark blue background
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    clearProgram = GLProgram(baseVertexShader, clearShader);
    splatProgram = GLProgram(baseVertexShader, splatShader);
    advectionProgram = GLProgram(baseVertexShader, advectionShader);
    divergenceProgram = GLProgram(baseVertexShader, divergenceShader);
    curlProgram = GLProgram(baseVertexShader, curlShader);
    vorticityProgram = GLProgram(baseVertexShader, vorticityShader);
    pressureProgram = GLProgram(baseVertexShader, pressureShader);
    gradienSubtractProgram = GLProgram(baseVertexShader, gradientSubtractShader);
    displayProgram = GLProgram(baseVertexShader, displayShader);

    initFramebuffers();
    multipleSplats(10 + 5);

    do
    {

        // Clear the screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        update();

        // Swap buffers
        glfwSwapInterval(1);
        glfwSwapBuffers(window);
        glfwPollEvents();

    } // Check if the ESC key was pressed or the window was closed
    while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
           glfwWindowShouldClose(window) == 0);
    glfwTerminate();

    return 0;
}
