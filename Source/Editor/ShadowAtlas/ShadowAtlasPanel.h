#pragma once

#include "Core/Types.h"
#include "Editor/EditorUI/EditorPanel.h"

class FShadowRenderer;
class FTexture2DArray;

class FShadowAtlasPanel : public IEditorPanel {
public:
	bool Init() override;
	void Tick(float DeltaTime) override;
	void OnRender() override;
	const char* GetPanelName() const override;
	void SetShadowRenderer(FShadowRenderer* ShadowRenderer);
	void Open();

private:
	void DrawAtlasPage(uint32 PageIndex, const FTexture2DArray& Atlas);

private:
	FShadowRenderer* mShadowRenderer{};
	int32 mPageIndex{};
	bool mLinearDepth{};
	bool mFocusRequested{};
};
