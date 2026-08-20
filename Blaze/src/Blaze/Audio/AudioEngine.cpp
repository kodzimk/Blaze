#include "AudioEngine.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Logging/Log.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace Blaze
{
	struct SoundSlot
	{
		ma_sound sound{};
		bool initialized = false;
	};

	struct OneShotSlot
	{
		ma_sound sound{};
		bool initialized = false;
	};

	static ma_engine s_Engine{};
	static bool s_Ready = false;
	static std::unordered_map<GameObject*, SoundSlot> s_Sounds;
	static std::vector<OneShotSlot> s_OneShots;
	static float s_ListenerX = 0.0f;
	static float s_ListenerY = 0.0f;

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

	void AudioEngine::SetListenerPosition(float x, float y)
	{
		s_ListenerX = x;
		s_ListenerY = y;
	}

	float AudioEngine::ComputeFalloffVolume(GameObject* object)
	{
		if (!object)
			return 0.0f;

		const float base = object->GetAudioVolume();
		if (!object->GetAudioFalloff())
			return base;

		float minDist = object->GetAudioMinDistance();
		float maxDist = object->GetAudioMaxDistance();
		if (maxDist < minDist)
			std::swap(minDist, maxDist);
		if (maxDist <= 1e-6f)
			return base;

		const glm::vec3 pos = object->GetPosition();
		const float dx = pos.x - s_ListenerX;
		const float dy = pos.y - s_ListenerY;
		const float dist = std::sqrt(dx * dx + dy * dy);

		if (dist <= minDist)
			return base;
		if (dist >= maxDist)
			return 0.0f;

		const float t = (dist - minDist) / (maxDist - minDist);
		return base * (1.0f - t);
	}

	static void DestroySlot(SoundSlot& slot)
	{
		if (slot.initialized)
		{
			ma_sound_uninit(&slot.sound);
			slot.initialized = false;
		}
	}

	static void DestroyOneShot(OneShotSlot& slot)
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

	void AudioEngine::StopOneShots()
	{
		for (OneShotSlot& slot : s_OneShots)
			DestroyOneShot(slot);
		s_OneShots.clear();
	}

	void AudioEngine::StopAll()
	{
		for (auto& entry : s_Sounds)
			DestroySlot(entry.second);
		s_Sounds.clear();
		StopOneShots();
	}

	void AudioEngine::Update()
	{
		for (auto& entry : s_Sounds)
		{
			GameObject* object = entry.first;
			SoundSlot& slot = entry.second;
			if (!object || !slot.initialized)
				continue;
			if (ma_sound_is_playing(&slot.sound) != MA_TRUE)
				continue;
			ma_sound_set_volume(&slot.sound, ComputeFalloffVolume(object));
		}

		if (s_OneShots.empty())
			return;

		for (auto it = s_OneShots.begin(); it != s_OneShots.end(); )
		{
			if (!it->initialized || ma_sound_is_playing(&it->sound) != MA_TRUE)
			{
				DestroyOneShot(*it);
				it = s_OneShots.erase(it);
			}
			else
			{
				++it;
			}
		}
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
		ma_sound_set_volume(&it->second.sound, ComputeFalloffVolume(object));
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
		ma_sound_set_volume(&slot.sound, ComputeFalloffVolume(object));
		ma_sound_set_looping(&slot.sound, object->GetAudioLoop() ? MA_TRUE : MA_FALSE);
		ma_sound_start(&slot.sound);
	}

	bool AudioEngine::PlayOneShot(const std::string& path, float volume)
	{
		if (path.empty())
			return false;
		if (!Init())
			return false;

		Update(); // reclaim finished slots first

		s_OneShots.emplace_back();
		OneShotSlot& slot = s_OneShots.back();
		ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC;
		ma_result result = ma_sound_init_from_file(
			&s_Engine,
			path.c_str(),
			flags,
			nullptr,
			nullptr,
			&slot.sound);

		if (result != MA_SUCCESS)
		{
			s_OneShots.pop_back();
			BZ_INFO("Failed to play one-shot '{0}' (code {1})", path, (int)result);
			return false;
		}

		slot.initialized = true;
		const float vol = (volume < 0.0f) ? 0.0f : ((volume > 4.0f) ? 4.0f : volume);
		ma_sound_set_volume(&slot.sound, vol);
		ma_sound_set_looping(&slot.sound, MA_FALSE);
		ma_sound_start(&slot.sound);
		return true;
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
