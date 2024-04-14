////////////////////////////////////////////////////////////////////////
//
//   Harvard University
//   CS175 : Computer Graphics
//   Professor Steven Gortler
//
////////////////////////////////////////////////////////////////////////

#include <cstddef>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define GLEW_STATIC
#include "GL/glew.h"
#include "GL/glfw3.h"

#include "arcball.h"
#include "asstcommon.h"
#include "cvec.h"
#include "drawer.h"
#include "geometrymaker.h"
#include "glsupport.h"
#include "keyframes.h"
#include "matrix4.h"
#include "picker.h"
#include "ppm.h"
#include "rigtform.h"
#include "scenegraph.h"

using namespace std;

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

enum SkyMode { WORLD_SKY = 0, SKY_SKY = 1 };

static bool g_mouseClickDown = false; // is the mouse button pressed
static bool g_mouseLClickButton, g_mouseRClickButton, g_mouseMClickButton;
static bool g_spaceDown = false; // space state, for middle mouse emulation
static double g_mouseClickX, g_mouseClickY; // coordinates for mouse click event
static int g_activeShader = 0;

static SkyMode g_activeCameraFrame = WORLD_SKY;

static bool g_displayArcball = true;
static double g_arcballScreenRadius = 100; // number of pixels
static double g_arcballScale = 1;

static bool g_pickingMode = false;

static int g_framesPerSecond = 60;
static int g_msBetweenKeyFrames = 2000;
static double g_lastFrameClock;
static bool g_playingAnimation = false;

static const int KEYFRAME_MS_INCR = 500;
static const string SCENE_FILE = "./scene.csv";

// -------- Shaders

static const int g_numShaders = 3, g_numRegularShaders = 3;
static const int PICKING_SHADER = 2;
static const char *const g_shaderFiles[g_numShaders][2] = {
    {"./shaders/basic-gl3.vshader", "./shaders/diffuse-gl3.fshader"},
    {"./shaders/basic-gl3.vshader", "./shaders/solid-gl3.fshader"},
    {"./shaders/basic-gl3.vshader", "./shaders/pick-gl3.fshader"}};
static vector<shared_ptr<ShaderState>>
    g_shaderStates; // our global shader states

// --------- Geometry

// Macro used to obtain relative offset of a field within a struct
#define FIELD_OFFSET(StructType, field) ((GLvoid *)offsetof(StructType, field))

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
    glBindVertexArray(NULL);
  }
};

typedef SgGeometryShapeNode<Geometry> MyShapeNode;

// Vertex buffer and index buffer associated with the ground and cube geometry
static shared_ptr<Geometry> g_ground, g_cube, g_sphere;

// --------- Scene

static const Cvec3 g_light1(2.0, 3.0, 14.0),
    g_light2(-2, -3.0, -5.0); // define two lights positions in world space

static shared_ptr<SgRootNode> g_world;
static shared_ptr<SgRbtNode> g_skyNode, g_groundNode, g_robot1Node,
    g_robot2Node;
static shared_ptr<KeyFrame> g_keyframes;
static unique_ptr<Animator> g_animator;

static shared_ptr<SgRbtNode> g_currentCameraNode;
static shared_ptr<SgRbtNode> g_currentPickedRbtNode;

///////////////// END OF G L O B A L S
/////////////////////////////////////////////////////

static void initGround() {
  // A x-z plane at y = g_groundY of dimension [-g_groundSize, g_groundSize]^2
  VertexPN vtx[4] = {VertexPN(-g_groundSize, g_groundY, -g_groundSize, 0, 1, 0),
                     VertexPN(-g_groundSize, g_groundY, g_groundSize, 0, 1, 0),
                     VertexPN(g_groundSize, g_groundY, g_groundSize, 0, 1, 0),
                     VertexPN(g_groundSize, g_groundY, -g_groundSize, 0, 1, 0)};
  unsigned short idx[] = {0, 1, 2, 0, 2, 3};
  g_ground.reset(new Geometry(&vtx[0], &idx[0], 4, 6));
}

