#include "glad/glad.h"
#include <GLFW/glfw3.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <string>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <vector>
//window width and height
#define WIDTH 600.f
#define HEIGHT 600.f


void Polar(float r, float theta, float* x, float* y, float originX, float originY) //converting cartesian to polar coordinates
{//radius, theta in degress, and the resulting "x" and "y" values depending on the axis of rotation
    theta *= 3.14 / 180;
    *x = originX + (r * cos(theta));//originX and originY shift the point being rotated around their respective values
    *y = originY + (r * sin(theta));//or basically, they move the origin of the polar coordinates
}


class Light //Base Light Class
{
protected: //necessary data for all children of this class
    glm::vec3 lightColor;
    float ambientStr;
    glm::vec3 ambientColor;
    float specStr;
    float specPhong;
    float intensity;

public: 
    Light() //constructor, default values for both types of lights are set here
    { 
        this->lightColor = glm::vec3(1, 1, 1);
        this->ambientStr = 0.1f;
        this->ambientColor = lightColor;
        this->specStr = 0.5f;
        this->specPhong = 16;
    }

    void glUniformLight(glm::vec3 cameraPos, GLuint* shaderProg) //attaches data to the shaders
    {
        GLuint lightColorAddress = glGetUniformLocation(*shaderProg, "lightColor");
        glUniform3fv(lightColorAddress, 1, glm::value_ptr(this->lightColor));
        GLuint ambientStrAddress = glGetUniformLocation(*shaderProg, "ambientStr");
        glUniform1f(ambientStrAddress, this->ambientStr);
        GLuint ambientColorAddress = glGetUniformLocation(*shaderProg, "ambientColor");
        glUniform3fv(ambientColorAddress, 1, glm::value_ptr(this->ambientColor));
        GLuint  cameraPosAddress = glGetUniformLocation(*shaderProg, "cameraPos");
        glUniform3fv(cameraPosAddress, 1, glm::value_ptr(cameraPos));
        GLuint specStrAddress = glGetUniformLocation(*shaderProg, "specStr");
        glUniform1f(specStrAddress, this->specStr);
        GLuint specPhongAddress = glGetUniformLocation(*shaderProg, "specPhong");
        glUniform1f(specPhongAddress, this->specPhong);
    }

    virtual void glUniformLight2(GLuint* shaderProg)//attaches children only data to the shaders
    {
        std::cout << "This is meant to be overriden below by Light's children.";
    }

};

class DirectionLight : public Light //Direction Light Class
{
private: glm::vec3 dirLight; //contains a vector representing the direction of the light


public:
    DirectionLight(): Light() //constructor using the base class's constructor to supplement initialization
    {
        this->intensity = 0.75f;
        this->dirLight = glm::vec3(0, 1, 0);//position is slightly above the ocean, but close enough 
    }
    void glUniformLight2(GLuint* shaderProg) //passes the intensity and direction of the light
    {
        GLuint dirLightAddress = glGetUniformLocation(*shaderProg, "dirLight");
        glUniform3fv(dirLightAddress, 1, glm::value_ptr(this->dirLight));
        GLuint intensityAddress = glGetUniformLocation(*shaderProg, "dirIntensity");
        glUniform1f(intensityAddress, this->intensity);
    }
};

class PointLight : public Light //Point Light Class, base for First and Third Person Cameras
{
private: float constant, linear, quadratic; //contains the components for attenuation
         glm::vec3 lightPos; //Position of the light
         bool release;//to avoid the intensity from switching at supersonic speeds

public:
    PointLight() : Light() //constructor using the base class's constructor to supplement initialization
    {
        this->lightPos = glm::vec3(0, 0, 0);
        this->constant = 1;
        this->linear = 0.35;
        this->quadratic = 0.44;
        this->release = true;
        this->intensity = 1.5f;
    }
    void glUniformLight2(GLuint* shaderProg) //passes the light's intensity, position and attenuation parts
    {
        GLuint lightAddress = glGetUniformLocation(*shaderProg, "lightPos");
        glUniform3fv(lightAddress, 1, glm::value_ptr(this->lightPos));
        GLuint constantAddress = glGetUniformLocation(*shaderProg, "constant");
        glUniform1f(constantAddress, this->constant);
        GLuint linearAddress = glGetUniformLocation(*shaderProg, "linear");
        glUniform1f(linearAddress, this->linear);
        GLuint quadraticAddress = glGetUniformLocation(*shaderProg, "quadratic");
        glUniform1f(quadraticAddress, this->quadratic);
        GLuint intensityAddress = glGetUniformLocation(*shaderProg, "pointIntensity");
        glUniform1f(intensityAddress, this->intensity);
    }

    void cycleIntensity(GLFWwindow* window) //swaps light intensity from high to mid, mid to low, and low to high
    {
        if (glfwGetKey(window, GLFW_KEY_F) && (release == true))//checks if the button is only now being pressed
        {
            if (intensity == 1.5f)
                intensity = 0.55f;
            else if (intensity == 0.55f)
                intensity = 0.15f;
            else if (intensity == 0.15f)
                intensity = 1.5f;
            release = false;
        }//locks light intensity switching until the F is released
        else if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE)
            release = true;
            
    }

    void setLightPos(glm::vec3 pos) //setter for light position (used for attaching it to the front of the submarine)
    {      
        this->lightPos = pos;
    }
};

class Camera //contains all necessay details about the camera
{
protected: float rotation, rotation2; 
         //rotation is around the x, rotation2 is around the y
protected: glm::vec3 cameraPos, Center; //vectors representing the camera's position and the center of the view
protected: bool firstMouse;
         float lastX, lastY;
         glm::vec3 WorldUp; //world up vector 
       //firstMouse is for checking the initial frame of rotating the view using the mouse
       //the floats lastX and lastY are there for rotating via mouse pointer

public:
    Camera()//constructor, sets all values to their default/initial state
    {
        firstMouse = true;
        rotation = 0.0f; 
        rotation2 = 0.0f;
        cameraPos = glm::vec3(0.0, 0.0f, 0.0f);
        Center = glm::vec3(0.0f, 0.0f, -10.0f);
        lastX = 0;
        lastY = 0;
        WorldUp = glm::vec3(0, 1.0f, 0);
    }

