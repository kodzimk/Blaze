workspace "Blaze"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)

project "Blaze"
	location "Blaze"
	kind "SharedLib"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	buildoptions 
	{ 
		"/utf-8"
    }

	files
	{
		"%{prj.name}/vendor/GLAD/src/**.c",
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/imgui/src/**.h",
		"%{prj.name}/vendor/imgui/src/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/imgui/include/**.cpp",
		"%{prj.name}/vendor/imgui/include/**.h",
		"%{prj.name}/vendor/glm/glm/**.inl"
	}

	includedirs
	{
		"%{prj.name}/vendor/spdlog/include",
		"%{prj.name}/src",
		"%{prj.name}/vendor/glfw3/include",
		"%{prj.name}/vendor/GLAD/include",
		"%{prj.name}/vendor/glm/glm"
	}

	libdirs
	{
	 	"%{prj.name}/vendor/glfw3/lib-vc2022"
	}

	links
	{
       "glfw3_mt.lib",
	   "glfw3.lib",
	   "opengl32.lib"
	}

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"BZ_PLATFORM_WINDOWS",
			"BZ_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}

		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox")
		}

	filter "configurations:Debug"
		defines "BZ_DEBUG"
		buildoptions "/MDd"
		symbols "On"

	filter "configurations:Release"
		defines "BZ_RELEASE"
		buildoptions "/MD"
		optimize "On"

	filter "configurations:Dist"
		defines "BZ_DIST"
		buildoptions "/MD"
		optimize "On"

project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	buildoptions 
	{ 
		"/utf-8"
    }

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
	}

	includedirs
	{
		"Blaze/vendor/spdlog/include",
		"Blaze/vendor/glfw3/include",
		"Blaze/vendor/GLAD/include",
		"Blaze/vendor/imgui/include",
		"Blaze/src",
		"Blaze/vendor/glm/glm"
	}

	links
	{
		"Blaze"
	}


	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"BZ_PLATFORM_WINDOWS"
		}

	filter "configurations:Debug"
		defines "BZ_DEBUG"
			buildoptions "/MDd"
		symbols "On"

	filter "configurations:Release"
		defines "BZ_RELEASE"
			buildoptions "/MD"
		optimize "On"

	filter "configurations:Dist"
		defines "BZ_DIST"
			buildoptions "/MD"
		optimize "On"