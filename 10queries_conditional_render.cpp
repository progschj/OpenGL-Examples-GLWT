/* OpenGL example code - Queries and conditional render
 * 
 * This example renders a "voxel landscape/cave" from the view of a
 * moveable camera. Occlusion queries and conditional rendering are used
 * to cull occluded parts of the world and timer queries are used
 * to measure the performance.
 * 
 * move with WASD keys and mouse use Q and E to "roll"
 * toggle occlusion culling with space
 * 
 * Autor: Jakob Progsch
 */

#include <GLXW/glxw.h>
#include <GLWT/glwt.h>

//glm is used to create perspective and transform matrices
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp> 
#include <glm/gtx/noise.hpp> 

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

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
    bool occlusion_cull;
    struct {
        float up;
        float right;
        float forward;
        float roll;
    } move;
    struct {
        int x, y;
    } mouse;
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

static void key_callback(GLWTWindow *window, int down, int keysym, int scancode, int mod, void *void_userdata)
{
    (void)window; (void)scancode; (void)mod;
    UserData *userdata = (UserData*)void_userdata;
    if(keysym == GLWT_KEY_ESCAPE)
        userdata->running = false;
    
    if(keysym == GLWT_KEY_SPACE && down)
        userdata->occlusion_cull = !userdata->occlusion_cull;
                
    switch(keysym)
    {
        case GLWT_KEY_W:
            userdata->move.forward = down;
            break;
        case GLWT_KEY_S:
            userdata->move.forward = -down;
            break;
        case GLWT_KEY_D:
            userdata->move.right = down;
            break;
        case GLWT_KEY_A:
            userdata->move.right = -down;
            break;
        case GLWT_KEY_Q:
            userdata->move.roll = down;
            break;
        case GLWT_KEY_E:
            userdata->move.roll = -down;
            break;
    }
}

static void motion_callback(GLWTWindow *window, int x, int y, int buttons, void *void_userdata)
{
    (void)window;
    UserData *userdata = (UserData*)void_userdata;
    userdata->mouse.x = x;
    userdata->mouse.y = y;
}

// chunk data structure that contains the information required to
// render and cull the chunks
struct Chunk {
    GLuint vbo, ibo, vao;
    GLuint bounding_vbo, bounding_ibo, bounding_vao;
    GLuint query;
    int quadcount;
    glm::vec3 center;
};

// predicate to allow sorting chunks by distance from a point
class DistancePred {
public:
    DistancePred(glm::vec3 p) : pos(p) { }
    bool operator()(const Chunk &a, const Chunk &b)
    {
        return glm::distance(pos, a.center) < glm::distance(pos, b.center);
    }
private:
    const glm::vec3 pos;
};