    void  resetMouse()//resets firstMouse to avoid a "jump" in camera view
    {
        firstMouse = true;
    }
    glm::vec3 getCameraPos()//returns the camera's position vector
    {
        return cameraPos;
    }

    glm::mat4 getRotationMatrix()//gets the rotation matrix depending on the camera's rotation-based point of view 
    {
        glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(this->rotation2), glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f)));
        //gets the rotation around the y axis (horizontal)                                                         negative to "uninverse" movement after rotation
        rotationMatrix = glm::rotate(rotationMatrix, glm::radians(this->rotation), glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f))); 
        //gets the rotation around the x axis (vertical)
        return rotationMatrix;
    }

    virtual void PlayerControl(GLFWwindow* window)
    { //different based on the camera being used at the time
    }



 

    glm::mat4 getViewMatrix() //returns the viewMatrix to be used by the 3D models
    {

        glm::mat4 cameraPositionMatrix = glm::mat4(1.0);
        cameraPositionMatrix = glm::translate(cameraPositionMatrix, this->cameraPos * -1.0f);
        //translates the cameraPositionMatrix based on cameraPos


        glm::vec3 F = glm::vec3(this->Center - this->cameraPos);
        //forward vector creation based on the Center and cameraPos

        F = glm::normalize(F);//normalizes F
        glm::vec3 R = glm::normalize(glm::cross(F, WorldUp));
        glm::vec3 U = glm::normalize(glm::cross(R, F));
        //creates the right and up vectors

        glm::mat4 cameraOrientation = glm::mat4(1.0f);

        cameraOrientation[0][0] = R.x;
        cameraOrientation[1][0] = R.y;
        cameraOrientation[2][0] = R.z;

        cameraOrientation[0][1] = U.x;
        cameraOrientation[1][1] = U.y;
        cameraOrientation[2][1] = U.z;

        cameraOrientation[0][2] = -F.x;
        cameraOrientation[1][2] = -F.y;
        cameraOrientation[2][2] = -F.z;
        //creates the camera orientation matrix from the right, up, and (negative) forward vectors

        cameraOrientation = glm::rotate(cameraOrientation, glm::radians(this->rotation), glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f)));
        cameraOrientation = glm::rotate(cameraOrientation, glm::radians(this->rotation2), glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)));
        //rotates the camera's view based on the current rotation values


        glm::mat4 viewMatrix = (cameraOrientation * cameraPositionMatrix);
        return viewMatrix; //creates and returns the view matrix
    }

    virtual glm::mat4 getProjection()
    {
        std::cout << "This isn't supposed to be called. This is meant to be overriden by the childrens' versions below";
        return glm::mat4(4.2);
    };
};

class OrthoCamera :public Camera//orthographic camera class
{
private: 
    float edgeL, edgeR, edgeT, edgeB, edgeN, edgeF; //left, right, top, bottom, near and far;components for ortho projection

public:

    OrthoCamera() :Camera() //constructor
    {
        cameraPos = glm::vec3(0.0, 0.0f, 5.0f);//moves the camera into place
        Center = glm::vec3(0.0f, cameraPos.y+3, 0.0f);
        rotation = 90.0f;//makes it face downwards

        this->edgeL = -15.0f;
        this->edgeR = 15.0f;
        this->edgeT = -15.0f;
        this->edgeB = 15.0f;
        this->edgeN = -100.0f;
        this->edgeF = 100.0f;
    }

    void resetPos(glm::vec3 subPos) //moves the camera to above the submrine
    {
        cameraPos = subPos + glm::vec3(0.0, 0.0f, 5.0f);
        Center = glm::vec3(subPos.x, cameraPos.y + 3, subPos.z);
    }
    void PlayerControl(GLFWwindow* window)
    { //allows for moving the camera based on the arrow keys pressed

        if (glfwGetKey(window, GLFW_KEY_UP) || glfwGetKey(window, GLFW_KEY_DOWN))
        {
            glm::vec4 rotatedVector = getRotationMatrix() * glm::vec4(glm::vec3(0, 0, -0.01), 0.0f);
            //multiplies the direction of movement by the rotations
            glm::vec3 offset = glm::vec3(rotatedVector.x, rotatedVector.y, rotatedVector.z);
            //gets the offset in order to move forward/backward based on the way the camera is facing

            if (glfwGetKey(window, GLFW_KEY_DOWN))//moves up
            {
                cameraPos += offset;//moves both the camera and center by the offset
                Center += offset;
            }
            if (glfwGetKey(window, GLFW_KEY_UP))//moves down
            {
                cameraPos -= offset;
                Center -= offset;
            }
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT) || glfwGetKey(window, GLFW_KEY_RIGHT))
        {
            glm::vec4 rotatedVector = getRotationMatrix() * glm::vec4(glm::vec3(0.01, 0, 0), 0.0f);
            glm::vec3 offset = glm::vec3(rotatedVector.x, rotatedVector.y, rotatedVector.z);

            if (glfwGetKey(window, GLFW_KEY_LEFT))//moves left
            {
                cameraPos -= offset;
                Center -= offset;
            }
            if (glfwGetKey(window, GLFW_KEY_RIGHT))//moves right
            {
                cameraPos += offset;
                Center += offset;
            }
        }

    }

    glm::mat4 getProjection()//gets orthographic projection
    {
        return glm::ortho(edgeL, edgeR, edgeT, edgeB, edgeN, edgeF);
    }
};

class PerspectiveCamera :public Camera //for moving the camera
{
protected:

    float FOV, width, height, near, far; //components for the perspective projection

public:

