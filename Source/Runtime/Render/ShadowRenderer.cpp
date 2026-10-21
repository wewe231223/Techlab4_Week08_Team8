#include "EnginePCH.h"
#include "ShadowRenderer.h"
#include "Core/StatOverlay.h"

#include "Engine/Scene.h"
#include "Engine/RectLightSceneProxy.h"
#include "Engine/SpotLightSceneProxy.h"
#include "Mesh.h"
#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "Shader.h"
#include "Texture2D.h"
#include "Texture2DArray.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace {
	struct FShadowPreviewConstants {
		FVector2 mUvMin{};
		FVector2 mUvMax{};
		FVector2 mNearFar{};
		uint32 mSlice{};
		uint32 mIsOrthographic{};
		uint32 mLinearDepth{};
		uint32 mPadding[3]{};
	};
	static_assert(sizeof(FShadowPreviewConstants) == 48);
}

FShadowRenderer::FShadowRenderer() {
}

FShadowRenderer::~FShadowRenderer() {
}

bool FShadowRenderer::Render(FScene& Scene, const FShadowCameraContext& CameraContext, int32 AtlasResolution) {
	mCurrentCameraContext = CameraContext;
	GatherShadowRequests(Scene);
	return RenderShadowAtlas(Scene, mShadowRequests, AtlasResolution);
}

void FShadowRenderer::Reset() {
	mDepthTexture.reset();
	mShadowRequests.Reset();
	mShadowMapAllocations.Reset();
	mShadowMapRenderParameters.Reset();
	mShadowMapReady = false;
	mPreviewReady = false;
	mAtlasPreviewReady = false;
}

void FShadowRenderer::SetShadowFilterMode(EShadowFilterMode Mode) {
	mShadowFilterMode = NormalizeShadowFilterMode(static_cast<int32>(Mode));
}

EShadowFilterMode FShadowRenderer::GetShadowFilterMode() const {
	return mShadowFilterMode;
}

uint32 FShadowRenderer::GetShadowMapCount() const {
	return GetShadowDepthTexture() ? mShadowMapRenderParameters.Num() : 0;
}

bool FShadowRenderer::Init() {
	if (mComparisonSampler) {
		return true;
	}
	D3D11_SAMPLER_DESC Desc{};
	Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	Desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	Desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	Desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	Desc.MaxAnisotropy = 1;
	Desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	std::fill(std::begin(Desc.BorderColor), std::end(Desc.BorderColor), 1.0f);
	return SUCCEEDED(RenderCommand::GetDevice()->CreateSamplerState(&Desc, mComparisonSampler.GetAddressOf()));
}

bool FShadowRenderer::SetShadowRasterizerState(EShadowDepthBiasPreset DepthBiasPreset, EShadowSlopeScaledDepthBiasPreset SlopeScaledDepthBiasPreset, bool CullBackFaces) {
	const EShadowDepthBiasPreset DepthPreset{ NormalizeShadowDepthBiasPreset(static_cast<int32>(DepthBiasPreset)) };
	const EShadowSlopeScaledDepthBiasPreset SlopePreset{ NormalizeShadowSlopeScaledDepthBiasPreset(static_cast<int32>(SlopeScaledDepthBiasPreset)) };
	const FShadowRasterizerStateKey Key{ DepthPreset, SlopePreset, CullBackFaces };
	FShadowRasterizerState& State{ mShadowRasterizerStates[Key] };
	if (!State) {
		D3D11_RASTERIZER_DESC Desc{};
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.CullMode = CullBackFaces ? D3D11_CULL_BACK : D3D11_CULL_NONE;
		Desc.DepthClipEnable = TRUE;
		Desc.DepthBias = GetShadowDepthBiasPreset(DepthPreset).mDepthBias;
		Desc.SlopeScaledDepthBias = GetShadowSlopeScaledDepthBiasPreset(SlopePreset).mSlopeScaledDepthBias;
		if (FAILED(RenderCommand::GetDevice()->CreateRasterizerState(&Desc, State.GetAddressOf()))) {
			return false;
		}
	}
	RenderCommand::GetContext()->RSSetState(State.Get());
	return true;
}

