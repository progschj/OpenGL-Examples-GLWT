/* OpenGL example code - Texture
 * 
 * apply a texture to the fullscreen quad of "Indexed VBO"
 * 
 * Autor: Jakob Progsch
 */

#include <GLXW/glxw.h>
#include <GLWT/glwt.h>

#include <iostream>
#include <string>
#include <vector>

struct UserData {
    bool running;
};

static void error_callback(const char *msg, void *userdata)
{
    std::cerr << msg << std::endl;
    ((UserData*)userdata)->running = false;
}

static void close_callback(GLWTWindow *window, void *userdata)
{
    (void)window;
    ((UserData*)userdata)->running = false;
}

static void key_callback(GLWTWindow *window, int down, int keysym, int scancode, int mod, void *userdata)
{
    (void)window; (void)down; (void)scancode; (void)mod;
    if(keysym == GLWT_KEY_ESCAPE)
        ((UserData*)userdata)->running = false;
}

// helper to check and display for shader compiler errors
bool check_shader_compile_status(GLuint obj)
{
    GLint status;
    glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE)
    {
        GLint length;
        glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(length);
        glGetShaderInfoLog(obj, length, &length, &log[0]);
        std::cerr << &log[0];
        return false;
    }
    return true;
}

// helper to check and display for shader linker error
bool check_program_link_status(GLuint obj)
{
    GLint status;
    glGetProgramiv(obj, GL_LINK_STATUS, &status);
    if(status == GL_FALSE)
    {
        GLint length;
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(length);
        glGetProgramInfoLog(obj, length, &length, &log[0]);
        std::cerr << &log[0];
        return false;   
    }
    return true;
}

