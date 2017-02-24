#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <stdlib.h>

#include <mpg123.h>
#include <ao/ao.h>
#include <assert.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;

#define BITS 8

struct VAO {
    GLuint VertexArrayID;
    GLuint VertexBuffer;
    GLuint ColorBuffer;

    GLenum PrimitiveMode;
    GLenum FillMode;
    int NumVertices;
};
typedef struct VAO VAO;

struct GLMatrices {
	glm::mat4 projection;
	glm::mat4 model;
	glm::mat4 view;
	GLuint MatrixID;
} Matrices;

typedef struct Block {
  float initY;
  float x;
  float y;
  int type;
} Block;

typedef struct Bucket {
  float topLeft;
  float topRight;
  float bottomLeft;
  float bottomRight;
  float initLeft;
  bool selected;
} Bucket;

typedef struct Mirror {
  float x;
  float y;
  float length;
  float angle;
  float intersectionX;
} Mirror;

typedef struct Cannon {
  float y;
  float angle;
  static const float length = 1.5;
  static const float thickness = 0.3;
  bool selected;
} Cannon;

typedef struct Points {
  float x;
  float y;
} Points;

int score, mirrorCount, wrongHits, wrongCatch;
float updateTime = 1, speed = 0.05, rayPoints[2];
double mouseX, mouseY;
Points potentialIntersections[10];
Block blockInfo[5000];
Bucket bucketInfo[2];
Mirror mirrorInfo[5];
Cannon cannonInfo;
VAO *bucket[2], * blocks[5000], * mirrors[5], *deathRay[100], *scoreTile[3][7], *cannon, *scoreBackground, *battery, *batteryTip, *batteryStatus;
bool selected, * keyStates = new bool[500];
static const float screenLeftX = -11.0;
static const float screenRightX = 11.0;
static const float screenTopY = 6.0;
static const float screenBottomY = -6.0;
static const float scoreLeftX = 5.0;
static const float scoreRightX = 10.0;
float displayLeft = -11.0, displayRight = 5.0, displayTop = 6.0, displayBottom = -6.0, horizontalZoom = 0, verticalZoom = 0;
// Indicate amount of "juice" left in battery
float juiceStartX = screenLeftX + 0.2, juiceStartY = screenTopY - 0.5, juiceEndY = screenTopY - 1.0, juiceEndX = juiceStartX;
GLFWwindow* windowCopy;

GLuint programID;

/* Function to load Shaders - Use it as it is */
GLuint LoadShaders(const char * vertex_file_path,const char * fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode;
	std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if(VertexShaderStream.is_open())
	{
		std::string Line = "";
		while(getline(VertexShaderStream, Line))
			VertexShaderCode += "\n" + Line;
		VertexShaderStream.close();
	}

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode;
	std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if(FragmentShaderStream.is_open()){
		std::string Line = "";
		while(getline(FragmentShaderStream, Line))
			FragmentShaderCode += "\n" + Line;
		FragmentShaderStream.close();
	}

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	// printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> VertexShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
	// fprintf(stdout, "%s\n", &VertexShaderErrorMessage[0]);

	// Compile Fragment Shader
	// printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> FragmentShaderErrorMessage(InfoLogLength);
	glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
	// fprintf(stdout, "%s\n", &FragmentShaderErrorMessage[0]);

	// Link the program
	// fprintf(stdout, "Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	std::vector<char> ProgramErrorMessage( max(InfoLogLength, int(1)) );
	glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
	// fprintf(stdout, "%s\n", &ProgramErrorMessage[0]);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

static void error_callback(int error, const char* description)
{
    // fprintf(stderr, "Error: %s\n", description);
}

void quit(GLFWwindow *window)
{
    glfwDestroyWindow(window);
    glfwTerminate();
//    exit(EXIT_SUCCESS);
}

/* Generate VAO, VBOs and return VAO handle */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat* color_buffer_data, GLenum fill_mode=GL_FILL)
{
    struct VAO* vao = new struct VAO;
    vao->PrimitiveMode = primitive_mode;
    vao->NumVertices = numVertices;
    vao->FillMode = fill_mode;

    // Create Vertex Array Object
    // Should be done after CreateWindow and before any other GL calls
    glGenVertexArrays(1, &(vao->VertexArrayID)); // VAO
    glGenBuffers (1, &(vao->VertexBuffer)); // VBO - vertices
    glGenBuffers (1, &(vao->ColorBuffer));  // VBO - colors

    glBindVertexArray (vao->VertexArrayID); // Bind the VAO
    glBindBuffer (GL_ARRAY_BUFFER, vao->VertexBuffer); // Bind the VBO vertices
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), vertex_buffer_data, GL_STATIC_DRAW); // Copy the vertices into VBO
    glVertexAttribPointer(
                          0,                  // attribute 0. Vertices
                          3,                  // size (x,y,z)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    glBindBuffer (GL_ARRAY_BUFFER, vao->ColorBuffer); // Bind the VBO colors
    glBufferData (GL_ARRAY_BUFFER, 3*numVertices*sizeof(GLfloat), color_buffer_data, GL_STATIC_DRAW);  // Copy the vertex colors
    glVertexAttribPointer(
                          1,                  // attribute 1. Color
                          3,                  // size (r,g,b)
                          GL_FLOAT,           // type
                          GL_FALSE,           // normalized?
                          0,                  // stride
                          (void*)0            // array buffer offset
                          );

    return vao;
}

/* Generate VAO, VBOs and return VAO handle - Common Color for all vertices */
struct VAO* create3DObject (GLenum primitive_mode, int numVertices, const GLfloat* vertex_buffer_data, const GLfloat red, const GLfloat green, const GLfloat blue, GLenum fill_mode=GL_FILL)
{
    GLfloat* color_buffer_data = new GLfloat [3*numVertices];
    for (int i=0; i<numVertices; i++) {
        color_buffer_data [3*i] = red;
        color_buffer_data [3*i + 1] = green;
        color_buffer_data [3*i + 2] = blue;
    }

    return create3DObject(primitive_mode, numVertices, vertex_buffer_data, color_buffer_data, fill_mode);
}

/* Render the VBOs handled by VAO */
void draw3DObject (struct VAO* vao)
{
    // Change the Fill Mode for this object
    glPolygonMode (GL_FRONT_AND_BACK, vao->FillMode);

    // Bind the VAO to use
    glBindVertexArray (vao->VertexArrayID);

    // Enable Vertex Attribute 0 - 3d Vertices
    glEnableVertexAttribArray(0);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->VertexBuffer);

    // Enable Vertex Attribute 1 - Color
    glEnableVertexAttribArray(1);
    // Bind the VBO to use
    glBindBuffer(GL_ARRAY_BUFFER, vao->ColorBuffer);

    // Draw the geometry !
    glDrawArrays(vao->PrimitiveMode, 0, vao->NumVertices); // Starting from vertex 0; 3 vertices total -> 1 triangle
}