bool FShadowRenderer::IsValidShadowView(const FShadowView& View) {
	if (!std::isfinite(View.mNearPlane) || View.mNearPlane < 0.0f || !std::isfinite(View.mWorldPosition.X) || !std::isfinite(View.mWorldPosition.Y) || !std::isfinite(View.mWorldPosition.Z)) {
		return false;
	}
	for (const FMatrix* Matrix : { &View.mView, &View.mProjection, &View.mViewProjection }) {
		for (uint32 Row{}; Row < 4; ++Row) {
			for (uint32 Column{}; Column < 4; ++Column) {
				if (!std::isfinite(Matrix->M[Row][Column])) {
					return false;
				}
			}
		}
	}
	for (uint32 PlaneIndex{}; PlaneIndex < 6; ++PlaneIndex) {
		const uint32 Column{ PlaneIndex / 2 };
		const float Scale{ PlaneIndex % 2 == 0 ? 1.0f : -1.0f };
		FVector Normal{ View.mViewProjection.M[0][Column] * Scale, View.mViewProjection.M[1][Column] * Scale, View.mViewProjection.M[2][Column] * Scale };
		if (PlaneIndex != 4) {
			Normal += FVector{ View.mViewProjection.M[0][3], View.mViewProjection.M[1][3], View.mViewProjection.M[2][3] };
		}
		const float LengthSquared{ Normal.SizeSquared() };
		if (!std::isfinite(LengthSquared) || LengthSquared <= 1.0e-12f) {
			return false;
		}
	}
	return true;
}

bool FShadowRenderer::IsValidShadowRequest(const FShadowMapRequest& Request) {
	if (!Request.mLightComponent || !Request.mSettings.mCastShadows || Request.mViewCount == 0 || Request.mViewCount > PointShadowFaceCount) {
		return false;
	}
	for (uint32 FaceIndex{}; FaceIndex < Request.mViewCount; ++FaceIndex) {
		if (!IsValidShadowView(Request.mViews[FaceIndex])) {
			return false;
		}
	}
	return true;
}

TArray<FShadowRenderer::FShadowAtlasAllocation> FShadowRenderer::BuildShadowAtlasAllocations(FShadowAtlas& Atlas, const TArray<FShadowMapRequest>& Requests, uint32 AtlasResolution) {
	Atlas.Initialize(AtlasResolution, static_cast<uint32>(mMaxAtlasPageCount));
	TArray<FShadowAtlas::FRequest> AllocationRequests{};
	AllocationRequests.SetNum(Requests.Num());
	for (uint32 Index{}; Index < static_cast<uint32>(Requests.Num()); ++Index) {
		const FShadowMapRequest& Request{ Requests[Index] };
		if (IsValidShadowRequest(Request) && (!Atlas.IsCacheEnabled() || (Request.mLightProxy && Request.mLightProxy->GetLightComponent() == Request.mLightComponent))) {
			// 요청 해상도를 2의 거듭제곱으로 맞춘다.
			AllocationRequests[Index] = FShadowAtlas::FRequest{ Request.mLightComponent, static_cast<uint32>(NormalizeShadowMapResolution(Request.mSettings.mResolution)), Request.mViewCount };
		}
	}
	return Atlas.UpdateAllocations(AllocationRequests);
}

int32 FShadowRenderer::NormalizeAtlasResolution(int32 Resolution) {
	const uint32 Size{ static_cast<uint32>(std::clamp(Resolution, MinShadowMapResolution, mMaxAtlasResolution)) };
	return static_cast<int32>(std::bit_ceil(Size));
}

bool FShadowRenderer::InitializeShadowPass() {
	if (!mShadowShader) {
		FShaderProgram* Shader{ FRenderResourceManager::GetShaderProgram("Resources/Shader/ShadowDepth.hlsl") };
		if (!Shader || !Shader->VertexShader || !Shader->VertexShader->IsValid() || !Shader->VertexShader->GetLayout() || Shader->PixelShader) {
			return false;
		}
		mShadowShader = Shader;
	}
	if (!mShadowViewCB || !mShadowViewCB->GetBuffer()) {
		mShadowViewCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	}
	if (!mShadowObjectCB || !mShadowObjectCB->GetBuffer()) {
		mShadowObjectCB = RenderCommand::CreateConstantBuffer(sizeof(FMatrix));
	}
	return mShadowViewCB && mShadowViewCB->GetBuffer() && mShadowObjectCB && mShadowObjectCB->GetBuffer();
}

