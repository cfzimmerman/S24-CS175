////////////////////////////////////////////////////////////////////////
//
//   Harvard University
//   CS175 : Computer Graphics
//   Professor Steven Gortler
//
////////////////////////////////////////////////////////////////////////

#include "rigtform.h"
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define GLEW_STATIC

#include "GL/glew.h"
#include "GL/glfw3.h"

#include "arcball.h"
#include "cvec.h"
#include "geometrymaker.h"
#include "glsupport.h"
#include "matrix4.h"
#include "ppm.h"

using namespace std; // for string, vector, iostream, and other standard C++
                     // stuff

// G L O B A L S ///////////////////////////////////////////////////

static const float g_frustMinFov = 60.0; // A minimal of 60 degree field of view
static float g_frustFovY =
    g_frustMinFov; // FOV in y direction (updated by updateFrustFovY)

static const float g_frustNear = -0.1;  // near plane
static const float g_frustFar = -50.0;  // far plane
static const float g_groundY = -2.0;    // y coordinate of the ground
static const float g_groundSize = 10.0; // half the ground length

static GLFWwindow *g_window;

static int g_windowWidth = 512;
static int g_windowHeight = 512;
static double g_wScale = 1;
static double g_hScale = 1;

static bool g_mouseClickDown = false; // is the mouse button pressed
static bool g_mouseLClickButton, g_mouseRClickButton, g_mouseMClickButton;
static bool g_spaceDown = false; // space state, for middle mouse emulation
static double g_mouseClickX, g_mouseClickY; // coordinates for mouse click event
static int g_activeShader = 0;

static const size_t g_numCubes = 2;
static const size_t g_numCycleObjs = g_numCubes + 1;
static const size_t g_numViewModes = 2;
static const double g_sphereInitRadius = 1.;

static size_t g_currObjInd = 0;
static size_t g_currEyeInd = 0;
static size_t g_currViewMode = 0;

struct ShaderState {
  GlProgram program;

  // Handles to uniform variables
  GLint h_uLight, h_uLight2;
  GLint h_uProjMatrix;
  GLint h_uModelViewMatrix;
  GLint h_uNormalMatrix;
  GLint h_uColor;

  // Handles to vertex attributes
  GLint h_aPosition;
  GLint h_aNormal;

  ShaderState(const char *vsfn, const char *fsfn) {
    readAndCompileShader(program, vsfn, fsfn);

    const GLuint h = program; // short hand

    // Retrieve handles to uniform variables
    h_uLight = safe_glGetUniformLocation(h, "uLight");
    h_uLight2 = safe_glGetUniformLocation(h, "uLight2");
    h_uProjMatrix = safe_glGetUniformLocation(h, "uProjMatrix");
    h_uModelViewMatrix = safe_glGetUniformLocation(h, "uModelViewMatrix");
    h_uNormalMatrix = safe_glGetUniformLocation(h, "uNormalMatrix");
    h_uColor = safe_glGetUniformLocation(h, "uColor");

    // Retrieve handles to vertex attributes
    h_aPosition = safe_glGetAttribLocation(h, "aPosition");
    h_aNormal = safe_glGetAttribLocation(h, "aNormal");

    glBindFragDataLocation(h, 0, "fragColor");
    checkGlErrors();
  }
};

static const int g_numShaders = 2;
static const char *const g_shaderFiles[g_numShaders][2] = {
    {"./shaders/basic-gl3.vshader", "./shaders/diffuse-gl3.fshader"},
    {"./shaders/basic-gl3.vshader", "./shaders/solid-gl3.fshader"}};
static vector<shared_ptr<ShaderState>>
    g_shaderStates; // our global shader states

// --------- Geometry

// Macro used to obtain relative offset of a field within a struct
#define FIELD_OFFSET(StructType, field) &(((StructType *)0)->field)

// A vertex with floating point position and normal
struct VertexPN {
  Cvec3f p, n;

  VertexPN() {}
  VertexPN(float x, float y, float z, float nx, float ny, float nz)
      : p(x, y, z), n(nx, ny, nz) {}