/**************************
 * Game specific code *
 **************************/

void checkAndSelect();
void moveSelected();
void resetMouseCoordinates();
void resetMirrors();
void resetPotentialIntersections();
void resetSelectState();

void zoomIn()
{
  horizontalZoom = min(horizontalZoom + 0.025f*(displayRight - displayLeft), 2.0f);
  displayRight = max(displayRight - horizontalZoom, 5.0f);
  displayLeft = displayRight - (screenRightX - screenLeftX - 6.0f) + 2*horizontalZoom;

  verticalZoom = min(verticalZoom + 0.025f*(displayTop - displayBottom), 1.5f);
  displayTop = displayBottom + (screenTopY - screenBottomY) - 2*verticalZoom;
}

void zoomOut()
{
  horizontalZoom = max(horizontalZoom - 0.025f*(displayRight - displayLeft), 0.0f);
  displayRight = min(displayRight + horizontalZoom, 5.0f);
  displayLeft = displayRight - (screenRightX - screenLeftX - 6.0f) + 2*horizontalZoom;

  verticalZoom = max(verticalZoom - 0.025f*(displayTop - displayBottom), 0.0f);
  displayTop = displayBottom + (screenTopY - screenBottomY) - 2*verticalZoom;
}

void panLeft()
{
  float temp = displayRight - displayLeft;
  displayLeft = max(displayLeft - 0.1f, screenLeftX);
  displayRight = displayLeft + temp;
}

void panRight()
{
  float temp = displayRight - displayLeft;
  displayRight = min(displayRight + 0.1f, screenRightX - 6.0f);
  displayLeft = displayRight - temp;
}

/* Executed when a regular key is pressed/released/held-down */
/* Prefered for Keyboard events */
void keyboard (GLFWwindow* window, int key, int scancode, int action, int mods)
{
  // Function is called first on GLFW_PRESS.
  if (action == GLFW_RELEASE) {
    keyStates[key] = false;
  }
  else if (action == GLFW_PRESS) {
    keyStates[key] = true;
    switch (key) {
        case GLFW_KEY_I:
            updateTime = max(updateTime - 0.25, 0.25);
            break;
        case GLFW_KEY_O:
            updateTime = min(updateTime + 0.25, 1.75);
            break;
        case GLFW_KEY_N:
            speed = min(speed + 0.01, 0.1);
            break;
        case GLFW_KEY_M:
            speed = max(speed - 0.001, 0.001);
            break;
        case GLFW_KEY_Q:
            quit(window);
            break;
        default:
            break;
    }
  }
}

void keyStateCheck()
{
  // Bucket Controls
  if (keyStates[GLFW_KEY_LEFT])
  {
    if(keyStates[GLFW_KEY_LEFT_ALT] || keyStates[GLFW_KEY_RIGHT_ALT])
    {
      bucketInfo[0].topLeft = max(bucketInfo[0].topLeft-0.1, screenLeftX+1.0);
      bucketInfo[0].topRight = bucketInfo[0].topLeft + 1.6;
    }
    else if(keyStates[GLFW_KEY_LEFT_CONTROL] || keyStates[GLFW_KEY_RIGHT_CONTROL])
    {
      bucketInfo[1].topLeft = max(bucketInfo[1].topLeft-0.1, screenLeftX+1.0);
      bucketInfo[1].topRight = bucketInfo[1].topLeft + 1.6;
    }

    // Pan Controls
    else
    {
      panLeft();
    }
  }
  if (keyStates[GLFW_KEY_RIGHT])
  {
    if(keyStates[GLFW_KEY_LEFT_ALT] || keyStates[GLFW_KEY_RIGHT_ALT])
    {
      bucketInfo[0].topRight = min(bucketInfo[0].topRight+0.1, screenRightX-7.0);
      bucketInfo[0].topLeft = bucketInfo[0].topRight - 1.6;
    }
    else if(keyStates[GLFW_KEY_LEFT_CONTROL] || keyStates[GLFW_KEY_RIGHT_CONTROL])
    {
      bucketInfo[1].topRight = min(bucketInfo[1].topRight+0.1, screenRightX-7.0);
      bucketInfo[1].topLeft = bucketInfo[1].topRight - 1.6;
    }

    // Pan Controls
    else
    {
      panRight();
    }
  }

  // Cannon Controls
  if(keyStates[GLFW_KEY_A])
  {
    cannonInfo.angle = min(cannonInfo.angle+0.1, 45.0);
  }

  if(keyStates[GLFW_KEY_D])
  {
    cannonInfo.angle = max(cannonInfo.angle-0.1, -45.0);
  }
  if(keyStates[GLFW_KEY_S])
  {
    cannonInfo.y = min(cannonInfo.y+0.1, 3.5);
    rayPoints[1] = cannonInfo.y;
  }
  if(keyStates[GLFW_KEY_F])
  {
    cannonInfo.y = max(cannonInfo.y-0.1, -3.5);
    rayPoints[1] = cannonInfo.y;
  }

  // Mouse Controls
  if(keyStates[GLFW_MOUSE_BUTTON_LEFT])
  {
    glfwGetCursorPos(windowCopy, &mouseX, &mouseY);
    mouseX = (mouseX*2*screenRightX/1100) - screenRightX;
    mouseY = screenTopY - (mouseY*2*screenTopY/600);
    // Mouse selection
    checkAndSelect();
    moveSelected();
  }

  if(keyStates[GLFW_MOUSE_BUTTON_RIGHT])
  {
    double tempX, tempY;
    tempX = (mouseX*2*screenRightX/1100) - screenRightX;
    tempY = screenTopY - (mouseY*2*screenTopY/600);
    glfwGetCursorPos(windowCopy, &mouseX, &mouseY);
    mouseX = (mouseX*2*screenRightX/1100) - screenRightX;
    mouseY = screenTopY - (mouseY*2*screenTopY/600);

    // Mouse selection
    if(mouseX < tempX)
    {
      panLeft;
    }
    else if(mouseX > tempX)
    {
      panRight;
    }
  }

  // Zoom Controls
  if(keyStates[GLFW_KEY_UP])
  {
    zoomIn();
  }
  else if(keyStates[GLFW_KEY_DOWN])
  {
    zoomOut();
  }
}

