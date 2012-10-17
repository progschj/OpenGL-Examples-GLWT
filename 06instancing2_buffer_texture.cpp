/* OpenGL example code - Instancing with texture buffer
 * 
 * create 8 instances of the cube from the perspective example
 * the difference to the instancing1 example is that we are
 * using a texture buffer for the per instance data instead of a
 * vertex buffer with divisor.
 * 
 * Autor: Jakob Progsch
 */
 
#include <GLXW/glxw.h>
#include <GLWT/glwt.h>
 
//glm is used to create perspective and transform matrices
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> 
 
#include <iostream>
#include <string>
#include <vector>

#include <time.h>
unsigned long long raw_time()
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return (unsigned long long)t.tv_sec * (unsigned long long)1000000000 + (unsigned long long)t.tv_nsec;
}
unsigned long long glwtGetNanoTime()
{
   static unsigned long long base = 0;
   if(base == 0)
      base = raw_time();
   return raw_time() - base;
}

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
        "uniform mat4 ViewProjection;\n" // the projection matrix uniform
        "uniform samplerBuffer offset_texture;\n" // the buffer_texture sampler
        "layout(location = 0) in vec4 vposition;\n"
        "layout(location = 1) in vec4 vcolor;\n"
        "out vec4 fcolor;\n"
        "void main() {\n"
        // access the buffer texture with the InstanceID (tbo[InstanceID])
        "   vec4 offset = texelFetch(offset_texture, gl_InstanceID);\n" 
        "   fcolor = vcolor;\n"
        "   gl_Position = ViewProjection*(vposition + offset);\n"
        "}\n";
        
    std::string fragment_source =
        "#version 330\n"
        "in vec4 fcolor;\n"
        "layout(location = 0) out vec4 FragColor;\n"
        "void main() {\n"
        "   FragColor = fcolor;\n"
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
    
    // obtain location of projection uniform
    GLint ViewProjection_location = glGetUniformLocation(shader_program, "ViewProjection");
    GLint offset_texture_location = glGetUniformLocation(shader_program, "offset_texture");
    
    
    // vao and vbo handles
    GLuint vao, vbo, tbo, ibo;
 
    // generate and bind the vao
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    
    // generate and bind the vertex buffer object
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
            
    // data for a cube
    GLfloat vertexData[] = {
    //  X     Y     Z           R     G     B
    // face 0:
       1.0f, 1.0f, 1.0f,       1.0f, 0.0f, 0.0f, // vertex 0
      -1.0f, 1.0f, 1.0f,       1.0f, 0.0f, 0.0f, // vertex 1
       1.0f,-1.0f, 1.0f,       1.0f, 0.0f, 0.0f, // vertex 2
      -1.0f,-1.0f, 1.0f,       1.0f, 0.0f, 0.0f, // vertex 3
 
    // face 1:
       1.0f, 1.0f, 1.0f,       0.0f, 1.0f, 0.0f, // vertex 0
       1.0f,-1.0f, 1.0f,       0.0f, 1.0f, 0.0f, // vertex 1
       1.0f, 1.0f,-1.0f,       0.0f, 1.0f, 0.0f, // vertex 2
       1.0f,-1.0f,-1.0f,       0.0f, 1.0f, 0.0f, // vertex 3
 
    // face 2:
       1.0f, 1.0f, 1.0f,       0.0f, 0.0f, 1.0f, // vertex 0
       1.0f, 1.0f,-1.0f,       0.0f, 0.0f, 1.0f, // vertex 1
      -1.0f, 1.0f, 1.0f,       0.0f, 0.0f, 1.0f, // vertex 2
      -1.0f, 1.0f,-1.0f,       0.0f, 0.0f, 1.0f, // vertex 3
      
    // face 3:
       1.0f, 1.0f,-1.0f,       1.0f, 1.0f, 0.0f, // vertex 0
       1.0f,-1.0f,-1.0f,       1.0f, 1.0f, 0.0f, // vertex 1
      -1.0f, 1.0f,-1.0f,       1.0f, 1.0f, 0.0f, // vertex 2
      -1.0f,-1.0f,-1.0f,       1.0f, 1.0f, 0.0f, // vertex 3
 
    // face 4:
      -1.0f, 1.0f, 1.0f,       0.0f, 1.0f, 1.0f, // vertex 0
      -1.0f, 1.0f,-1.0f,       0.0f, 1.0f, 1.0f, // vertex 1
      -1.0f,-1.0f, 1.0f,       0.0f, 1.0f, 1.0f, // vertex 2
      -1.0f,-1.0f,-1.0f,       0.0f, 1.0f, 1.0f, // vertex 3
 
    // face 5:
       1.0f,-1.0f, 1.0f,       1.0f, 0.0f, 1.0f, // vertex 0
      -1.0f,-1.0f, 1.0f,       1.0f, 0.0f, 1.0f, // vertex 1
       1.0f,-1.0f,-1.0f,       1.0f, 0.0f, 1.0f, // vertex 2
      -1.0f,-1.0f,-1.0f,       1.0f, 0.0f, 1.0f, // vertex 3
    }; // 6 faces with 4 vertices with 6 components (floats)
 
    // fill with data
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*6*4*6, vertexData, GL_STATIC_DRAW);
                    
           
    // set up generic attrib pointers
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(GLfloat), (char*)0 + 0*sizeof(GLfloat));
 
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(GLfloat), (char*)0 + 3*sizeof(GLfloat));
 
    
    // generate and bind the index buffer object
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            
    GLuint indexData[] = {
        // face 0:
        0,1,2,      // first triangle
        2,1,3,      // second triangle
        // face 1:
        4,5,6,      // first triangle
        6,5,7,      // second triangle
        // face 2:
        8,9,10,     // first triangle
        10,9,11,    // second triangle
        // face 3:
        12,13,14,   // first triangle
        14,13,15,   // second triangle
        // face 4:
        16,17,18,   // first triangle
        18,17,19,   // second triangle
        // face 5:
        20,21,22,   // first triangle
        22,21,23,   // second triangle
    };
 
    // fill with data
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*6*2*3, indexData, GL_STATIC_DRAW);
    
    // "unbind" vao
    glBindVertexArray(0);
    

    // generate and bind the buffer object containing the
    // instance offsets
    glGenBuffers(1, &tbo);
    glBindBuffer(GL_TEXTURE_BUFFER, tbo);
            
    // the offsets
    GLfloat translationData[] = {
                 2.0f, 2.0f, 2.0f, 0.0f,  // cube 0
                 2.0f, 2.0f,-2.0f, 0.0f,  // cube 1
                 2.0f,-2.0f, 2.0f, 0.0f,  // cube 2
                 2.0f,-2.0f,-2.0f, 0.0f,  // cube 3
                -2.0f, 2.0f, 2.0f, 0.0f,  // cube 4
                -2.0f, 2.0f,-2.0f, 0.0f,  // cube 5
                -2.0f,-2.0f, 2.0f, 0.0f,  // cube 6
                -2.0f,-2.0f,-2.0f, 0.0f,  // cube 7
    }; // 8 offsets with 3 components each
 
    // fill with data
    glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat)*4*8, translationData, GL_STATIC_DRAW);

    // texture handle
    GLuint buffer_texture;
    
    // generate and bind the buffer texture
    glGenTextures(1, &buffer_texture);
    glBindTexture(GL_TEXTURE_BUFFER, buffer_texture);
    
    // tell the buffer texture to use 
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, tbo);
    
    
    // we are drawing 3d objects so we want depth testing
    glEnable(GL_DEPTH_TEST);
 
 
    while(userdata.running)
    {   
        // get the time in seconds
        float t = glwtGetNanoTime()*1.e-9f;
        
        // update events
        glwtEventHandle(0);
        
        // clear first
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // use the shader program
        glUseProgram(shader_program);
        
        // calculate ViewProjection matrix
        glm::mat4 Projection = glm::perspective(90.0f, 4.0f / 3.0f, 0.1f, 100.f);
        
        // translate the world/view position
        glm::mat4 View = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
        
        // make the camera rotate around the origin
        View = glm::rotate(View, 90.0f*t, glm::vec3(1.0f, 1.0f, 1.0f)); 
        
        glm::mat4 ViewProjection = Projection*View;

        // bind texture to texture unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_BUFFER, buffer_texture);
         
         
        // set the matrix uniform
        glUniformMatrix4fv(ViewProjection_location, 1, GL_FALSE, glm::value_ptr(ViewProjection)); 
        
        // set texture uniform
        glUniform1i(offset_texture_location, 0);
    
        
        // bind the vao
        glBindVertexArray(vao);


        // draw
        // the additional parameter indicates how many instances to render
        glDrawElementsInstanced(GL_TRIANGLES, 6*6, GL_UNSIGNED_INT, 0, 8);
       
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
        
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    glDeleteBuffers(1, &tbo);
    
    glDeleteTextures(1, &buffer_texture);
    
    glDetachShader(shader_program, vertex_shader);      
    glDetachShader(shader_program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteProgram(shader_program);
    

    glwtWindowDestroy(window);
    glwtQuit();
    return 0;
}

