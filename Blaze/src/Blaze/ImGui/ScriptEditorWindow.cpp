#include "ScriptEditorWindow.h"

#include "../vendor/imgui/src/imgui.h"
#include "../vendor/imgui/src/imgui_internal.h"
#include "Blaze/Logging/Log.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace Blaze
{
	ScriptEditorWindow* ScriptEditorWindow::s_Instance = nullptr;

	const ScriptEditorWindow::CompletionItem* ScriptEditorWindow::GetCompletionCatalog(int& outCount)
	{
		static const CompletionItem kItems[] = {
			{ "OnStart", "OnStart", "function OnStart()" },
			{ "OnUpdate", "OnUpdate", "function OnUpdate(dt)" },
			{ "OnCollisionBegin", "OnCollisionBegin", "function OnCollisionBegin(other)" },
			{ "OnCollisionEnd", "OnCollisionEnd", "function OnCollisionEnd(other)" },
			{ "OnCollisionEnter", "OnCollisionEnter", "alias of OnCollisionBegin" },
			{ "OnCollisionExit", "OnCollisionExit", "alias of OnCollisionEnd" },
			{ "OnTriggerBegin", "OnTriggerBegin", "function OnTriggerBegin(other) - trigger overlaps" },
			{ "OnTriggerEnd", "OnTriggerEnd", "function OnTriggerEnd(other)" },
			{ "OnClicked", "OnWidgetClicked", "function OnWidgetClicked(widgetPath, buttonName)" },
			{ "OnWidgetClicked", "OnWidgetClicked", "function OnWidgetClicked(widgetId, buttonName)" },
			{ "CreateWidget", "CreateWidget", "ui = CreateWidget(\"HUD\")" },
			{ "LoadWidget", "LoadWidget", "ui = LoadWidget(\"content/a.widget\")" },
			{ "ShowWidget", "ShowWidget", "ui = ShowWidget(\"content/a.widget\")" },
			{ "HideWidget", "HideWidget", "HideWidget(ui) or HideWidget(path)" },
			{ "SetWidgetText", "SetWidgetText", "SetWidgetText(ui, \"Title\", \"Hi\") or SetWidgetText(\"content/a.widget\", \"Title\", \"Hi\")" },
			{ "GetWidgetText", "GetWidgetText", "t = GetWidgetText(ui, \"Title\")" },
			{ "AddText", "AddText", "ui:AddText(name, text [, ax, ay [, parent]])" },
			{ "AddButton", "AddButton", "ui:AddButton(name, text [, ax, ay [, parent]])" },
			{ "AddImage", "AddImage", "ui:AddImage(name, texturePath [, ax, ay [, parent]])" },
			{ "OnTriggerEnter", "OnTriggerEnter", "alias of OnTriggerBegin" },
			{ "OnTriggerExit", "OnTriggerExit", "alias of OnTriggerEnd" },
			{ "Delay", "Delay", "Delay(seconds, function() ... end) - UE Delay" },
			{ "SetTimer", "SetTimer", "id = SetTimer(fn, rate [, looping]) or SetTimer(rate, fn [, looping])" },
			{ "ClearTimer", "ClearTimer", "ClearTimer(id)" },
			{ "IsTimerActive", "IsTimerActive", "IsTimerActive(id)" },
			{ "ClearAllTimers", "ClearAllTimers", "ClearAllTimers()" },
			{ "Tween", "Tween", "Tween(from, to, duration, onUpdate [, onComplete [, ease]])" },
			{ "TweenVec2", "TweenVec2", "TweenVec2(x0,y0,x1,y1, duration, onUpdate [, onComplete [, ease]])" },
			{ "EmitParticles", "EmitParticles", "EmitParticles(x, y, count [, speed, life, size])" },
			{ "Particles", "Particles", "Particles.Burst(x,y,count) / Particles.Emit(...) / Particles.Clear()" },
			{ "Destroy", "Destroy", "Destroy(gameObject) or Destroy()" },
			{ "FindByName", "FindByName", "obj = FindByName(\"Camera\")" },
			{ "FindAllByName", "FindAllByName", "list = FindAllByName(\"Coin\")" },
			{ "SetTexture", "SetTexture", "SetTexture(\"content/tex.png\") or obj:SetTexture(path)" },
			{ "GetTexture", "GetTexture", "path = GetTexture()" },
			{ "SetZoom", "SetZoom", "SetZoom(1.5) - camera zoom" },
			{ "GetZoom", "GetZoom", "z = GetZoom()" },
			{ "SetDrivesView", "SetDrivesView", "SetDrivesView(true) - active game camera" },
			{ "DrivesView", "DrivesView", "DrivesView()" },
			{ "IsCamera", "IsCamera", "IsCamera()" },
			{ "GetChildren", "GetChildren", "kids = GetChildren()" },
			{ "GetChild", "GetChild", "child = GetChild(1) - 1-based" },
			{ "GetChildCount", "GetChildCount", "n = GetChildCount()" },
			{ "SetCameraFollow", "SetCameraFollow", "SetCameraFollow() / SetCameraFollow(0.12) / SetCameraFollow(target, smooth, ox, oy)" },
			{ "ClearCameraFollow", "ClearCameraFollow", "ClearCameraFollow() - stop camera follow" },
			{ "EnableMouse", "EnableMouse", "EnableMouse(true) - show cursor in Game view" },
			{ "ShowCursor", "ShowCursor", "ShowCursor(true/false)" },
			{ "SetCursorVisible", "SetCursorVisible", "SetCursorVisible(true/false)" },
			{ "IsMouseEnabled", "IsMouseEnabled", "IsMouseEnabled()" },
			{ "Spawn", "Spawn", "Spawn() / Spawn(\"text\") / Spawn(\"box\") / Spawn({type=,tag=,x=,y=})" },
			{ "SetText", "SetText", "SetText(\"Hello\") - Text GameObject" },
			{ "GetText", "GetText", "text = GetText() - Text GameObject" },
			{ "SetFontSize", "SetFontSize", "SetFontSize(48)" },
			{ "FindByTag", "FindByTag", "obj = FindByTag(\"Player\")" },
			{ "FindAllByTag", "FindAllByTag", "list = FindAllByTag(\"Enemy\")" },
			{ "SetTag", "SetTag", "SetTag(\"Player\")" },
			{ "AddTag", "AddTag", "AddTag(\"Enemy\")" },
			{ "GetTag", "GetTag", "tag = GetTag()" },
			{ "HasTag", "HasTag", "HasTag(\"Player\")" },
			{ "GetCurrentMapName", "GetCurrentMapName", "name = GetCurrentMapName()" },
			{ "OpenMap", "OpenMap", "OpenMap(\"Main\") or OpenMap(\"Level2\")" },
			{ "self", "self", "this GameObject (nil in map scripts)" },
			{ "mapName", "mapName", "current map name (map scripts)" },

			{ "print", "print", "print(...)" },
			{ "PrintString", "PrintString", "PrintString(message, duration?)" },
			{ "SetVisible", "SetVisible", "SetVisible(true/false) or obj:SetVisible(false)" },
			{ "IsVisible", "IsVisible", "IsVisible() or obj:IsVisible()" },
			{ "SetLocation", "SetLocation", "SetLocation(x, y [, z])" },
			{ "SetPosition", "SetPosition", "SetPosition(x, y [, z])" },
			{ "GetLocation", "GetLocation", "x, y = GetLocation()" },
			{ "SetRotation", "SetRotation", "SetRotation(degrees)" },
			{ "GetRotation", "GetRotation", "degrees = GetRotation()" },
			{ "GetRightVector", "GetRightVector", "rx, ry = GetRightVector() - local +X" },
			{ "GetUpVector", "GetUpVector", "ux, uy = GetUpVector() - local +Y" },
			{ "GetLeftVector", "GetLeftVector", "lx, ly = GetLeftVector()" },
			{ "GetDownVector", "GetDownVector", "dx, dy = GetDownVector()" },
			{ "Rotate", "Rotate", "Rotate(degrees)" },
			{ "SetColor", "SetColor", "SetColor(r,g,b [,a]) or SetColor({r=,g=,b=,a=})" },
			{ "GetColor", "GetColor", "r,g,b,a = GetColor()" },
			{ "SetScale", "SetScale", "SetScale(x [, y])" },
			{ "GetScale", "GetScale", "x, y = GetScale()" },
			{ "Translate", "Translate", "Translate(x, y) or Translate({x=, y=})" },
			{ "SetVelocity", "SetVelocity", "SetVelocity(x, y) - needs PhysicsRigidbody2D" },
			{ "GetVelocity", "GetVelocity", "vx, vy = GetVelocity()" },
			{ "AddVelocity", "AddVelocity", "AddVelocity(x, y)" },
			{ "ApplyForce", "ApplyForce", "ApplyForce(fx, fy) - continuous force" },
			{ "ApplyImpulse", "ApplyImpulse", "ApplyImpulse(ix, iy) - instant kick" },
			{ "Raycast", "Raycast", "hit = Raycast(x1,y1,x2,y2 [, drawDebug]) — hit.Hit, hit.Object, hit.Name, hit.Location" },
			{ "RaycastAll", "RaycastAll", "hits = RaycastAll(...) — array of hit structs" },
			{ "OverlapPoint", "OverlapPoint", "objs = OverlapPoint(x, y [, mask])" },
			{ "OverlapCircle", "OverlapCircle", "objs = OverlapCircle(x, y, radius [, mask])" },
			{ "MASK_ALL", "MASK_ALL", "MASK_ALL - hit all collision layers" },
			{ "SetGravity", "SetGravity", "SetGravity(x, y) world gravity" },
			{ "GetGravity", "GetGravity", "gx, gy = GetGravity()" },
			{ "SetGravityScale", "SetGravityScale", "SetGravityScale(1.0)" },
			{ "GetGravityScale", "GetGravityScale", "scale = GetGravityScale()" },
			{ "SetKinematic", "SetKinematic", "SetKinematic(true/false)" },
			{ "IsKinematic", "IsKinematic", "IsKinematic()" },
			{ "SetMass", "SetMass", "SetMass(mass)" },
			{ "GetMass", "GetMass", "mass = GetMass()" },
			{ "HasPhysics", "HasPhysics", "HasPhysics() - rigidbody collision?" },
			{ "IsGrounded", "IsGrounded", "IsGrounded([maxSlopeDeg]) - standing on floor?" },
			{ "CheckGround", "CheckGround", "CheckGround([probePx [, maxSlope]]) - grounded + feet ray" },
			{ "IsTouchingCeiling", "IsTouchingCeiling", "IsTouchingCeiling([maxSlopeDeg])" },
			{ "IsTouchingWall", "IsTouchingWall", "IsTouchingWall()" },
			{ "IsTouchingWallLeft", "IsTouchingWallLeft", "IsTouchingWallLeft()" },
			{ "IsTouchingWallRight", "IsTouchingWallRight", "IsTouchingWallRight()" },
			{ "GetGroundNormal", "GetGroundNormal", "nx, ny = GetGroundNormal([maxSlopeDeg])" },
			{ "GetContactCount", "GetContactCount", "n = GetContactCount([includeTriggers])" },
			{ "SetTrigger", "SetTrigger", "SetTrigger(true) - overlap only" },
			{ "ShowWidget", "ShowWidget", "ShowWidget(ui) or ShowWidget(\"path.widget\")" },
			{ "HideWidget", "HideWidget", "HideWidget(ui) or HideWidget(path)" },
			{ "CreateWidget", "CreateWidget", "ui = CreateWidget(\"HUD\") - empty canvas" },
			{ "LoadWidget", "LoadWidget", "ui = LoadWidget(\"content/a.widget\")" },
			{ "SetWidgetText", "SetWidgetText", "SetWidgetText(ui, \"Title\", \"Hi\")" },
			{ "GetWidgetText", "GetWidgetText", "GetWidgetText(ui, nodeName)" },
			{ "AddText", "AddText", "score = ui:AddText(\"Score\", \"0\", 0.5, 0.1)" },
			{ "AddButton", "AddButton", "btn = ui:AddButton(\"Play\", \"Play\", 0.5, 0.6)" },
			{ "AddImage", "AddImage", "logo = ui:AddImage(\"Logo\", \"content/icon.png\", 0.5, 0.2)" },
			{ "Find", "Find", "el = ui:Find(\"Score\")" },
			{ "FindText", "FindText", "score = ui:FindText(\"Score\")" },
			{ "FindButton", "FindButton", "btn = ui:FindButton(\"Play\")" },
			{ "FindImage", "FindImage", "logo = ui:FindImage(\"Logo\")" },
			{ "SetTexture", "SetTexture", "logo:SetTexture(\"content/icon.png\")" },
			{ "GetTexture", "GetTexture", "path = logo:GetTexture()" },
			{ "SetImage", "SetImage", "logo:SetImage(\"content/icon.png\")" },
			{ "GetImage", "GetImage", "path = logo:GetImage()" },
			{ "SetWidgetImage", "SetWidgetImage", "SetWidgetImage(ui, \"Logo\", \"content/icon.png\")" },
			{ "IsTrigger", "IsTrigger", "IsTrigger()" },
			{ "SetCollisionLayer", "SetCollisionLayer", "SetCollisionLayer(LAYER_PLAYER)" },
			{ "GetCollisionLayer", "GetCollisionLayer", "layer = GetCollisionLayer()" },
			{ "SetCollisionMask", "SetCollisionMask", "SetCollisionMask(maskBits)" },
			{ "SetCollidesWithLayer", "SetCollidesWithLayer", "SetCollidesWithLayer(LAYER_ENEMY, true)" },
			{ "LAYER_DEFAULT", "LAYER_DEFAULT", "collision layer 0" },
			{ "LAYER_PLAYER", "LAYER_PLAYER", "collision layer 1" },
			{ "LAYER_ENEMY", "LAYER_ENEMY", "collision layer 2" },
			{ "LAYER_PROJECTILE", "LAYER_PROJECTILE", "collision layer 3" },
			{ "LAYER_TRIGGER", "LAYER_TRIGGER", "collision layer 4" },
			{ "LAYER_GROUND", "LAYER_GROUND", "collision layer 5" },
			{ "LAYER_PICKUP", "LAYER_PICKUP", "collision layer 6" },
			{ "PlayAudio", "PlayAudio", "PlayAudio() or PlayAudio(obj) - Sound component" },
			{ "PlayOneShot", "PlayOneShot", "PlayOneShot(\"content/audio/hit.wav\" [, volume])" },
			{ "PlaySound", "PlaySound", "PlaySound(path [, volume]) - alias of PlayOneShot" },
			{ "StopAudio", "StopAudio", "StopAudio() or StopAudio(obj)" },
			{ "StopAudio", "StopAudio", "StopAudio() or StopAudio(obj)" },
			{ "IsAudioPlaying", "IsAudioPlaying", "IsAudioPlaying()" },
			{ "SetAudioVolume", "SetAudioVolume", "SetAudioVolume(0.0 to 1.0)" },
			{ "GetAudioVolume", "GetAudioVolume", "vol = GetAudioVolume()" },
			{ "SetAudioLoop", "SetAudioLoop", "SetAudioLoop(true/false)" },
			{ "GetAudioLoop", "GetAudioLoop", "GetAudioLoop()" },
			{ "SetAudioPath", "SetAudioPath", "SetAudioPath(\"content/audio/x.wav\")" },
			{ "GetAudioPath", "GetAudioPath", "path = GetAudioPath()" },
			{ "HasAudio", "HasAudio", "HasAudio()" },
			{ "SetFlipbook", "SetFlipbook", "SetFlipbook(cols, rows [, fps])" },
			{ "SetFlipbookFrameCount", "SetFlipbookFrameCount", "SetFlipbookFrameCount(n) 0=full grid" },
			{ "SetFlipbookFps", "SetFlipbookFps", "SetFlipbookFps(12)" },
			{ "PlayFlipbook", "PlayFlipbook", "PlayFlipbook() or PlayFlipbook(\"Idle\")" },
			{ "StopFlipbook", "StopFlipbook", "StopFlipbook()" },
			{ "PauseFlipbook", "PauseFlipbook", "PauseFlipbook()" },
			{ "SetFlipbookFrame", "SetFlipbookFrame", "SetFlipbookFrame(index)" },
			{ "GetFlipbookFrame", "GetFlipbookFrame", "frame = GetFlipbookFrame()" },
			{ "SetFlipbookLoop", "SetFlipbookLoop", "SetFlipbookLoop(true)" },
			{ "IsFlipbookPlaying", "IsFlipbookPlaying", "IsFlipbookPlaying()" },
			{ "AddFlipbookClip", "AddFlipbookClip", "AddFlipbookClip(\"Idle\", 0, 7 [, fps [, loop]])" },
			{ "SetFlipbookClip", "SetFlipbookClip", "SetFlipbookClip(\"Idle\", 0, 7 [, fps, loop])" },
			{ "RemoveFlipbookClip", "RemoveFlipbookClip", "RemoveFlipbookClip(\"Idle\")" },
			{ "ClearFlipbookClips", "ClearFlipbookClips", "ClearFlipbookClips()" },
			{ "SetActiveFlipbookClip", "SetActiveFlipbookClip", "SetActiveFlipbookClip(\"Idle\")" },
			{ "GetActiveFlipbookClip", "GetActiveFlipbookClip", "name = GetActiveFlipbookClip()" },
			{ "HasFlipbookClip", "HasFlipbookClip", "HasFlipbookClip(\"Idle\")" },
			{ "GetFlipbookClipCount", "GetFlipbookClipCount", "n = GetFlipbookClipCount()" },
			{ "GetFlipbookClipName", "GetFlipbookClipName", "name = GetFlipbookClipName(i)" },
			{ "SetFlipbookAsset", "SetFlipbookAsset", "SetFlipbookAsset(\"content/anims/Player.flipbook\")" },
			{ "GetFlipbookAsset", "GetFlipbookAsset", "path = GetFlipbookAsset()" },
			{ "SaveFlipbookAsset", "SaveFlipbookAsset", "SaveFlipbookAsset()" },
			{ "CreateSaveObject", "CreateSaveObject", "data = CreateSaveObject([\"PlayerSave\"])" },
			{ "SaveSaveObject", "SaveSaveObject", "SaveSaveObject(\"Slot1\", data)" },
			{ "LoadSaveObject", "LoadSaveObject", "data = LoadSaveObject(\"Slot1\" [, \"PlayerSave\"])" },
			{ "CastSaveObject", "CastSaveObject", "save = CastSaveObject(data, \"PlayerSave\")" },
			{ "DoesSaveObjectExist", "DoesSaveObjectExist", "DoesSaveObjectExist(\"Slot1\")" },
			{ "GetSaveObjectPath", "GetSaveObjectPath", "path = GetSaveObjectPath(\"Slot1\")" },
			{ "SetTilemapSize", "SetTilemapSize", "SetTilemapSize(width, height)" },
			{ "SetTileCellSize", "SetTileCellSize", "SetTileCellSize(32)" },
			{ "SetTileset", "SetTileset", "SetTileset(cols, rows)" },
			{ "SetTile", "SetTile", "SetTile(x, y, tileId)  -1 clears" },
			{ "GetTile", "GetTile", "id = GetTile(x, y)" },
			{ "FillTiles", "FillTiles", "FillTiles(tileId)" },
			{ "ClearTiles", "ClearTiles", "ClearTiles()" },
			{ "GetTileLayerCount", "GetTileLayerCount", "GetTileLayerCount()" },
			{ "GetActiveTileLayer", "GetActiveTileLayer", "GetActiveTileLayer()" },
			{ "SetActiveTileLayer", "SetActiveTileLayer", "SetActiveTileLayer(index)" },
			{ "AddTileLayer", "AddTileLayer", "AddTileLayer([name])" },
			{ "RemoveTileLayer", "RemoveTileLayer", "RemoveTileLayer(index)" },
			{ "SetTileLayerVisible", "SetTileLayerVisible", "SetTileLayerVisible(index, visible)" },
			{ "SetTileLayerCollision", "SetTileLayerCollision", "SetTileLayerCollision(index, collision)" },
			{ "SetTileLayerOpacity", "SetTileLayerOpacity", "SetTileLayerOpacity(index, opacity)" },
			{ "SetTileOnLayer", "SetTileOnLayer", "SetTileOnLayer(layer, x, y, tileId)" },
			{ "GetTileOnLayer", "GetTileOnLayer", "GetTileOnLayer(layer, x, y)" },
			{ "ImportTiledMap", "ImportTiledMap", "ImportTiledMap(tiledPath [, outMapPath]) -> map path" },
			{ "GetTilemapSize", "GetTilemapSize", "w, h = GetTilemapSize()" },
			{ "WorldToTile", "WorldToTile", "tx, ty, ok = WorldToTile(wx, wy)" },
			{ "TileCenter", "TileCenter", "cx, cy = TileCenter(tx, ty)" },
			{ "SetTileCollision", "SetTileCollision", "SetTileCollision(true)" },
			{ "SetTileSolid", "SetTileSolid", "SetTileSolid(tileId, true)" },
			{ "IsTileSolid", "IsTileSolid", "solid = IsTileSolid(tileId)" },
			{ "IsSolidAt", "IsSolidAt", "solid = IsSolidAt(x, y)" },
			{ "SetParent", "SetParent", "SetParent(obj) or SetParent() to clear" },
			{ "GetParent", "GetParent", "parent = GetParent()" },
			{ "ClearParent", "ClearParent", "ClearParent() keep world pose" },
			{ "GetWorldLocation", "GetWorldLocation", "x, y = GetWorldLocation()" },
			{ "MOUSE_LEFT", "MOUSE_LEFT", "mouse button constant" },
			{ "MOUSE_RIGHT", "MOUSE_RIGHT", "mouse button constant" },
			{ "MOUSE_MIDDLE", "MOUSE_MIDDLE", "mouse button constant" },

		
			{ "Log", "Log", "Log.Info / Log.Warn" },
			{ "Log.Info", "Log.Info", "Log.Info(message)" },
			{ "Log.Warn", "Log.Warn", "Log.Warn(message)" },

		
			{ "Input", "Input", "Input.* keyboard / mouse" },
			{ "Input.IsKeyDown", "Input.IsKeyDown", "Input.IsKeyDown(\"A\")" },
			{ "Input.IsKeyPressed", "Input.IsKeyPressed", "Input.IsKeyPressed(\"SPACE\")" },
			{ "Input.IsMouseDown", "Input.IsMouseDown", "Input.IsMouseDown(MOUSE_LEFT)" },
			{ "Input.IsMousePressed", "Input.IsMousePressed", "Input.IsMousePressed(MOUSE_LEFT)" },
			{ "Input.GetMouseX", "Input.GetMouseX", "Input.GetMouseX()" },
			{ "Input.GetMouseY", "Input.GetMouseY", "Input.GetMouseY()" },
			{ "Input.GetMouseWorld", "Input.GetMouseWorld", "wx, wy = Input.GetMouseWorld()" },
			{ "Input.GetMouseWorldX", "Input.GetMouseWorldX", "Input.GetMouseWorldX()" },
			{ "Input.GetMouseWorldY", "Input.GetMouseWorldY", "Input.GetMouseWorldY()" },
			{ "Input.IsMouseOverGame", "Input.IsMouseOverGame", "Input.IsMouseOverGame()" },
			{ "Input.ScreenToWorld", "Input.ScreenToWorld", "wx, wy = Input.ScreenToWorld(sx, sy)" },
			{ "GetMouseWorld", "GetMouseWorld", "wx, wy = GetMouseWorld()" },

		
			{ "function", "function", "Lua keyword" },
			{ "local", "local", "Lua keyword" },
			{ "if", "if", "Lua keyword" },
			{ "then", "then", "Lua keyword" },
			{ "else", "else", "Lua keyword" },
			{ "elseif", "elseif", "Lua keyword" },
			{ "end", "end", "Lua keyword" },
			{ "return", "return", "Lua keyword" },
			{ "for", "for", "Lua keyword" },
			{ "while", "while", "Lua keyword" },
			{ "do", "do", "Lua keyword" },
			{ "repeat", "repeat", "Lua keyword" },
			{ "until", "until", "Lua keyword" },
			{ "break", "break", "Lua keyword" },
			{ "true", "true", "Lua boolean" },
			{ "false", "false", "Lua boolean" },
			{ "nil", "nil", "Lua nil" },
			{ "and", "and", "Lua keyword" },
			{ "or", "or", "Lua keyword" },
			{ "not", "not", "Lua keyword" },
			{ "math", "math", "Lua math library" },
			{ "string", "string", "Lua string library" },
			{ "table", "table", "Lua table library" },
			{ "pairs", "pairs", "pairs(t)" },
			{ "ipairs", "ipairs", "ipairs(t)" },
			{ "tonumber", "tonumber", "tonumber(v)" },
			{ "tostring", "tostring", "tostring(v)" },
			{ "type", "type", "type(v)" },
		};

		outCount = (int)(sizeof(kItems) / sizeof(kItems[0]));
		return kItems;
	}

	static bool StartsWithIgnoreCase(const char* text, const char* prefix)
	{
		if (!text || !prefix)
			return false;
		while (*prefix)
		{
			char a = (char)std::tolower((unsigned char)*text++);
			char b = (char)std::tolower((unsigned char)*prefix++);
			if (a != b)
				return false;
			if (!*(text - 1) && *prefix)
				return false;
		}
		return true;
	}

	static bool IsIdentChar(char c)
	{
		return std::isalnum((unsigned char)c) || c == '_' || c == '.';
	}

	int ScriptEditorWindow::ScriptEditCallback(ImGuiInputTextCallbackData* data)
	{
		EditUserData* ud = (EditUserData*)data->UserData;
		if (!ud || !ud->editor || !ud->buf)
			return 0;

		ScriptEditorWindow* editor = ud->editor;

		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			size_t needed = (size_t)data->BufTextLen + 1;
			if (ud->buf->size() < needed)
				ud->buf->resize(needed + 1024);
			data->Buf = ud->buf->data();
			data->BufSize = (int)ud->buf->size();
			return 0;
		}

		if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
		{
			// Tab always lands here (never use AllowTabInput).
			if (!editor->m_Filtered.empty())
			{
				editor->AcceptCompletion();
				editor->ApplyCompletionToCallback(data);
			}
			else
			{
				data->InsertChars(data->CursorPos, "\t");
				editor->m_Dirty = true;
			}
			return 0;
		}

		if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways)
		{
			editor->m_CursorPos = data->CursorPos;

			if (editor->m_AcceptCompletion && !editor->m_PendingInsert.empty())
				editor->ApplyCompletionToCallback(data);

			if (!editor->m_SuppressCompletions)
				editor->RebuildCompletionFilter(data->Buf, data->CursorPos);
			else
				editor->m_ShowCompletions = false;
		}

		return 0;
	}

	ScriptEditorWindow::ScriptEditorWindow()
		: m_Open(false), m_Dirty(false)
	{
		s_Instance = this;
		m_EditBuf.assign(4096, '\0');
		m_EditUserData.buf = &m_EditBuf;
		m_EditUserData.editor = this;
	}

	ScriptEditorWindow::~ScriptEditorWindow()
	{
		if (s_Instance == this)
			s_Instance = nullptr;
	}

	void ScriptEditorWindow::OpenFile(const std::string& path)
	{
		if (s_Instance)
			s_Instance->Open(path);
	}

	bool ScriptEditorWindow::IsFocused()
	{
		return s_Instance && s_Instance->m_Open && s_Instance->m_WindowFocused;
	}

	bool ScriptEditorWindow::IsBlockingMainEditor()
	{
		if (!s_Instance || !s_Instance->m_Open)
			return false;
		return s_Instance->m_WindowFocused || s_Instance->m_ShowCompletions;
	}

	void ScriptEditorWindow::SyncEditBufferFromString()
	{
		m_EditBuf.assign(m_Buffer.begin(), m_Buffer.end());
		m_EditBuf.push_back('\0');
		if (m_EditBuf.size() < 4096)
			m_EditBuf.resize(4096, '\0');
	}

	void ScriptEditorWindow::SyncStringFromEditBuffer()
	{
		if (m_EditBuf.empty())
		{
			m_Buffer.clear();
			return;
		}
		m_Buffer.assign(m_EditBuf.data());
	}

	void ScriptEditorWindow::Open(const std::string& path)
	{
		if (path.empty())
			return;

		if (m_Open && m_Dirty && !m_Path.empty() && m_Path != path)
		{
			SyncStringFromEditBuffer();
			SaveFile();
		}

		if (!LoadFile(path))
			return;

		SyncEditBufferFromString();
		m_Open = true;
		m_Dirty = false;
		m_FocusRequest = true;
		m_PlaceFloating = true;
		m_ShowCompletions = false;
		m_ForceCompletions = false;
		m_AcceptCompletion = false;
		m_SuppressCompletions = false;
		m_RefocusEditor = false;
		m_Filtered.clear();
	}

	bool ScriptEditorWindow::LoadFile(const std::string& path)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in.is_open())
		{
			BZ_INFO("Script editor: failed to open {0}", path);
			return false;
		}

		std::ostringstream ss;
		ss << in.rdbuf();
		m_Buffer = ss.str();
		m_Path = path;
		return true;
	}

	bool ScriptEditorWindow::SaveFile()
	{
		if (m_Path.empty())
			return false;

		SyncStringFromEditBuffer();

		std::ofstream out(m_Path, std::ios::binary | std::ios::trunc);
		if (!out.is_open())
		{
			BZ_INFO("Script editor: failed to save {0}", m_Path);
			return false;
		}

		out.write(m_Buffer.data(), (std::streamsize)m_Buffer.size());
		out.close();
		m_Dirty = false;
		BZ_INFO("Saved script: {0}", m_Path);
		return true;
	}

	void ScriptEditorWindow::RebuildCompletionFilter(const char* buf, int cursor)
	{
		m_Filtered.clear();
		m_Prefix.clear();
		m_WordStart = cursor;

		if (!buf || cursor < 0)
		{
			m_ShowCompletions = false;
			return;
		}

		int start = cursor;
		while (start > 0 && IsIdentChar(buf[start - 1]))
			--start;

		m_WordStart = start;
		m_Prefix.assign(buf + start, buf + cursor);

		const bool afterDot = !m_Prefix.empty() && m_Prefix.back() == '.';
		const bool wantPopup = m_ForceCompletions || m_Prefix.size() >= 1 || afterDot;

		if (!wantPopup)
		{
			m_ShowCompletions = false;
			return;
		}

		int count = 0;
		const CompletionItem* catalog = GetCompletionCatalog(count);
		for (int i = 0; i < count; ++i)
		{
			const CompletionItem& item = catalog[i];
			if (m_Prefix.empty() || StartsWithIgnoreCase(item.insert, m_Prefix.c_str()))
			{
				if (!m_ForceCompletions &&
					m_Prefix.size() == std::strlen(item.insert) &&
					StartsWithIgnoreCase(item.insert, m_Prefix.c_str()))
				{
					continue;
				}
				m_Filtered.push_back(&item);
			}
		}

		if (m_Filtered.empty())
		{
			m_ShowCompletions = false;
			return;
		}

		if (m_SelectedCompletion < 0 || m_SelectedCompletion >= (int)m_Filtered.size())
			m_SelectedCompletion = 0;

		m_ShowCompletions = true;
	}

	void ScriptEditorWindow::AcceptCompletion(int index)
	{
		if (m_Filtered.empty())
			return;

		if (index >= 0 && index < (int)m_Filtered.size())
			m_SelectedCompletion = index;

		if (m_SelectedCompletion < 0 || m_SelectedCompletion >= (int)m_Filtered.size())
			return;

		m_PendingInsert = m_Filtered[m_SelectedCompletion]->insert;
		m_AcceptCompletion = true;
		m_ForceCompletions = false;
		m_SuppressCompletions = true;
		m_ShowCompletions = false;
	}

	bool ScriptEditorWindow::ApplyCompletionToCallback(ImGuiInputTextCallbackData* data)
	{
		if (!m_AcceptCompletion || m_PendingInsert.empty() || !data)
			return false;

		int start = m_WordStart;
		int end = data->CursorPos;
		if (start < 0) start = 0;
		if (end < start) end = start;
		if (end > data->BufTextLen) end = data->BufTextLen;

		data->DeleteChars(start, end - start);
		data->InsertChars(start, m_PendingInsert.c_str());

		m_AcceptCompletion = false;
		m_PendingInsert.clear();
		m_ShowCompletions = false;
		m_ForceCompletions = false;
		m_SuppressCompletions = true;
		m_Dirty = true;
		return true;
	}

	void ScriptEditorWindow::ApplyCompletionToBuffer()
	{
		if (!m_AcceptCompletion || m_PendingInsert.empty())
			return;

		SyncStringFromEditBuffer();

		int len = (int)m_Buffer.size();
		int start = m_WordStart;
		int end = m_CursorPos;
		if (start < 0) start = 0;
		if (end < start) end = start;
		if (end > len) end = len;

		m_Buffer.replace((size_t)start, (size_t)(end - start), m_PendingInsert);
		SyncEditBufferFromString();

		m_CursorPos = start + (int)m_PendingInsert.size();
		m_AcceptCompletion = false;
		m_PendingInsert.clear();
		m_ShowCompletions = false;
		m_ForceCompletions = false;
		m_SuppressCompletions = true;
		m_Dirty = true;
		m_RefocusEditor = true;
	}

	static bool IsLuaIdentStart(char c)
	{
		return std::isalpha((unsigned char)c) || c == '_';
	}

	static bool IsLuaIdentChar(char c)
	{
		return std::isalnum((unsigned char)c) || c == '_';
	}

	static bool IsLuaKeyword(const char* start, int len)
	{
		static const char* kWords[] = {
			"and", "break", "do", "else", "elseif", "end", "false", "for", "function",
			"goto", "if", "in", "local", "nil", "not", "or", "repeat", "return",
			"then", "true", "until", "while"
		};
		for (const char* word : kWords)
		{
			if ((int)std::strlen(word) == len && std::strncmp(start, word, (size_t)len) == 0)
				return true;
		}
		return false;
	}

	void ScriptEditorWindow::DrawLuaSyntaxHighlight(const char* text, const ImVec2& textPos)
	{
		if (!text)
			return;

		ImDrawList* draw = ImGui::GetWindowDrawList();
		ImFont* font = ImGui::GetFont();
		const float fontSize = ImGui::GetFontSize();
		const float lineH = ImGui::GetTextLineHeight();

		const ImU32 colDefault = IM_COL32(220, 220, 225, 255);
		const ImU32 colKeyword = IM_COL32(86, 156, 214, 255);
		const ImU32 colString  = IM_COL32(206, 145, 120, 255);
		const ImU32 colComment = IM_COL32(106, 153, 85, 255);
		const ImU32 colNumber  = IM_COL32(181, 206, 168, 255);
		const ImU32 colApi     = IM_COL32(78, 201, 176, 255);
		const ImU32 colFunc    = IM_COL32(220, 220, 170, 255);

		static const std::unordered_set<std::string> kApi = {
			"Input", "Translate", "Rotate", "SetLocation", "SetPosition", "GetLocation",
			"SetRotation", "GetRotation", "GetRightVector", "GetUpVector", "GetLeftVector", "GetDownVector",
			"SetScale", "GetScale", "SetColor", "GetColor",
			"Destroy", "Spawn", "FindByTag", "FindAllByTag", "FindByName", "FindAllByName",
			"EmitParticles", "Particles",
			"Delay", "SetTimer", "ClearTimer", "IsTimerActive", "ClearAllTimers", "Tween", "TweenVec2",
			"SetTag", "AddTag", "GetTag",
			"HasTag", "print", "PrintString", "Log", "OnStart", "OnUpdate",
			"OnCollisionBegin", "OnCollisionEnd", "OnTriggerBegin", "OnTriggerEnd", "OnWidgetClicked", "OpenMap", "GetCurrentMapName",
			"ShowWidget", "HideWidget", "HideAllWidgets", "CreateWidget", "LoadWidget",
			"SetWidgetText", "GetWidgetText", "AddText", "AddButton", "AddImage",
			"FindImage", "SetTexture", "GetTexture", "SetImage", "GetImage", "SetWidgetImage",
			"SetZoom", "GetZoom", "SetDrivesView", "DrivesView", "IsCamera",
			"GetChildren", "GetChild", "GetChildCount", "SetCameraFollow", "ClearCameraFollow",
			"EnableMouse", "ShowCursor", "SetCursorVisible", "IsMouseEnabled", "IsCursorVisible",
			"Find", "FindText", "FindButton",
			"SetVisible", "SetSize", "SetPosition", "SetAnchor", "SetFontSize",
			"SetVelocity", "GetVelocity", "AddVelocity", "ApplyForce", "ApplyImpulse",
			"Raycast", "RaycastAll", "OverlapPoint", "OverlapCircle", "MASK_ALL",
			"SetGravity", "GetGravity", "SetGravityScale", "GetGravityScale",
			"SetKinematic", "IsKinematic", "SetMass", "GetMass", "HasPhysics",
			"IsGrounded", "CheckGround", "IsTouchingCeiling", "IsTouchingWall",
			"IsTouchingWallLeft", "IsTouchingWallRight", "GetGroundNormal", "GetContactCount",
			"SetTrigger", "IsTrigger", "SetCollisionLayer", "GetCollisionLayer", "SetCollisionMask", "SetCollidesWithLayer",
			"LAYER_DEFAULT", "LAYER_PLAYER", "LAYER_ENEMY", "LAYER_PROJECTILE", "LAYER_TRIGGER", "LAYER_GROUND", "LAYER_PICKUP",
			"PlayAudio", "PlayOneShot", "PlaySound", "StopAudio", "IsAudioPlaying", "SetAudioVolume", "GetAudioVolume",
			"SetAudioLoop", "GetAudioLoop", "SetAudioPath", "GetAudioPath", "HasAudio",
			"SetFlipbook", "SetFlipbookFrameCount", "SetFlipbookFps", "PlayFlipbook", "StopFlipbook",
			"PauseFlipbook", "SetFlipbookFrame", "GetFlipbookFrame", "SetFlipbookLoop", "IsFlipbookPlaying",
			"AddFlipbookClip", "SetFlipbookClip", "RemoveFlipbookClip", "ClearFlipbookClips",
			"SetActiveFlipbookClip", "GetActiveFlipbookClip", "HasFlipbookClip",
			"GetFlipbookClipCount", "GetFlipbookClipName",
			"SetFlipbookAsset", "GetFlipbookAsset", "SaveFlipbookAsset",
			"CreateSaveObject", "SaveSaveObject", "LoadSaveObject", "CastSaveObject",
			"DoesSaveObjectExist", "GetSaveObjectPath",
			"SetTilemapSize", "SetTileCellSize", "SetTileset", "SetTile", "GetTile", "FillTiles", "ClearTiles", "GetTilemapSize",
			"GetTileLayerCount", "GetActiveTileLayer", "SetActiveTileLayer", "AddTileLayer", "RemoveTileLayer",
			"SetTileLayerVisible", "SetTileLayerCollision", "SetTileLayerOpacity", "SetTileOnLayer", "GetTileOnLayer", "ImportTiledMap",
			"WorldToTile", "TileCenter", "SetTileCollision", "SetTileSolid", "IsTileSolid", "IsSolidAt",
			"SetParent", "GetParent", "ClearParent", "GetWorldLocation",
			"self", "mapName", "selfName"
		};

		ImVec2 pen = textPos;
		const char* p = text;
		while (*p)
		{
			if (*p == '\n')
			{
				pen.x = textPos.x;
				pen.y += lineH;
				++p;
				continue;
			}
			if (*p == '\r')
			{
				++p;
				continue;
			}

			// Comment: -- or --[[
			if (p[0] == '-' && p[1] == '-')
			{
				const char* start = p;
				if (p[2] == '[' && p[3] == '[')
				{
					p += 4;
					while (*p && !(p[0] == ']' && p[1] == ']'))
					{
						if (*p == '\n')
						{
							draw->AddText(font, fontSize, pen, colComment, start, p);
							pen.x = textPos.x;
							pen.y += lineH;
							start = p + 1;
						}
						++p;
					}
					if (*p)
						p += 2;
				}
				else
				{
					while (*p && *p != '\n')
						++p;
				}
				ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, start, p);
				draw->AddText(font, fontSize, pen, colComment, start, p);
				pen.x += size.x;
				continue;
			}

			// String
			if (*p == '"' || *p == '\'')
			{
				const char quote = *p;
				const char* start = p++;
				while (*p && *p != quote && *p != '\n')
				{
					if (*p == '\\' && p[1])
						p += 2;
					else
						++p;
				}
				if (*p == quote)
					++p;
				ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, start, p);
				draw->AddText(font, fontSize, pen, colString, start, p);
				pen.x += size.x;
				continue;
			}
			if (p[0] == '[' && p[1] == '[')
			{
				const char* start = p;
				p += 2;
				while (*p && !(p[0] == ']' && p[1] == ']'))
				{
					if (*p == '\n')
					{
						draw->AddText(font, fontSize, pen, colString, start, p);
						pen.x = textPos.x;
						pen.y += lineH;
						start = p + 1;
					}
					++p;
				}
				if (*p)
					p += 2;
				ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, start, p);
				draw->AddText(font, fontSize, pen, colString, start, p);
				pen.x += size.x;
				continue;
			}

			// Number
			if (std::isdigit((unsigned char)*p) || (*p == '.' && std::isdigit((unsigned char)p[1])))
			{
				const char* start = p++;
				while (std::isdigit((unsigned char)*p) || *p == '.' || *p == 'e' || *p == 'E' || *p == 'x' || *p == 'X')
					++p;
				ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, start, p);
				draw->AddText(font, fontSize, pen, colNumber, start, p);
				pen.x += size.x;
				continue;
			}

			// Identifier / keyword / API
			if (IsLuaIdentStart(*p))
			{
				const char* start = p++;
				while (IsLuaIdentChar(*p))
					++p;
				const int len = (int)(p - start);
				ImU32 color = colDefault;
				if (IsLuaKeyword(start, len))
					color = colKeyword;
				else
				{
					std::string word(start, start + len);
					if (kApi.find(word) != kApi.end())
						color = colApi;
					else
					{
						const char* q = p;
						while (*q == ' ' || *q == '\t')
							++q;
						if (*q == '(')
							color = colFunc;
					}
				}
				ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, start, p);
				draw->AddText(font, fontSize, pen, color, start, p);
				pen.x += size.x;
				continue;
			}

			// Single character / punctuation
			const char* start = p++;
			ImVec2 size = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, start, p);
			draw->AddText(font, fontSize, pen, colDefault, start, p);
			pen.x += size.x;
		}
	}

	void ScriptEditorWindow::DrawCompletionsPopup()
	{
		if (!m_ShowCompletions || m_Filtered.empty() || !m_HasPopupAnchor)
			return;

		ImVec2 pos(m_PopupAnchorX, m_PopupAnchorY);

		const float rowH = ImGui::GetTextLineHeightWithSpacing();
		const float height = (std::min)(8.0f, (float)m_Filtered.size()) * rowH + 12.0f;

		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(360.0f, height), ImGuiCond_Always);

	
		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoNavInputs |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_Tooltip |
			ImGuiWindowFlags_NoDocking;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.14f, 0.14f, 0.17f, 0.98f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.45f, 0.70f, 0.90f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.40f, 0.70f, 0.85f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.48f, 0.80f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.48f, 0.80f, 1.0f));

		if (ImGui::Begin("##ScriptCompletions", nullptr, flags))
		{
			for (int i = 0; i < (int)m_Filtered.size(); ++i)
			{
				const CompletionItem* item = m_Filtered[i];
				const bool selected = (i == m_SelectedCompletion);

				char row[256];
				std::snprintf(row, sizeof(row), "%s  -  %s##cmp%d", item->label, item->detail, i);

				if (ImGui::Selectable(row, selected))
				{
					AcceptCompletion(i);
					ApplyCompletionToBuffer();
				}

				if (selected)
					ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::End();

		ImGui::PopStyleColor(5);
		ImGui::PopStyleVar();
	}

	void ScriptEditorWindow::OnUpdate()
	{
		if (!m_Open)
		{
			m_WindowFocused = false;
			return;
		}

		if (m_EditBuf.empty())
			SyncEditBufferFromString();

		std::string title = "Script Editor";
		if (!m_Path.empty())
		{
			title += " - ";
			title += m_Path;
		}
		if (m_Dirty)
			title += " *";
		title += "###ScriptEditor";

		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.13f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.09f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.07f, 0.07f, 0.09f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.92f, 1.0f));

		if (m_PlaceFloating)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImVec2 size(680.0f, 460.0f);
			ImVec2 pos(
				viewport->WorkPos.x + viewport->WorkSize.x - size.x - 24.0f,
				viewport->WorkPos.y + 72.0f);
			ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
			ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
			ImGui::SetNextWindowSize(size, ImGuiCond_Always);
			ImGui::SetNextWindowViewport(viewport->ID);
			m_PlaceFloating = false;
		}

		if (m_FocusRequest)
		{
			ImGui::SetNextWindowFocus();
			m_FocusRequest = false;
		}

		const ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoCollapse;

		if (!ImGui::Begin(title.c_str(), &m_Open, flags))
		{
			m_WindowFocused = false;
			ImGui::End();
			ImGui::PopStyleColor(6);
			return;
		}

		m_WindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (m_WindowFocused)
			ImGui::SetNextFrameWantCaptureKeyboard(true);

		if (ImGui::Button("Save"))
			SaveFile();
		ImGui::SameLine();
		ImGui::TextDisabled("Ctrl+S save  |  Ctrl+Space IntelliSense");
		ImGui::Separator();

		ImVec2 avail = ImGui::GetContentRegionAvail();
		if (avail.x < 50.0f) avail.x = 50.0f;
		if (avail.y < 50.0f) avail.y = 50.0f;

		ImGui::BeginChild("##ScriptCodeChild", avail, true,
			ImGuiWindowFlags_AlwaysVerticalScrollbar);

		ImVec2 textSize = ImGui::GetContentRegionAvail();
		if (textSize.x < 1.0f) textSize.x = 1.0f;
		if (textSize.y < 1.0f) textSize.y = 1.0f;

		if (m_EditBuf.size() < 2)
			m_EditBuf.assign(4096, '\0');

		m_EditUserData.buf = &m_EditBuf;
		m_EditUserData.editor = this;

		ImGuiInputTextFlags textFlags =
			ImGuiInputTextFlags_CallbackResize |
			ImGuiInputTextFlags_CallbackAlways |
			ImGuiInputTextFlags_CallbackCompletion;

		ImGuiIO& io = ImGui::GetIO();
		const bool editorActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		if (editorActive && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space, false))
		{
			m_ForceCompletions = true;
			m_SuppressCompletions = false;
			m_ShowCompletions = true;
		}

		// Clear suppress once user types something new.
		if (m_SuppressCompletions && editorActive)
		{
			for (int n = 0; n < io.InputQueueCharacters.Size; n++)
			{
				if (io.InputQueueCharacters[n] != 0)
				{
					m_SuppressCompletions = false;
					break;
				}
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false) ||
				ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			{
				m_SuppressCompletions = false;
			}
		}

		if (m_ShowCompletions && editorActive)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
				ImGui::IsKeyPressed(ImGuiKey_Enter, false))
			{
				// Esc closes list; Enter closes list and still inserts a newline in the editor.
				m_ShowCompletions = false;
				m_ForceCompletions = false;
				m_SuppressCompletions = true;
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false))
			{
				if (m_SelectedCompletion > 0)
					--m_SelectedCompletion;
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false))
			{
				if (m_SelectedCompletion + 1 < (int)m_Filtered.size())
					++m_SelectedCompletion;
			}
			// Tab applies via CallbackCompletion. Enter goes to a new line.
		}

		if (m_RefocusEditor)
		{
			ImGui::SetKeyboardFocusHere();
			m_RefocusEditor = false;
		}

		// Full-height text box so the outer child scrolls gutter + highlight + editor together.
		int lineCount = 1;
		for (const char* p = m_EditBuf.data(); p && *p; ++p)
		{
			if (*p == '\n')
				++lineCount;
		}
		const float lineH = ImGui::GetTextLineHeight();
		const ImVec2 framePad = ImGui::GetStyle().FramePadding;
		float contentH = (float)lineCount * lineH + framePad.y * 2.0f + 4.0f;
		if (contentH < textSize.y)
			contentH = textSize.y;

		int gutterDigits = 1;
		for (int n = lineCount; n >= 10; n /= 10)
			++gutterDigits;
		if (gutterDigits < 2)
			gutterDigits = 2;
		const float digitW = ImGui::CalcTextSize("0").x;
		const float gutterPad = 10.0f;
		const float gutterW = digitW * (float)gutterDigits + gutterPad * 2.0f;
		float editorW = textSize.x - gutterW;
		if (editorW < 40.0f)
			editorW = 40.0f;
		ImVec2 editorSize(editorW, contentH);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 gutterOrigin = ImGui::GetCursorScreenPos();

		// Reserve gutter space in ImGui layout (so InputText cannot cover it).
		ImGui::InvisibleButton("##ScriptGutter", ImVec2(gutterW, contentH));
		ImGui::SameLine(0.0f, 0.0f);

		// Colored tokens under transparent InputText (keeps caret/selection on top).
		{
			ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
			const ImU32 frameBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
			dl->AddRectFilled(cursorScreen,
				ImVec2(cursorScreen.x + editorSize.x, cursorScreen.y + editorSize.y), frameBg);
			DrawLuaSyntaxHighlight(m_EditBuf.data(),
				ImVec2(cursorScreen.x + framePad.x, cursorScreen.y + framePad.y));
		}

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		if (ImGui::InputTextMultiline("##ScriptCode", m_EditBuf.data(), m_EditBuf.size(),
			editorSize, textFlags, ScriptEditCallback, &m_EditUserData))
		{
			m_Dirty = true;
			m_SuppressCompletions = false;
			SyncStringFromEditBuffer();
		}
		const bool editorItemActive = ImGui::IsItemActive();
		const ImVec2 itemMin = ImGui::GetItemRectMin();
		ImGui::PopStyleColor(4);

		// Draw on the child window list (not Foreground) so the gutter never covers Game / other panels.
		{
			const ImU32 gutterBg = IM_COL32(28, 28, 36, 255);
			const ImU32 gutterBorder = IM_COL32(70, 70, 90, 255);
			const ImU32 gutterText = IM_COL32(170, 170, 190, 255);

			const ImVec2 clipMin = ImGui::GetWindowPos();
			const ImVec2 clipMax = ImVec2(clipMin.x + ImGui::GetWindowSize().x, clipMin.y + ImGui::GetWindowSize().y);
			dl->PushClipRect(clipMin, clipMax, true);

			dl->AddRectFilled(gutterOrigin,
				ImVec2(gutterOrigin.x + gutterW, gutterOrigin.y + contentH), gutterBg);
			dl->AddLine(
				ImVec2(gutterOrigin.x + gutterW - 0.5f, gutterOrigin.y),
				ImVec2(gutterOrigin.x + gutterW - 0.5f, gutterOrigin.y + contentH),
				gutterBorder, 1.0f);

			char numBuf[16];
			for (int line = 1; line <= lineCount; ++line)
			{
				std::snprintf(numBuf, sizeof(numBuf), "%d", line);
				const ImVec2 numSize = ImGui::CalcTextSize(numBuf);
				dl->AddText(
					ImVec2(gutterOrigin.x + gutterW - gutterPad - numSize.x,
						gutterOrigin.y + framePad.y + (float)(line - 1) * lineH),
					gutterText,
					numBuf);
			}
			dl->PopClipRect();
		}

		// ImGui draws the caret with ImGuiCol_Text (which is transparent above) — draw our own.
		if (editorItemActive)
		{
			const char* text = m_EditBuf.data();
			int cursor = m_CursorPos;
			int len = 0;
			while (text[len])
				++len;
			if (cursor < 0) cursor = 0;
			if (cursor > len) cursor = len;

			int lineStart = 0;
			int line = 0;
			for (int i = 0; i < cursor; ++i)
			{
				if (text[i] == '\n')
				{
					++line;
					lineStart = i + 1;
				}
			}

			ImFont* font = ImGui::GetFont();
			const float fontSize = ImGui::GetFontSize();
			ImVec2 prefixSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text + lineStart, text + cursor);
			ImVec2 caretPos(
				itemMin.x + framePad.x + prefixSize.x,
				itemMin.y + framePad.y + (float)line * lineH);

			const float blink = (float)ImFmod((float)ImGui::GetTime(), 1.20f);
			if (blink <= 0.80f)
			{
				dl->AddLine(
					ImVec2(caretPos.x, caretPos.y + 0.5f),
					ImVec2(caretPos.x, caretPos.y + lineH - 1.5f),
					IM_COL32(230, 230, 235, 255),
					1.0f);
			}
		}

		// Anchor popup under the code box; draw later after End() so it stays on top.
		{
			ImVec2 amin = ImGui::GetItemRectMin();
			ImVec2 amax = ImGui::GetItemRectMax();
			m_PopupAnchorX = amin.x;
			m_PopupAnchorY = amax.y + 4.0f;
			m_HasPopupAnchor = true;
		}

		// Mouse/Enter accept may have been queued while InputText wasn't applying yet.
		if (m_AcceptCompletion)
			ApplyCompletionToBuffer();

		ImGui::EndChild();

		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
		{
			SaveFile();
		}

		if (m_Dirty)
			SyncStringFromEditBuffer();

		ImGui::End();
		ImGui::PopStyleColor(6);

		// Draw after the editor window so suggestions aren't covered by it.
		DrawCompletionsPopup();
	}
}