    PerspectiveCamera(float width, float height) :Camera() //constructor
    {   //camera default position and rotations
        cameraPos = glm::vec3(0.0, 0.0f, 0.0f);
        Center = glm::vec3(0.0f, 0.0f, cameraPos.z-25);

        this->FOV = 60.0f;
        this->width = width;
        this->height = height;
        this->near = 0.1f;
        this->far = 100.f;
    }

    
    glm::mat4 getProjection() //gets perspective projection
    {
        return glm::perspective(glm::radians(FOV), width / height, near, far);
    }
};



class Shader //Shader Class
{
private: GLuint shaderProg;//Shader Program
public: 
    Shader() = default;
    Shader(const char* vertPath, const char* fragPath)//Creates shader program based in the files givenm
    {
        GLuint vertexShader,fragShader;
        bool vNULL=true, fNULL=true;
        this->shaderProg = glCreateProgram();
        if (vertPath != NULL)//vertex shader creation
        {
            vNULL = false;
            std::fstream vertSrc(vertPath);
            std::stringstream vertBuff;
            vertBuff << vertSrc.rdbuf();

            std::string vertS = vertBuff.str();
            const char* v = vertS.c_str();
            vertexShader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertexShader, 1, &v, NULL);
            glCompileShader(vertexShader);
            glAttachShader(shaderProg, vertexShader);//attaches the vertex shader
        }

        if (fragPath != NULL) //fragment shader creation
        {
            fNULL = false;
            std::fstream fragSrc(fragPath);
            std::stringstream fragBuff;
            fragBuff << fragSrc.rdbuf();

            std::string fragS = fragBuff.str();
            const char* f = fragS.c_str();
            fragShader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragShader, 1, &f, NULL);
            glCompileShader(fragShader);
            glAttachShader(shaderProg, fragShader);
        }
        
        //links the full shader
        glLinkProgram(shaderProg);

        //cleanup
        if (!vNULL)
        glDeleteShader(vertexShader);
        if(!fNULL)
        glDeleteShader(fragShader);
    }
    void useProgram()//tells the program to use this shader for the models below
    {
        glUseProgram(shaderProg);
    }

    GLuint* getProgram() //gets the shader program for drawing the models
    {
        return &this->shaderProg;
    }
};

class Model //Model Class
{
private: GLuint VAO,VBO; //VAO and VBO of the model
         std::vector<GLfloat> fullVertexData; //vertex data
         int size; //number of vertex attributes to account for

public:
    Model() = default;

    Model(const char* filepath) //for the submarine, or models which need to be normal mapped
    {
        std::string path = filepath; //get the model from the file path
        std::vector <tinyobj::shape_t> shapes;
        std::vector <tinyobj::material_t> material;
        std::string warning, error;

        tinyobj::attrib_t attributes;
        //load the model and the necessary information that comes with it
        bool success = tinyobj::LoadObj(&attributes, &shapes, &material, &warning, &error, path.c_str());
        //getting tangents and bitangents
        std::vector<glm::vec3> tangents, bitangents;
        
        tinyobj::index_t vData1, vData2, vData3;
        glm::vec3 v1, v2, v3;
        glm::vec2 uv1, uv2, uv3;
        glm::vec3 deltaPos1, deltaPos2;
        glm::vec2 deltaUV1, deltaUV2;
        float r;
        glm::vec3 tangent, bitangent;
        for (int i = 0; i < shapes[0].mesh.indices.size(); i += 3)
        {
            vData1 = shapes[0].mesh.indices[i];
            vData2 = shapes[0].mesh.indices[i+1];
            vData3 = shapes[0].mesh.indices[i+2];

            v1 = glm::vec3(attributes.vertices[vData1.vertex_index * 3],
                           attributes.vertices[(vData1.vertex_index * 3) + 1],
                           attributes.vertices[(vData1.vertex_index * 3) + 2]);

            v2 = glm::vec3(attributes.vertices[vData2.vertex_index * 3],
                attributes.vertices[(vData2.vertex_index * 3) + 1],
                attributes.vertices[(vData2.vertex_index * 3) + 2]);

            v3 = glm::vec3(attributes.vertices[vData3.vertex_index * 3],
                attributes.vertices[(vData3.vertex_index * 3) + 1],
                attributes.vertices[(vData3.vertex_index * 3) + 2]);

            uv1 = glm::vec2(attributes.texcoords[vData1.texcoord_index * 2], attributes.texcoords[(vData1.texcoord_index * 2)+1]);
            uv2 = glm::vec2(attributes.texcoords[vData2.texcoord_index * 2], attributes.texcoords[(vData2.texcoord_index * 2) + 1]);
            uv3= glm::vec2(attributes.texcoords[vData3.texcoord_index * 2], attributes.texcoords[(vData3.texcoord_index * 2) + 1]);

            deltaPos1 = v2 - v1;
            deltaPos2 = v3 - v1;

            deltaUV1 = uv2 - uv1;
            deltaUV2 = uv3 - uv1;

            r = 1.0f / ((deltaUV1.x * deltaUV2.y) - (deltaUV1.y * deltaUV2.x));
            tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
            bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;

            tangents.push_back(tangent);
            tangents.push_back(tangent);
            tangents.push_back(tangent);

            bitangents.push_back(bitangent);
            bitangents.push_back(bitangent);
            bitangents.push_back(bitangent);
        }

        //gets the vertex data of the main model, while also pushing back the tangents and bitangents
        for (int i = 0; i < shapes[0].mesh.indices.size(); i++) {
            tinyobj::index_t vData = shapes[0].mesh.indices[i];

           this->fullVertexData.push_back(attributes.vertices[(vData.vertex_index * 3)]);
           this->fullVertexData.push_back(attributes.vertices[(vData.vertex_index * 3) + 1]);
           this->fullVertexData.push_back(attributes.vertices[(vData.vertex_index * 3) + 2]);

           this->fullVertexData.push_back(attributes.normals[(vData.normal_index * 3)]);
           this->fullVertexData.push_back(attributes.normals[(vData.normal_index * 3) + 1]);
           this->fullVertexData.push_back(attributes.normals[(vData.normal_index * 3) + 2]);

           this->fullVertexData.push_back(attributes.texcoords[(vData.texcoord_index * 2)]);
           this->fullVertexData.push_back(attributes.texcoords[(vData.texcoord_index * 2) + 1]);

           this->fullVertexData.push_back(tangents[i].x);
           this->fullVertexData.push_back(tangents[i].y);
           this->fullVertexData.push_back(tangents[i].z);

           this->fullVertexData.push_back(bitangents[i].x);
           this->fullVertexData.push_back(bitangents[i].y);
           this->fullVertexData.push_back(bitangents[i].z);
            
        }
        //initializes the VAO, VBO for the model
        glGenVertexArrays(1, &this->VAO);
        glGenBuffers(1, &this->VBO);

        //Binds the VAO and VBO
        glBindVertexArray(this->VAO);
        glBindVertexArray(this->VBO);
        glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * this->fullVertexData.size(), this->fullVertexData.data(), GL_DYNAMIC_DRAW);