static void initCubes() {
  int ibLen, vbLen;
  getCubeVbIbLen(vbLen, ibLen);

  // Temporary storage for cube geometry
  vector<VertexPN> vtx(vbLen);
  vector<unsigned short> idx(ibLen);

  makeCube(1, vtx.begin(), idx.begin());
  g_cube.reset(new Geometry(&vtx[0], &idx[0], vbLen, ibLen));
}

static void initSphere() {
  int ibLen, vbLen;
  getSphereVbIbLen(20, 10, vbLen, ibLen);

  // Temporary storage for sphere geometry
  vector<VertexPN> vtx(vbLen);
  vector<unsigned short> idx(ibLen);
  makeSphere(1, 20, 10, vtx.begin(), idx.begin());
  g_sphere.reset(new Geometry(&vtx[0], &idx[0], vtx.size(), idx.size()));
}

static void initRobots() {
  // Init whatever geometry needed for the robots
}

// takes a projection matrix and send to the the shaders
inline void sendProjectionMatrix(const ShaderState &curSS,
                                 const Matrix4 &projMatrix) {
  GLfloat glmatrix[16];
  projMatrix.writeToColumnMajorMatrix(glmatrix); // send projection matrix
  safe_glUniformMatrix4fv(curSS.h_uProjMatrix, glmatrix);
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

enum ManipMode { ARCBALL_ON_PICKED, ARCBALL_ON_SKY, EGO_MOTION };

static ManipMode getManipMode() {
  // if nothing is picked or the picked transform is the transfrom we are
  // viewing from
  if (g_currentPickedRbtNode == NULL ||
      g_currentPickedRbtNode == g_currentCameraNode) {
    if (g_currentCameraNode == g_skyNode && g_activeCameraFrame == WORLD_SKY)
      return ARCBALL_ON_SKY;
    else
      return EGO_MOTION;
  } else
    return ARCBALL_ON_PICKED;
}

static bool shouldUseArcball() {
  return (g_currentPickedRbtNode != 0);
  //  return getManipMode() != EGO_MOTION;
}

// The translation part of the aux frame either comes from the current
// active object, or is the identity matrix when
static RigTForm getArcballRbt() {
  switch (getManipMode()) {
  case ARCBALL_ON_PICKED:
    return getPathAccumRbt(g_world, g_currentPickedRbtNode);
  case ARCBALL_ON_SKY:
    return RigTForm();
  case EGO_MOTION:
    return getPathAccumRbt(g_world, g_currentCameraNode);
  default:
    throw runtime_error("Invalid ManipMode");
  }
}

static void updateArcballScale() {
  RigTForm arcballEye =
      inv(getPathAccumRbt(g_world, g_currentCameraNode)) * getArcballRbt();
  double depth = arcballEye.getTranslation()[2];
  if (depth > -CS175_EPS)
    g_arcballScale = 0.02;
  else
    g_arcballScale = getScreenToEyeScale(depth, g_frustFovY, g_windowHeight);
}

static void drawArcBall(const ShaderState &curSS) {
  // switch to wire frame mode
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  RigTForm arcballEye =
      inv(getPathAccumRbt(g_world, g_currentCameraNode)) * getArcballRbt();
  Matrix4 MVM = rigTFormToMatrix(arcballEye) *
                Matrix4::makeScale(Cvec3(1, 1, 1) * g_arcballScale *
                                   g_arcballScreenRadius);
  sendModelViewNormalMatrix(curSS, MVM, normalMatrix(MVM));

  safe_glUniform3f(curSS.h_uColor, 0.27, 0.82, 0.35); // set color
  g_sphere->draw(curSS);

  // switch back to solid mode
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

static void drawStuff(const ShaderState &curSS, bool picking) {
  // if we are not translating, update arcball scale
  if (!(g_mouseMClickButton || (g_mouseLClickButton && g_mouseRClickButton) ||
        (g_mouseLClickButton && !g_mouseRClickButton && g_spaceDown)))
    updateArcballScale();

  // build & send proj. matrix to vshader
  const Matrix4 projmat = makeProjectionMatrix();
  sendProjectionMatrix(curSS, projmat);

  const RigTForm eyeRbt = getPathAccumRbt(g_world, g_currentCameraNode);
  const RigTForm invEyeRbt = inv(eyeRbt);

  const Cvec3 eyeLight1 = Cvec3(invEyeRbt * Cvec4(g_light1, 1));
  const Cvec3 eyeLight2 = Cvec3(invEyeRbt * Cvec4(g_light2, 1));
  safe_glUniform3f(curSS.h_uLight, eyeLight1[0], eyeLight1[1], eyeLight1[2]);
  safe_glUniform3f(curSS.h_uLight2, eyeLight2[0], eyeLight2[1], eyeLight2[2]);

  if (!picking) {
    Drawer drawer(invEyeRbt, curSS);
    g_world->accept(drawer);

    if (g_displayArcball && shouldUseArcball())
      drawArcBall(curSS);
  } else {
    Picker picker(invEyeRbt, curSS);
    g_world->accept(picker);
    glFlush();
    g_currentPickedRbtNode = picker.getRbtNodeAtXY(g_mouseClickX * g_wScale,
                                                   g_mouseClickY * g_hScale);
    if (g_currentPickedRbtNode == g_groundNode)
      g_currentPickedRbtNode = shared_ptr<SgRbtNode>(); // set to NULL

    cout << (g_currentPickedRbtNode ? "Part picked" : "No part picked") << endl;
  }
}

static void display() {
  glUseProgram(g_shaderStates[g_activeShader]->program);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  drawStuff(*g_shaderStates[g_activeShader], false);

  glfwSwapBuffers(g_window);

  checkGlErrors();
}

static void pick() {
  // We need to set the clear color to black, for pick rendering.
  // so let's save the clear color
  GLdouble clearColor[4];
  glGetDoublev(GL_COLOR_CLEAR_VALUE, clearColor);

  glClearColor(0, 0, 0, 0);

  // using PICKING_SHADER as the shader
  glUseProgram(g_shaderStates[PICKING_SHADER]->program);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  drawStuff(*g_shaderStates[PICKING_SHADER], true);

  // Now set back the clear color
  glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

  checkGlErrors();
}

static void reshape(GLFWwindow *window, const int w, const int h) {
  int width, height;
  glfwGetFramebufferSize(g_window, &width, &height);
  glViewport(0, 0, width, height);

  g_windowWidth = w;
  g_windowHeight = h;
  // cerr << "Size of window is now " << g_windowWidth << "x" << g_windowHeight
  //     << endl;
  g_arcballScreenRadius = max(1.0, min(h, w) * 0.25);
  updateFrustFovY();
}

static Cvec3 getArcballDirection(const Cvec2 &p, const double r) {
  double n2 = norm2(p);
  if (n2 >= r * r)
    return normalize(Cvec3(p, 0));
  else
    return normalize(Cvec3(p, sqrt(r * r - n2)));
}

static RigTForm moveArcball(const Cvec2 &p0, const Cvec2 &p1) {
  const Matrix4 projMatrix = makeProjectionMatrix();
  const RigTForm eyeInverse =
      inv(getPathAccumRbt(g_world, g_currentCameraNode));
  const Cvec3 arcballCenter = getArcballRbt().getTranslation();
  const Cvec3 arcballCenter_ec = Cvec3(eyeInverse * Cvec4(arcballCenter, 1));

  if (arcballCenter_ec[2] > -CS175_EPS)
    return RigTForm();

  Cvec2 ballScreenCenter =
      getScreenSpaceCoord(arcballCenter_ec, projMatrix, g_frustNear,
                          g_frustFovY, g_windowWidth, g_windowHeight);
  const Cvec3 v0 =
      getArcballDirection(p0 - ballScreenCenter, g_arcballScreenRadius);
  const Cvec3 v1 =
      getArcballDirection(p1 - ballScreenCenter, g_arcballScreenRadius);

  return RigTForm(Quat(0.0, v1[0], v1[1], v1[2]) *
                  Quat(0.0, -v0[0], -v0[1], -v0[2]));
}

void handleAnimation() {
  if (!g_playingAnimation) {
    return;
  }
  double alpha = 1. / (g_msBetweenKeyFrames * (g_framesPerSecond / 1000.));
  if (g_animator->poll(alpha)) {
    // returning true indicates the animation has room to run
    return;
  };
  // animation just finished
  g_playingAnimation = false;
}

static RigTForm doMtoOwrtA(const RigTForm &M, const RigTForm &O,
                           const RigTForm &A) {
  return A * M * inv(A) * O;
}

static RigTForm getMRbt(const double dx, const double dy) {
  RigTForm M;

  if (g_mouseLClickButton && !g_mouseRClickButton && !g_spaceDown) {
    if (shouldUseArcball())
      M = moveArcball(Cvec2(g_mouseClickX, g_mouseClickY),
                      Cvec2(g_mouseClickX + dx, g_mouseClickY + dy));
    else
      M = RigTForm(Quat::makeXRotation(-dy) * Quat::makeYRotation(dx));
  } else {
    double movementScale = getManipMode() == EGO_MOTION ? 0.02 : g_arcballScale;
    if (g_mouseRClickButton && !g_mouseLClickButton) {
      M = RigTForm(Cvec3(dx, dy, 0) * movementScale);
    } else if (g_mouseMClickButton ||
               (g_mouseLClickButton && g_mouseRClickButton) ||
               (g_mouseLClickButton && g_spaceDown)) {
      M = RigTForm(Cvec3(0, 0, -dy) * movementScale);
    }
  }

  switch (getManipMode()) {
  case ARCBALL_ON_PICKED:
    break;
  case ARCBALL_ON_SKY:
    M = inv(M);
    break;
  case EGO_MOTION:
    //    if (g_mouseLClickButton && !g_mouseRClickButton && !g_spaceDown)
    //    // only invert rotation
    M = inv(M);
    break;
  }
  return M;
}

static RigTForm makeMixedFrame(const RigTForm &objRbt, const RigTForm &eyeRbt) {
  return transFact(objRbt) * linFact(eyeRbt);
}

// l = w X Y Z
// o = l O
// a = w A = l (Z Y X)^1 A = l A'
// o = a (A')^-1 O
//   => a M (A')^-1 O = l A' M (A')^-1 O

static void motion(GLFWwindow *window, double x, double y) {
  if (!g_mouseClickDown)
    return;
  y = g_windowHeight - y - 1;

  const double dx = x - g_mouseClickX;
  const double dy = y - g_mouseClickY;

  const RigTForm M = getMRbt(dx, dy); // the "action" matrix

  // the matrix for the auxiliary frame (the w.r.t.)
  RigTForm A = makeMixedFrame(getArcballRbt(),
                              getPathAccumRbt(g_world, g_currentCameraNode));

  shared_ptr<SgRbtNode> target;
  switch (getManipMode()) {
  case ARCBALL_ON_PICKED:
    target = g_currentPickedRbtNode;
    break;
  case ARCBALL_ON_SKY:
    target = g_skyNode;
    break;
  case EGO_MOTION:
    target = g_currentCameraNode;
    break;
  }

  A = inv(getPathAccumRbt(g_world, target, 1)) * A;

  if ((g_mouseLClickButton && !g_mouseRClickButton && !g_spaceDown) // rotating
      && target == g_skyNode) {
    RigTForm My = getMRbt(dx, 0);
    RigTForm Mx = getMRbt(0, dy);
    RigTForm B = makeMixedFrame(getArcballRbt(), RigTForm());
    RigTForm O = doMtoOwrtA(Mx, target->getRbt(), A);
    O = doMtoOwrtA(My, O, B);
    target->setRbt(O);
  } else {
    target->setRbt(doMtoOwrtA(M, target->getRbt(), A));
  }
  g_mouseClickX += dx;
  g_mouseClickY += dy;
}

static void mouse(GLFWwindow *window, int button, int state, int mods) {
  double x, y;
  glfwGetCursorPos(window, &x, &y);

  g_mouseClickX = x;
  g_mouseClickY =
      g_windowHeight - y - 1; // conversion from GLFW window-coordinate-system
                              // to OpenGL window-coordinate-system

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

  if (g_pickingMode && button == GLFW_MOUSE_BUTTON_LEFT &&
      state == GLFW_PRESS) {
    pick();
    g_pickingMode = false;
    cerr << "Picking mode is off" << endl;
  }
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
           << "v\t\tCycle view\n"
           << "drag left mouse to rotate\n"
           << endl;
      break;
    case GLFW_KEY_S:
      glFlush();
      writePpmScreenshot(g_windowWidth, g_windowHeight, "out.ppm");
      break;
    case GLFW_KEY_F:
      g_activeShader = (g_activeShader + 1) % g_numRegularShaders;
      break;
    case GLFW_KEY_V: {
      shared_ptr<SgRbtNode> viewers[] = {g_skyNode, g_robot1Node, g_robot2Node};
      for (int i = 0; i < 3; ++i) {
        if (g_currentCameraNode == viewers[i]) {
          g_currentCameraNode = viewers[(i + 1) % 3];
          break;
        }
      }
    } break;
    case GLFW_KEY_M:
      g_activeCameraFrame = SkyMode((g_activeCameraFrame + 1) % 2);
      cerr << "Editing sky eye w.r.t. "
           << (g_activeCameraFrame == WORLD_SKY ? "world-sky frame\n"
                                                : "sky-sky frame\n")
           << endl;
      break;
    case GLFW_KEY_P:
      g_pickingMode = !g_pickingMode;
      cerr << "Picking mode is " << (g_pickingMode ? "on" : "off") << endl;
      break;
    case GLFW_KEY_C:
      printf("Overwriting scene graph with current frame.\n");
      if (!g_keyframes->overwrite_sg_from_frame()) {
        fprintf(stderr, "🛑 Cannot overwrite scene from empty keyframe.\n");
      };
      g_keyframes->dbg_frame_info();
      break;
    case GLFW_KEY_U:
      printf("Overwriting the current frame with scene graph contents.\n");
      g_keyframes->overwrite_frame_from_sg();
      g_keyframes->dbg_frame_info();
      break;
    case GLFW_KEY_COMMA: // Left arrow "<"
      if (!(mods & GLFW_MOD_SHIFT)) {
        break;
      }
      printf("Moving frame cursor left.\n");
      if (!g_keyframes->move_cursor_left()) {
        fprintf(stderr, "Cannot go left on a cursor already at 0.\n");
      };
      g_keyframes->dbg_frame_info();
      break;
    case GLFW_KEY_PERIOD: // Right arrow ">"
      if (!(mods & GLFW_MOD_SHIFT)) {
        break;
      }
      printf("Moving frame cursor right.\n");
      if (!g_keyframes->move_cursor_right()) {
        fprintf(stderr, "Cannot go right on a cursor already at the end.\n");
      };
      g_keyframes->dbg_frame_info();
      break;
    case GLFW_KEY_D:
      printf("Deleting current frame.\n");
      if (!g_keyframes->del_curr_frame()) {
        fprintf(stderr, "Cannot delete, frame sequence is empty.\n");
      };
      g_keyframes->dbg_frame_info();
      break;
    case GLFW_KEY_N:
      printf("Creating new frame from scene graph.\n");
      g_keyframes->snapshot_scene();
      g_keyframes->dbg_frame_info();
      break;
    case GLFW_KEY_I:
      printf("Reading scene from %s.\n", SCENE_FILE.c_str());
      g_keyframes->import_scene(SCENE_FILE);
      break;
    case GLFW_KEY_W: {
      printf("Writing scene to %s.\n", SCENE_FILE.c_str());
      g_keyframes->export_scene(SCENE_FILE);
      break;
    }
    case GLFW_KEY_Y:
      printf("Beginning animation.\n");
      if (!g_animator->restart()) {
        fprintf(stderr, "Animation failed, are there more than 3 keyframes?\n");
      } else {
        g_playingAnimation = true;
      };
      break;
    case GLFW_KEY_EQUAL: // "+" key
      if (!(mods & GLFW_MOD_SHIFT)) {
        break;
      }
      g_msBetweenKeyFrames =
          max(g_msBetweenKeyFrames - KEYFRAME_MS_INCR, KEYFRAME_MS_INCR);
      printf("Secs per frame: %f\n", g_msBetweenKeyFrames / 1000.);
      break;
    case GLFW_KEY_MINUS:
      g_msBetweenKeyFrames += KEYFRAME_MS_INCR;
      printf("Secs per frame: %f\n", g_msBetweenKeyFrames / 1000.);
      break;
    case GLFW_KEY_SPACE:
      g_spaceDown = true;
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

  g_window = glfwCreateWindow(g_windowWidth, g_windowHeight, "Assignment 6",
                              NULL, NULL);
  if (!g_window) {
    fprintf(stderr, "Failed to create GLFW window or OpenGL context\n");
    exit(1);
  }
  glfwMakeContextCurrent(g_window);
  glewInit();

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

  // cout << screen_width << " " << screen_height << endl;
  // cout << pixel_width << " " << pixel_width << endl;

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
  initSphere();
  initRobots();
}

static void constructRobot(shared_ptr<SgTransformNode> base,
                           const Cvec3 &color) {
  const float ARM_LEN = 0.7, ARM_THICK = 0.25, LEG_LEN = 1, LEG_THICK = 0.25,
              TORSO_LEN = 1.5, TORSO_THICK = 0.25, TORSO_WIDTH = 1,
              HEAD_SIZE = 0.7;
  const int NUM_JOINTS = 10, NUM_SHAPES = 10;

  struct JointDesc {
    int parent;
    float x, y, z;
  };

  JointDesc jointDesc[NUM_JOINTS] = {
      {-1},                                    // torso
      {0, TORSO_WIDTH / 2, TORSO_LEN / 2, 0},  // upper right arm
      {0, -TORSO_WIDTH / 2, TORSO_LEN / 2, 0}, // upper left arm
      {1, ARM_LEN, 0, 0},                      // lower right arm
      {2, -ARM_LEN, 0, 0},                     // lower left arm
      {0, TORSO_WIDTH / 2 - LEG_THICK / 2, -TORSO_LEN / 2,
       0}, // upper right leg
      {0, -TORSO_WIDTH / 2 + LEG_THICK / 2, -TORSO_LEN / 2,
       0},                     // upper left leg
      {5, 0, -LEG_LEN, 0},     // lower right leg
      {6, 0, -LEG_LEN, 0},     // lower left
      {0, 0, TORSO_LEN / 2, 0} // head
  };

  struct ShapeDesc {
    int parentJointId;
    float x, y, z, sx, sy, sz;
    shared_ptr<Geometry> geometry;
  };

  ShapeDesc shapeDesc[NUM_SHAPES] = {
      {0, 0, 0, 0, TORSO_WIDTH, TORSO_LEN, TORSO_THICK, g_cube}, // torso
      {1, ARM_LEN / 2, 0, 0, ARM_LEN / 2, ARM_THICK / 2, ARM_THICK / 2,
       g_sphere}, // upper right arm
      {2, -ARM_LEN / 2, 0, 0, ARM_LEN / 2, ARM_THICK / 2, ARM_THICK / 2,
       g_sphere}, // upper left arm
      {3, ARM_LEN / 2, 0, 0, ARM_LEN, ARM_THICK, ARM_THICK,
       g_cube}, // lower right arm
      {4, -ARM_LEN / 2, 0, 0, ARM_LEN, ARM_THICK, ARM_THICK,
       g_cube}, // lower left arm
      {5, 0, -LEG_LEN / 2, 0, LEG_THICK / 2, LEG_LEN / 2, LEG_THICK / 2,
       g_sphere}, // upper right leg
      {6, 0, -LEG_LEN / 2, 0, LEG_THICK / 2, LEG_LEN / 2, LEG_THICK / 2,
       g_sphere}, // upper left leg
      {7, 0, -LEG_LEN / 2, 0, LEG_THICK, LEG_LEN, LEG_THICK,
       g_cube}, // lower right leg
      {8, 0, -LEG_LEN / 2, 0, LEG_THICK, LEG_LEN, LEG_THICK,
       g_cube}, // lower left leg
      {9, 0, static_cast<float>(HEAD_SIZE / 2 * 1.5), 0, HEAD_SIZE / 2,
       HEAD_SIZE / 2, HEAD_SIZE / 2, g_sphere}, // head
  };

  shared_ptr<SgTransformNode> jointNodes[NUM_JOINTS];

  for (int i = 0; i < NUM_JOINTS; ++i) {
    if (jointDesc[i].parent == -1)
      jointNodes[i] = base;
    else {
      jointNodes[i].reset(new SgRbtNode(
          RigTForm(Cvec3(jointDesc[i].x, jointDesc[i].y, jointDesc[i].z))));
      jointNodes[jointDesc[i].parent]->addChild(jointNodes[i]);
    }
  }
  for (int i = 0; i < NUM_SHAPES; ++i) {
    shared_ptr<MyShapeNode> shape(new MyShapeNode(
        shapeDesc[i].geometry, color,
        Cvec3(shapeDesc[i].x, shapeDesc[i].y, shapeDesc[i].z), Cvec3(0, 0, 0),
        Cvec3(shapeDesc[i].sx, shapeDesc[i].sy, shapeDesc[i].sz)));
    jointNodes[shapeDesc[i].parentJointId]->addChild(shape);
  }
}

static void initScene() {
  g_world.reset(new SgRootNode());

  g_skyNode.reset(new SgRbtNode(RigTForm(Cvec3(0.0, 0.25, 4.0))));

  g_groundNode.reset(new SgRbtNode());
  g_groundNode->addChild(shared_ptr<MyShapeNode>(
      new MyShapeNode(g_ground, Cvec3(0.1, 0.95, 0.1))));

  g_robot1Node.reset(new SgRbtNode(RigTForm(Cvec3(-2, 1, 0))));
  g_robot2Node.reset(new SgRbtNode(RigTForm(Cvec3(2, 2, 0))));

  constructRobot(g_robot1Node, Cvec3(1, 0, 0)); // a Red robot
  constructRobot(g_robot2Node, Cvec3(0, 0, 1)); // a Blue robot

  g_world->addChild(g_skyNode);
  g_world->addChild(g_groundNode);
  g_world->addChild(g_robot1Node);
  g_world->addChild(g_robot2Node);

  g_currentCameraNode = g_skyNode;

  // Keep this at the bottom, the keyframes expect there to be
  // no additions or subtractions from the entries in the scene
  // graph.
  g_keyframes.reset(new KeyFrame(g_world));
  g_animator.reset(new Animator(g_keyframes));
}

void glfwLoop() {
  g_lastFrameClock = glfwGetTime();
  while (!glfwWindowShouldClose(g_window)) {
    double now = glfwGetTime();
    if (now - g_lastFrameClock >= 1. / g_framesPerSecond) {
      handleAnimation();
      display();
      g_lastFrameClock = now;
    }
    glfwPollEvents();
  }
  printf("end loop\n");
}

int main(int argc, char *argv[]) {
  try {
    initGlfwState();

    // on Mac, we shouldn't use GLEW.
#ifndef __MAC__
    glewInit(); // load the OpenGL extensions

    if (!GLEW_VERSION_3_0)
      throw runtime_error("Error: card/driver does not support OpenGL "
                          "Shading Language v1.3");
#endif

    initGLState();
    initShaders();
    initGeometry();
    initScene();

    glfwLoop();
    return 0;
  } catch (const runtime_error &e) {
    cout << "Exception caught: " << e.what() << endl;
    return -1;
  }
}