bool FShadowRenderer::RenderShadowAtlas(FScene& Scene, const TArray<FShadowMapRequest>& Requests, int32 AtlasResolution) {
	mDepthTexture.reset();
	mShadowMapReady = false;
	mPreviewReady = false;
	mAtlasPreviewReady = false;
	mShadowMapAllocations.Reset();
	mShadowMapRenderParameters.Reset();

	ConfigureShadowCache(Scene, Requests);
	FShadowAtlas& Atlas{ Scene.GetShadowAtlas() };
	const uint32 Size{ static_cast<uint32>(NormalizeAtlasResolution(AtlasResolution)) };
	const TArray<FShadowAtlasAllocation> Allocations{ BuildShadowAtlasAllocations(Atlas, Requests, Size) };
	uint32 ParameterCount{};
	for (const FShadowAtlasAllocation& Allocation : Allocations) {
		ParameterCount += Allocation.mViewCount;
	}

	if (ParameterCount == 0 || !InitializeShadowPass() || !Atlas.UpdateTextureResource()) {
		return false;
	}
	mDepthTexture = Atlas.GetDepthTexture();
	if (!mShadowMapBuffer || mShadowMapBuffer->GetMaxCount() < ParameterCount) {
		mShadowMapBuffer = RenderCommand::CreateDynamicStructuredBuffer(sizeof(FShadowMapRenderParameters), ParameterCount);
	}
	if (!mShadowMapBuffer || !mShadowMapBuffer->GetBuffer() || !mShadowMapBuffer->GetSRV()) {
		return false;
	}
	mShadowMapRenderParameters.Reserve(ParameterCount);

	if (mShadowCacheEnabled) {
		if (!InitializeShadowClear()) {
			return false;
		}
	}

	// 이번 호출의 모든 맵에 공유하여 Primitive의 중복 변경 검사를 막는다.
	const uint64 UpdateSerial{ mShadowCacheEnabled ? Scene.BeginShadowUpdate() : 0 };
	const TSharedPtr<FTexture2DArray> DepthTexture{ mDepthTexture.lock() };
	RenderCommand::UnbindShaderResource(mTextureSlot, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel | EShaderBindFlagBits::Compute);
	FRenderingInfo PassInfo{};
	PassInfo.DepthStencil.Texture = DepthTexture.get();
	PassInfo.DepthStencil.LoadOp = mShadowCacheEnabled ? ERenderTargetLoadOp::Load : ERenderTargetLoadOp::Clear;
	PassInfo.ViewportSetting.Width = DepthTexture->GetWidth();
	PassInfo.ViewportSetting.Height = DepthTexture->GetHeight();

	for (uint32 PageIndex{}; PageIndex < Atlas.GetPageCount(); ++PageIndex) {
		PassInfo.DepthStencil.ArraySlice = PageIndex;
		bool PassStarted{};
		if (!mShadowCacheEnabled) {
			BeginShadowPass(PassInfo);
			PassStarted = true;
		}

		for (uint32 Index{}; Index < static_cast<uint32>(Requests.Num()); ++Index) {
			const FShadowAtlasAllocation& Allocation{ Allocations[Index] };
			if (Allocation.mViewCount == 0 || Allocation.mPageIndex != PageIndex) {
				continue;
			}

			if (mShadowCacheEnabled) {
				UpdateLightShadowMaps(Scene, Requests[Index], Allocation, Atlas, PassInfo, PassStarted, UpdateSerial);
			} else {
				RenderLightShadowMaps(Scene, Requests[Index], Allocation);
			}
		}

		if (PassStarted) {
			RenderCommand::EndRenderPass(PassInfo);
		}
	}

	if (mShadowMapReady) {
		RenderCommand::UpdateBufferData(mShadowMapBuffer.get(), mShadowMapRenderParameters.GetData(), mShadowMapRenderParameters.Num() * sizeof(FShadowMapRenderParameters));
		RenderShadowPreview();
		RenderShadowAtlasPreview();
	}
	return mShadowMapReady;
}

void FShadowRenderer::BindShadowMaps() {
	if (FTexture2DArray* Texture{ GetShadowDepthTexture() }) {
		RenderCommand::BindShaderResource(mTextureSlot, Texture, EShaderBindFlagBits::Pixel);
	} else {
		RenderCommand::UnbindShaderResource(mTextureSlot, EShaderBindFlagBits::Pixel);
	}
	RenderCommand::BindShaderResource(mBufferSlot, GetShadowDepthTexture() ? mShadowMapBuffer.get() : nullptr, EShaderBindFlagBits::Pixel);
	ID3D11SamplerState* Sampler{ mComparisonSampler.Get() };
	RenderCommand::GetContext()->PSSetSamplers(mSamplerSlot, 1, &Sampler);
}

bool FShadowRenderer::DrawShadowCasters(const FRenderQueue& Queue, const FShadowView& View, const FShadowAtlasTile& Tile) {
	RenderCommand::SetViewport(Tile.mX, Tile.mY, Tile.mResolution, Tile.mResolution);
	RenderCommand::UpdateBufferData(mShadowViewCB.get(), &View.mViewProjection, sizeof(FMatrix));
	UStaticMesh* BoundMesh{};
	uint32 BoundLOD{};
	bool IsComplete{ true };
	for (uint32 Index{}; Index < static_cast<uint32>(Queue.Num()); ++Index) {
		const FRenderPacket& Packet{ Queue[Index] };
		if (!Packet.Proxy || !Packet.Proxy->GetComponent() || !Packet.Mesh) {
			IsComplete = false;
			continue;
		}

		FVertexBuffer* VertexBuffer{ Packet.Mesh->GetVertexBuffer(Packet.LODIndex) };
		FIndexBuffer* IndexBuffer{ Packet.Mesh->GetIndexBuffer(Packet.LODIndex) };
		if (!VertexBuffer || !VertexBuffer->GetBuffer() || !IndexBuffer || !IndexBuffer->GetBuffer() || Packet.StartIndex > IndexBuffer->GetIndexCount() || Packet.IndexCount > IndexBuffer->GetIndexCount() - Packet.StartIndex) {
			IsComplete = false;
			continue;
		}
		if (BoundMesh != Packet.Mesh || BoundLOD != Packet.LODIndex) {
			RenderCommand::BindMesh(Packet.Mesh, Packet.LODIndex);
			BoundMesh = Packet.Mesh;
			BoundLOD = Packet.LODIndex;
		}
		const FMatrix& LocalToWorld{ Packet.Model ? *Packet.Model : Packet.Proxy->GetLocalToWorld() };
		RenderCommand::UpdateBufferData(mShadowObjectCB.get(), &LocalToWorld, sizeof(FMatrix));
		RenderCommand::DrawIndexed(Packet.IndexCount, Packet.StartIndex);
	}
	return IsComplete;
}