            this->size = 14;// 3 from the vertices, 3 from the normals, 2 from the textures, and 6 from the tangents and bitangents

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, this->size * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        GLintptr normalPtr = 3 * sizeof(float);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, this->size * sizeof(float), (void*)normalPtr);
            glEnableVertexAttribArray(1);
        
        GLintptr texcoordPtr = 6 * sizeof(float);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, this->size * sizeof(float), (void*)texcoordPtr);
            glEnableVertexAttribArray(2);

       GLintptr tangentPtr = 8 * sizeof(float);
            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, this->size * sizeof(float), (void*)tangentPtr);
            glEnableVertexAttribArray(3);

       GLintptr bitangentPtr = 11 * sizeof(float);
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, this->size * sizeof(float), (void*)bitangentPtr);
            glEnableVertexAttribArray(4);
    }
    

    Model(const char* filepath, bool normals, bool textureCoords)
    {//same as above, without normal mapping and also allows for choosing to exclude texture coordinates and normals
        std::string path = filepath; 
        std::vector <tinyobj::shape_t> shapes;
        std::vector <tinyobj::material_t> material;
        std::string warning, error;

        tinyobj::attrib_t attributes;

        bool success = tinyobj::LoadObj(&attributes, &shapes, &material, &warning, &error, path.c_str());

        //gets the vertex data of the main model
        for (int i = 0; i < shapes[0].mesh.indices.size(); i++) {
            tinyobj::index_t vData = shapes[0].mesh.indices[i];

                this->fullVertexData.push_back(attributes.vertices[(vData.vertex_index * 3)]);
                this->fullVertexData.push_back(attributes.vertices[(vData.vertex_index * 3) + 1]);
                this->fullVertexData.push_back(attributes.vertices[(vData.vertex_index * 3) + 2]);
            
            if (normals==true) {
                this->fullVertexData.push_back(attributes.normals[(vData.normal_index * 3)]);
                this->fullVertexData.push_back(attributes.normals[(vData.normal_index * 3) + 1]);
                this->fullVertexData.push_back(attributes.normals[(vData.normal_index * 3) + 2]);
            }
            if (textureCoords==true) {
                this->fullVertexData.push_back(attributes.texcoords[(vData.texcoord_index * 2)]);
                this->fullVertexData.push_back(attributes.texcoords[(vData.texcoord_index * 2) + 1]);
            }
        }
        //initializes the VAO, VBO for the model
        glGenVertexArrays(1, &this->VAO);
        glGenBuffers(1, &this->VBO);
 
        //Binds the VAO and VBO
        glBindVertexArray(this->VAO);
        //Currently editing VAO = null
        glBindVertexArray(this->VBO);
        //Currently editing VBO = null

        glBindBuffer(GL_ARRAY_BUFFER, this->VBO);//stores vertex data in array

        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * this->fullVertexData.size(), this->fullVertexData.data(), GL_DYNAMIC_DRAW);
        

        if (normals == true && textureCoords == true)//changes size based on the parameters
            this->size = 8; 
        else if (normals == true)
            this->size = 6;
        else if (textureCoords == true)
            this->size = 5;
        else
            this->size = 3;


        
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, this->size * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        //only occurs if the approriate boolean parameter is true
        GLintptr normalPtr = 3 * sizeof(float);
        if (normals == true) {
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, this->size * sizeof(float), (void*)normalPtr);
            glEnableVertexAttribArray(1);
        }

        GLintptr texcoordPtr = 6 * sizeof(float);
        if (textureCoords == true) {
            if(normals==false)
            texcoordPtr = 3 * sizeof(float);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, this->size * sizeof(float), (void*)texcoordPtr);
            glEnableVertexAttribArray(2);
        }  
        
    }

    GLuint* getVAO()//gets the VAO
    {
        return &this->VAO;
    }
    int getVertexDataSize()//gets the full vertex data size
    {
        return this->fullVertexData.size();
    }
    int getSize() //gets the number of full vertex data components to account for
    {
        return this->size;
    }
    void cleanUp()//deletes the VAO and VBO
    {
       glDeleteVertexArrays(1, &this->VAO);
       glDeleteBuffers(1, &this->VBO);
    }

};

class Entity
{
protected: float x, y, z; //translate
protected: float thetaX, thetaY, thetaZ; //rotate
protected: float sx, sy, sz;//scale
protected: Model model;//model to be associated with the entity

public: Entity() = default;//constructors
public: Entity(float x, float y, float z,
    float thetaX, float thetaY, float thetaZ,
    float sx, float sy, float sz, Model model)
{
    this->x = x;
    this->y = y;
    this->z = z;
    this->thetaX = thetaX;
    this->thetaY = thetaY;
    this->thetaZ = thetaZ;
    this->sx = sx;
    this->sy = sy;
    this->sz = sz;
    this->model = model;
}

