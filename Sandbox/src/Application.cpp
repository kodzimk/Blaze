#include <Blaze.h>

class ExampleLayer : public Blaze::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
		render = new Blaze::Renderer(vertexSource,fragmentSource);
		render->CreateObject(vertices);
		camera = Blaze::Camera::CreateCamera(glm::vec3(0.0f,0.0f,1.0f));
	}

	~ExampleLayer()
	{
		delete render;
		delete camera;
	}

	void OnUpdate() override
	{
		camera->CameraMove();
		render->BeginScene(*camera,"matrix");
		render->Render(*camera);
		render->EndScene();
	}

	void OnEvent(Blaze::Event& event) override
	{
		static float height = 900.f, width = 1600.f;
		if (event.GetEventType() == Blaze::EventType::WindowResize)
		{
			auto& e = (Blaze::WindowResizeEvent&)event;
			camera->zoom *= e.GetWidth() / width;
			camera->zoom *= e.GetHeight() / height;

			camera->GetCameraProp().projection = glm::ortho((float)(-e.GetWidth() / 2), (float)(e.GetWidth() / 2), (float)(-e.GetHeight() / 2), (float)(e.GetHeight() / 2), -100.f, 100.f);

			width = e.GetWidth();
			height = e.GetHeight();
		}
	}

public:
	std::vector<float> vertices = {
	-100.f,-100.f,0.0f,
	-100.f,100.f,0.0f,
	0.0f,100.0f,0.0f,
	};

	Blaze::Renderer* render;
	Blaze::Camera* camera;

	const char* vertexSource = "#version 330 core\n"
		"layout (location = 0) in vec3 position;\n"
		"uniform mat4 matrix;\n"
		"uniform mat4 ortho;\n"
		"uniform float zoom;\n"
		"out vec3 currentPos;\n"
		"void main() {\n"
		"gl_Position = vec4(matrix * ortho * vec4(position * zoom,1.0f));\n"
		"currentPos = position;\n"
		"}\0";

	const char* fragmentSource = "#version 330 core\n"
		"uniform vec4 color;\n"
		"out vec4 colour;\n"
		"void main() {\n"
		"colour = color;\n"
		"}\0";
};

class Sandbox : public Blaze::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
	}

	~Sandbox()
	{

	}

};

Blaze::Application* Blaze::CreateApplication()
{
	return new Sandbox();
}