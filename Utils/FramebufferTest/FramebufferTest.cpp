#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>

int main()
{
    if (!glfwInit()) { printf("glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1600, 900, "FramebufferTest", nullptr, nullptr);
    if (!window) { printf("window failed\n"); return 2; }
    glfwMakeContextCurrent(window);
    int version = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
    printf("glad version=%d\n", version);
    printf("glCreateFramebuffers=%p glCreateTextures=%p glCreateRenderbuffers=%p\n", (void*)glCreateFramebuffers, (void*)glCreateTextures, (void*)glCreateRenderbuffers);

    GLuint fbo=0, color=0, depth=0;
    glCreateFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glCreateTextures(GL_TEXTURE_2D, 1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1280, 720, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    printf("after color: err=0x%X\n", glGetError());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    printf("after attach color: err=0x%X\n", glGetError());
    glCreateRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1280, 720);
    printf("after depth: err=0x%X\n", glGetError());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
    printf("after attach depth: err=0x%X\n", glGetError());
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    printf("status=0x%X (%s)\n", status, status == GL_FRAMEBUFFER_COMPLETE ? "complete" : "incomplete");
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &color);
    glDeleteRenderbuffers(1, &depth);
    glfwDestroyWindow(window);
    glfwTerminate();
    return status == GL_FRAMEBUFFER_COMPLETE ? 0 : 3;
}