// world function that defines the voxel data
float world_function(glm::vec3 pos)
{
    return glm::perlin(0.1f*(pos+glm::vec3(100,100,100)));
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
    win_callbacks.motion_callback = motion_callback;
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

    // draw shader
    std::string vertex_source =
        "#version 330\n"
        "uniform mat4 ViewProjection;\n"
        "layout(location = 0) in vec4 vposition;\n"
        "layout(location = 1) in vec3 normal;\n"
        "out vec4 fcolor;\n"
        "void main() {\n"
        "   float brightness = dot(normal,normalize(vec3(1,2,3)));\n"
        "   brightness = 0.3+((brightness>0)?0.7*brightness:0.3*brightness);\n"
        "   fcolor = vec4(brightness,brightness,brightness,1);\n"
        "   gl_Position = ViewProjection*vposition;\n"
        "}\n";
        
    std::string fragment_source =
        "#version 330\n"
        "in vec4 fcolor;\n"
        "layout(location = 0) out vec4 FragColor;\n"
        "void main() {\n"
        "   FragColor = abs(fcolor);\n"
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
    GLint DrawViewProjection_location = glGetUniformLocation(shader_program, "ViewProjection");
 

    // trivial shader for occlusion queries
    std::string query_vertex_source =
        "#version 330\n"
        "uniform mat4 ViewProjection;\n"
        "layout(location = 0) in vec4 vposition;\n"
        "void main() {\n"
        "   gl_Position = ViewProjection*vposition;\n"
        "}\n";
        
    std::string query_fragment_source =
        "#version 330\n"
        "void main() {\n"
        "}\n";
    
    // program and shader handles
    GLuint query_shader_program, query_vertex_shader, query_fragment_shader;

    // create and compiler vertex shader
    query_vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    source = query_vertex_source.c_str();
    length = query_vertex_source.size();
    glShaderSource(query_vertex_shader, 1, &source, &length); 
    glCompileShader(query_vertex_shader);
    if(!check_shader_compile_status(query_vertex_shader))
    {
        return 1;
    }
 
    // create and compiler fragment shader
    query_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    source = query_fragment_source.c_str();
    length = query_fragment_source.size();
    glShaderSource(query_fragment_shader, 1, &source, &length);   
    glCompileShader(query_fragment_shader);
    if(!check_shader_compile_status(query_fragment_shader))
    {
        return 1;
    }
    
    // create program
    query_shader_program = glCreateProgram();
    
    // attach shaders
    glAttachShader(query_shader_program, query_vertex_shader);
    glAttachShader(query_shader_program, query_fragment_shader);
    
    // link the program and check for errors
    glLinkProgram(query_shader_program);
    check_program_link_status(query_shader_program);
    
    // obtain location of projection uniform
    GLint QueryViewProjection_location = glGetUniformLocation(query_shader_program, "ViewProjection");


    // chunk container and chunk parameters
    std::vector<Chunk> chunks;
    int chunkrange = 4;
    int chunksize = 32;
    
    // chunk extraction
    std::cout << "generating chunks, this may take a while." << std::endl;
    
    // iterate over all chunks we want to extract        
    for(int i = -chunkrange;i<chunkrange;++i)
        for(int j = -chunkrange;j<chunkrange;++j)
            for(int k = -chunkrange;k<chunkrange;++k)
    {
        
        Chunk chunk;
        
        // chunk data
        
        // generate and bind the vao
        glGenVertexArrays(1, &chunk.vao);
        glBindVertexArray(chunk.vao);
        
        // generate and bind the vertex buffer object
        glGenBuffers(1, &chunk.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
        
        std::vector<glm::vec3> vertexData;
        glm::vec3 offset = static_cast<float>(chunksize) * glm::vec3(i,j,k);
        float threshold = 0.0f;
        // iterate over all blocks within the chunk
        for(int x = 0;x<chunksize;++x)
            for(int y = 0;y<chunksize;++y)
                for(int z = 0;z<chunksize;++z)
                {
                    glm::vec3 pos = glm::vec3(x,y,z) + offset;
                    // insert quads if current block is solid and neighbors are not
                    if(world_function(pos)<threshold)
                    {
                        if(world_function(pos+glm::vec3(1,0,0))>=threshold)
                        {
                            vertexData.push_back(pos+0.5f*glm::vec3( 1, 1, 1));
                            vertexData.push_back(glm::vec3( 1, 0, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3( 1,-1, 1));
                            vertexData.push_back(glm::vec3( 1, 0, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3( 1, 1,-1));
                            vertexData.push_back(glm::vec3( 1, 0, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3( 1,-1,-1));
                            vertexData.push_back(glm::vec3( 1, 0, 0));
                        }
                        if(world_function(pos+glm::vec3(0,1,0))>=threshold)
                        {
                            vertexData.push_back(pos+0.5f*glm::vec3( 1, 1, 1));
                            vertexData.push_back(glm::vec3( 0, 1, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3( 1, 1,-1));
                            vertexData.push_back(glm::vec3( 0, 1, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1, 1, 1));
                            vertexData.push_back(glm::vec3( 0, 1, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1, 1,-1));
                            vertexData.push_back(glm::vec3( 0, 1, 0));
                        }
                        if(world_function(pos+glm::vec3(0,0,1))>=threshold)
                        {
                            vertexData.push_back(pos+0.5f*glm::vec3( 1, 1, 1));
                            vertexData.push_back(glm::vec3( 0, 0, 1));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1, 1, 1));
                            vertexData.push_back(glm::vec3( 0, 0, 1));
                            vertexData.push_back(pos+0.5f*glm::vec3( 1,-1, 1));
                            vertexData.push_back(glm::vec3( 0, 0, 1));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1,-1, 1));
                            vertexData.push_back(glm::vec3( 0, 0, 1));
                        }
                        if(world_function(pos-glm::vec3(1,0,0))>=threshold)
                        {
                            vertexData.push_back(pos+0.5f*glm::vec3(-1, 1, 1));
                            vertexData.push_back(glm::vec3(-1, 0, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1, 1,-1));
                            vertexData.push_back(glm::vec3(-1, 0, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1,-1, 1));
                            vertexData.push_back(glm::vec3(-1, 0, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1,-1,-1));
                            vertexData.push_back(glm::vec3(-1, 0, 0));
                        }
                        if(world_function(pos-glm::vec3(0,1,0))>=threshold)
                        {
                            vertexData.push_back(pos+0.5f*glm::vec3( 1,-1, 1));
                            vertexData.push_back(glm::vec3( 0,-1, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1,-1, 1));
                            vertexData.push_back(glm::vec3( 0,-1, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3( 1,-1,-1));
                            vertexData.push_back(glm::vec3( 0,-1, 0));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1,-1,-1));
                            vertexData.push_back(glm::vec3( 0,-1, 0));
                        }
                        if(world_function(pos-glm::vec3(0,0,1))>=threshold)
                        {
                            vertexData.push_back(pos+0.5f*glm::vec3( 1, 1,-1));
                            vertexData.push_back(glm::vec3( 0, 0,-1));
                            vertexData.push_back(pos+0.5f*glm::vec3( 1,-1,-1));
                            vertexData.push_back(glm::vec3( 0, 0,-1));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1, 1,-1));
                            vertexData.push_back(glm::vec3( 0, 0,-1));
                            vertexData.push_back(pos+0.5f*glm::vec3(-1,-1,-1));
                            vertexData.push_back(glm::vec3( 0, 0,-1));
                        }
                    }
                }
        // upload
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*vertexData.size(), &vertexData[0], GL_STATIC_DRAW);
                        
               
        // set up generic attrib pointers
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(GLfloat), (char*)0 + 0*sizeof(GLfloat));
     
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(GLfloat), (char*)0 + 3*sizeof(GLfloat));
        
        
        // generate and bind the index buffer object
        glGenBuffers(1, &chunk.ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.ibo);
        
        chunk.quadcount = vertexData.size()/8;
        std::vector<GLuint> indexData(6*chunk.quadcount);
        for(int i = 0;i<chunk.quadcount;++i)
        {
            indexData[6*i + 0] = 4*i + 0;
            indexData[6*i + 1] = 4*i + 1;
            indexData[6*i + 2] = 4*i + 2;
            indexData[6*i + 3] = 4*i + 2;
            indexData[6*i + 4] = 4*i + 1;
            indexData[6*i + 5] = 4*i + 3;
        }
        
        // upload
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*indexData.size(), &indexData[0], GL_STATIC_DRAW);


        // chunk bounding box

        // generate and bind the vao
        glGenVertexArrays(1, &chunk.bounding_vao);
        glBindVertexArray(chunk.bounding_vao);
        
        // generate and bind the vertex buffer object
        glGenBuffers(1, &chunk.bounding_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, chunk.bounding_vbo);
                
        // data for the bounding cube
        GLfloat boundingVertexData[] = {
        //  X                           Y                           Z 
        // face 0:
            offset.x+chunksize-0.5f,    offset.y+chunksize-0.5f,    offset.z+chunksize-0.5f,
            offset.x-0.5f,              offset.y+chunksize-0.5f,    offset.z+chunksize-0.5f,
            offset.x+chunksize-0.5f,    offset.y-0.5f,              offset.z+chunksize-0.5f,
            offset.x-0.5f,              offset.y-0.5f,              offset.z+chunksize-0.5f,

        // face 1:
            offset.x+chunksize-0.5f,    offset.y+chunksize-0.5f,    offset.z+chunksize-0.5f,
            offset.x+chunksize-0.5f,    offset.y-0.5f,              offset.z+chunksize-0.5f,
            offset.x+chunksize-0.5f,    offset.y+chunksize-0.5f,    offset.z-0.5f,
            offset.x+chunksize-0.5f,    offset.y-0.5f,              offset.z-0.5f,

        // face 2:
            offset.x+chunksize-0.5f,    offset.y+chunksize-0.5f,    offset.z+chunksize-0.5f,
            offset.x+chunksize-0.5f,    offset.y+chunksize-0.5f,    offset.z-0.5f,
            offset.x-0.5f,              offset.y+chunksize-0.5f,    offset.z+chunksize-0.5f,
            offset.x-0.5f,              offset.y+chunksize-0.5f,    offset.z-0.5f,
          
        // face 3:
            offset.x+chunksize-0.5f,    offset.y+chunksize-0.5f,    offset.z-0.5f,
            offset.x+chunksize-0.5f,    offset.y-0.5f,              offset.z-0.5f,
            offset.x-0.5f,              offset.y+chunksize-0.5f,    offset.z-0.5f,
            offset.x-0.5f,              offset.y-0.5f,              offset.z-0.5f,

        // face 4:
            offset.x-0.5f,              offset.y+chunksize-0.5f,    offset.z+chunksize-0.5f,
            offset.x-0.5f,              offset.y+chunksize-0.5f,    offset.z-0.5f,
            offset.x-0.5f,              offset.y-0.5f,              offset.z+chunksize-0.5f,
            offset.x-0.5f,              offset.y-0.5f,              offset.z-0.5f,

        // face 5:
            offset.x+chunksize-0.5f,    offset.y-0.5f,              offset.z+chunksize-0.5f,
            offset.x-0.5f,              offset.y-0.5f,              offset.z+chunksize-0.5f,
            offset.x+chunksize-0.5f,    offset.y-0.5f,              offset.z-0.5f,
            offset.x-0.5f,              offset.y-0.5f,              offset.z-0.5f,
        }; // 6 faces with 4 vertices with 6 components (floats)

        // fill with data
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*6*4*3, boundingVertexData, GL_STATIC_DRAW);
                        
               
        // set up generic attrib pointers
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), (char*)0 + 0*sizeof(GLfloat));
        
        // generate and bind the index buffer object
        glGenBuffers(1, &chunk.bounding_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.bounding_ibo);
                
        GLuint boundingIndexData[] = {
             0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 8, 9,10,10, 9,11,
            12,13,14,14,13,15,16,17,18,18,17,19,20,21,22,22,21,23,
        };

        // fill with data
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint)*6*2*3, boundingIndexData, GL_STATIC_DRAW);
        
        // generate the query object for the occlusion query
        glGenQueries(1, &chunk.query);
        
        // set the center location of the chunk
        chunk.center = offset + 0.5f*chunksize;
        
        // add to container
        chunks.push_back(chunk);
    }
    
    // "unbind" vao
    glBindVertexArray(0);

    // timer query setup
    // use multiple queries to avoid stalling on getting the results
    const int querycount = 5;
    GLuint queries[querycount];
    int current_query = 0;
    glGenQueries(querycount, queries);
    
    // we are drawing 3d objects so we want depth testing
    glEnable(GL_DEPTH_TEST);

    // camera position and orientation
    glm::vec3 position;
    glm::mat4 rotation = glm::mat4(1.0f);
    
    float t = glwtGetNanoTime()*1.e-9f;
    userdata.occlusion_cull = true;
    userdata.move.forward = 0;
    userdata.move.right = 0;
    userdata.move.up = 0;
    userdata.move.roll = 0;
    userdata.mouse.x = 0;
    userdata.mouse.y = 0;
    
    int mousex = userdata.mouse.x;
    int mousey = userdata.mouse.y;
    while(userdata.running)
    {   
        // calculate timestep
        float new_t = glwtGetNanoTime()*1.e-9f;
        float dt = new_t - t;
        t = new_t;

        // update events
        glwtEventHandle(0);

        // update mouse differential
        int tmpx = userdata.mouse.x;
        int tmpy = userdata.mouse.y;
        glm::vec2 mousediff(tmpx-mousex, tmpy-mousey);
        mousex = tmpx;
        mousey = tmpy;
        
        // find up, forward and right vector
        glm::mat3 rotation3(rotation);
        glm::vec3 up = glm::transpose(rotation3)*glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::transpose(rotation3)*glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 forward = glm::transpose(rotation3)*glm::vec3(0.0f, 0.0f,-1.0f);
        
        // apply mouse rotation
        rotation = glm::rotate(rotation,  0.2f*mousediff.x, up);
        rotation = glm::rotate(rotation,  0.2f*mousediff.y, right);
        
        // roll
        rotation = glm::rotate(rotation, 180.0f*dt*userdata.move.roll, forward); 
 
        
        // movement
        position += 10.0f*dt*forward*userdata.move.forward;
        position += 10.0f*dt*right*userdata.move.right;
        position += 10.0f*dt*up*userdata.move.up;
        
        
        // calculate ViewProjection matrix
        glm::mat4 Projection = glm::perspective(90.0f, 4.0f / 3.0f, 0.1f, 200.f);
        glm::mat4 View = rotation*glm::translate(glm::mat4(1.0f), -position);
        glm::mat4 ViewProjection = Projection*View;


        // set matrices for both shaders
        glUseProgram(query_shader_program);
        glUniformMatrix4fv(QueryViewProjection_location, 1, GL_FALSE, glm::value_ptr(ViewProjection)); 
        glUseProgram(shader_program);
        glUniformMatrix4fv(DrawViewProjection_location, 1, GL_FALSE, glm::value_ptr(ViewProjection));
        
        // set clear color to sky blue
        glClearColor(0.5f,0.8f,1.0f,1.0f);
        
        // clear
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // sort chunks by distance
        std::sort(chunks.begin(), chunks.end(), DistancePred(position));
        
        size_t i = 0;
        float maxdist = chunksize;

        // start timer query
        glBeginQuery(GL_TIME_ELAPSED, queries[current_query]);
        
        // peel chunks
        while(i!=chunks.size())
        {
            size_t j = i;
            if(userdata.occlusion_cull)
            {
                // start occlusion queries and render for the current slice
                glDisable(GL_CULL_FACE);
                
                // we don't want the queries to actually render something
                glDepthMask(GL_FALSE);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                glUseProgram(query_shader_program);
                for(;j<chunks.size() && glm::distance(chunks[j].center, position)<maxdist;++j)
                {
                    // frustum culling
                    glm::vec4 projected = ViewProjection*glm::vec4(chunks[j].center,1);
                    if( (glm::distance(chunks[j].center,position) > chunksize) &&
                        (std::max(std::abs(projected.x), std::abs(projected.y)) > projected.w+chunksize))
                        continue;
                    
                    // begin occlusion query
                    glBeginQuery(GL_ANY_SAMPLES_PASSED, chunks[j].query);
                    
                    // draw bounding box
                    glBindVertexArray(chunks[j].bounding_vao);
                    glDrawElements(GL_TRIANGLES, 6*6, GL_UNSIGNED_INT, 0);
                    
                    // end occlusion query
                    glEndQuery(GL_ANY_SAMPLES_PASSED);
                }
                j = i;
            }

            // render the current slice
            glEnable(GL_CULL_FACE);
            
            // turn rendering back on
            glDepthMask(GL_TRUE);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glUseProgram(shader_program);
            for(;j<chunks.size() && glm::distance(chunks[j].center, position)<maxdist;++j)
            {
                // frustum culling
                glm::vec4 projected = ViewProjection*glm::vec4(chunks[j].center,1);
                if( (glm::distance(chunks[j].center,position) > chunksize) &&
                    (std::max(std::abs(projected.x), std::abs(projected.y)) > projected.w+chunksize))
                    continue;
                
                // begin conditional render
                if(userdata.occlusion_cull)
                    glBeginConditionalRender(chunks[j].query, GL_QUERY_BY_REGION_WAIT);
                
                // draw chunk
                glBindVertexArray(chunks[j].vao);
                glDrawElements(GL_TRIANGLES, 6*chunks[j].quadcount, GL_UNSIGNED_INT, 0);
                
                // end conditional render
                if(userdata.occlusion_cull)
                    glEndConditionalRender();
            }
            i = j;
            maxdist += 2*chunksize;
        }
        
        // end timer query
        glEndQuery(GL_TIME_ELAPSED);
        
        // display timer query results from querycount frames before
        if(GL_TRUE == glIsQuery(queries[(current_query+1)%querycount]))
        {
            GLuint64 result;
            glGetQueryObjectui64v(queries[(current_query+1)%querycount], GL_QUERY_RESULT, &result);
            std::cout << result*1.e-6 << " ms/frame" << std::endl;
        }
        // advance query counter
        current_query = (current_query + 1)%querycount;
        
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
    
    for(size_t i = 0;i<chunks.size();++i)
    {    
        glDeleteVertexArrays(1, &chunks[i].vao);
        glDeleteBuffers(1, &chunks[i].vbo);
        glDeleteBuffers(1, &chunks[i].ibo);
        glDeleteVertexArrays(1, &chunks[i].bounding_vao);
        glDeleteBuffers(1, &chunks[i].bounding_vbo);
        glDeleteBuffers(1, &chunks[i].bounding_ibo);
        glDeleteQueries(1, &chunks[i].query);
    }
    
    glDeleteQueries(querycount, queries);
    
    glDetachShader(shader_program, vertex_shader);	
    glDetachShader(shader_program, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteProgram(shader_program);

    glDetachShader(query_shader_program, query_vertex_shader);	
    glDetachShader(query_shader_program, query_fragment_shader);
    glDeleteShader(query_vertex_shader);
    glDeleteShader(query_fragment_shader);
    glDeleteProgram(query_shader_program);
    
    glwtWindowDestroy(window);
    glwtQuit();
    return 0;
}