  // Define copy constructor and assignment operator from GenericVertex so we
  // can use make* functions from geometrymaker.h
  VertexPN(const GenericVertex &v) { *this = v; }

  VertexPN &operator=(const GenericVertex &v) {
    p = v.pos;
    n = v.normal;
    return *this;
  }
};

struct Geometry {
  GlBufferObject vbo, ibo;
  GlArrayObject vao;
  int vboLen, iboLen;

  Geometry(VertexPN *vtx, unsigned short *idx, int vboLen, int iboLen) {
    this->vboLen = vboLen;
    this->iboLen = iboLen;

    // Now create the VBO and IBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPN) * vboLen, vtx,
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * iboLen, idx,
                 GL_STATIC_DRAW);
  }

  void draw(const ShaderState &curSS) {
    // bind the object's VAO
    glBindVertexArray(vao);

    // Enable the attributes used by our shader
    safe_glEnableVertexAttribArray(curSS.h_aPosition);
    safe_glEnableVertexAttribArray(curSS.h_aNormal);

    // bind vbo
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    safe_glVertexAttribPointer(curSS.h_aPosition, 3, GL_FLOAT, GL_FALSE,
                               sizeof(VertexPN), FIELD_OFFSET(VertexPN, p));
    safe_glVertexAttribPointer(curSS.h_aNormal, 3, GL_FLOAT, GL_FALSE,
                               sizeof(VertexPN), FIELD_OFFSET(VertexPN, n));

    // bind ibo
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    // draw!
    glDrawElements(GL_TRIANGLES, iboLen, GL_UNSIGNED_SHORT, 0);

    // Disable the attributes used by our shader
    safe_glDisableVertexAttribArray(curSS.h_aPosition);
    safe_glDisableVertexAttribArray(curSS.h_aNormal);

    // disable VAO
    glBindVertexArray((GLuint)NULL);
  }
};

// Vertex buffer and index buffer associated with the ground and cube geometry
static shared_ptr<Geometry> g_ground;
static shared_ptr<Geometry> g_cubes[g_numCubes];
static shared_ptr<Geometry> g_spheres[g_numCubes];

// --------- Scene

static const Cvec3 g_light1(2.0, 3.0, 14.0),
    g_light2(-2, -3.0, -5.0); // define two lights positions in world space
static RigTForm g_skyRbt = RigTForm(Cvec3(0.0, 0.25, 4.0));
static RigTForm g_cubeRbt[g_numCubes] = {RigTForm(Cvec3(-1., 0., 0.)),
                                         RigTForm(Cvec3(1., 0., 0.))};
static const RigTForm g_skyInitRbt = g_skyRbt;
static const RigTForm g_cubeInitRbts[g_numCubes] = {g_cubeRbt[0], g_cubeRbt[1]};
static Cvec3f g_cubeColors[g_numCubes] = {Cvec3f(0., 0.34, 0.69),
                                          Cvec3f(0.69, 0, 0.07)};
static double g_sphereScales[g_numCubes] = {1., 1.};
static RigTForm *g_cycleObjects[g_numCycleObjs] = {&g_skyRbt, &g_cubeRbt[0],
                                                   &g_cubeRbt[1]};
///////////////// END OF G L O B A L S
/////////////////////////////////////////////////////

static void initGround() {
  // A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2
  VertexPN vtx[4] = {
      VertexPN(-g_groundSize, g_groundY, -g_groundSize, 0, 1, 0),
      VertexPN(-g_groundSize, g_groundY, g_groundSize, 0, 1, 0),
      VertexPN(g_groundSize, g_groundY, g_groundSize, 0, 1, 0),
      VertexPN(g_groundSize, g_groundY, -g_groundSize, 0, 1, 0),
  };
  unsigned short idx[] = {0, 1, 2, 0, 2, 3};
  g_ground.reset(new Geometry(&vtx[0], &idx[0], 4, 6));
}