int main()
{
    int width = 640;
    int height = 480;
   
    UserData userdata;
    userdata.running = true;
    
    GLWTConfig glwt_config;
    glwt_config.red_bits = 8;
    glwt_config.green_bits = 8;
    glwt_config.blue_bits = 8;
    glwt_config.alpha_bits = 8;
    glwt_config.depth_bits = 24;
    glwt_config.stencil_bits = 8;
    glwt_config.samples = 0;
    glwt_config.sample_buffers = 0;
    glwt_config.api = GLWT_API_OPENGL | GLWT_PROFILE_CORE;
    glwt_config.api_version_major = 3;
    glwt_config.api_version_minor = 3;
    
    GLWTAppCallbacks app_callbacks;
    app_callbacks.error_callback = error_callback;
    app_callbacks.userdata = &userdata;
    
    if(glwtInit(&glwt_config, &app_callbacks) != 0)
    {
        std::cerr << "failed to init GLWT" << std::endl;
        return 1;
    }
 
    GLWTWindowCallbacks win_callbacks;
    win_callbacks.close_callback = close_callback;
    win_callbacks.expose_callback = 0;
    win_callbacks.resize_callback = 0;
    win_callbacks.show_callback = 0;
    win_callbacks.focus_callback = 0;
    win_callbacks.key_callback = key_callback,
    win_callbacks.motion_callback = 0;
    win_callbacks.button_callback = 0;
    win_callbacks.mouseover_callback = 0;
    win_callbacks.userdata = &userdata;
    
    // create a window
    GLWTWindow *window = glwtWindowCreate("", width, height, &win_callbacks, 0);
    if(window == 0)
    {
        std::cerr << "failed to open window" << std::endl;
        glwtQuit();
        return 1;
    }
    
    if (glxwInit())
    {
        std::cerr << "failed to init GLXW" << std::endl;
        glwtWindowDestroy(window);
        glwtQuit();
        return 1;
    }
    
    glwtWindowShow(window, 1);
    glwtMakeCurrent(window);
    glwtSwapInterval(window, 1);

    // shader source code
    std::string vertex_source =
        "#version 330\n"
        "layout(location = 0) in vec4 vposition;\n"
        "layout(location = 1) in vec2 vtexcoord;\n"
        "out vec2 ftexcoord;\n"
        "void main() {\n"
        "   ftexcoord = vtexcoord;\n"
        "   gl_Position = vposition;\n"
        "}\n";
        
    std::string fragment_source =
        "#version 330\n"
        "uniform sampler2D tex;\n" // texture uniform
        "in vec2 ftexcoord;\n"
        "layout(location = 0) out vec4 FragColor;\n"
        "void main() {\n"
        "   FragColor = texture(tex, ftexcoord);\n"
        "}\n";
   
    // program and shader handles
    GLuint shader_program, vertex_shader, fragment_shader;
    
    // we need these to properly pass the strings
    const char *source;
    int length;

    // create and compiler vertex shader
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    source = vertex_source.c_str();
    length = vertex_source.size();
    glShaderSource(vertex_shader, 1, &source, &length); 
    glCompileShader(vertex_shader);
    if(!check_shader_compile_status(vertex_shader))
    {
        return 1;
    }
 
    // create and compiler fragment shader
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    source = fragment_source.c_str();
    length = fragment_source.size();
    glShaderSource(fragment_shader, 1, &source, &length);   
    glCompileShader(fragment_shader);
    if(!check_shader_compile_status(fragment_shader))
    {
        return 1;
    }
    
    // create program
    shader_program = glCreateProgram();
    
    // attach shaders
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    
    // link the program and check for errors
    glLinkProgram(shader_program);
    check_program_link_status(shader_program);
    
    // get texture uniform location
    GLint texture_location = glGetUniformLocation(shader_program, "tex");
    
    // vao and vbo handle
    GLuint vao, vbo, ibo;
 
    // generate and bind the vao
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    
    // generate and bind the vertex buffer object
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
            
    // data for a fullscreen quad (this time with texture coords)
    GLfloat vertexData[] = {
    //  X     Y     Z           U     V     
       1.0f, 1.0f, 0.0f,       1.0f, 1.0f, // vertex 0
      -1.0f, 1.0f, 0.0f,       0.0f, 1.0f, // vertex 1
       1.0f,-1.0f, 0.0f,       1.0f, 0.0f, // vertex 2
      -1.0f,-1.0f, 0.0f,       0.0f, 0.0f, // vertex 3
    }; // 4 vertices with 5 components (floats) each

    // fill with data
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*4*5, vertexData, GL_STATIC_DRAW);
                    
           
    // set up generic attrib pointers
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), (char*)0 + 0*sizeof(GLfloat));
 
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), (char*)0 + 3*sizeof(GLfloat));
    
    
    // generate and bind the index buffer object
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            
    GLuint indexData[] = {
        0,1,2, // first triangle
        2,1,3, // second triangle
    };

    // fill with data
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*2*3, indexData, GL_STATIC_DRAW);
    
    // "unbind" vao
    glBindVertexArray(0);

    // texture handle
    GLuint texture;
    
    // generate texture
    glGenTextures(1, &texture);

    // bind the texture
    glBindTexture(GL_TEXTURE_2D, texture);

    // create some image data
    std::vector<GLubyte> image(4*width*height);
    for(int j = 0;j<height;++j)
        for(int i = 0;i<width;++i)
        {
            size_t index = j*width + i;
            image[4*index + 0] = 0xFF*(j/10%2)*(i/10%2); // R
            image[4*index + 1] = 0xFF*(j/13%2)*(i/13%2); // G
            image[4*index + 2] = 0xFF*(j/17%2)*(i/17%2); // B
            image[4*index + 3] = 0xFF;                   // A
        }
    
    // set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // set texture content
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &image[0]);
    
    while(userdata.running)
    {
        // update events
        glwtEventHandle(0);
            
        // clear first
        glClear(GL_COLOR_BUFFER_BIT);
        
        // use the shader program
        glUseProgram(shader_program);

        // bind texture to texture unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        
        // set texture uniform
        glUniform1i(texture_location, 0);
        
        // bind the vao
        glBindVertexArray(vao);
        
        // draw
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
       
        // check for errors
        GLenum error = glGetError();
        if(error != GL_NO_ERROR)
        {
            userdata.running = false;       
        }
        
        // finally swap buffers
        glwtSwapBuffers(window);       
    }
    
    // delete the created objects
    
    glDeleteTextures(1, &texture);
    
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    
    glDetachShader(shader_program, vertex_shader);	
    glDetachShader(shader_program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteProgram(shader_program);

    glwtWindowDestroy(window);
    glwtQuit();
    return 0;
}

