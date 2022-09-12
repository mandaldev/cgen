#include <GLFW/glfw3.h>
#include <thread>

using namespace std::literals;

int main(int argc, const char *argv[])
{
	GLFWwindow *window = nullptr;
	
	/* Initialize the library */
	if (!glfwInit())
	    return -1;
	
	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}
	
	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		std::this_thread::sleep_for(16666us);
		/* Poll for and process events */
		glfwPollEvents();
	}
	
	glfwTerminate();
	return 0;
}
