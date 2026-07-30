#include "AudioEngine.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Logging/Log.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <unordered_map>

namespace Blaze
{
	struct SoundSlot
	{
		ma_sound sound{};
		bool initialized = false;
	};

	static ma_engine s_Engine{};
	static bool s_Ready = false;
	static std::unordered_map<GameObject*, SoundSlot> s_Sounds;

	bool AudioEngine::Init()
	{
		if (s_Ready)
			return true;

		ma_result result = ma_engine_init(nullptr, &s_Engine);
		if (result != MA_SUCCESS)
		{
			BZ_INFO("AudioEngine failed to init (code {0})", (int)result);
			return false;
		}

		s_Ready = true;
		BZ_INFO("AudioEngine ready");
		return true;
	}

	void AudioEngine::Shutdown()
	{
		StopAll();
		if (s_Ready)
		{
			ma_engine_uninit(&s_Engine);
			s_Ready = false;
		}
	}

	bool AudioEngine::IsReady()
	{
		return s_Ready;
	}

	static void DestroySlot(SoundSlot& slot)
	{
		if (slot.initialized)
		{
			ma_sound_uninit(&slot.sound);
			slot.initialized = false;
		}
	}

	void AudioEngine::RemoveObject(GameObject* object)
	{
		if (!object)
			return;
		auto it = s_Sounds.find(object);
		if (it == s_Sounds.end())
			return;
		DestroySlot(it->second);
		s_Sounds.erase(it);
	}

	void AudioEngine::Stop(GameObject* object)
	{
		if (!object)
			return;
		auto it = s_Sounds.find(object);
		if (it == s_Sounds.end() || !it->second.initialized)
			return;
		ma_sound_stop(&it->second.sound);
	}

	void AudioEngine::StopAll()
	{
		for (auto& entry : s_Sounds)
			DestroySlot(entry.second);
		s_Sounds.clear();
	}

	bool AudioEngine::IsPlaying(GameObject* object)
	{
		if (!object || !s_Ready)
			return false;
		auto it = s_Sounds.find(object);
		if (it == s_Sounds.end() || !it->second.initialized)
			return false;
		return ma_sound_is_playing(&it->second.sound) == MA_TRUE;
	}

	void AudioEngine::SyncVolume(GameObject* object)
	{
		if (!object || !s_Ready)
			return;
		auto it = s_Sounds.find(object);
		if (it == s_Sounds.end() || !it->second.initialized)
			return;
		ma_sound_set_volume(&it->second.sound, object->GetAudioVolume());
	}

	void AudioEngine::SyncLoop(GameObject* object)
	{
		if (!object || !s_Ready)
			return;
		auto it = s_Sounds.find(object);
		if (it == s_Sounds.end() || !it->second.initialized)
			return;
		ma_sound_set_looping(&it->second.sound, object->GetAudioLoop() ? MA_TRUE : MA_FALSE);
	}

	void AudioEngine::Play(GameObject* object)
	{
		if (!object || !object->HasAudio())
			return;
		if (object->GetAudioPath().empty())
			return;
		if (!Init())
			return;

		RemoveObject(object);

		SoundSlot& slot = s_Sounds[object];
		slot = SoundSlot{};
		ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC;
		ma_result result = ma_sound_init_from_file(
			&s_Engine,
			object->GetAudioPath().c_str(),
			flags,
			nullptr,
			nullptr,
			&slot.sound);

		if (result != MA_SUCCESS)
		{
			s_Sounds.erase(object);
			BZ_INFO("Failed to load audio '{0}' (code {1})", object->GetAudioPath(), (int)result);
			return;
		}

		slot.initialized = true;
		ma_sound_set_volume(&slot.sound, object->GetAudioVolume());
		ma_sound_set_looping(&slot.sound, object->GetAudioLoop() ? MA_TRUE : MA_FALSE);
		ma_sound_start(&slot.sound);
	}

	void AudioEngine::OnPlayModeStart(const std::vector<GameObject*>& objects)
	{
		if (!Init())
			return;

		for (GameObject* object : objects)
		{
			if (!object || !object->HasAudio())
				continue;
			if (object->GetAudioPlayOnStart())
				Play(object);
		}
	}

	void AudioEngine::OnPlayModeStop()
	{
		StopAll();
	}
}