/* Executed for character input (like in text boxes) */
void keyboardChar (GLFWwindow* window, unsigned int key)
{
	switch (key) {
		case 'Q':
		case 'q':
            quit(window);
            break;
		default:
			break;
	}
}

/* Executed when a mouse button is pressed/released */
void mouseButton (GLFWwindow* window, int button, int action, int mods)
{
  switch (button) {
      case GLFW_MOUSE_BUTTON_LEFT:
          if (action == GLFW_RELEASE) {
              keyStates[button] = false;
              resetMouseCoordinates();
              resetSelectState();
          }
          else if (action == GLFW_PRESS) {
              keyStates[button] = true;
          }
          break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            if (action == GLFW_RELEASE) {
                keyStates[button] = false;
                resetMouseCoordinates();
                resetSelectState();
              }
            else if (action == GLFW_PRESS) {
                keyStates[button] = true;
            }
            break;
      default:
          break;
  }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  if(yoffset == 1)
  {
    zoomIn();
  }
  else if(yoffset == -1)
  {
    zoomOut();
  }
}

void draw();
/* Executed when window is resized to 'width' and 'height' */
/* Modify the bounds of the screen here in glm::ortho or Field of View in glm::Perspective */
void reshapeWindow (GLFWwindow* window, int width, int height)
{
    int fbwidth=width, fbheight=height;
    /* With Retina display on Mac OS X, GLFW's FramebufferSize
     is different from WindowSize */
    glfwGetFramebufferSize(window, &fbwidth, &fbheight);

    GLfloat fov = 90.0f;
    glEnable(GL_SCISSOR_TEST);

    glViewport ((GLsizei) (fbwidth - 300), 0, (GLsizei) fbwidth, (GLsizei) fbheight);
    glScissor((GLsizei) (fbwidth - 300), 0, (GLsizei) fbwidth, (GLsizei) fbheight);
    Matrices.projection = glm::ortho(screenRightX - 6.0f, screenRightX*2.0f, screenBottomY, screenTopY, 0.1f, 500.0f);
    draw();

    glViewport (0, 0, (GLsizei) (fbwidth - 300), (GLsizei) fbheight);
    glScissor (0, 0, (GLsizei) (fbwidth - 300), (GLsizei) fbheight);
    Matrices.projection = glm::ortho(displayLeft, displayRight, displayBottom, displayTop, 0.1f, 500.0f);
    draw();
}

void initialize()
{
  int i;

  score = 0;
  wrongHits = 0;
  wrongCatch = 0;
  srand((unsigned)time(0));

  /* Initializing y - coordinates of all blocks to something outside range - Replaced by actual coordinates on creation */
  for (i = 0; i < 5000; i++)
  {
    blockInfo[i].y = -100;
    blockInfo[i].type = 0;
  }

  // Initializing the pressed state of all keys to false
  for(i = 0; i < 500; i++)
  {
    keyStates[i] = false;
  }

  // Initializing the coordinates of all mirrors to something outside range - Replaced by actual coordinates on creation
  for(i = 0; i < 5; i++)
  {
    mirrorInfo[i].x = -100;
    mirrorInfo[i].y = -100;
  }

  cannonInfo.y = 0;
  cannonInfo.angle = 0;

  rayPoints[0] = screenLeftX;
  rayPoints[1] = 0.0;

  resetMouseCoordinates();
  resetMirrors();
  resetPotentialIntersections();
  resetSelectState();
}

void checkAndSelect()
{
  int i, flag = 0;

  if(selected)
    return;

  // Check if Bucket is selected
  for(i = 0; i < 2; i++)
  {
    if(mouseY <= screenBottomY + 1.0 && screenBottomY <= mouseY)
    {
      float C1 = screenBottomY - mouseY + (mouseX - bucketInfo[i].bottomLeft)/(bucketInfo[i].topLeft - bucketInfo[i].bottomLeft);
      float C2 = (bucketInfo[i].bottomRight - bucketInfo[i].bottomLeft)/(bucketInfo[i].topLeft - bucketInfo[i].bottomLeft);

      float C3 = screenBottomY - mouseY + (mouseX - bucketInfo[i].bottomRight)/(bucketInfo[i].topRight - bucketInfo[i].bottomRight);
      float C4 = (bucketInfo[i].bottomLeft - bucketInfo[i].bottomRight)/(bucketInfo[i].topRight - bucketInfo[i].bottomRight);

      if(C1*C2 >= 0 && C3*C4 >= 0)
      {
        bucketInfo[i].selected = true;
        selected = true;
        flag++;
        break;
      }
    }
  }

  // Check if Cannon is selected
  if(flag == 0)
  {
    float Y2 = cannonInfo.y + cannonInfo.length*sin(cannonInfo.angle*M_PI/180.0);

    if(mouseY <= max(cannonInfo.y, Y2) && min(cannonInfo.y, Y2) <= mouseY)
    {
      if(mouseX <= screenLeftX + cannonInfo.length*cos(cannonInfo.angle*M_PI/180.0))
      {
        cannonInfo.selected = true;
        selected = true;
      }
    }
  }
}

void resetMirrors ()
{
  int i;
  for(i = 0; i < mirrorCount; i++)
  {
    mirrorInfo[i].intersectionX = 100;
  }
}

void resetPotentialIntersections ()
{
  int i;
  for(i = 0; i < mirrorCount; i++)
  {
    potentialIntersections[i].x = 100;
    potentialIntersections[i].y = 100;
  }
}

void resetMouseCoordinates()
{
  mouseX = 100.0;
  mouseY = 100.0;
}

void resetSelectState()
{
  int i;

  selected = false;

  cannonInfo.selected = false;

  for(i = 0; i < 2; i++)
  {
    bucketInfo[i].selected = false;
  }
}
int getMin (Points arr[])
{
  int i, j = 0;
  float x = 100.0, y = 100.0;

  for(i = 0; i < mirrorCount; i++)
  {
    if(arr[i].x < x)
    {
      x = arr[i].x;
      y = arr[i].y;
      j = i;
    }
  }
  return j;
}

int getMax (Points arr[])
{
  int i, j = 0;
  float x = -100.0, y = -100.0;

  for(i = 0; i < mirrorCount; i++)
  {
    if(arr[i].x > x && arr[i].x != 100.0)
    {
      x = arr[i].x;
      y = arr[i].y;
      j = i;
    }
  }
  return j;
}

