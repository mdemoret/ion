﻿#include "Window.hpp"
#include "ion/portgfx/glheaders.h"
#include <GLFW/glfw3.h>
#include "Scene.hpp"
#include "Camera.hpp"
#include "ion/math/angleutils.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/base/settingmanager.h"
#include "Macros.h"


#ifndef ION_PRODUCTION
#include "ion/remote/remoteserver.h"

std::unique_ptr<ion::remote::RemoteServer> Snapshot::Window::s_Remote;
#endif

ION_REGISTER_ASSETS(SnapshotShaders);

using namespace ion::math;

namespace Snapshot{

Window::Window(uint32_t width, uint32_t height, const string& title) :
   m_MouseDownButton(0),
   m_MouseLastPos(Point2d::Zero()),
   m_KeyboardDownButton(0),
   m_LastFrameElapsedSec(0.0)
{
   glfwWindowHint(GLFW_SAMPLES, 4); //Set multisampling on

   m_WindowPtr = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);

   if (!m_WindowPtr)
   {
      throw std::runtime_error("Error initializing window");
   }

   glfwMakeContextCurrent(m_WindowPtr);

   glfwSetWindowUserPointer(m_WindowPtr, this);

   m_Scene = new Scene(m_Keyboard);

   m_Scene->GetCamera()->SetViewportBounds(Vector2ui(width, height));
}

Window::~Window()
{
   if (m_Scene)
   {
      delete m_Scene;
      m_Scene = nullptr;
   }

   if (m_WindowPtr)
      glfwDestroyWindow(m_WindowPtr);

#ifndef ION_PRODUCTION
   s_Remote.reset(NULL);
#endif
}

bool Window::Update()
{
   double currElapsed = glfwGetTime();
   double diffElapsed = currElapsed - m_LastFrameElapsedSec;
   m_LastFrameElapsedSec = currElapsed;

   return m_Scene->Update(currElapsed, diffElapsed);
}

void Window::Render()
{
   m_Scene->Render();
}

int GetMouseButtonValue(int glfwButton)
{
   if (glfwButton == GLFW_MOUSE_BUTTON_LEFT)
      return 1 << 0;
   else if (glfwButton == GLFW_MOUSE_BUTTON_RIGHT)
      return 1 << 1;
   else if (glfwButton == GLFW_MOUSE_BUTTON_MIDDLE)
      return 1 << 2;

   return 0;
}

int GetKeyButtonValue(int glfwButton)
{
   if (glfwButton == GLFW_KEY_V)
      return 1 << 0;
   else if (glfwButton == GLFW_KEY_N)
      return 1 << 1;
   else if (glfwButton == GLFW_KEY_B)
      return 1 << 2;

   return 0;
}

void Window::ProcessResize(const Vector2ui& size)
{
   m_Scene->GetCamera()->SetViewportBounds(size);
}

void Window::ProcessKey(int key, int scancode, int action, int mods)
{
   //Special processing for keys that trigger events on their own
   switch (key)
   {
   case GLFW_KEY_ESCAPE:
      m_Keyboard.Escape(action);
      break;
   case GLFW_KEY_LEFT:
      m_Keyboard.LeftArrow(action);
      break;
   case GLFW_KEY_RIGHT:
      m_Keyboard.RightArrow(action);
      break;
   case GLFW_KEY_R:
      m_Keyboard.R(action, mods);
      break;
   default:
      //Now process keys that need the state maintained during some other action
      if (action == GLFW_PRESS)
      {
         //Mouse down
         m_KeyboardDownButton |= GetKeyButtonValue(key);
      }
      else if (action == GLFW_RELEASE)
      {
         //Mouse up
         m_KeyboardDownButton &= ~GetKeyButtonValue(key);
      }
   }
}

void Window::ProcessMouseMove(const Point2d& pos)
{
   if (m_MouseDownButton > 0)
   {
      Vector2d delta = pos - m_MouseLastPos;

      if ((m_MouseDownButton & GetMouseButtonValue(GLFW_MOUSE_BUTTON_LEFT)) > 0)
      {
         if (m_KeyboardDownButton == 0)
         {
            //Rotating
            delta *= 0.1;
            m_Scene->GetCamera()->DeltaViewpoint(-Angled::FromDegrees(delta[0]), Angled::FromDegrees(delta[1]), Vector1d::Zero());
         }
         else
         {
            //Panning
            Vector3d offset = Vector3d::Fill(0.0);

            if ((m_KeyboardDownButton & GetKeyButtonValue(GLFW_KEY_V)) > 0)
               offset[0] = delta[1] * 0.1;

            if ((m_KeyboardDownButton & GetKeyButtonValue(GLFW_KEY_N)) > 0)
               offset[1] = delta[1] * 0.1;

            if ((m_KeyboardDownButton & GetKeyButtonValue(GLFW_KEY_B)) > 0)
               offset[2] = delta[1] * 0.1;

            m_Scene->GetCamera()->OffsetPositionWorld(offset);
         }
      }
      else if ((m_MouseDownButton & GetMouseButtonValue(GLFW_MOUSE_BUTTON_RIGHT)) > 0)
      {
         if (m_KeyboardDownButton == 0)
         {
            //Zooming
            m_Scene->GetCamera()->ScaleViewpoint(1.0, 1.0, 1.0f + delta[1] * 0.005);
         }
         else
         {
            //Scaling
            Vector3d scale = m_Scene->GetCamera()->Scale();

            if ((m_KeyboardDownButton & GetKeyButtonValue(GLFW_KEY_V)) > 0)
               scale[0] *= 1.0f + delta[1] * 0.005;

            if ((m_KeyboardDownButton & GetKeyButtonValue(GLFW_KEY_N)) > 0)
               scale[1] *= 1.0f + delta[1] * 0.005;

            if ((m_KeyboardDownButton & GetKeyButtonValue(GLFW_KEY_B)) > 0)
               scale[2] *= 1.0f + delta[1] * 0.005;

            m_Scene->GetCamera()->SetScale(scale);
         }
      }
   }

   //Save off previous version
   m_MouseLastPos = pos;
}

void Window::ProcessMouseButton(int button, int action, int mods)
{
   if (action == GLFW_PRESS)
   {
      //Mouse down
      m_MouseDownButton |= GetMouseButtonValue(button);
   }
   else if (action == GLFW_RELEASE)
   {
      //Mouse up
      m_MouseDownButton &= ~GetMouseButtonValue(button);
   }
}

void Window::ProcessScroll(const Vector2d& offset) {}


void Window::ProcessFileDrop(vector<string>& files)
{
   auto* setting = dynamic_cast<ion::base::Setting<std::vector<std::string>>*>(ion::base::SettingManager::GetSetting(SETTINGS_INPUT_FILES));

   if (setting != nullptr)
      setting->SetValue(files);
}

Window* Window::GetWindowFromGlfw(GLFWwindow* windowPtr)
{
   void* userPtr = glfwGetWindowUserPointer(windowPtr);

   if (userPtr != nullptr)
      return static_cast<Window*>(userPtr);

   return nullptr;
}

#ifndef ION_PRODUCTION
ion::remote::RemoteServer* Window::ResetRemoteServer()
{
#if defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL)
   s_Remote.reset(new ion::remote::RemoteServer(0));
   s_Remote->SetEmbedLocalSourcedFiles(true);
#else
   s_Remote.reset(new ion::remote::RemoteServer(1234));
#endif
   return s_Remote.get();
}

ion::remote::RemoteServer* Window::GetRemoteServer() { return s_Remote.get(); }
#endif
}