static void initCubes() {
  for (size_t ind = 0; ind < g_numCubes; ind++) {
    int vbLen, ibLen;
    getCubeVbIbLen(vbLen, ibLen);

    // Temporary storage for cube geometry
    vector<VertexPN> vtx(vbLen);
    vector<unsigned short> idx(ibLen);

    makeCube(1, vtx.begin(), idx.begin());
    g_cubes[ind].reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));
  }
}

static void initSpheres(int dim, float radius) {
  for (size_t ind = 0; ind < g_numCubes; ind++) {
    int vbLen, ibLen;
    getSphereVbIbLen(dim, dim, vbLen, ibLen);

    // Temporary storage for sphere geometry
    vector<VertexPN> vtx(vbLen);
    vector<unsigned short> idx(ibLen);

    makeSphere(radius, dim, dim, vtx.begin(), idx.begin());
    g_spheres[ind].reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));
  }
}

// takes a projection matrix and send to the the shaders
static void sendProjectionMatrix(const ShaderState &curSS,
                                 const Matrix4 &projMatrix) {
  GLfloat glmatrix[16];
  projMatrix.writeToColumnMajorMatrix(glmatrix); // send projection matrix
  safe_glUniformMatrix4fv(curSS.h_uProjMatrix, glmatrix);
}

// takes MVM and its normal matrix to the shaders
static void sendModelViewNormalMatrix(const ShaderState &curSS,
                                      const Matrix4 &MVM, const Matrix4 &NMVM) {
  GLfloat glmatrix[16];
  MVM.writeToColumnMajorMatrix(glmatrix); // send MVM
  safe_glUniformMatrix4fv(curSS.h_uModelViewMatrix, glmatrix);

  NMVM.writeToColumnMajorMatrix(glmatrix); // send NMVM
  safe_glUniformMatrix4fv(curSS.h_uNormalMatrix, glmatrix);
}

// update g_frustFovY from g_frustMinFov, g_windowWidth, and g_windowHeight
static void updateFrustFovY() {
  if (g_windowWidth >= g_windowHeight)
    g_frustFovY = g_frustMinFov;
  else {
    const double RAD_PER_DEG = 0.5 * CS175_PI / 180;
    g_frustFovY =
        atan2(sin(g_frustMinFov * RAD_PER_DEG) * g_windowHeight / g_windowWidth,
              cos(g_frustMinFov * RAD_PER_DEG)) /
        RAD_PER_DEG;
  }
}

static Matrix4 makeProjectionMatrix() {
  return Matrix4::makeProjection(
      g_frustFovY, g_windowWidth / static_cast<double>(g_windowHeight),
      g_frustNear, g_frustFar);
}

/// Returns a "scale" such that `init = curr * scale` where `curr` has the same
/// proportions as `init`.
double getSphereScale(const RigTForm &currObjWorld,
                      const RigTForm &initObjWorld,
                      const RigTForm &currEyeWorld,
                      const RigTForm &initEyeWorld) {
  RigTForm invInitEye = inv(initEyeWorld);
  RigTForm invCurrEye = inv(currEyeWorld);
  Cvec3 initObjInEyes = (invInitEye * initObjWorld).getTranslation();
  Cvec3 currObjInEyes = (invCurrEye * currObjWorld).getTranslation();
  double initScale =
      getScreenToEyeScale(initObjInEyes(2), g_frustFovY, g_windowHeight);
  double currScale =
      getScreenToEyeScale(currObjInEyes(2), g_frustFovY, g_windowHeight);
  return currScale / initScale;
}