// Creates the cannon that shoots the death ray
void createCannon ()
{

  const GLfloat vertex_buffer_data [] = {
    screenLeftX + cannonInfo.length, cannonInfo.thickness/2, 0, // vertex 1
    screenLeftX, cannonInfo.thickness/2, 0, // vertex 2
    screenLeftX, -cannonInfo.thickness/2, 0, // vertex 3

    screenLeftX, -cannonInfo.thickness/2, 0, // vertex 3
    screenLeftX + cannonInfo.length, -cannonInfo.thickness/2, 0, // vertex 4
    screenLeftX + cannonInfo.length, cannonInfo.thickness/2, 0  // vertex 1
  };

  const GLfloat color_buffer_data [] = {
    255, 255, 255, // color 1
    255, 255, 255, // color 2
    255, 255, 255, // color 3

    255, 255, 255, // color 3
    255, 255, 255, // color 4
    255, 255, 255,  // color 1
  };

  // create3DObject creates and returns a handle to a VAO that can be used later
  cannon = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

// Creates the death ray that destroys the rogue blocks
void createDeathRay (float startPointX, float startPointY, float endPointX, float endPointY, int val)
{

  const GLfloat vertex_buffer_data [] = {
    endPointX, endPointY, 0, // vertex 1
    startPointX, startPointY, 0, // vertex 2
  };

  const GLfloat color_buffer_data [] = {
    255, 255, 255, // color 1
    255, 255, 255, // color 2
  };

  // create3DObject creates and returns a handle to a VAO that can be used later
  deathRay[val] = create3DObject(GL_LINES, 2, vertex_buffer_data, color_buffer_data, GL_FILL);
}

// Creates the mirror objects that reflect the death ray
void createMirrors ()
{

  int i;

  mirrorCount = rand()%3 + 3;

  for(i = 0; i < mirrorCount; i++)
  {
    mirrorInfo[i].x = -6.94 + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(8.94)));
    mirrorInfo[i].y = -2.94 + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(5.94)));
    mirrorInfo[i].angle = (1.0f + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(88.0f))));
    mirrorInfo[i].length = min(4.0, (screenRightX - 6.0 - mirrorInfo[i].x)/cos(mirrorInfo[i].angle*M_PI/180.0f));

    // GL3 accepts only Triangles. Quads are not supported
    const GLfloat vertex_buffer_data [] = {
      mirrorInfo[i].x, mirrorInfo[i].y, 0, // vertex 1
      mirrorInfo[i].x + mirrorInfo[i].length*cos(mirrorInfo[i].angle*M_PI/180.0f), mirrorInfo[i].y + mirrorInfo[i].length*sin(mirrorInfo[i].angle*M_PI/180.0f), 0, // vertex 2
    };
    const GLfloat color_buffer_data [] = {
      255, 255, 255, // color 1
      255, 255, 255, // color 2
    };

    glLineWidth(12.5);
    // create3DObject creates and returns a handle to a VAO that can be used later
    mirrors[i] = create3DObject(GL_LINES, 2, vertex_buffer_data, color_buffer_data, GL_FILL);
  }
}

// Create Death Ray Battery

void createBattery ()
{
  // GL3 accepts only Triangles. Quads are not supported
  const GLfloat vertex_buffer_data1 [] = {
    screenLeftX + 0.2 , screenTopY - 0.5, 0,   // vertex 1
    screenLeftX + 1.0, screenTopY - 0.5, 0,  // vertex 2
    screenLeftX + 1.0, screenTopY - 1.0, 0,

    screenLeftX + 1.0, screenTopY - 1.0, 0,
    screenLeftX + 0.2, screenTopY - 1.0, 0,
    screenLeftX+ 0.2, screenTopY - 0.5, 0
   };

  const GLfloat color_buffer_data1 [] = {
    255 , 255, 255,
    255 , 255, 255,
    255 , 255, 255,

    255 , 255, 255,
    255 , 255, 255,
    255 , 255, 255
  };

  // create3DObject creates and returns a handle to a VAO that can be used later
  battery = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data1, color_buffer_data1, GL_LINE);

  // GL3 accepts only Triangles. Quads are not supported
  const GLfloat vertex_buffer_data2 [] = {
    screenLeftX + 1.0, screenTopY - 0.70, 0,   // vertex 1
    screenLeftX + 1.1, screenTopY - 0.70, 0,  // vertex 2
    screenLeftX + 1.1, screenTopY - 0.80, 0,

    screenLeftX + 1.1, screenTopY - 0.80, 0,
    screenLeftX + 1.0, screenTopY - 0.80, 0,
    screenLeftX + 1.0, screenTopY - 0.70, 0
   };

  const GLfloat color_buffer_data2 [] = {
    255 , 255, 255,
    255 , 255, 255,
    255 , 255, 255,

    255 , 255, 255,
    255 , 255, 255,
    255 , 255, 255
  };

  // create3DObject creates and returns a handle to a VAO that can be used later
  batteryTip = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data2, color_buffer_data2, GL_FILL);

    // GL3 accepts only Triangles. Quads are not supported
  const GLfloat vertex_buffer_data3 [] = {
    juiceStartX, juiceStartY, 0,
    juiceEndX, juiceStartY, 0,
    juiceEndX, juiceEndY, 0,

    juiceEndX, juiceEndY, 0,
    juiceStartX, juiceEndY, 0,
    juiceStartX, juiceStartY, 0,
   };

  const GLfloat color_buffer_data3 [] = {
    0.0, 255, 0.0,
    0.0, 255, 0.0,
    0.0, 255, 0.0,
    0.0, 255, 0.0,
    0.0, 255, 0.0,
    0.0, 255, 0.0
  };

  batteryStatus = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data3, color_buffer_data3, GL_FILL);

}

// Creates the Buckets used to catch the falling Blocks
void createBuckets ()
{
  int i;
  float r = 255, g = 0, initLoc = -4.5;

  for(i = 0; i < 2; i++)
  {
    // GL3 accepts only Triangles. Quads are not supported
    const GLfloat vertex_buffer_data [] = {
      initLoc - 0.3, screenBottomY, 0, // vertex 1
      initLoc + 0.3, screenBottomY, 0, // vertex 2
      initLoc + 0.8, screenBottomY + 1.0, 0, // vertex 3

      initLoc + 0.8, screenBottomY + 1.0, 0, // vertex 3
      initLoc - 0.8, screenBottomY + 1.0, 0, // vertex 4
      initLoc - 0.3, screenBottomY, 0  // vertex 1
    };

    bucketInfo[i].topLeft = initLoc - 0.8;
    bucketInfo[i].topRight = initLoc + 0.8;
    bucketInfo[i].bottomLeft = initLoc - 0.3;
    bucketInfo[i].bottomRight = initLoc + 0.3;
    bucketInfo[i].initLeft = bucketInfo[i].topLeft;

    const GLfloat color_buffer_data [] = {
      r, g, 0, // color 1
      r, g, 0, // color 2
      r, g, 0, // color 3

      r, g, 0, // color 3
      r, g, 0, // color 4
      r, g, 0  // color 1
    };

    // create3DObject creates and returns a handle to a VAO that can be used later
    bucket[i] = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);

    r = 0;
    g = 255;
    initLoc = -1.5;
  }
}

