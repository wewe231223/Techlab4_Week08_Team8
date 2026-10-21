#include "EnginePCH.h"
#include "ShadowAtlasPanel.h"

#include "Render/ShadowRenderer.h"
#include "Render/Texture2D.h"
#include "Render/Texture2DArray.h"

#include <algorithm>
#include <format>

bool FShadowAtlasPanel::Init() {
	SetOpen(false);
	return true;
}

void FShadowAtlasPanel::Tick(float DeltaTime) {
	if (mShadowRenderer) {
		mShadowRenderer->RequestShadowAtlasPreview(IsOpen() ? mPageIndex : -1, mLinearDepth);
	}
}

void FShadowAtlasPanel::OnRender() {
	ImGui::SetNextWindowSize(ImVec2{ 760.0f, 820.0f }, ImGuiCond_FirstUseEver);
	if (mFocusRequested) {
		ImGui::SetNextWindowCollapsed(false);
		ImGui::SetNextWindowFocus();
		mFocusRequested = false;
	}

	bool Open{ IsOpen() };
	const bool Visible{ ImGui::Begin(GetPanelName(), &Open) };
	SetOpen(Open);
	if (!Open || !Visible) {
		if (mShadowRenderer) {
			mShadowRenderer->RequestShadowAtlasPreview(-1);
		}
		ImGui::End();
		return;
	}

	FTexture2DArray* Atlas{ mShadowRenderer ? mShadowRenderer->GetShadowDepthTexture() : nullptr };
	if (!Atlas || Atlas->GetArraySize() == 0) {
		ImGui::TextDisabled("No shadow atlas is available. Enable shadows on a light in the scene.");
		ImGui::End();
		return;
	}

	mPageIndex = std::clamp(mPageIndex, 0, static_cast<int32>(Atlas->GetArraySize()) - 1);
	int32 DisplayMode{ mLinearDepth ? 1 : 0 };
	const char* DisplayModes[]{ "Raw", "Linear" };
	ImGui::Combo("Depth Display", &DisplayMode, DisplayModes, 2);
	mLinearDepth = DisplayMode == 1;
	ImGui::TextDisabled(mLinearDepth ? "Linear: normalized light-view distance per tile" : "Raw: stored depth (red channel)");
	ImGui::TextDisabled("Last rendered view | Yellow: atlas and shadow map tile boundaries");
	if (ImGui::BeginTabBar("ShadowAtlasPages")) {
		for (uint32 PageIndex{}; PageIndex < Atlas->GetArraySize(); ++PageIndex) {
			const FString Label{ std::format("Page {}", PageIndex) };
			if (ImGui::BeginTabItem(Label.c_str())) {
				mPageIndex = static_cast<int32>(PageIndex);
				mShadowRenderer->RequestShadowAtlasPreview(mPageIndex, mLinearDepth);
				DrawAtlasPage(PageIndex, *Atlas);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}

const char* FShadowAtlasPanel::GetPanelName() const {
	return "Shadow Atlas";
}

void FShadowAtlasPanel::SetShadowRenderer(FShadowRenderer* ShadowRenderer) {
	mShadowRenderer = ShadowRenderer;
}

void FShadowAtlasPanel::Open() {
	SetOpen(true);
	mFocusRequested = true;
}

void FShadowAtlasPanel::DrawAtlasPage(uint32 PageIndex, const FTexture2DArray& Atlas) {
	ImGui::Text("%u x %u | Page %u / %u", Atlas.GetWidth(), Atlas.GetHeight(), PageIndex + 1, Atlas.GetArraySize());
	FTexture2D* Preview{ mShadowRenderer->GetShadowAtlasPreviewTexture(PageIndex) };
	if (!Preview || !Preview->GetSRV()) {
		ImGui::TextDisabled("Preparing atlas preview...");
		return;
	}

	const ImVec2 AvailableSize{ ImGui::GetContentRegionAvail() };
	const float Size{ std::max(1.0f, std::min(AvailableSize.x, AvailableSize.y) - 2.0f) };
	const ImVec2 ImageMin{ ImGui::GetCursorScreenPos() };
	const ImVec2 ImageMax{ ImageMin.x + Size, ImageMin.y + Size };
	ImGui::Image(reinterpret_cast<ImTextureID>(Preview->GetSRV()), ImVec2{ Size, Size });

	ImDrawList* DrawList{ ImGui::GetWindowDrawList() };
	const ImU32 BoundaryColor{ IM_COL32(255, 255, 0, 255) };
	DrawList->PushClipRect(ImageMin, ImageMax, true);
	for (const FShadowMapRenderParameters& Parameters : mShadowRenderer->GetShadowMapRenderParameters()) {
		if (Parameters.mArraySlice != PageIndex) {
			continue;
		}
		const FVector4& UvScaleBias{ Parameters.mUvScaleBias };
		const ImVec2 TileMin{ ImageMin.x + UvScaleBias.Z * Size, ImageMin.y + UvScaleBias.W * Size };
		const ImVec2 TileMax{ TileMin.x + UvScaleBias.X * Size, TileMin.y + UvScaleBias.Y * Size };
		DrawList->AddRect(TileMin, TileMax, BoundaryColor, 0.0f, 1.5f);
	}
	DrawList->AddRect(ImageMin, ImageMax, BoundaryColor, 0.0f, 2.0f);
	DrawList->PopClipRect();
}
