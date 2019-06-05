#ifndef SHADER_HPP
#define SHADER_HPP

GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path);
GLuint LoadShadersCode(const std::string& VertexShaderCode,const std::string& FragmentShaderCode);
#endif