FTexture2DArray* FShadowRenderer::GetShadowDepthTexture() const {
	return mShadowMapReady ? mDepthTexture.lock().get() : nullptr;
}

const TArray<FShadowMapRenderParameters>& FShadowRenderer::GetShadowMapRenderParameters() const {
	return mShadowMapRenderParameters;
}

const FShadowMapRenderParameters* FShadowRenderer::GetShadowMapRenderParameters(const ULightComponent* LightComponent, uint32 FaceIndex) const {
	const FShadowMapAllocation* Allocation{ mShadowMapAllocations.Find(LightComponent) };
	if (!GetShadowDepthTexture() || !Allocation || FaceIndex >= Allocation->mViewCount || Allocation->mFirstIndex < 0) {
		return nullptr;
	}
	const uint32 Index{ static_cast<uint32>(Allocation->mFirstIndex) + FaceIndex };
	return mShadowMapRenderParameters.IsValidIndex(static_cast<int32>(Index)) ? &mShadowMapRenderParameters[Index] : nullptr;
}

int32 FShadowRenderer::GetShadowMapIndex(const ULightComponent* LightComponent) const {
	const FShadowMapAllocation* Allocation{ mShadowMapAllocations.Find(LightComponent) };
	return GetShadowDepthTexture() && Allocation ? Allocation->mFirstIndex : InvalidShadowMapIndex;
}

uint32 FShadowRenderer::GetShadowMapViewCount(const ULightComponent* LightComponent) const {
	const FShadowMapAllocation* Allocation{ mShadowMapAllocations.Find(LightComponent) };
	return GetShadowDepthTexture() && Allocation ? Allocation->mViewCount : 0;
}

void FShadowRenderer::RequestShadowPreview(const ULightComponent* LightComponent, uint32 FaceIndex, bool LinearDepth) {
	mPreviewLight = LightComponent;
	mPreviewFace = FaceIndex;
	mPreviewLinearDepth = LinearDepth;
}

FTexture2D* FShadowRenderer::GetShadowPreviewTexture() const {
	return mPreviewReady ? mPreviewTexture.get() : nullptr;
}

void FShadowRenderer::RequestShadowAtlasPreview(int32 PageIndex, bool LinearDepth) {
	mAtlasPreviewPage = PageIndex;
	mAtlasPreviewLinearDepth = LinearDepth;
	if (PageIndex < 0) {
		mAtlasPreviewReady = false;
	}
}

FTexture2D* FShadowRenderer::GetShadowAtlasPreviewTexture(uint32 PageIndex) const {
	return mAtlasPreviewReady && mRenderedAtlasPreviewPage == PageIndex ? mAtlasPreviewTexture.get() : nullptr;
}

bool FShadowRenderer::InitializePreviewPass(TUniquePtr<FTexture2D>& Texture, uint32 Resolution) {
	if (!mPreviewShader) {
		mPreviewShader = FRenderResourceManager::GetShaderProgram("Resources/Shader/ShadowPreview.hlsl");
		if (!mPreviewShader || !mPreviewShader->PixelShader) {
			mPreviewShader = nullptr;
			return false;
		}
	}
	if (!mPreviewCB || !mPreviewCB->GetBuffer()) {
		mPreviewCB = RenderCommand::CreateConstantBuffer(sizeof(FShadowPreviewConstants));
	}
	if (!Texture || !Texture->GetRTV() || !Texture->GetSRV() || Texture->GetWidth() != Resolution || Texture->GetHeight() != Resolution) {
		D3D11_TEXTURE2D_DESC Desc{};
		Desc.Width = Resolution;
		Desc.Height = Resolution;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		Desc.SampleDesc.Count = 1;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		Texture = RenderCommand::CreateTexture2D(Desc);
	}
	return mPreviewCB && mPreviewCB->GetBuffer() && Texture && Texture->GetRTV() && Texture->GetSRV();
}

