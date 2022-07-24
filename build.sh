#/bin/bash
g++ -Wall bwm.cpp backends/imgui_impl_glfw.cpp backends/imgui_impl_opengl3.cpp -o bwm -lglfw -limgui -lGL