      void draw(GLuint* shaderProg,Camera* cam, GLuint* texture, GLuint* normalTex ,std::vector<Light*> Lights)//draws the 3D Model
      {

          //gets the projection matrix from the camera
          glm::mat4 projection = cam->getProjection();
          //gets the view matrix from the camera
          glm::mat4 viewMatrix = cam->getViewMatrix();


          //creates the transformation matrix using the 3D models's attributes
          glm::mat4 transformation_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(this->x, this->y, this->z));
          transformation_matrix = glm::scale(transformation_matrix, glm::vec3(this->sx, this->sy, this->sz));
          transformation_matrix = glm::rotate(transformation_matrix, glm::radians(thetaX), glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f)));
          transformation_matrix = glm::rotate(transformation_matrix, glm::radians(thetaY), glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)));
          transformation_matrix = glm::rotate(transformation_matrix, glm::radians(thetaZ), glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f)));

          glActiveTexture(GL_TEXTURE0);
          GLuint tex0address = glGetUniformLocation(*shaderProg, "tex0");//texture related stuff
          glBindTexture(GL_TEXTURE_2D, *texture);
          glUniform1i(tex0address, 0);

          if (normalTex)//if normal mapped, 
          {
              glActiveTexture(GL_TEXTURE1);
              GLuint tex1address = glGetUniformLocation(*shaderProg, "norm_tex");//texture related stuff
              glBindTexture(GL_TEXTURE_2D, *normalTex);
              glUniform1i(tex1address, 1);
          }

          //passes and recieves the necessary information to and from the shaders
          unsigned int viewLoc = glGetUniformLocation(*shaderProg, "view");
          glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));

          unsigned int projLoc = glGetUniformLocation(*shaderProg, "projection");
          glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

          unsigned int transformLoc = glGetUniformLocation(*shaderProg, "transform");
          glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transformation_matrix));

          //passes and recieves the necessary information to and from the shaders (lights)
          Lights[1]->glUniformLight(cam->getCameraPos(), shaderProg);

          Lights[0]->glUniformLight2(shaderProg);//direction light stuff
          Lights[1]->glUniformLight2(shaderProg);//point light stuff 

          //Binds the VAO 
          glBindVertexArray(*this->model.getVAO());

          //draws the 3D model
          glDrawArrays(GL_TRIANGLES, 0, this->model.getVertexDataSize() / this->model.getSize());

      }

      glm::vec3 getPos() //gets the entity's position
      {
          return glm::vec3(this->x, this->y, this->z);
      }

      void cleanUp() //calls the models clean up function to delete its VAO and VBO
      {
          this->model.cleanUp();
      }

};
    



class Player: public Entity //Player Class, derived from the entity
{

public: Player(float x, float y, float z,//constructor, that uses Entity's constructor as a base
    float thetaX, float thetaY, float thetaZ,
    float sx, float sy, float sz, Model model)
    : Entity( x,  y,  z,
         thetaX,  thetaY,  thetaZ,
        sx, sy, sz, model) {
}

      glm::mat4 getRotationMatrix()//gets the rotation matrix depending on the camera's rotation-based point of view 
      {
          glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(this->thetaY), glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)));
          //gets the rotation around the y axis (horizontal)                                                        
          return rotationMatrix;
      }

      void PlayerControls(GLFWwindow* window)//for player controls
      {
          if (glfwGetKey(window, GLFW_KEY_W) || glfwGetKey(window, GLFW_KEY_S))
          {//moves forward/backwards based on the where the player is looking 
              glm::vec4 rotatedVector = this->getRotationMatrix() * glm::vec4(glm::vec3(0, 0, -0.01), 0.0f);
              //multiplies the direction of movement by the rotations
              glm::vec3 offset = glm::vec3(rotatedVector.x, rotatedVector.y, rotatedVector.z);
              //gets the offset in order to move forward/backward based on the way the camera is facing

              if (glfwGetKey(window, GLFW_KEY_W))//moves forward
              {
                  this->x += offset.x;
                  this->z += offset.z;
              }
              else //moves backward
              {
                  this->x -= offset.x;
                  this->z -= offset.z;
              }
          }

          if (glfwGetKey(window, GLFW_KEY_A)) //rotates the model left
          {
              this->thetaY += 0.1f;
          }
          else if (glfwGetKey(window, GLFW_KEY_D))//rotates the model right
          {
              this->thetaY -= 0.1f;
          }

          if ((glfwGetKey(window, GLFW_KEY_Q) || glfwGetKey(window, GLFW_KEY_E)))
          {
              if (glfwGetKey(window, GLFW_KEY_Q) && this->y <= 0)//moves the submarine up...
              {
                  this->y += 0.01;
                  if (this->y > 0) {//...unless it's at the top of the ocean, at which point, it can't go any further up
                      this->y = 0.0f;
                  }
              }
              else if (glfwGetKey(window, GLFW_KEY_E))//moves down
              {
                  this->y -= 0.01;
              }
          }
          std::cout << this->y << std::endl;//prints the current depth of the submarine
      }
};

class Texture {//for loading textures
protected: GLuint texture;//actual texture
public:
    Texture() = default;
    Texture(const char* filepath, bool alpha)
    {//loads the texture
        int img_width, img_height, colorChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* tex_bytes = stbi_load(filepath, &img_width, &img_height, &colorChannels, 0);
        glGenTextures(1, &this->texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, this->texture);

        if(alpha==true)//chooses between rgba and rgb based on the boolean parameter
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_bytes);
        else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, tex_bytes);

        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(tex_bytes);//cleans up
    }
    GLuint* getTexture()// retrieves the texture
    {
        return &this->texture;
    }

};