bool FShadowRenderer::RenderPreviewTexture(FTexture2D& Texture, const FVector2& UvMin, const FVector2& UvMax, uint32 PageIndex, bool LinearDepth, const FShadowMapRenderParameters* Parameters) {
	const TSharedPtr<FTexture2DArray> DepthTexture{ mDepthTexture.lock() };
	if (!DepthTexture || PageIndex >= DepthTexture->GetArraySize()) {
		return false;
	}

	FShadowPreviewConstants Constants{};
	Constants.mUvMin = UvMin;
	Constants.mUvMax = UvMax;
	Constants.mSlice = PageIndex;
	Constants.mLinearDepth = LinearDepth ? 1u : 0u;
	if (Parameters) {
		Constants.mNearFar = Parameters->mNearFar;
		Constants.mIsOrthographic = Parameters->mIsOrthographic;
	}

	FRenderingInfo PassInfo{};
	FRenderingDesc& ColorTarget{ PassInfo.ColorRenderTargets.AddDefaulted_GetRef() };
	ColorTarget.Texture = &Texture;
	ColorTarget.ClearValue.ColorClearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
	PassInfo.ViewportSetting.Width = Texture.GetWidth();
	PassInfo.ViewportSetting.Height = Texture.GetHeight();
	RenderCommand::BeginRenderPass(PassInfo);
	RenderCommand::BindShaderProgram(mPreviewShader);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RenderCommand::SetRasterizerState(ERasterizerState::SolidBack);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Disabled);
	RenderCommand::BindConstantBuffer(0, mPreviewCB.get(), EShaderBindFlagBits::Pixel);
	RenderCommand::BindShaderResource(0, DepthTexture.get(), EShaderBindFlagBits::Pixel);
	if (!LinearDepth || Parameters) {
		RenderCommand::UpdateBufferData(mPreviewCB.get(), &Constants);
		RenderCommand::Draw(3);
	} else {
		for (const FShadowMapRenderParameters& TileParameters : mShadowMapRenderParameters) {
			if (TileParameters.mArraySlice != PageIndex) {
				continue;
			}
			const FVector4& Tile{ TileParameters.mUvScaleBias };
			const FVector2& TexelSize{ TileParameters.mTexelSize };
			Constants.mUvMin = FVector2{ Tile.Z + TexelSize.X * 0.5f, Tile.W + TexelSize.Y * 0.5f };
			Constants.mUvMax = FVector2{ Tile.Z + Tile.X - TexelSize.X * 0.5f, Tile.W + Tile.Y - TexelSize.Y * 0.5f };
			Constants.mNearFar = TileParameters.mNearFar;
			Constants.mIsOrthographic = TileParameters.mIsOrthographic;
			RenderCommand::SetViewport(static_cast<uint32>(Tile.Z * Texture.GetWidth()), static_cast<uint32>(Tile.W * Texture.GetHeight()), static_cast<uint32>(Tile.X * Texture.GetWidth()), static_cast<uint32>(Tile.Y * Texture.GetHeight()));
			RenderCommand::UpdateBufferData(mPreviewCB.get(), &Constants);
			RenderCommand::Draw(3);
		}
	}
	// 다음 프레임에 아틀라스를 DSV로 쓸 때 SRV로 남아 있지 않도록 바로 해제한다.
	RenderCommand::UnbindShaderResource(0, EShaderBindFlagBits::Pixel);
	RenderCommand::EndRenderPass(PassInfo);
	return true;
}

void FShadowRenderer::RenderShadowPreview() {
	const FShadowMapRenderParameters* Parameters{ mPreviewLight ? GetShadowMapRenderParameters(mPreviewLight, mPreviewFace) : nullptr };
	if (!Parameters || !InitializePreviewPass(mPreviewTexture, mPreviewResolution)) {
		return;
	}

	const FVector2 UvMin{ Parameters->mUvScaleBias.Z + Parameters->mTexelSize.X * 0.5f, Parameters->mUvScaleBias.W + Parameters->mTexelSize.Y * 0.5f };
	const FVector2 UvMax{ Parameters->mUvScaleBias.Z + Parameters->mUvScaleBias.X - Parameters->mTexelSize.X * 0.5f, Parameters->mUvScaleBias.W + Parameters->mUvScaleBias.Y - Parameters->mTexelSize.Y * 0.5f };
	mPreviewReady = RenderPreviewTexture(*mPreviewTexture, UvMin, UvMax, Parameters->mArraySlice, mPreviewLinearDepth, Parameters);
}

void FShadowRenderer::RenderShadowAtlasPreview() {
	FTexture2DArray* DepthTexture{ GetShadowDepthTexture() };
	if (mAtlasPreviewPage < 0 || !DepthTexture || DepthTexture->GetArraySize() == 0 || !InitializePreviewPass(mAtlasPreviewTexture, mAtlasPreviewResolution)) {
		return;
	}

	mRenderedAtlasPreviewPage = std::min(static_cast<uint32>(mAtlasPreviewPage), DepthTexture->GetArraySize() - 1);
	mAtlasPreviewReady = RenderPreviewTexture(*mAtlasPreviewTexture, FVector2{}, FVector2{ 1.0f, 1.0f }, mRenderedAtlasPreviewPage, mAtlasPreviewLinearDepth);
}


