/************************************************************************************
 
 Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
 Copyright   :   Copyright Brad Davis. All Rights reserved.
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 
 ************************************************************************************/

#include "Common.h"


static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
  instance->onKey(key, scancode, action, mods);
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
  instance->onMouseButton(button, action, mods);
}

static void CursorEnterCallback(GLFWwindow* window, int enter) {
  GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
  instance->onMouseEnter(enter);
}

static void MouseMoveCallback(GLFWwindow* window, double x, double y) {
  GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
  instance->onMouseMove(x, y);
}

static void CharacterCallback(GLFWwindow* window, unsigned int codepoint)
{
  GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
  instance->onCharacter(codepoint);
}



static void ScrollCallback(GLFWwindow * window, double x, double y) {
  GlfwApp * instance = (GlfwApp *)glfwGetWindowUserPointer(window);
  instance->onScroll(x, y);
}

static void ErrorCallback(int error, const char* description) {
  FAIL(description);
}


GlfwApp::GlfwApp() {
    // Initialize the GLFW system for creating and positioning windows
    if (!glfwInit()) {
      FAIL("Failed to initialize GLFW");
    }
    glfwSetErrorCallback(ErrorCallback);
  }

GlfwApp::~GlfwApp() {
    glfwDestroyWindow(window);
    glfwTerminate();
  }

int GlfwApp::run() {
  try {
    preCreate();
    window = createRenderingTarget(windowSize, windowPosition);
    postCreate();

    if (!window) {
      FAIL("Unable to create OpenGL window");
    }

    initGl();
    // Ensure we shutdown the GL resources even if we throw an exception
    Finally f([&]{
      shutdownGl();
    });

    int framecount = 0;
    long start = Platform::elapsedMillis();
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      ++frame;
      update();
      draw();
      finishFrame();
      long now = Platform::elapsedMillis();
      ++framecount;
      if ((now - start) >= 2000) {
        float elapsed = (now - start) / 1000.f;
        fps = (float)framecount / elapsed;
        SAY("FPS: %0.2f", fps);
        start = now;
        framecount = 0;
      }
    }
  }
  catch (std::runtime_error & err) {
    SAY(err.what());
  }
  return 0;
}


int GlfwApp::getFrame() const {
  return this->frame;
}

void GlfwApp::preCreate() {
  glfwWindowHint(GLFW_DEPTH_BITS, 16);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  // Without this line we get
  // FATAL (86): NSGL: The targeted version of OS X only supports OpenGL 3.2 and later versions if they are forward-compatible
  ON_MAC([]{
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  });
#ifdef DEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
}

void GlfwApp::postCreate()  {
  glfwMakeContextCurrent(window);
  windowAspect = aspect(windowSize);
  windowAspectInverse = 1.0f / windowAspect;
  glfwSetWindowUserPointer(window, this);
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetCursorPosCallback(window, MouseMoveCallback);
  glfwSetCursorEnterCallback(window, CursorEnterCallback);
  glfwSetCharCallback(window, CharacterCallback);
  glfwSetScrollCallback(window, ScrollCallback);
  glfwSwapInterval(1);

  // Initialize the OpenGL bindings
  // For some reason we have to set this experminetal flag to properly
  // init GLEW if we use a core context.
  glewExperimental = GL_TRUE;
  if (0 != glewInit()) {
    FAIL("Failed to initialize GLEW");
  }
  glGetError();

  //#if DEBUG
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  GLuint unusedIds = 0;
  if (glDebugMessageCallback) {
    glDebugMessageCallback(oria::debugCallback, this);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
      0, &unusedIds, true);
  }
  else if (glDebugMessageCallbackARB) {
    glDebugMessageCallbackARB(oria::debugCallback, this);
    glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE,
      0, &unusedIds, true);
  }
  //#endif
}

const glm::uvec2 & GlfwApp::getSize() const {
  return windowSize;
}

const glm::ivec2 & GlfwApp::getPosition() const {
  return windowPosition;
}

GLFWwindow * GlfwApp::getWindow() {
  return window;
}

void GlfwApp::initGl() {
  using namespace oglplus;
  DefaultFramebuffer().Bind(Framebuffer::Target::Draw);
  DefaultFramebuffer().Bind(Framebuffer::Target::Read);
  Context::Enable(Capability::CullFace);
  Context::Enable(Capability::DepthTest);
  Context::Disable(Capability::Dither);
}

void GlfwApp::shutdownGl() {
  Platform::runShutdownHooks();
}

void GlfwApp::update() {}

void GlfwApp::finishFrame() {
  glfwSwapBuffers(window);
}

void GlfwApp::destroyWindow() {
  glfwSetKeyCallback(window, nullptr);
  glfwDestroyWindow(window);
}


void GlfwApp::onKey(int key, int scancode, int action, int mods) {
  if (GLFW_PRESS != action) {
    return;
  }

  switch (key) {
  case GLFW_KEY_ESCAPE:
    glfwSetWindowShouldClose(window, 1);
    return;

  case GLFW_KEY_S:
    if (mods & GLFW_MOD_SHIFT) {
      screenshot();
    }
    return;
  }
}

void GlfwApp::onCharacter(unsigned int code_point) {}
void GlfwApp::onMouseButton(int button, int action, int mods) {}
void GlfwApp::onMouseMove(double x, double y) {}
void GlfwApp::onMouseEnter(int entered) {}
void GlfwApp::onScroll(double x, double y) {}

void GlfwApp::viewport(const glm::ivec2 & pos, const glm::uvec2 & size) {
  oglplus::Context::Viewport(pos.x, pos.y, size.x, size.y);
}

void GlfwApp::renderStringAt(const std::string & string, float x, float y) {
  renderStringAt(string, vec2(x, y));
}

void GlfwApp::screenshot() {
  //#ifdef HAVE_OPENCV
  //    //use fast 4-byte alignment (default anyway) if possible
  //    glFlush();
  //    cv::Mat img(windowSize.y, windowSize.x, CV_8UC3);
  //    glPixelStorei(GL_PACK_ALIGNMENT, (img.step & 3) ? 1 : 4);
  //    glPixelStorei(GL_PACK_ROW_LENGTH, img.step / img.elemSize());
  //    glReadPixels(0, 0, img.cols, img.rows, GL_BGR, GL_UNSIGNED_BYTE, img.data);
  //    cv::flip(img, img, 0);
  //
  //    static int counter = 0;
  //    static char buffer[128];
  //    sprintf(buffer, "screenshot%05i.png", counter++);
  //    bool success = cv::imwrite(buffer, img);
  //    if (!success) {
  //      throw std::runtime_error("Failed to write image");
  //    }
  //#endif
}



void GlfwApp::renderStringAt(const std::string & str, const glm::vec2 & pos) {
  MatrixStack & mv = Stacks::modelview();
  MatrixStack & pr = Stacks::projection();
  mv.push().identity();
  pr.push().top() = glm::ortho(
    -1.0f, 1.0f,
    -windowAspectInverse, windowAspectInverse,
    -100.0f, 100.0f);
  glm::vec2 cursor(pos.x, windowAspectInverse * pos.y);
  oria::renderString(str, cursor, 18.0f);
  pr.pop();
  mv.pop();
}