class TextureNormal: public Texture//for loading the texture normal
{
public:
    TextureNormal() = default;
    TextureNormal(const char* filepath, bool alpha)//same as above, with changes 
    {
        int img_width, img_height, colorChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* tex_bytes = stbi_load(filepath, &img_width, &img_height, &colorChannels, 0);
        glGenTextures(1, &this->texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, this->texture);
        //changes here, because this texture is used for normal mapping
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

        if (alpha == true)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img_width, img_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_bytes);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_width, img_height, 0, GL_RGB, GL_UNSIGNED_BYTE, tex_bytes);

        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(tex_bytes);
    }
};
class Filter : public Entity//filter used for the first person view
{
private: Camera* attachedCam;//for attaching the filter to the camera
       float rotation; //amount of rotation for polar movement
       void updatePos()//updates the position of the filter based on the location and rotation of the camera
       {
           y = this->attachedCam->getCameraPos().y;
           Polar(0.2, this->rotation, &this->x, &this->z, this->attachedCam->getCameraPos().x, this->attachedCam->getCameraPos().z);
       }
public: 
    Filter() = default;

    Filter(float x, float y, float z,
        float thetaX, float thetaY, float thetaZ,
        float sx, float sy, float sz, Model model)
        : Entity(x, y, z,
            thetaX, thetaY, thetaZ,
            sx, sy, sz, model) {}
    void attachCam(Camera* attachedCam)//attaches the filter to the camera
    {
        this->attachedCam = attachedCam;
        this->thetaY = 0;
        this->rotation = 270;
        this->updatePos();
    }
    void rotateCam(GLFWwindow* window) //horizontally rotates and moves the filter based on the camera
    {
        if (glfwGetKey(window, GLFW_KEY_A))//increments and decrements the related values based on the key pressed
        {
            thetaY += 0.1;
            rotation -= 0.1;
        }
        if (glfwGetKey(window, GLFW_KEY_D))
        {
            thetaY -= 0.1;
            rotation += 0.1;
        }
        this->updatePos();//updates filter position
    }

    glm::vec3 getCamFront()
    {//used for conveniently positioning the point light

        Polar(2.5f, this->rotation, &this->x, &this->z, this->attachedCam->getCameraPos().x, this->attachedCam->getCameraPos().z);
        //moves the filter to the position 2.5 ahead of where the submarine is (rotations accounted for)
        glm::vec3 camFront = this->getPos();//saves this position

        Polar(0.2f, this->rotation, &this->x, &this->z, this->attachedCam->getCameraPos().x, this->attachedCam->getCameraPos().z);
        //moves it back to where it originally was
        return camFront;//returns the saved position
    }
};

class ThirdPersonCamera : public PerspectiveCamera
{
private:
    Player* submarine;//attached submarine
    float thetaY;//for amount to rotate via polar movement
public: 

    ThirdPersonCamera(float width, float height, Player* submarine) : PerspectiveCamera(width, height)
    {//constructor
        this->firstMouse = true;
        this->submarine = submarine;//attaches the submarine
        this->Center = this->submarine->getPos();
        this->cameraPos = glm::vec3(0.0f, 0.0f, Center.z + 6);
        this->thetaY = 90;
        this->updatePos();
     
    }
    
    void updatePos()//updates the position of the camera based on the values changed by MouseControls below
    {
        //moves the camera to the appropriate position
        Polar(6, this->thetaY, &this->cameraPos.x, &this->cameraPos.z,
            this->submarine->getPos().x, this->submarine->getPos().z);
 
        this->cameraPos.y = this->submarine->getPos().y;//sets the camera to the same depth as the subs
        this->Center = submarine->getPos();//sets the center of the camera to the submarine
    }

    void MouseControls(GLFWwindow* window, double xpos, double ypos)
    {
        glfwGetCursorPos(window, &xpos, &ypos);
        //stores the x and y positions of the cursor in xpos, and ypos respectively
        if (this->firstMouse)
        {
            this->lastX = xpos;
            this->firstMouse = false;
        }//assignes the initial "last x and y values" of the mouse cursor to be the ones initially gotten to avoid a sudden jump in camera movement

        float offsetX = xpos - this->lastX;//gets the offset to move by based on horizontal movement
        this->lastX = xpos;
        
        offsetX *= 0.1f;//multiplies the offsets gotten earlier by 0.1 to control the camera movement so it doesn't
                         //go crazy

        this->thetaY += offsetX;//moves the camera around the submarine based on the offset later
         }
    };
class FPSCamera :public PerspectiveCamera
{
private:
    Filter* filter;//attached filter to the camera

public:

    FPSCamera(float width, float height,Filter* filter) :PerspectiveCamera(width,height) //constructor
    {
        this->filter = filter; //attaches the camera to the filter
        this->filter->attachCam(this);
    }

    Filter* getFilter()//returns the filter
    {
        return this->filter;
    }
    void PlayerControls(GLFWwindow* window)
    {//moves the camera alongside the player (same as player movement)
        if (glfwGetKey(window, GLFW_KEY_W) || glfwGetKey(window, GLFW_KEY_S))
        {
            glm::vec4 rotatedVector = getRotationMatrix() * glm::vec4(glm::vec3(0, 0, -0.01), 0.0f);
            //multiplies the direction of movement by the rotations
            glm::vec3 offset2 = glm::vec3(rotatedVector.x, rotatedVector.y, rotatedVector.z);
            if (glfwGetKey(window, GLFW_KEY_W))//moves forward
            {
                cameraPos += offset2;//moves both the camera and center by the offset
                Center += offset2;
            }
            else //moves backward
            {
                cameraPos -= offset2;
                Center -= offset2;
            }
            //gets the offset in order to move forward/backward based on the way the camera is facing
        }

        if (glfwGetKey(window, GLFW_KEY_A)) //rotates the camera left
        {
            this->rotation2 -= 0.1f;
        }
        else if (glfwGetKey(window, GLFW_KEY_D))//rotates the camera right
        {
            this->rotation2 += 0.1;
        }

        if ((glfwGetKey(window, GLFW_KEY_Q) || glfwGetKey(window, GLFW_KEY_E)))
        {
            if (glfwGetKey(window, GLFW_KEY_Q) && this->cameraPos.y <= 0)//moves up unless the ceiling depth of 0 is reached
            {
                cameraPos += glm::vec3(0, 0.01, 0);
                Center += glm::vec3(0, 0.01, 0);
                if (this->cameraPos.y > 0) {
                    this->cameraPos.y = 0.0f;
                    this->Center.y = 0.0f;
                }
            }
            else if (glfwGetKey(window, GLFW_KEY_E))//moves down
            {
                cameraPos -= glm::vec3(0, 0.01, 0);
                Center -= glm::vec3(0, 0.01, 0);
            }
        }
        this->filter->rotateCam(window);//rotates the filter based on the camera's orientation
    }
};

