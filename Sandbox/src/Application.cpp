#include <Blaze.h>

class ExampleLayer : public Blaze::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
		render = new Blaze::Renderer();
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

		render->Render();
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