void FShadowRenderer::GatherShadowRequests(const FScene& Scene) {
	mShadowRequests.Reset();
	mShadowRequests.Reserve(Scene.DirectionalLightProxies.Num() + Scene.LocalLightProxies.Num());
	FStatOverlay::UpdateCSMStats(FCSMDebugSnapshot{});
	FBox CasterBounds{};
	if (!Scene.DirectionalLightProxies.IsEmpty() && GatherShadowCasterBounds(Scene, CasterBounds)) {
		for (FDirectionalLightSceneProxy* Proxy : Scene.DirectionalLightProxies) {
			if (!Proxy || !Proxy->IsVisible() || !Proxy->GetLightComponent()->CastsShadows()) {
				continue;
			}

			FShadowMapRequest Request{};
			if (Proxy->IsCSMEnabled()) {
				if (!Proxy->BuildShadowViews(CasterBounds, Request.mViews, mCurrentCameraContext)) {
					continue;
				}
				Request.mViewCount = Proxy->GetCascadeCount();
			} else if (!Proxy->BuildShadowView(CasterBounds, Request.mViews[0])) {
				continue;
			}

			Request.mSettings = Proxy->GetShadowSettings();
			Request.mLightComponent = Proxy->GetLightComponent();
			Request.mLightProxy = Proxy;
			mShadowRequests.Add(Request);
		}
	}

	for (FLocalLightSceneProxy* Proxy : Scene.LocalLightProxies) {
		if (!Proxy || !Proxy->IsVisible() || !Proxy->GetLightComponent()->CastsShadows()) {
			continue;
		}
		FShadowMapRequest Request{};
		const ELightType Type{ Proxy->GetLightComponent()->GetLightType() };
		if (Type == ELightType::Spot) {
			if (!static_cast<const FSpotLightSceneProxy*>(Proxy)->BuildShadowView(Request.mViews[0])) {
				continue;
			}
		} else if (Type == ELightType::Point) {
			if (!static_cast<const FPointLightSceneProxy*>(Proxy)->BuildShadowViews(Request.mViews)) {
				continue;
			}
			Request.mViewCount = PointShadowFaceCount;
		} else if (Type == ELightType::Rect) {
			if (!static_cast<const FRectLightSceneProxy*>(Proxy)->BuildShadowView(Request.mViews[0])) {
				continue;
			}
		} else {
			continue;
		}
		Request.mSettings = Proxy->GetShadowSettings();
		Request.mLightComponent = Proxy->GetLightComponent();
		Request.mLightProxy = Proxy;
		mShadowRequests.Add(Request);
	}
}

bool FShadowRenderer::GatherShadowCasterBounds(const FScene& Scene, FBox& OutBounds) {
	bool HasBounds{};
	Scene.BVH.Query([](const FBox&) {
		return true;
	}, [&OutBounds, &HasBounds](const FPrimitiveSceneProxy* Proxy) {
		if (!FLightSceneProxy::IsShadowCaster(Proxy)) {
			return;
		}
		const FAABB& Bounds{ Proxy->GetBounds() };
		const FBox CasterBounds{ Bounds.Center - Bounds.Extent, Bounds.Center + Bounds.Extent };
		if (HasBounds) {
			OutBounds.Expand(CasterBounds);
		} else {
			OutBounds = CasterBounds;
			HasBounds = true;
		}
	});
	return HasBounds;
}