// Creates the block objects that fall from top
void createBlocks ()
{

  int i, type;
  float xval, r, g, b;

  for(i = 0; i < 5000; i++)
  {
    r = 255, g = 0, b = 0;
    xval = -5.94 + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(9.44)));
    blockInfo[i].x = xval;
    blockInfo[i].initY = 5.7;
    type = rand()%3;
    if(type == 1)
    {
      r = 0, g = 255;
      blockInfo[i].type = 1;
    }

    else if(type == 2)
    {
      r = 0, g = 191, b = 255;
      blockInfo[i].type = 2;
    }
    // GL3 accepts only Triangles. Quads are not supported
    const GLfloat vertex_buffer_data [] = {
      -0.05, 5.7, 0, // vertex 1
      +0.05, 5.7, 0, // vertex 2
      +0.05, 6, 0, // vertex 3

      +0.05, 6, 0, // vertex 3
      -0.05 , 6, 0, // vertex 4
      -0.05, 5.7, 0  // vertex 1
    };

    const GLfloat color_buffer_data [] = {
      r, g, b, // color 1
      r, g, b, // color 2
      r, g, b, // color 3

      r, g, b, // color 3
      r, g, b, // color 4
      r, g, b, // color 1
    };

    // create3DObject creates and returns a handle to a VAO that can be used later
    blocks[i] = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
  }
}

// Creates the score tiles for seven segment display
void createScoreTile ()
{
  int i, j;

  for(i = 0; i < 3; i++)
  {
    for(j = 0; j < 7; j++)
    {
      // GL3 accepts only Triangles. Quads are not supported
      const GLfloat vertex_buffer_data [] = {
        -0.05, 0, 0, // vertex 1
        +0.05, 0, 0, // vertex 2
        +0.05, 1, 0, // vertex 3

        +0.05, 1, 0, // vertex 3
        -0.05 , 1, 0, // vertex 4
        -0.05, 0, 0  // vertex 1
      };

      const GLfloat color_buffer_data [] = {
        255, 255, 255, // color 1
        255, 255, 255, // color 2
        255, 255, 255, // color 3

        255, 255, 255, // color 3
        255, 255, 255, // color 4
        255, 255, 255, // color 1
      };

      // create3DObject creates and returns a handle to a VAO that can be used later
      scoreTile[i][j] = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
    }
  }

  const GLfloat vertex_buffer_data [] = {
    screenRightX - 6.0, screenBottomY, 0, // vertex 1
    screenRightX, screenBottomY, 0, // vertex 2
    screenRightX, screenTopY, 0, // vertex 3

    screenRightX, screenTopY, 0, // vertex 3
    screenRightX - 6.0, screenTopY, 0, // vertex 4
    screenRightX - 6.0, screenBottomY, 0  // vertex 1
  };

  const GLfloat color_buffer_data [] = {
    102, 0, 51, // color 1
    102, 0, 51, // color 2
    102, 0, 51, // color 3

    102, 0, 51, // color 3
    102, 0, 51, // color 4
    102, 0, 51, // color 1
  };

  scoreBackground = create3DObject(GL_TRIANGLES, 6, vertex_buffer_data, color_buffer_data, GL_FILL);
}

void insertBlock()
{
  int i;

  /* Checking if there is any slot left for block creation */
  for(i = 0; i < 5000; i++)
  {
    if(blockInfo[i].y == -100)
    {
      blockInfo[i].y = 0;
      blockInfo[i].x = -5.94 + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(9.44)));
      break;
    }
  }
}

void moveSelected()
{
  int i;
  float temp;

  // Move selected bucket
  for(i = 0; i < 2; i++)
  {
    if(bucketInfo[i].selected)
    {
      bucketInfo[i].topLeft = max(mouseX, screenLeftX + 1.0);
      if(bucketInfo[i].topLeft > 0)
      {
        bucketInfo[i].topLeft = min(mouseX, screenRightX - 8.6);
      }
      bucketInfo[i].topRight = bucketInfo[i].topLeft + 1.6;
      return;
    }
  }

  // Move selected Cannon
  if(cannonInfo.selected)
  {
    cannonInfo.y = max(mouseY, -3.5);
    cannonInfo.y = min(mouseY, 3.5);
    rayPoints[1] = cannonInfo.y;
    return;
  }

  temp = atan2((mouseY - cannonInfo.y) , (mouseX - screenLeftX))*180.0f/M_PI;
  cannonInfo.angle = min(temp, 45.0f);
  if(cannonInfo.angle < 0)
  {
    cannonInfo.angle = max(temp, -45.0f);
  }
}

float camera_rotation_angle = 90;

void drawNumber(int Number, int index)
{
  Matrices.view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane
  glm::mat4 VP = Matrices.projection * Matrices.view;
  glm::mat4 MVP;	// MVP = Projection * View * Model
  glm::mat4 rotateTile = glm::rotate((float)(-90.0f*M_PI/180.0f), glm::vec3(0,0,1));

  if(Number != 1 && Number != 4 && Number != 7)
  {
    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translateTile = glm::translate (glm::vec3(8.25 - (float)index - 0.5f*index, 0.0f, 0.0f)); // glTranslatef
    Matrices.model *= translateTile*rotateTile;
    MVP = VP * Matrices.model; // MVP = p * V * M
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(scoreTile[index][0]);
  }

  if(Number == 0 || Number == 2 || Number == 6 || Number == 8)
  {
    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translateTile = glm::translate (glm::vec3(8.25 - (float)index - 0.5f*index - 0.05f, 0.1f, 0.0f)); // glTranslatef
    Matrices.model *= translateTile;
    MVP = VP * Matrices.model; // MVP = p * V * M
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(scoreTile[index][1]);
  }

  if(Number != 0 && Number != 1 && Number != 7)
  {
    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translateTile = glm::translate (glm::vec3(8.25 - (float)index - 0.5f*index, 1.1f, 0.0f)); // glTranslatef
    Matrices.model *= translateTile*rotateTile;
    MVP = VP * Matrices.model; // MVP = p * V * M
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(scoreTile[index][2]);
  }

  if(Number != 2)
  {
    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translateTile = glm::translate (glm::vec3(8.25 - (float)index - 0.5f*index + 1.05f, 0.1f, 0.0f)); // glTranslatef
    Matrices.model *= translateTile;
    MVP = VP * Matrices.model; // MVP = p * V * M
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(scoreTile[index][3]);
  }

  if(Number != 1 && Number != 2 && Number != 3 && Number != 7)
  {
    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translateTile = glm::translate (glm::vec3(8.25 - (float)index - 0.5f*index - 0.05f, 1.2f, 0.0f)); // glTranslatef
    Matrices.model *= translateTile;
    MVP = VP * Matrices.model; // MVP = p * V * M
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(scoreTile[index][4]);
  }

  if(Number != 1 && Number != 4)
  {
    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translateTile = glm::translate (glm::vec3(8.25 - (float)index - 0.5f*index, 2.20f, 0.0f)); // glTranslatef
    Matrices.model *= translateTile*rotateTile;
    MVP = VP * Matrices.model; // MVP = p * V * M
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(scoreTile[index][5]);
  }

  if(Number != 5 && Number != 6)
  {
    Matrices.model = glm::mat4(1.0f);
    glm::mat4 translateTile = glm::translate (glm::vec3(8.25 - (float)index - 0.5f*index + 1.05f, 1.2f, 0.0f)); // glTranslatef
    Matrices.model *= translateTile;
    MVP = VP * Matrices.model; // MVP = p * V * M
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
    draw3DObject(scoreTile[index][6]);
  }
}

