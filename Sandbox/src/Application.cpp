#include <Blaze.h>

class ExampleLayer : public Blaze::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
		render = new Blaze::Renderer(vertexSource,fragmentSource);
		render->CreateCamera(0.0f, 0.0f, 1.0f);
		render->CreateObject(vertices);
	}

	~ExampleLayer()
	{
		
	}

	void OnUpdate() override
	{

		if (Blaze::Input::IsKeyPressed(BZ_KEY_SPACE))
			BZ_INFO("Pressed!!!!!!");

		render->BeginScene("matrix");
		render->Render();
		render->EndScene();
	}

	void OnEvent(Blaze::Event& event) override
	{/*
		BZ_TRACE("{0}", event.GetName());*/
	}

public:
	std::vector<float> vertices = {
	-0.5f,-0.5f,
	-0.5f,0.5f,
	0.0f,0.5f
	};
	Blaze::Renderer* render;

	const char* vertexSource = "#version 330 core\n"
		"layout (location = 0) in vec3 position;\n"
		"uniform mat4 matrix;\n"
		"out vec3 currentPos;\n"
		"void main() {\n"
		"gl_Position = matrix * vec4(position, 1.0f);\n"
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