FShadowMapRenderParameters FShadowRenderer::BuildShadowMapRenderParameters(const FShadowView& View, const FLightShadowSettings& Settings, const FShadowAtlasTile& Tile) const {
	const TSharedPtr<FTexture2DArray> DepthTexture{ mDepthTexture.lock() };
	FShadowMapRenderParameters Parameters{};
	if (!DepthTexture || !DepthTexture->GetSRV() || !DepthTexture->GetDSV() || Tile.mResolution == 0 || Tile.mResolution > DepthTexture->GetWidth() || Tile.mResolution > DepthTexture->GetHeight() || Tile.mX > DepthTexture->GetWidth() - Tile.mResolution || Tile.mY > DepthTexture->GetHeight() - Tile.mResolution) {
		return Parameters;
	}

	Parameters.mLightViewProjection = View.mViewProjection;
	Parameters.mTexelSize = FVector2{ 1.0f / static_cast<float>(DepthTexture->GetWidth()), 1.0f / static_cast<float>(DepthTexture->GetHeight()) };
	Parameters.mUvScaleBias = FVector4{ static_cast<float>(Tile.mResolution) * Parameters.mTexelSize.X, static_cast<float>(Tile.mResolution) * Parameters.mTexelSize.Y, static_cast<float>(Tile.mX) * Parameters.mTexelSize.X, static_cast<float>(Tile.mY) * Parameters.mTexelSize.Y };
	Parameters.mNormalBias = std::isfinite(Settings.mNormalBias) ? std::max(Settings.mNormalBias, 0.0f) : FLightShadowSettings{}.mNormalBias;
	Parameters.mNearFar = FVector2{ View.mNearPlane, View.mFarPlane };
	Parameters.mMinCasterDepth = NormalizeShadowMinCasterDepth(Settings.mMinCasterDepth);
	Parameters.mSourceRadius = FVector2{ NormalizeShadowSourceRadius(Settings.mSourceRadius.X), NormalizeShadowSourceRadius(Settings.mSourceRadius.Y) };
	Parameters.mSourceAngleTanHalf = std::tan(FMath::DegreesToRadians(NormalizeShadowSourceAngle(Settings.mSourceAngle)) * 0.5f);
	Parameters.mIsOrthographic = View.mIsOrthographic ? 1u : 0u;
	return Parameters;
}

void FShadowRenderer::SetShadowCacheEnabled(bool Enabled) {
	mShadowCacheEnabled = Enabled;
}

bool FShadowRenderer::IsShadowCacheEnabled() const {
	return mShadowCacheEnabled;
}

void FShadowRenderer::SetMinShadowCasterTexelRadius(float TexelRadius) {
	mMinShadowCasterTexelRadius = std::isfinite(TexelRadius) ? std::max(TexelRadius, 0.0f) : 0.0f;
}

float FShadowRenderer::GetMinShadowCasterTexelRadius() const {
	return mMinShadowCasterTexelRadius;
}

void FShadowRenderer::ConfigureShadowCache(FScene& Scene, const TArray<FShadowMapRequest>& Requests) {
	FShadowAtlas& Atlas{ Scene.GetShadowAtlas() };
	if (Atlas.IsCacheEnabled() == mShadowCacheEnabled) {
		return;
	}

	Atlas.SetCacheEnabled(mShadowCacheEnabled);
	if (mShadowCacheEnabled) {
		return;
	}

	Scene.ResetShadowCasterTracking();
	for (const FShadowMapRequest& Request : Requests) {
		if (Request.mLightProxy) {
			Request.mLightProxy->ResetShadowMaps();
		}
	}
}

