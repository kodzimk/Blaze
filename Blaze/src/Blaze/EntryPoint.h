#pragma once

#ifdef BZ_PLATFORM_WINDOWS

extern Blaze::Application* Blaze::CreateApplication();

int main(int argc,char** argv)
{
	Blaze::Log::Init();
	BZ_CORE_WARN("initalized Log");
	int a = 5;
	BZ_INFO(a);
	
	auto app = Blaze::CreateApplication();
	app->Run();
	delete app;
}

#endif