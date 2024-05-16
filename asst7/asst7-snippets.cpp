// Global variables for animation timing

// Frames to render per second
static int g_framesPerSecond = 60;
// 2 seconds between keyframes
static int g_msBetweenKeyFrames = 2000;

static double g_lastFrameClock;

// Is the animation playing?
static bool g_playingAnimation = false;
// Time since last key frame
static int g_animateTime = 0;

...

    // Given t in the range [0, n], perform interpolation for the
    // particular t. Returns true if we are at the end of the animation
    // playback, or false otherwise.
    bool
    interpolate(double t) {
  ...
}

// Call every frame to advance the animation
void animationUpdate() {
  if (g_playingAnimation) {
    bool endReached = interpolate((float)g_animateTime / g_msBetweenKeyFrames);
    if (!endReached) {
      g_animateTime += 1000. / g_framesPerSecond;
    } else {
      // finish and clean up
      g_playingAnimation = false;
      g_animateTime = 0;
      // TODO...
    }
  }
}

...

    // Modify GLFW main loop to update the animation at the proper times
    static void
    glfwLoop() {
  g_lastFrameClock = glfwGetTime();

  while (!glfwWindowShouldClose(g_window)) {
    double thisTime = glfwGetTime();
    if (thisTime - g_lastFrameClock >= 1. / g_framesPerSecond) {

      animationUpdate();

      display();
      g_lastFrameClock = thisTime;
    }

    glfwPollEvents();
  }
}

// in keyboard use following snippet to detect the '>'
case GLFW_KEY_PERIOD: // >
if (!(mods & GLFW_MOD_SHIFT))
  break;
//          your code here
break;