void drawScore()
{
  int i, temp = score;
  for(i = 0; i < 3; i++)
  {
    drawNumber(temp%10, i);
    temp /= 10;
  }
}

/* Render the scene with openGL */
/* Edit this function according to your assignment */
void draw ()
{

  int i, flag = 0, j = 0, temp;
  float tempX, rayAngle, initRayX, initRayY, slopeRay, slopeMirror, cRay, cMirror, rayX2, rayY2, x2, y2, yIntercept, xIntercept;
  // clear the color and depth in the frame buffer
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // use the loaded shader program
  // Don't change unless you know what you are doing
  glUseProgram (programID);

  // Eye - Location of camera. Don't change unless you are sure!!
  glm::vec3 eye ( 5*cos(camera_rotation_angle*M_PI/180.0f), 0, 5*sin(camera_rotation_angle*M_PI/180.0f) );
  // Target - Where is the camera looking at.  Don't change unless you are sure!!
  glm::vec3 target (0, 0, 0);
  // Up - Up vector defines tilt of camera.  Don't change unless you are sure!!
  glm::vec3 up (0, 1, 0);

  // Compute Camera matrix (view)
  // Matrices.view = glm::lookAt( eye, target, up ); // Rotating Camera for 3D
  //  Don't change unless you are sure!!
  Matrices.view = glm::lookAt(glm::vec3(0,0,3), glm::vec3(0,0,0), glm::vec3(0,1,0)); // Fixed camera for 2D (ortho) in XY plane

  // Compute ViewProject matrix as view/camera might not be changed for this frame (basic scenario)
  //  Don't change unless you are sure!!
  glm::mat4 VP = Matrices.projection * Matrices.view;

  // Send our transformation to the currently bound shader, in the "MVP" uniform
  // For each model you render, since the MVP will be different (at least the M part)
  //  Don't change unless you are sure!!
  glm::mat4 MVP;	// MVP = Projection * View * Model

  /* Render your scene */
  createBattery ();

  Matrices.model = glm::mat4(1.0f);
  MVP = VP * Matrices.model; // MVP = p * V * M
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
  draw3DObject(battery);
  draw3DObject(batteryTip);
  draw3DObject(batteryStatus);

  draw3DObject(scoreBackground);
  drawScore();

  // Draw Buckets
  for(i = 0; i < 2; i++)
  {
    // Load identity to model matrix
    Matrices.model = glm::mat4(1.0f);

    glm::mat4 translateBucket = glm::translate (glm::vec3(bucketInfo[i].topLeft - bucketInfo[i].initLeft, 0.0f, 0.0f)); // glTranslatef
    Matrices.model *= translateBucket;
    MVP = VP * Matrices.model; // MVP = p * V * M
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

    draw3DObject(bucket[i]);
  }

  // Draw Blocks
  for(i = 0; i < 5000; i++)
  {
    // Load identity to model matrix
    Matrices.model = glm::mat4(1.0f);

    if(blockInfo[i].y >= -12)
    {
      glm::mat4 translateBlock = glm::translate (glm::vec3(blockInfo[i].x, blockInfo[i].y, 0.0f)); // glTranslatef
      Matrices.model *= translateBlock;
      MVP = VP * Matrices.model; // MVP = p * V * M
      glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);
      draw3DObject(blocks[i]);
      blockInfo[i].y -= speed;
    }
  }

  // Draw Mirrors
  for(i = 0; i < mirrorCount; i++)
  {
    // Load identity to model matrix
    Matrices.model = glm::mat4(1.0f);

    MVP = VP * Matrices.model; // MVP = p * V * M
    glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

    draw3DObject(mirrors[i]);
  }

  //Draw Cannon
  Matrices.model = glm::mat4(1.0f);

  glm::mat4 actualTranslateCannon = glm::translate (glm::vec3(-cannonInfo.y*sin(cannonInfo.angle*M_PI/180.0f),cannonInfo.y*cos(cannonInfo.angle*M_PI/180.0f), 0));

  glm::mat4 translateCannon = glm::translate (glm::vec3(-screenLeftX, -cannonInfo.y, 0));        // glTranslatef
  glm::mat4 rotateCannon = glm::rotate((float)(cannonInfo.angle*M_PI/180.0f), glm::vec3(0,0,1)); // rotate about vector (-1,1,1)
  glm::mat4 translateCannon2 = glm::translate (glm::vec3(screenLeftX, cannonInfo.y, 0));

  Matrices.model *= (actualTranslateCannon * translateCannon2 * rotateCannon * translateCannon);
  MVP = VP * Matrices.model;
  glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

  draw3DObject(cannon);

  //Draw Death Ray
  if(keyStates[GLFW_KEY_SPACE] && juiceEndX > juiceStartX)
  {
    juiceEndX = max(juiceEndX - 0.01f, juiceStartX);
    rayAngle = cannonInfo.angle;
    initRayX = rayPoints[0];
    initRayY = rayPoints[1];
    resetMirrors();

    j = 0;
    while(1)
    {
      flag = 0;
      slopeRay = tan(rayAngle*M_PI/180.0f);
      rayX2 = initRayX + 1000*cos(rayAngle*M_PI/180.0f);
      rayY2 = initRayY + 1000*sin(rayAngle*M_PI/180.0f);
      cRay = (initRayY*rayX2 - initRayX*rayY2)/(rayX2 - initRayX);

      tempX = 100.0;
      if(rayX2 - initRayX < 0)
      {
        tempX = -100.0;
      }

      // Check for intersections with any of the falling blocks
      for(i = 0; i < 5000; i++)
      {
        if(slopeRay*(blockInfo[i].x) + cRay <= blockInfo[i].initY + blockInfo[i].y + 0.3
        && blockInfo[i].initY + blockInfo[i].y <= slopeRay*(blockInfo[i].x) + cRay && blockInfo[i].y > -12
        && ((rayX2 - initRayX)*(blockInfo[i].x - initRayX) > 0.0))
        {
          // First block to right (if ray moving towards right)
          if(tempX > blockInfo[i].x && rayX2 - initRayX > 0)
          {
            tempX = blockInfo[i].x;
            temp = i;
          }

          // First block to left (if ray moving towards left)
          else if(tempX < blockInfo[i].x && rayX2 - initRayX < 0)
          {
            tempX = blockInfo[i].x;
            temp = i;
          }
        }
      }

      // Finding out all the mirrors that the Death Ray might intersect with
      for(i = 0; i < mirrorCount; i++)
      {
        slopeMirror = tan(mirrorInfo[i].angle*M_PI/180.0f);
        x2 = mirrorInfo[i].x + mirrorInfo[i].length*cos(mirrorInfo[i].angle*M_PI/180.0f);
        y2 = mirrorInfo[i].y + mirrorInfo[i].length*sin(mirrorInfo[i].angle*M_PI/180.0f);
        cMirror = (mirrorInfo[i].y*x2 - mirrorInfo[i].x*y2)/(x2 - mirrorInfo[i].x);

        // Find intercept if ray is not parallel to mirror
        if(slopeRay != slopeMirror)
        {
          yIntercept = (slopeRay*cMirror - slopeMirror*cRay)/(slopeRay - slopeMirror);
          xIntercept = (cMirror - cRay)/(slopeRay - slopeMirror);

          if((mirrorInfo[i].x <= xIntercept) && (xIntercept <= x2) && (abs(mirrorInfo[i].intersectionX - xIntercept) > 0.0001)
          && ((rayX2 - initRayX)*(xIntercept-initRayX) > 0.0))
          {
            potentialIntersections[i].x = xIntercept;
            potentialIntersections[i].y = yIntercept;
            flag++;
          }
        }
      }

      // Ray intersects with a block before intersecting with a mirror
      if(abs(tempX) < 100 &&
          (((tempX < potentialIntersections[getMin(potentialIntersections)].x) && (rayX2 - initRayX > 0.0))
            || ((((potentialIntersections[getMax(potentialIntersections)].x != 100.0 && tempX > potentialIntersections[getMax(potentialIntersections)].x))
            || (potentialIntersections[getMax(potentialIntersections)].x == 100.0)) && (rayX2 - initRayX < 0.0)))
          )
      {
        createDeathRay(initRayX, initRayY, blockInfo[temp].x, blockInfo[i].initY + blockInfo[temp].y, j);

        Matrices.model = glm::mat4(1.0f);
        MVP = VP * Matrices.model;
        glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

        draw3DObject(deathRay[j++]);

        juiceEndX = juiceStartX;

        blockInfo[temp].y = -100;
        if(blockInfo[temp].type != 2)
        {
          score = max(score - 20, 0);
          wrongHits++;
        }
        else
        {
          score += 10;
        }
        break;
      }

      // Death Ray does not intersect with any mirror
      else if(flag == 0)
      {
        if(cos(rayAngle*M_PI/180.0f) > 0)
        {
          if(sin(rayAngle*M_PI/180.0f) > 0)
          {
            tempX = min((screenTopY - cRay)/slopeRay, screenRightX - 6.0f);
            createDeathRay(initRayX, initRayY, tempX, slopeRay*tempX + cRay, j);
          }
          else if(sin(rayAngle*M_PI/180.0f) < 0)
          {
            tempX = min((screenBottomY - cRay)/slopeRay, screenRightX - 6.0f);
            createDeathRay(initRayX, initRayY, tempX, slopeRay*tempX + cRay, j);
          }

          else
          {
            tempX = screenRightX - 6.0f;
            createDeathRay(initRayX, initRayY, tempX, slopeRay*tempX + cRay, j);
          }
        }

        else
        {
          if(sin(rayAngle*M_PI/180.0f) > 0)
          {
            tempX = max((screenTopY - cRay)/slopeRay, screenLeftX);
            createDeathRay(initRayX, initRayY, tempX, slopeRay*tempX + cRay, j);
          }
          else if(sin(rayAngle*M_PI/180.0f) < 0)
          {
            tempX = max((screenBottomY - cRay)/slopeRay, screenLeftX);
            createDeathRay(initRayX, initRayY, tempX, slopeRay*tempX + cRay, j);
          }
          else
          {
            tempX = -11.0;
            createDeathRay(initRayX, initRayY, tempX, slopeRay*tempX + cRay, j);
          }
        }
        // createDeathRay(initRayX, initRayY, initRayX + 1000*cos(rayAngle*M_PI/180.0f), initRayY + 1000*sin(rayAngle*M_PI/180.0f), j);

        Matrices.model = glm::mat4(1.0f);
        MVP = VP * Matrices.model;
        glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

        draw3DObject(deathRay[j++]);
        break;
      }
      /* Find the closest mirror that the Death Ray intersects with and draw a
         line segment till the point of intersection */
      else
      {
        temp = getMax(potentialIntersections);

        // Ray is moving towards right
        if(rayX2 - initRayX > 0.0)
        {
          temp = getMin(potentialIntersections);
        }

        createDeathRay(initRayX, initRayY, potentialIntersections[temp].x, potentialIntersections[temp].y, j);

        Matrices.model = glm::mat4(1.0f);
        MVP = VP * Matrices.model;
        glUniformMatrix4fv(Matrices.MatrixID, 1, GL_FALSE, &MVP[0][0]);

        draw3DObject(deathRay[j++]);

        initRayX = potentialIntersections[temp].x;
        initRayY = potentialIntersections[temp].y;
        rayAngle = 2*(mirrorInfo[temp].angle) - rayAngle;
        mirrorInfo[temp].intersectionX = potentialIntersections[temp].x;
      }
      resetPotentialIntersections();
    }
  }
}