// callbacks below are for getting the keyboard and mouse inputs
void Key_Callback(GLFWwindow* window, int key, int scancode, int action, int mods)
//window pointer, keycode of the press, physical position of the press, press/hold, modifier keys
{}

void Mouse_Callback(GLFWwindow* window, double xpos, double ypos)
{}

void Mouse_Click_Callback(GLFWwindow* window, int button, int action, int mods)
{}

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(WIDTH, HEIGHT, "Machine_Project", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    gladLoadGL();

    //sets up the viewport
    glViewport(0, 0, WIDTH, HEIGHT);

    //the lines below allow for keyboard and mouse input
    glfwSetKeyCallback(window, Key_Callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetCursorPosCallback(window, Mouse_Callback);
    glfwSetMouseButtonCallback(window, Mouse_Click_Callback);

    //loads the textures for the submarine, enemies, and filter.
    Texture submarineTex("3D/submarine.jpg", false), 
     enemyTex1("3D/Freighter_C.jpg", false),
     enemyTex2("3D/Intergalactic Spaceship_rough.jpg", false),
     enemyTex3("3D/Star_Fighter_color.jpg", false),
     enemyTex4("3D/Intergalactic Spaceship_color_4.jpg", false),
     enemyTex5("3D/Aircraft C.jpg", false),
     enemyTex6("3D/Diffuse_3.png", true),
     gradient("3D/gradient.png", true);
    //for submarine normals
    TextureNormal submarineNormalTex("3D/submarine_normal.jpg", false);
    glEnable(GL_DEPTH_TEST);

    //skybox texture locations
    std::string facesSkybox[]
    {
    "Skybox/uw_lf.jpg",
    "Skybox/uw_rt.jpg",
    "Skybox/uw_up.jpg",
    "Skybox/uw_dn.jpg",
    "Skybox/uw_ft.jpg",
    "Skybox/uw_bk.jpg" };


    //skybox vertices and indices
    float size = 10.f;
    float skyboxVertices[]{
        -size, -size, size, //0
        size, -size, size,  //1
        size, -size, -size, //2
        -size, -size, -size,//3
        -size, size, size,  //4
        size, size, size,   //5
        size, size, -size,  //6
        -size, size, -size  //7
    };

    //Skybox Indices
    unsigned int skyboxIndices[]{
        1,2,6,
        6,5,1,

        0,4,7,
        7,3,0,

        4,5,6,
        6,7,4,

        0,3,2,
        2,1,0,

        0,1,5,
        5,4,0,

        3,7,6,
        6,2,3
    };
    //skybox texture loading
    unsigned int skyboxTex;
    glGenTextures(1, &skyboxTex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTex);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_LINEAR);
    for (unsigned int i = 0; i < 6; i++)
    {
        int w, h, skyCChannel;
        stbi_set_flip_vertically_on_load(false);
        unsigned char* data = stbi_load(facesSkybox[i].c_str(), &w, &h, &skyCChannel, 0);

        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }

    }
    stbi_set_flip_vertically_on_load(true);
   
    //loads the shaders for the submarine, enemies, filter and skybox
    Shader sub("Shaders/submarine.vert","Shaders/submarine.frag"), main("Shaders/sample.vert", "Shaders/sample.frag"), filter("Shaders/filter.vert", "Shaders/filter.frag"),
        underwater("Shaders/skybox.vert","Shaders/skybox.frag");

    std::vector<Light*> Lights = { new DirectionLight(),new PointLight() };
    PointLight* Point = (PointLight*)Lights[1];
    //light 1 is direction, light 2 is point light

    //creates submarine and positions it at the origin
    Player Submarine(0, 0, 0, 0, 0, 0, 0.8, 0.8, 0.8, Model("3D/submarine.obj"));

    Entity //creates the six enemies
    Enemy(0, -2, -20, 0, 0, 0, 0.5, 0.5, 0.5, Model("3D/Freigther_BI_Export.obj", true, true)),
    Enemy2(0, -15, 20, 0, 180, 0, 0.5, 0.5, 0.5, Model("3D/Intergalactic_Spaceship-(Wavefront).obj", true, true)),
    Enemy3(20, -10, 0, 0, 0, 0, 0.5, 0.5, 0.5, Model("3D/Star_Fighter.obj", true, true)),
    Enemy4(-20, -7, 0, 0, 90, 0, 0.5, 0.5, 0.5, Model("3D/Intergalactic_Spaceships_Version_2.obj", true, true)),
    Enemy5(10, -4, 10, 0, 225, 0, 0.5, 0.5, 0.5, Model("3D/Futuristic combat jet.obj", true, true)),
    Enemy6(-6, -6, -6, 0, 225, 0, 5, 5, 5, Model("3D/Underwater Vehicle.obj", true, true));;
    //creates the filter for the FPS
    Filter Plane(0, 0, -0.2, 0, 0, 0, 3, 3, 1, Model("3D/plane.obj", true, true));

    //Camera Creation and attachment to the filter/submarine if necessary
    FPSCamera FPS(WIDTH,HEIGHT,&Plane);
    ThirdPersonCamera TPS(WIDTH, HEIGHT, &Submarine);
    OrthoCamera BirdsEye;
    std::vector <Camera*> Cameras =//pushes them into a vector for easy access
    {
        &FPS,&TPS,&BirdsEye
    };

    //for the skybox later
    auto BirdsEyeView = FPS.getViewMatrix();
    auto BirdsEyeProjection = FPS.getProjection();


    



    unsigned int skyboxVAO, skyboxVBO, skyboxEBO;//skybox "model" creation
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glGenBuffers(1, &skyboxEBO);

    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        sizeof(GL_INT) * 36,
        &skyboxIndices,
        GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);

    glm::mat4 sky_view = glm::mat4(1.f);

    glEnable(GL_BLEND);//for the filter
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Point->setLightPos(FPS.getFilter()->getPos());//sets the light's position to the light source model's

    bool release = true;//to avoid constant camera flipping from holding the 1 key
    int cameraIndex = 1;//camera being used
    float scale = 2.5;//scale of the skybox
    double xpos = 0 , ypos=0;//for mouse movement

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (glfwGetKey(window, GLFW_KEY_1) && release)
        {//switches the camera from FPS to TPS and vice versa
            if (cameraIndex == 1)
            {
                cameraIndex = 0;
                scale = 5.5;//larger skybox, means more distance the camera can see
            }
                
            else
            {
                cameraIndex = 1;
                scale = 2.5;//smaller skybox, means less  distance the camera can see
            }
                
            release = false;//cannot switch until the one key is released
        }
        else if (glfwGetKey(window, GLFW_KEY_2))//switches to birds eye view camera
        {
            BirdsEye.resetPos(Submarine.getPos());//resets the ortho camera to be above the submarine
            scale = 8.5;
            cameraIndex = 2;
        }
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE)
            release = true;


           
           if (cameraIndex != 2)
           {//allows the player to move the camera and submarine as long as ortho cam isn't enabled
              Submarine.PlayerControls(window);
              FPS.PlayerControls(window);
           }
           else//only allows panning the camera around in ortho view
             BirdsEye.PlayerControl(window);

           if (cameraIndex == 1)
           {   //moves the TPS camera around when it is enabled, and while left click is held
               if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT))
                   TPS.MouseControls(window, xpos, ypos);
               if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
                   TPS.resetMouse();//resets the first mouse bool when left click is released
           }
            TPS.updatePos();//updates the position of the TPS
            Point->setLightPos(FPS.getFilter()->getCamFront());//sets the point light's position to be ahead of the sub
            Point->cycleIntensity(window);//allows for the intensity of the pointlight to be cycled when F is pressed
       
         glDepthMask(GL_FALSE);//prepares to draw the skybox
         glDepthMask(GL_LEQUAL);
         underwater.useProgram();//skybox shader is used
       
         //updates the skybox's rotation scale based on the camera for view distance
        glm::mat4 transformation_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale, scale, scale)); 
        unsigned int SviewLoc = glGetUniformLocation(*underwater.getProgram(), "view");
        unsigned int SprojLoc = glGetUniformLocation(*underwater.getProgram(), "projection");
        unsigned int StransformLoc = glGetUniformLocation(*underwater.getProgram(), "transform");
        if (cameraIndex == 2) //if current camera is ortho/birds eye
        {   //camera is rotated to show the bottom of the skybox
            transformation_matrix = glm::rotate(transformation_matrix, glm::radians(90.f), glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f)));
            //drawn in perspective so the skybox is shown properly
            sky_view = glm::mat4(glm::mat3(BirdsEyeView));
            glUniformMatrix4fv(SprojLoc, 1, GL_FALSE, glm::value_ptr(BirdsEyeProjection));
        }
        else
        {//skybox is drawn appropriately based on the current camera
            sky_view = glm::mat4(glm::mat3(Cameras.at(cameraIndex)->getViewMatrix()));
            glUniformMatrix4fv(SprojLoc, 1, GL_FALSE, glm::value_ptr(Cameras.at(cameraIndex)->getProjection()));
        }
        //passes the data to the shaders
        glUniformMatrix4fv(SviewLoc, 1, GL_FALSE, glm::value_ptr(sky_view));
        glUniformMatrix4fv(StransformLoc, 1, GL_FALSE, glm::value_ptr(transformation_matrix));
        //draws the skybox
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTex);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glDepthMask(GL_TRUE);
        glDepthMask(GL_LESS);


        main.useProgram();// enemy shader is used
        //enemies are drawn below
        Enemy.draw(main.getProgram(),Cameras.at(cameraIndex), enemyTex1.getTexture(),NULL,Lights);
        Enemy2.draw(main.getProgram(), Cameras.at(cameraIndex), enemyTex2.getTexture(), NULL, Lights);
        Enemy3.draw(main.getProgram(), Cameras.at(cameraIndex), enemyTex3.getTexture(), NULL, Lights);
        Enemy4.draw(main.getProgram(), Cameras.at(cameraIndex), enemyTex4.getTexture(), NULL, Lights);
        Enemy5.draw(main.getProgram(), Cameras.at(cameraIndex), enemyTex5.getTexture(), NULL, Lights);
        Enemy6.draw(main.getProgram(), Cameras.at(cameraIndex), enemyTex6.getTexture(), NULL, Lights);
        if (cameraIndex == 0)
        {//draws the filter if the current camera is FPS
          filter.useProgram();//uses filter's shader
          FPS.getFilter()->draw(filter.getProgram(), &FPS, gradient.getTexture(),NULL,Lights);
        }
        else//doesn't draw submarine in FPS so the camera isn't blocked by it
        {
            sub.useProgram();//uses submarine's shader
            Submarine.draw(sub.getProgram(), Cameras.at(cameraIndex), submarineTex.getTexture(), submarineNormalTex.getTexture(), Lights);
        }//draws the submarine above


        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    //deletes the VAO and VBOs of the models used
    Submarine.cleanUp();
    Enemy.cleanUp();
    Enemy2.cleanUp();
    Enemy3.cleanUp();
    Enemy4.cleanUp();
    Enemy5.cleanUp();
    Enemy6.cleanUp();
    Plane.cleanUp();

    //cleans up the skybox by deleting its VAO,VBO and EBO
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteBuffers(1, &skyboxEBO);

    glfwTerminate();
    return 0;
}