void FShadowRenderer::BeginShadowPass(const FRenderingInfo& PassInfo) {
	RenderCommand::BeginRenderPass(PassInfo);
	RenderCommand::BindShaderProgram(mShadowShader);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::BindConstantBuffer(0, mShadowViewCB.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(2, mShadowObjectCB.get(), EShaderBindFlagBits::Vertex);
}

void FShadowRenderer::RenderLightShadowMaps(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlasAllocation& Allocation) {
	if (!SetShadowRasterizerState(Request.mSettings.mDepthBiasPreset, Request.mSettings.mSlopeScaledDepthBiasPreset, Request.mViews[0].mCullBackFaces)) {
		return;
	}

	for (uint32 FaceIndex{}; FaceIndex < Allocation.mViewCount; ++FaceIndex) {
		const FShadowAtlasTile& Tile{ Allocation.mTiles[FaceIndex] };
		FLightSceneProxy::GatherShadowCasters(Scene, mShadowRenderQueue, Request.mViews[FaceIndex], static_cast<int32>(Tile.mResolution), mMinShadowCasterTexelRadius);
		DrawShadowCasters(mShadowRenderQueue, Request.mViews[FaceIndex], Tile);
	}
	AddShadowMapParameters(Request, Allocation, Request.mViews);
}

void FShadowRenderer::UpdateLightShadowMaps(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlasAllocation& Allocation, FShadowAtlas& Atlas, const FRenderingInfo& PassInfo, bool& PassStarted, uint64 UpdateSerial) {
	// 재사용한 면에는 실제 깊이 맵을 만들었던 캐시의 View를 전달한다.
	FShadowView Views[PointShadowFaceCount]{};
	// 모든 면의 재사용 또는 렌더링이 완료되어야 광원의 샘플링 정보를 등록한다.
	bool IsComplete{ true };
	for (uint32 FaceIndex{}; FaceIndex < Allocation.mViewCount; ++FaceIndex) {
		const FShadowCacheEntry* Entry{ UpdateShadowFace(Scene, Request, Allocation, FaceIndex, Atlas, PassInfo, PassStarted, UpdateSerial) };
		if (Entry) {
			Views[FaceIndex] = Entry->mView;
		} else {
			IsComplete = false;
		}
	}

	if (IsComplete) {
		AddShadowMapParameters(Request, Allocation, Views);
	}
}

const FShadowCacheEntry* FShadowRenderer::UpdateShadowFace(FScene& Scene, const FShadowMapRequest& Request, const FShadowAtlasAllocation& Allocation, uint32 FaceIndex, FShadowAtlas& Atlas, const FRenderingInfo& PassInfo, bool& PassStarted, uint64 UpdateSerial) {
	if (!Request.mLightProxy->EvaluateShadowMap(Scene, Request, Allocation, FaceIndex, Atlas, UpdateSerial, mMinShadowCasterTexelRadius)) {
		Atlas.InvalidateCacheEntry(Request.mLightComponent, FaceIndex);
		const FLightSceneProxy::FShadowMapState* Map{ Request.mLightProxy->GetShadowMap(FaceIndex) };
		if (!Map || !Request.mLightProxy->BuildShadowRenderQueue(mShadowRenderQueue, FaceIndex) || !SetShadowRasterizerState(Map->mInput.mDepthBiasPreset, Map->mInput.mSlopeScaledDepthBiasPreset, Map->mInput.mView.mCullBackFaces)) {
			return nullptr;
		}

		if (!PassStarted) {
			BeginShadowPass(PassInfo);
			PassStarted = true;
		}

		const FShadowAtlasTile& Tile{ Allocation.mTiles[FaceIndex] };
		ClearShadowTile(Tile);
		RenderCommand::BindShaderProgram(mShadowShader);
		RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
		SetShadowRasterizerState(Map->mInput.mDepthBiasPreset, Map->mInput.mSlopeScaledDepthBiasPreset, Map->mInput.mView.mCullBackFaces);
		if (!DrawShadowCasters(mShadowRenderQueue, Map->mInput.mView, Tile)) {
			return nullptr;
		}

		Request.mLightProxy->CommitShadowCasters(FaceIndex);
		Atlas.StoreCacheEntry(Request.mLightComponent, FaceIndex, Map->mInput);
	}

	const FShadowCacheEntry* Entry{ Atlas.GetCacheEntry(Request.mLightComponent, FaceIndex) };
	return Entry && Entry->mHasCachedContent ? Entry : nullptr;
}

void FShadowRenderer::AddShadowMapParameters(const FShadowMapRequest& Request, const FShadowAtlasAllocation& Allocation, const FShadowView* Views) {
	const int32 FirstIndex{ static_cast<int32>(mShadowMapRenderParameters.Num()) };
	for (uint32 FaceIndex{}; FaceIndex < Allocation.mViewCount; ++FaceIndex) {
		FShadowMapRenderParameters Parameters{ BuildShadowMapRenderParameters(Views[FaceIndex], Request.mSettings, Allocation.mTiles[FaceIndex]) };
		Parameters.mTextureIndex = 0;
		Parameters.mArraySlice = Allocation.mPageIndex;
		mShadowMapRenderParameters.Add(Parameters);
	}

	mShadowMapAllocations.Add(Request.mLightComponent, FShadowMapAllocation{ FirstIndex, Allocation.mViewCount });
	mShadowMapReady = true;
}

bool FShadowRenderer::InitializeShadowClear() {
	if (!mShadowClearShader) {
		FShaderProgram* Shader{ FRenderResourceManager::GetShaderProgram("Resources/Shader/ShadowClear.hlsl") };
		if (!Shader || !Shader->VertexShader || !Shader->VertexShader->IsValid() || Shader->VertexShader->GetLayout() || Shader->PixelShader) {
			return false;
		}

		mShadowClearShader = Shader;
	}

	if (!mShadowClearDepthState) {
		D3D11_DEPTH_STENCIL_DESC Desc{};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		Desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		Desc.StencilEnable = TRUE;
		Desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		Desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		Desc.BackFace = Desc.FrontFace;

		if (FAILED(RenderCommand::GetDevice()->CreateDepthStencilState(&Desc, mShadowClearDepthState.GetAddressOf()))) {
			return false;
		}
	}

	if (!mShadowClearRasterizerState) {
		D3D11_RASTERIZER_DESC Desc{};
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.CullMode = D3D11_CULL_NONE;
		Desc.DepthClipEnable = TRUE;

		if (FAILED(RenderCommand::GetDevice()->CreateRasterizerState(&Desc, mShadowClearRasterizerState.GetAddressOf()))) {
			return false;
		}
	}

	return true;
}

void FShadowRenderer::ClearShadowTile(const FShadowAtlasTile& Tile) {
	RenderCommand::SetViewport(Tile.mX, Tile.mY, Tile.mResolution, Tile.mResolution);
	RenderCommand::BindShaderProgram(mShadowClearShader);
	RenderCommand::GetContext()->RSSetState(mShadowClearRasterizerState.Get());
	RenderCommand::GetContext()->OMSetDepthStencilState(mShadowClearDepthState.Get(), 0);
	RenderCommand::Draw(3);
}