static void drawStuff() {
  // short hand for current shader state
  const ShaderState &curSS = *g_shaderStates[g_activeShader];

  // build & send proj. matrix to vshader
  const Matrix4 projmat = makeProjectionMatrix();
  sendProjectionMatrix(curSS, projmat);

  // use the skyRbt as the eyeRbt
  const RigTForm eyeRbt = *g_cycleObjects[g_currEyeInd];
  const RigTForm invEyeRbt = inv(eyeRbt);

  if (!g_mouseClickDown) {
    // recompute arcball scale after motion is complete
    for (size_t sphereInd = 0; sphereInd < g_numCubes; sphereInd++) {
      g_sphereScales[sphereInd] =
          getSphereScale(g_cubeRbt[sphereInd], g_cubeInitRbts[sphereInd],
                         *g_cycleObjects[g_currEyeInd], g_skyInitRbt);
    }
  }

  const Cvec3 eyeLight1 = Cvec3(
      invEyeRbt * Cvec4(g_light1, 1)); // g_light1 position in eye coordinates
  const Cvec3 eyeLight2 = Cvec3(
      invEyeRbt * Cvec4(g_light2, 1)); // g_light2 position in eye coordinates
  safe_glUniform3f(curSS.h_uLight, eyeLight1[0], eyeLight1[1], eyeLight1[2]);
  safe_glUniform3f(curSS.h_uLight2, eyeLight2[0], eyeLight2[1], eyeLight2[2]);

  // draw ground
  // ===========
  //
  const RigTForm groundRbt = RigTForm(); // identity
  Matrix4 MVM = rigTFormToMatrix(invEyeRbt * groundRbt);
  Matrix4 NMVM = normalMatrix(MVM);
  sendModelViewNormalMatrix(curSS, MVM, NMVM);
  safe_glUniform3f(curSS.h_uColor, 0.1, 0.95, 0.1); // set color
  g_ground->draw(curSS);

  // draw cubes
  // ==========
  for (size_t ind = 0; ind < g_numCubes; ind++) {
    MVM = rigTFormToMatrix(invEyeRbt * g_cubeRbt[ind]);
    NMVM = normalMatrix(MVM);
    sendModelViewNormalMatrix(curSS, MVM, NMVM);
    safe_glUniform3f(curSS.h_uColor, g_cubeColors[ind][0], g_cubeColors[ind][1],
                     g_cubeColors[ind][2]);
    g_cubes[ind]->draw(curSS);
  }

  // draw the sphere that's currently selected (if any)
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  if (g_currObjInd != 0) {
    size_t sphereInd = g_currObjInd - 1;
    MVM = rigTFormToMatrix(invEyeRbt * g_cubeRbt[sphereInd]) *
          Matrix4::makeScale(Cvec3(g_sphereScales[sphereInd]));
    NMVM = normalMatrix(MVM);
    sendModelViewNormalMatrix(curSS, MVM, NMVM);

    const float SPHERE_DARKEN = 0.5;
    safe_glUniform3f(curSS.h_uColor, g_cubeColors[sphereInd][0] * SPHERE_DARKEN,
                     g_cubeColors[sphereInd][1] * SPHERE_DARKEN,
                     g_cubeColors[sphereInd][2] * SPHERE_DARKEN);
    g_spheres[sphereInd]->draw(curSS);
  }
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static void display() {
  glUseProgram(g_shaderStates[g_activeShader]->program);
  // clear framebuffer color&depth
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  drawStuff();

  glfwSwapBuffers(g_window); // show the back buffer (where we rendered stuff)

  checkGlErrors();
}

static void reshape(GLFWwindow *window, const int w, const int h) {
  int width, height;
  glfwGetFramebufferSize(g_window, &width, &height);
  glViewport(0, 0, width, height);

  g_windowWidth = w;
  g_windowHeight = h;
  cerr << "Size of window is now " << g_windowWidth << "x" << g_windowHeight
       << endl;
  updateFrustFovY();
}

/// Returns a vector `v` as defined in the notes
/// (lecture 9, page 2)
Cvec3 getArcballVec(Cvec2 &screenSphere, double radius, double x, double y) {
  Cvec3 arcballVec = Cvec3(x - screenSphere(0), y - screenSphere(1), 0);
  double distFromCtr = sqrt(pow(arcballVec(0), 2) + pow(arcballVec(1), 2));

  assert(radius >= 0);
  if (distFromCtr <= radius) {
    arcballVec(2) =
        sqrt(pow(radius, 2) - pow(arcballVec(0), 2) - pow(arcballVec(1), 2));
  }

  return normalize(arcballVec);
}

/// Returns an arcball rotation from prev mouseClickX and Y to
/// the new coordinate.
Quat getArcballRotation(RigTForm &currObj, RigTForm &currEye, Cvec2 &prevClick,
                        Cvec2 &nextClick, double sphereScale) {
  if (prevClick(0) == nextClick(0) && prevClick(1) == nextClick(1)) {
    // Return identity if no transformation occurred.
    return Quat();
  }
  RigTForm objInEyes = inv(currEye) * currObj;
  Matrix4 proj = makeProjectionMatrix();
  Cvec2 screenSphereInEyes =
      getScreenSpaceCoord(objInEyes.getTranslation(), proj, g_frustNear,
                          g_frustFovY, g_windowWidth, g_windowHeight);

  double radius =
      (g_sphereInitRadius / getScreenToEyeScale(objInEyes.getTranslation()(2),
                                                g_frustFovY, g_windowHeight)) *
      sphereScale;
  Cvec3 prev =
      getArcballVec(screenSphereInEyes, radius, prevClick(0), prevClick(1));
  Cvec3 next =
      getArcballVec(screenSphereInEyes, radius, nextClick(0), nextClick(1));

  return Quat(0, next) * Quat(0, prev * -1);
}

/// Implements both rotation and translation for normal objects like
/// cubes.
void moveSimple(RigTForm &mtx) {
  RigTForm aux = transFact(*g_cycleObjects[g_currObjInd]) *
                 linFact(*g_cycleObjects[g_currEyeInd]);
  g_cycleObjects[g_currObjInd]->consume(aux * mtx * inv(aux) *
                                        (*g_cycleObjects[g_currObjInd]));
}

/// Implements orbit rotation for the sky cam.
void rotateEyeOrbit(const double &dx, const double &dy) {
  RigTForm xAux = linFact(*g_cycleObjects[g_currEyeInd]);
  g_cycleObjects[g_currObjInd]->consume(
      xAux * RigTForm(Quat::makeXRotation(dy)) * inv(xAux) *
      (*g_cycleObjects[g_currObjInd]));

  g_cycleObjects[g_currObjInd]->consume(RigTForm(Quat::makeYRotation(-dx)) *
                                        (*g_cycleObjects[g_currObjInd]));
}

/// Implements ego rotation for the sky cam.
void rotateEyeEgo(const double &dx, const double &dy) {
  g_cycleObjects[g_currObjInd]->consume(
      (*g_cycleObjects[g_currEyeInd]) * RigTForm(Quat::makeXRotation(dy)) *
      inv(*g_cycleObjects[g_currEyeInd]) * (*g_cycleObjects[g_currObjInd]));

  RigTForm yAux = transFact(*g_cycleObjects[g_currEyeInd]);
  g_cycleObjects[g_currObjInd]->consume(
      yAux * RigTForm(Quat::makeYRotation(-dx)) * inv(yAux) *
      (*g_cycleObjects[g_currObjInd]));
}

/// Handles all eye movement events. If no rotation should occur,
/// dx and dy should be zero.
void moveEye(RigTForm &mtx, const double &dx, const double &dy) {
  assert(g_currEyeInd == 0);
  g_cycleObjects[g_currObjInd]->consume(*g_cycleObjects[g_currObjInd] *
                                        inv(transFact(mtx)));
  if (g_currViewMode == 0) {
    rotateEyeOrbit(dx, dy);
  } else {
    rotateEyeEgo(dx, dy);
  }
}

/// Purely for readability.
bool eyeMovement() { return g_currObjInd == 0; }

static void motion(GLFWwindow *window, double x, double y) {
  const double dx = x - g_mouseClickX;
  const double dy = g_windowHeight - y - 1 - g_mouseClickY;

  if (!g_mouseClickDown) {
    return;
  }
  Cvec2 prev = Cvec2(g_mouseClickX, g_mouseClickY);
  g_mouseClickX = x;
  g_mouseClickY = g_windowHeight - y - 1;

  // Left button down
  if (g_mouseLClickButton && !g_mouseRClickButton && !g_spaceDown) {
    if (!eyeMovement()) {
      Cvec2 now = Cvec2(g_mouseClickX, g_mouseClickY);
      assert(g_currObjInd - 1 < g_numCubes);
      RigTForm mtx = RigTForm(getArcballRotation(
          *g_cycleObjects[g_currObjInd], *g_cycleObjects[g_currEyeInd], prev,
          now, g_sphereScales[g_currObjInd - 1]));

      moveSimple(mtx);
      return;
    }
    RigTForm mtx = RigTForm(Quat::makeXRotation(-dy) * Quat::makeYRotation(dx));
    moveEye(mtx, dx, dy);
    return;
  }

  // Right button down
  if (g_mouseRClickButton && !g_mouseLClickButton) {
    if (!eyeMovement()) {
      Cvec3 currObjInEyes =
          (inv(*g_cycleObjects[g_currEyeInd]) * *g_cycleObjects[g_currObjInd])
              .getTranslation();
      double amnt =
          getScreenToEyeScale(currObjInEyes(2), g_frustFovY, g_windowHeight);
      RigTForm mtx = RigTForm(Cvec3(dx, dy, 0) * amnt);
      moveSimple(mtx);
      return;
    }
    RigTForm mtx = RigTForm(Cvec3(dx, dy, 0) * 0.01);
    moveEye(mtx, 0, 0);
    return;
  }

  // Middle button down or left + space
  if (g_mouseMClickButton || (g_mouseLClickButton && g_mouseRClickButton) ||
      (g_mouseLClickButton && !g_mouseRClickButton && g_spaceDown)) {
    RigTForm mtx = RigTForm(Cvec3(0, 0, -dy) * 0.01);
    if (!eyeMovement()) {
      moveSimple(mtx);
      return;
    }
    moveEye(mtx, 0, 0);
  }
}

static void mouse(GLFWwindow *window, int button, int state, int mods) {
  double x, y;
  glfwGetCursorPos(window, &x, &y);

  g_mouseClickX = x;
  // conversion from window-coordinate-system to OpenGL window-coordinate-system
  g_mouseClickY = g_windowHeight - y - 1;

  g_mouseLClickButton |=
      (button == GLFW_MOUSE_BUTTON_LEFT && state == GLFW_PRESS);
  g_mouseRClickButton |=
      (button == GLFW_MOUSE_BUTTON_RIGHT && state == GLFW_PRESS);
  g_mouseMClickButton |=
      (button == GLFW_MOUSE_BUTTON_MIDDLE && state == GLFW_PRESS);

  g_mouseLClickButton &=
      !(button == GLFW_MOUSE_BUTTON_LEFT && state == GLFW_RELEASE);
  g_mouseRClickButton &=
      !(button == GLFW_MOUSE_BUTTON_RIGHT && state == GLFW_RELEASE);
  g_mouseMClickButton &=
      !(button == GLFW_MOUSE_BUTTON_MIDDLE && state == GLFW_RELEASE);

  g_mouseClickDown =
      g_mouseLClickButton || g_mouseRClickButton || g_mouseMClickButton;
}

static void keyboard(GLFWwindow *window, int key, int scancode, int action,
                     int mods) {
  if (action == GLFW_PRESS || action == GLFW_REPEAT) {
    switch (key) {
    case GLFW_KEY_ESCAPE:
      exit(0);
    case GLFW_KEY_H:
      cout << " ============== H E L P ==============\n\n"
           << "h\t\thelp menu\n"
           << "s\t\tsave screenshot\n"
           << "f\t\tToggle flat shading on/off.\n"
           << "o\t\tCycle object to edit\n"
           << "v\t\tCycle view\n"
           << "drag left mouse to rotate\n"
           << endl;
      break;
    case GLFW_KEY_S:
      glFlush();
      writePpmScreenshot(g_windowWidth, g_windowHeight, "out.ppm");
      break;
    case GLFW_KEY_F:
      g_activeShader = (g_activeShader + 1) % g_numShaders;
      break;
    case GLFW_KEY_SPACE:
      g_spaceDown = true;
      break;
    case GLFW_KEY_M:
      if (g_currEyeInd == 0 && g_currObjInd == 0) {
        g_currViewMode = (g_currViewMode + 1) % g_numViewModes;
        printf("View mode %lu\n", g_currViewMode);
      }
      break;
    case GLFW_KEY_O:
      g_currObjInd = (g_currObjInd + 1) % g_numCycleObjs;
      if (g_currObjInd == 0 && g_currEyeInd != 0) {
        // Prevent cube perspective from modifying the sky camera
        g_currObjInd++;
      }
      printf("Now controlling object %lu\n", g_currObjInd);
      break;
    case GLFW_KEY_V:
      g_currEyeInd = (g_currEyeInd + 1) % g_numCycleObjs;
      if (g_currEyeInd != 0 && g_currObjInd == 0) {
        g_currObjInd++;
        printf("Shifting control to object %lu to protect the sky cam\n",
               g_currObjInd);
      }
      printf("Active eye is object %lu\n", g_currEyeInd);
      break;
    }
  } else {
    switch (key) {
    case GLFW_KEY_SPACE:
      g_spaceDown = false;
      break;
    }
  }
}

void error_callback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

static void initGlfwState() {
  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  glfwWindowHint(GLFW_SRGB_CAPABLE, GL_TRUE);

  g_window = glfwCreateWindow(g_windowWidth, g_windowHeight, "Assignment 3",
                              NULL, NULL);
  if (!g_window) {
    fprintf(stderr, "Failed to create GLFW window or OpenGL context\n");
    exit(1);
  }
  glfwMakeContextCurrent(g_window);

  glfwSwapInterval(1);

  glfwSetErrorCallback(error_callback);
  glfwSetMouseButtonCallback(g_window, mouse);
  glfwSetCursorPosCallback(g_window, motion);
  glfwSetWindowSizeCallback(g_window, reshape);
  glfwSetKeyCallback(g_window, keyboard);

  int screen_width, screen_height;
  glfwGetWindowSize(g_window, &screen_width, &screen_height);
  int pixel_width, pixel_height;
  glfwGetFramebufferSize(g_window, &pixel_width, &pixel_height);

  cout << screen_width << " " << screen_height << endl;
  cout << pixel_width << " " << pixel_width << endl;

  g_wScale = pixel_width / screen_width;
  g_hScale = pixel_height / screen_height;
}

static void initGLState() {
  glClearColor(128. / 255., 200. / 255., 255. / 255., 0.);
  glClearDepth(0.);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glReadBuffer(GL_BACK);
  glEnable(GL_FRAMEBUFFER_SRGB);
}

static void initShaders() {
  g_shaderStates.resize(g_numShaders);
  for (int i = 0; i < g_numShaders; ++i) {
    g_shaderStates[i].reset(
        new ShaderState(g_shaderFiles[i][0], g_shaderFiles[i][1]));
  }
}

static void initGeometry() {
  initGround();
  initCubes();
  initSpheres(16, g_sphereInitRadius);
}

void glfwLoop() {
  while (!glfwWindowShouldClose(g_window)) {
    display();
    glfwWaitEvents();
  }
  printf("end loop\n");
}

int main(int argc, char *argv[]) {
  try {
    initGlfwState();

    glewInit(); // load the OpenGL extensions
#ifndef __MAC__

    if (!GLEW_VERSION_3_0)
      throw runtime_error("Error: card/driver does not support OpenGL "
                          "Shading Language v1.3");
#endif

    initGLState();
    initShaders();
    initGeometry();

    glfwLoop();
    return 0;
  } catch (const runtime_error &e) {
    cout << "Exception caught: " << e.what() << endl;
    return -1;
  }
}