void updateScores()
{
  int i;
  for(i = 0; i < 5000; i++)
  {
    if(blockInfo[i].y > -11.0 && blockInfo[i].y + screenTopY <= -4.7)
    {
      if((bucketInfo[0].topLeft <= bucketInfo[1].topLeft && bucketInfo[0].topRight >= bucketInfo[1].topLeft) ||
        (bucketInfo[1].topLeft <= bucketInfo[0].topLeft && bucketInfo[1].topRight >= bucketInfo[0].topLeft)
      )
      {
        // No change in score
      }
      else if(blockInfo[i].type == 0)
      {
        {
          if(bucketInfo[0].topLeft <= blockInfo[i].x && blockInfo[i].x <= bucketInfo[0].topRight)
          {
            blockInfo[i].y = -100;
            score += 5;
          }
          else if(bucketInfo[1].topLeft <= blockInfo[i].x && blockInfo[i].x <= bucketInfo[1].topRight)
          {
            blockInfo[i].y = -100;
            score = max(score - 10, 0);
          }
        }
      }
      else if(blockInfo[i].type == 1)
      {
        if(bucketInfo[0].topLeft <= blockInfo[i].x && blockInfo[i].x <= bucketInfo[0].topRight)
        {
          blockInfo[i].y = -100;
          score = max(score - 10, 0);
        }
        else if(bucketInfo[1].topLeft <= blockInfo[i].x && blockInfo[i].x <= bucketInfo[1].topRight)
        {
          blockInfo[i].y = -100;
          score += 5;
        }
      }
      else if(blockInfo[i].type == 2)
      {
        if(bucketInfo[0].topLeft <= blockInfo[i].x && blockInfo[i].x <= bucketInfo[0].topRight)
        {
          wrongCatch++;
        }
        else if(bucketInfo[1].topLeft <= blockInfo[i].x && blockInfo[i].x <= bucketInfo[1].topRight)
        {
          wrongCatch++;
        }
      }
    }
  }
}

