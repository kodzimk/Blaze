#pragma once
#include "Blaze/Core/Core.h"
#include "Blaze/UI/WidgetAsset.h"
#include "Blaze/UI/UISystem.h"
#include "bzpch.h"

namespace Blaze
{
	class Texture;

	enum class WidgetDragMode
	{
		None = 0,
		Move,
		ResizeNW, ResizeN, ResizeNE,
		ResizeW, ResizeE,
		ResizeSW, ResizeS, ResizeSE
	};

	class BLAZE_API WidgetEditorWindow
	{
	public:
		WidgetEditorWindow();
		~WidgetEditorWindow();

		void OnUpdate();
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }
		bool IsBlockingMainEditor() const { return m_Open && m_Focused; }

		WidgetNode* GetSelectedNode();
		WidgetAsset& GetAsset() { return m_Asset; }
		const WidgetAsset& GetAsset() const { return m_Asset; }
		void MarkDirty() { m_Dirty = true; }
		bool TryRenameNode(WidgetNode* node, const std::string& newName);

		void DrawInspector();

		static void OpenFile(const std::string& path);
		static bool IsBlockingMainEditorStatic();
		static WidgetEditorWindow* Get() { return s_Instance; }

	private:
		void Open(const std::string& path);
		bool Save();
		void DrawHierarchy();
		void DrawNodeTree(WidgetNode& node, int depth);
		void DrawComponentsPanel();
		void DrawToolbar();
		void DrawPreview();
		void DrawImageTextureField(WidgetNode& node);
		Texture* GetThumbnail(const std::string& path);
		WidgetNode* FindSelected();
		void AddChild(WidgetType type);
		void DeleteSelected();
		void StartRename(WidgetNode& node);
		void CommitRename();
		void CancelRename();
		bool IsNameTaken(const std::string& name, const WidgetNode* except) const;

		struct PreviewHit
		{
			std::string name;
			UIRect rect;
			UIRect parentRect;
		};

		void CollectPreviewHits(const WidgetNode& node, const UIRect& parent, std::vector<PreviewHit>& hits) const;
		WidgetDragMode HitTestHandle(const UIRect& rect, float mx, float my, float handlePad) const;
		void ApplyDrag(WidgetNode& node, const UIRect& parentRect, float logicalX, float logicalY);
		void EnsurePointAnchors(WidgetNode& node, const UIRect& parentRect, const UIRect& rect);
		void ApplyAnchorPreset(WidgetNode& node, const glm::vec2& amin, const glm::vec2& amax, const glm::vec2& pivot);
		bool GetNodePreviewRects(const std::string& name, UIRect& outRect, UIRect& outParent) const;
		void DrawPreviewAnchorTools(WidgetNode& node, const UIRect& parentRect,
			float originX, float originY, float scale, bool canvasHovered, bool& consumedClick);

	private:
		bool m_Open = false;
		bool m_Focused = false;
		bool m_Dirty = false;
		WidgetAsset m_Asset;
		std::string m_SelectedName;
		bool m_PendingDelete = false;

		std::string m_RenamingName;
		char m_RenameBuffer[128] = {};
		bool m_FocusRename = false;

		char m_NameEditBuffer[128] = {};
		std::string m_NameEditTarget;

		std::unordered_map<std::string, Texture*> m_Thumbnails;

		WidgetDragMode m_DragMode = WidgetDragMode::None;
		std::string m_DragNodeName;
		glm::vec2 m_DragStartMouse = glm::vec2(0.0f);
		glm::vec2 m_DragStartPos = glm::vec2(0.0f);
		glm::vec2 m_DragStartSize = glm::vec2(0.0f);
		UIRect m_DragStartRect{};
		UIRect m_DragParentRect{};
		bool m_ResizingDesign = false;
		glm::vec2 m_DesignResizeStart = glm::vec2(0.0f);
		glm::vec2 m_DesignResizeStartSize = glm::vec2(1280.0f, 720.0f);

		float m_PreviewZoom = 1.0f;
		glm::vec2 m_PreviewPan = glm::vec2(0.0f);
		bool m_PanningPreview = false;
		glm::vec2 m_PanStartMouse = glm::vec2(0.0f);
		glm::vec2 m_PanStartOffset = glm::vec2(0.0f);
		int m_AnchorPadDrag = 0; // 0 none, 1 min, 2 max, 3 point

		static WidgetEditorWindow* s_Instance;
	};
}