/* Initialise glfw window, I/O callbacks and the renderer to use */
/* Nothing to Edit here */
GLFWwindow* initGLFW (int width, int height)
{
    GLFWwindow* window; // window desciptor/handle

    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
//        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Sample OpenGL 3.3 Application", NULL, NULL);

    if (!window) {
        glfwTerminate();
//        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSwapInterval( 1 );

    /* --- register callbacks with GLFW --- */

    /* Register function to handle window resizes */
    /* With Retina display on Mac OS X GLFW's FramebufferSize
     is different from WindowSize */
    glfwSetFramebufferSizeCallback(window, reshapeWindow);
    glfwSetWindowSizeCallback(window, reshapeWindow);

    /* Register function to handle window close */
    glfwSetWindowCloseCallback(window, quit);

    /* Register function to handle keyboard input */
    glfwSetKeyCallback(window, keyboard);      // general keyboard input
    glfwSetCharCallback(window, keyboardChar);  // simpler specific character handling

    /* Register function to handle mouse click */
    glfwSetMouseButtonCallback(window, mouseButton);  // mouse button clicks
    glfwSetScrollCallback(window, scroll_callback);

    windowCopy = window;
    return window;
}

/* Initialize the OpenGL rendering properties */
/* Add all the models to be created here */
void initGL (GLFWwindow* window, int width, int height)
{
    /* Objects should be created before any other gl function and shaders */
	// Create the models
  createBlocks (); // Generate the VAO, VBOs, vertices data & copy into the array buffer
  createBuckets ();
  createMirrors ();
  createScoreTile ();
  createCannon ();

	// Create and compile our GLSL program from the shaders
	programID = LoadShaders( "Sample_GL.vert", "Sample_GL.frag" );
	// Get a handle for our "MVP" uniform
	Matrices.MatrixID = glGetUniformLocation(programID, "MVP");

	reshapeWindow (window, width, height);

    // Background color of the scene
	glClearColor (0.0f, 0.0f, 0.0f, 0.0f); // R, G, B, A
	glClearDepth (1.0f);

	glEnable (GL_DEPTH_TEST);
	glDepthFunc (GL_LEQUAL);

    cout << "VENDOR: " << glGetString(GL_VENDOR) << endl;
    cout << "RENDERER: " << glGetString(GL_RENDERER) << endl;
    cout << "VERSION: " << glGetString(GL_VERSION) << endl;
    cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
}

int main (int argc, char** argv)
{
  int width = 1100;
  int height = 600;

  mpg123_handle *mh;
  unsigned char * buffer;
  size_t buffer_size;
  size_t done;
  int i, bufferCount, err;

  int defaultDriver;
  ao_device *dev;

  ao_sample_format format;
  int channels, encoding;
  long rate;

    GLFWwindow* window = initGLFW(width, height);

  initialize();

	initGL (window, width, height);

    double last_update_time = glfwGetTime(), current_time;

  ao_initialize();

  defaultDriver = ao_default_driver_id();
  mpg123_init();
  mh = mpg123_new(NULL, &err);
  buffer_size = 4096;
  buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

  /* open the file and get the decoding format */
  mpg123_open(mh, "bomberman.mp3");
  mpg123_getformat(mh, &rate, &channels, &encoding);

  /* set the output format and open the output device */
  format.bits = mpg123_encsize(encoding) * BITS;
  format.rate = rate;
  format.channels = channels;
  format.byte_format = AO_FMT_NATIVE;
  format.matrix = 0;
  dev = ao_open_live(defaultDriver, &format, NULL);

    /* Draw in loop */
    while (!glfwWindowShouldClose(window) && wrongHits < 10 && wrongCatch != 1) {

        if(juiceEndX <= screenLeftX + 1.0)
        {
          juiceEndX += 0.008;
        }

        // Update Scores
        updateScores();

        // OpenGL Draw commands
        keyStateCheck();
        reshapeWindow (window, width, height);

        /* Play sound */
        if (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK)
        {
          ao_play(dev, (char *)buffer, done);
        }
        else
        {
          mpg123_seek(mh, 0, SEEK_SET);
        }

        // Swap Frame Buffer in double buffering
        glfwSwapBuffers(window);

        // Poll for Keyboard and mouse events
        glfwPollEvents();

        // Control based on time (Time based transformation like 5 degrees rotation every 0.5s)
        current_time = glfwGetTime(); // Time in seconds
        if ((current_time - last_update_time) >= updateTime) { // atleast 1s elapsed since last frame
            // do something every 0.5 seconds ..
            insertBlock();
            last_update_time = current_time;
        }
    }

    printf("\n\nGame Over!\n______________________\n\nYou Final Score is %d\n\n", score);
    /* clean up */
    free(buffer);
    ao_close(dev);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    ao_shutdown();

    glfwTerminate();
//    exit(EXIT_SUCCESS);
}
