#include "EnginePCH.h"
#include "ShadowRenderer.h"

#include "Asset/LOD/StaticMeshLODSelector.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Scene.h"
#include "Engine/RectLightSceneProxy.h"
#include "Engine/SpotLightSceneProxy.h"
#include "Mesh.h"
#include "RenderCommand.h"
#include "RenderResourceManager.h"
#include "Shader.h"
#include "Texture2D.h"

#include <algorithm>
#include <bit>
#include <cmath>

FShadowRenderer::FShadowRenderer() {
}

FShadowRenderer::~FShadowRenderer() {
}

bool FShadowRenderer::Render(FScene& Scene, int32 AtlasResolution) {
	GatherShadowRequests(Scene);
	return RenderShadowAtlas(Scene, mShadowRequests, AtlasResolution);
}

void FShadowRenderer::Reset() {
	mShadowRequests.Reset();
	mShadowMapAllocations.Reset();
	mShadowMapRenderParameters.Reset();
	mShadowMapReady = false;
}

void FShadowRenderer::SetShadowFilterMode(EShadowFilterMode Mode) {
	mShadowFilterMode = NormalizeShadowFilterMode(static_cast<int32>(Mode));
}

EShadowFilterMode FShadowRenderer::GetShadowFilterMode() const {
	return mShadowFilterMode;
}

uint32 FShadowRenderer::GetShadowMapCount() const {
	return mShadowMapReady ? mShadowMapRenderParameters.Num() : 0;
}

bool FShadowRenderer::InitializeComparisonSampler() {
	if (mComparisonSampler) {
		return true;
	}
	D3D11_SAMPLER_DESC Desc{};
	Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
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
	if (!Request.mSettings.mCastShadows || (Request.mViewCount != 1 && Request.mViewCount != PointShadowFaceCount)) {
		return false;
	}
	for (uint32 FaceIndex{}; FaceIndex < Request.mViewCount; ++FaceIndex) {
		if (!IsValidShadowView(Request.mViews[FaceIndex])) {
			return false;
		}
	}
	return true;
}

TArray<FShadowRenderer::FShadowAtlasAllocation> FShadowRenderer::BuildShadowAtlasAllocations(const TArray<FShadowMapRequest>& Requests, uint32 AtlasResolution) {
	TArray<FShadowAtlasAllocation> Allocations{};
	Allocations.SetNum(Requests.Num());
	TArray<uint32> SortedIndices{};
	for (uint32 Index{}; Index < static_cast<uint32>(Requests.Num()); ++Index) {
		if (IsValidShadowRequest(Requests[Index])) {
			SortedIndices.Add(Index);
		}
	}
	std::sort(SortedIndices.begin(), SortedIndices.end(), [&Requests](uint32 Left, uint32 Right) {
		const int32 LeftResolution{ NormalizeShadowMapResolution(Requests[Left].mSettings.mResolution) };
		const int32 RightResolution{ NormalizeShadowMapResolution(Requests[Right].mSettings.mResolution) };
		return LeftResolution != RightResolution ? LeftResolution > RightResolution : Left < Right;
	});

	constexpr int32 LevelCount{ static_cast<int32>(std::bit_width(static_cast<uint32>(mMaxAtlasResolution / MinShadowMapResolution))) };
	TArray<FShadowAtlasTile> FreeTilesByLevel[LevelCount]{};
	FreeTilesByLevel[0].Add(FShadowAtlasTile{ 0, 0, AtlasResolution });
	for (uint32 Index : SortedIndices) {
		const uint32 Resolution{ static_cast<uint32>(NormalizeShadowMapResolution(Requests[Index].mSettings.mResolution)) };
		if (Resolution > AtlasResolution) {
			continue;
		}
		const int32 TargetLevel{ static_cast<int32>(std::countr_zero(AtlasResolution / Resolution)) };
		uint32 AvailableTileCount{};
		for (int32 Level{}; Level <= TargetLevel; ++Level) {
			AvailableTileCount += FreeTilesByLevel[Level].Num() * (1u << (2 * (TargetLevel - Level)));
		}
		if (AvailableTileCount < Requests[Index].mViewCount) {
			continue;
		}

		FShadowAtlasAllocation& Allocation{ Allocations[Index] };
		Allocation.mViewCount = Requests[Index].mViewCount;
		for (uint32 FaceIndex{}; FaceIndex < Allocation.mViewCount; ++FaceIndex) {
			int32 AvailableLevel{ TargetLevel };
			while (FreeTilesByLevel[AvailableLevel].IsEmpty()) {
				--AvailableLevel;
			}
			FShadowAtlasTile Tile{ FreeTilesByLevel[AvailableLevel].Last() };
			FreeTilesByLevel[AvailableLevel].RemoveLast();
			while (AvailableLevel < TargetLevel) {
				Tile.mResolution /= 2;
				++AvailableLevel;
				FreeTilesByLevel[AvailableLevel].Add(FShadowAtlasTile{ Tile.mX + Tile.mResolution, Tile.mY, Tile.mResolution });
				FreeTilesByLevel[AvailableLevel].Add(FShadowAtlasTile{ Tile.mX, Tile.mY + Tile.mResolution, Tile.mResolution });
				FreeTilesByLevel[AvailableLevel].Add(FShadowAtlasTile{ Tile.mX + Tile.mResolution, Tile.mY + Tile.mResolution, Tile.mResolution });
			}
			Allocation.mTiles[FaceIndex] = Tile;
		}
	}
	return Allocations;
}


int32 FShadowRenderer::NormalizeAtlasResolution(int32 Resolution) {
	const uint32 Size{ static_cast<uint32>(std::clamp(Resolution, MinShadowMapResolution, mMaxAtlasResolution)) };
	return static_cast<int32>(std::bit_ceil(Size));
}

bool FShadowRenderer::InitializeAtlasResource(int32 Resolution) {
	const uint32 Size{ static_cast<uint32>(NormalizeAtlasResolution(Resolution)) };
	if (mDepthTexture && mDepthTexture->GetRawPtr() && mDepthTexture->GetSRV() && mDepthTexture->GetDSV() && mDepthTexture->GetWidth() == Size && mDepthTexture->GetHeight() == Size) {
		return true;
	}

	D3D11_TEXTURE2D_DESC Desc{};
	Desc.Width = Size;
	Desc.Height = Size;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	TUniquePtr<FTexture2D> DepthTexture{ RenderCommand::CreateTexture2D(Desc) };
	if (!DepthTexture || !DepthTexture->GetRawPtr() || !DepthTexture->GetDSV() || !DepthTexture->GetSRV()) {
		return false;
	}

	mDepthTexture = std::move(DepthTexture);
	return true;
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
	return mShadowViewCB && mShadowViewCB->GetBuffer() && mShadowObjectCB && mShadowObjectCB->GetBuffer() && InitializeComparisonSampler();
}

bool FShadowRenderer::RenderShadowAtlas(FScene& Scene, const TArray<FShadowMapRequest>& Requests, int32 AtlasResolution) {
	mShadowMapReady = false;
	mShadowMapAllocations.Reset();
	mShadowMapRenderParameters.Reset();
	const uint32 Size{ static_cast<uint32>(NormalizeAtlasResolution(AtlasResolution)) };
	const TArray<FShadowAtlasAllocation> Allocations{ BuildShadowAtlasAllocations(Requests, Size) };
	uint32 ParameterCount{};
	for (const FShadowAtlasAllocation& Allocation : Allocations) {
		ParameterCount += Allocation.mViewCount;
	}
	if (ParameterCount == 0 || !InitializeShadowPass() || !InitializeAtlasResource(static_cast<int32>(Size))) {
		return false;
	}
	if (!mShadowMapBuffer || mShadowMapBuffer->GetMaxCount() < ParameterCount) {
		mShadowMapBuffer = RenderCommand::CreateDynamicStructuredBuffer(sizeof(FShadowMapRenderParameters), ParameterCount);
	}
	if (!mShadowMapBuffer || !mShadowMapBuffer->GetBuffer() || !mShadowMapBuffer->GetSRV()) {
		return false;
	}
	mShadowMapRenderParameters.Reserve(ParameterCount);

	FTexture2D* DepthTexture{ mDepthTexture.get() };
	RenderCommand::UnbindShaderResource(mTextureSlot, EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel | EShaderBindFlagBits::Compute);
	FRenderingInfo PassInfo{};
	PassInfo.DepthStencil.Texture = DepthTexture;
	PassInfo.ViewportSetting.Width = DepthTexture->GetWidth();
	PassInfo.ViewportSetting.Height = DepthTexture->GetHeight();
	RenderCommand::BeginRenderPass(PassInfo);
	RenderCommand::BindShaderProgram(mShadowShader);
	RenderCommand::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	RenderCommand::SetBlendState(EBlendState::Opaque);
	RenderCommand::SetDepthStencilState(EDepthStencilState::Default);
	RenderCommand::BindConstantBuffer(0, mShadowViewCB.get(), EShaderBindFlagBits::Vertex);
	RenderCommand::BindConstantBuffer(2, mShadowObjectCB.get(), EShaderBindFlagBits::Vertex);
	for (uint32 Index{}; Index < static_cast<uint32>(Requests.Num()); ++Index) {
		const FShadowAtlasAllocation& Allocation{ Allocations[Index] };
		const FShadowMapRequest& Request{ Requests[Index] };
		if (Allocation.mViewCount == 0 || !SetShadowRasterizerState(Request.mSettings.mDepthBiasPreset, Request.mSettings.mSlopeScaledDepthBiasPreset, Request.mViews[0].mCullBackFaces)) {
			continue;
		}
		const int32 FirstIndex{ static_cast<int32>(mShadowMapRenderParameters.Num()) };
		for (uint32 FaceIndex{}; FaceIndex < Allocation.mViewCount; ++FaceIndex) {
			const FShadowAtlasTile& Tile{ Allocation.mTiles[FaceIndex] };
			DrawShadowCasters(Scene, Request.mViews[FaceIndex], Tile);
			FShadowMapRenderParameters Parameters{ BuildShadowMapRenderParameters(Request.mViews[FaceIndex], Request.mSettings, Tile) };
			Parameters.mTextureIndex = 0;
			mShadowMapRenderParameters.Add(Parameters);
		}
		if (Request.mLightComponent) {
			mShadowMapAllocations.Add(Request.mLightComponent, FShadowMapAllocation{ FirstIndex, Allocation.mViewCount });
		}
		mShadowMapReady = true;
	}
	RenderCommand::EndRenderPass(PassInfo);
	if (mShadowMapReady) {
		RenderCommand::UpdateBufferData(mShadowMapBuffer.get(), mShadowMapRenderParameters.GetData(), mShadowMapRenderParameters.Num() * sizeof(FShadowMapRenderParameters));
	}
	return mShadowMapReady;
}

void FShadowRenderer::BindShadowMaps() {
	if (FTexture2D* Texture{ GetShadowDepthTexture() }) {
		RenderCommand::BindShaderResource(mTextureSlot, Texture, EShaderBindFlagBits::Pixel);
	} else {
		RenderCommand::UnbindShaderResource(mTextureSlot, EShaderBindFlagBits::Pixel);
	}
	RenderCommand::BindShaderResource(mBufferSlot, mShadowMapReady ? mShadowMapBuffer.get() : nullptr, EShaderBindFlagBits::Pixel);
	ID3D11SamplerState* Sampler{ mComparisonSampler.Get() };
	RenderCommand::GetContext()->PSSetSamplers(mSamplerSlot, 1, &Sampler);
}

void FShadowRenderer::DrawShadowCasters(FScene& Scene, const FShadowView& View, const FShadowAtlasTile& Tile) {
	GatherShadowCasters(Scene, mShadowRenderQueue, View, static_cast<int32>(Tile.mResolution));
	RenderCommand::SetViewport(Tile.mX, Tile.mY, Tile.mResolution, Tile.mResolution);
	RenderCommand::UpdateBufferData(mShadowViewCB.get(), &View.mViewProjection, sizeof(FMatrix));
	UStaticMesh* BoundMesh{};
	uint32 BoundLOD{};
	for (const FRenderPacket& Packet : mShadowRenderQueue) {
		FVertexBuffer* VertexBuffer{ Packet.Mesh->GetVertexBuffer(Packet.LODIndex) };
		FIndexBuffer* IndexBuffer{ Packet.Mesh->GetIndexBuffer(Packet.LODIndex) };
		if (!VertexBuffer || !VertexBuffer->GetBuffer() || !IndexBuffer || !IndexBuffer->GetBuffer() || Packet.StartIndex > IndexBuffer->GetIndexCount() || Packet.IndexCount > IndexBuffer->GetIndexCount() - Packet.StartIndex) {
			continue;
		}
		if (BoundMesh != Packet.Mesh || BoundLOD != Packet.LODIndex) {
			RenderCommand::BindMesh(Packet.Mesh, Packet.LODIndex);
			BoundMesh = Packet.Mesh;
			BoundLOD = Packet.LODIndex;
		}
		RenderCommand::UpdateBufferData(mShadowObjectCB.get(), &Packet.Proxy->GetLocalToWorld(), sizeof(FMatrix));
		RenderCommand::DrawIndexed(Packet.IndexCount, Packet.StartIndex);
	}
}

FTexture2D* FShadowRenderer::GetShadowDepthTexture() const {
	return mShadowMapReady ? mDepthTexture.get() : nullptr;
}

const TArray<FShadowMapRenderParameters>& FShadowRenderer::GetShadowMapRenderParameters() const {
	return mShadowMapRenderParameters;
}

const FShadowMapRenderParameters* FShadowRenderer::GetShadowMapRenderParameters(const ULightComponent* LightComponent, uint32 FaceIndex) const {
	const FShadowMapAllocation* Allocation{ mShadowMapAllocations.Find(LightComponent) };
	if (!mShadowMapReady || !Allocation || FaceIndex >= Allocation->mViewCount || Allocation->mFirstIndex < 0) {
		return nullptr;
	}
	const uint32 Index{ static_cast<uint32>(Allocation->mFirstIndex) + FaceIndex };
	return mShadowMapRenderParameters.IsValidIndex(static_cast<int32>(Index)) ? &mShadowMapRenderParameters[Index] : nullptr;
}

int32 FShadowRenderer::GetShadowMapIndex(const ULightComponent* LightComponent) const {
	const FShadowMapAllocation* Allocation{ mShadowMapAllocations.Find(LightComponent) };
	return mShadowMapReady && Allocation ? Allocation->mFirstIndex : InvalidShadowMapIndex;
}

uint32 FShadowRenderer::GetShadowMapViewCount(const ULightComponent* LightComponent) const {
	const FShadowMapAllocation* Allocation{ mShadowMapAllocations.Find(LightComponent) };
	return mShadowMapReady && Allocation ? Allocation->mViewCount : 0;
}

bool FShadowRenderer::IsShadowCaster(const FPrimitiveSceneProxy* Proxy) {
	if (!Proxy || !Proxy->IsVisible() || !Proxy->CastsShadows() || !Proxy->GetMesh() || !Proxy->GetRenderState()) {
		return false;
	}
	const UStaticMeshComponent* Component{ Cast<UStaticMeshComponent>(Proxy->GetComponent()) };
	if (Component && Component->IsEditorIcon()) {
		return false;
	}
	for (const FCachedMeshSection& Section : Proxy->GetRenderState()->Sections) {
		if (Section.Material && Section.Material->BlendState == EBlendState::Opaque && Section.IndexCount != 0) {
			return true;
		}
	}
	return false;
}

void FShadowRenderer::GatherShadowRequests(const FScene& Scene) {
	mShadowRequests.Reset();
	mShadowRequests.Reserve(Scene.DirectionalLightProxies.Num() + Scene.LocalLightProxies.Num());
	FBox CasterBounds{};
	if (!Scene.DirectionalLightProxies.IsEmpty() && GatherShadowCasterBounds(Scene, CasterBounds)) {
		for (const FDirectionalLightSceneProxy* Proxy : Scene.DirectionalLightProxies) {
			FShadowMapRequest Request{};
			if (Proxy && Proxy->BuildShadowView(CasterBounds, Request.mViews[0])) {
				Request.mSettings = Proxy->GetShadowSettings();
				Request.mLightComponent = Proxy->GetLightComponent();
				mShadowRequests.Add(Request);
			}
		}
	}

	for (const FLocalLightSceneProxy* Proxy : Scene.LocalLightProxies) {
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
		mShadowRequests.Add(Request);
	}
}

bool FShadowRenderer::GatherShadowCasterBounds(const FScene& Scene, FBox& OutBounds) {
	bool HasBounds{};
	Scene.BVH.Query([](const FBox&) {
		return true;
	}, [&OutBounds, &HasBounds](const FPrimitiveSceneProxy* Proxy) {
		if (!IsShadowCaster(Proxy)) {
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

void FShadowRenderer::GatherShadowCasters(FScene& Scene, FRenderQueue& OutQueue, const FShadowView& View, int32 Resolution) {
	OutQueue.Reset();
	const FFrustumPlanes Frustum{ ExtractFrustumPlanes(View.mViewProjection) };
	const float ProjectionScale{ std::max(std::abs(View.mProjection.M[1][0]), std::abs(View.mProjection.M[2][1])) };
	const uint32 Size{ static_cast<uint32>(std::clamp(Resolution, MinShadowMapResolution, MaxShadowMapResolution)) };
	FLODViewContext LODView{};
	LODView.Width = Size;
	LODView.Height = Size;
	LODView.CameraPosition = View.mWorldPosition;
	LODView.CameraForward = FVector{ View.mView.M[0][0], View.mView.M[1][0], View.mView.M[2][0] };
	LODView.ProjectionScaleSquared = ProjectionScale * ProjectionScale;
	LODView.NearZ = View.mNearPlane;
	LODView.bOrthographic = View.mIsOrthographic;
	LODView.ViewProjection = View.mViewProjection;
	LODView.Prepare();

	Scene.BVH.QueryCull(FrustumAllPlanesMask, [&Frustum](const FBox& Bounds, uint32& Mask) {
		return static_cast<EBVHCullResult>(ClassifyBoxInFrustum(Bounds, Frustum, Mask));
	}, [&OutQueue, &LODView, &View](FPrimitiveSceneProxy* Proxy) {
		if (!IsShadowCaster(Proxy)) {
			return;
		}

		FLODViewContext CasterLODView{ LODView };
		if (View.mIsOmnidirectional) {
			CasterLODView.CameraForward = (Proxy->GetLODSphere().Center - View.mWorldPosition).GetSafeNormal();
			CasterLODView.Prepare();
		}
		const uint32 LODIndex{ SelectLOD(*Proxy, CasterLODView) };
		const FCachedMeshLOD& LOD{ Proxy->GetLOD(LODIndex) };
		for (uint32 SectionIndex{}; SectionIndex < LOD.NumSections; ++SectionIndex) {
			const FCachedMeshSection& Section{ Proxy->GetSection(LOD.FirstSection + SectionIndex) };
			if (!Section.Material || Section.Material->BlendState != EBlendState::Opaque || Section.IndexCount == 0) {
				continue;
			}

			FRenderPacket& Packet{ OutQueue.AddDefaulted_GetRef() };
			Packet.Proxy = Proxy;
			Packet.Mesh = Proxy->GetMesh();
			Packet.Material = Section.Material;
			Packet.StartIndex = Section.StartIndex;
			Packet.IndexCount = Section.IndexCount;
			Packet.LODIndex = static_cast<uint8>(LODIndex);
		}
	});
}

FShadowMapRenderParameters FShadowRenderer::BuildShadowMapRenderParameters(const FShadowView& View, const FLightShadowSettings& Settings, const FShadowAtlasTile& Tile) const {
	const FTexture2D* DepthTexture{ mDepthTexture.get() };
	FShadowMapRenderParameters Parameters{};
	if (!DepthTexture || !DepthTexture->GetSRV() || !DepthTexture->GetDSV() || Tile.mResolution == 0 || Tile.mResolution > DepthTexture->GetWidth() || Tile.mResolution > DepthTexture->GetHeight() || Tile.mX > DepthTexture->GetWidth() - Tile.mResolution || Tile.mY > DepthTexture->GetHeight() - Tile.mResolution) {
		return Parameters;
	}

	Parameters.mLightViewProjection = View.mViewProjection;
	Parameters.mTexelSize = FVector2{ 1.0f / static_cast<float>(DepthTexture->GetWidth()), 1.0f / static_cast<float>(DepthTexture->GetHeight()) };
	Parameters.mUvScaleBias = FVector4{ static_cast<float>(Tile.mResolution) * Parameters.mTexelSize.X, static_cast<float>(Tile.mResolution) * Parameters.mTexelSize.Y, static_cast<float>(Tile.mX) * Parameters.mTexelSize.X, static_cast<float>(Tile.mY) * Parameters.mTexelSize.Y };
	Parameters.mNormalBias = std::isfinite(Settings.mNormalBias) ? std::max(Settings.mNormalBias, 0.0f) : FLightShadowSettings{}.mNormalBias;
	Parameters.NearFar.X = View.mNearPlane;
	Parameters.NearFar.Y = View.mFarPlane;
	Parameters.mMinCasterDepth = NormalizeShadowMinCasterDepth(Settings.mMinCasterDepth);
	Parameters.mSourceRadius = FVector2{ NormalizeShadowSourceRadius(Settings.mSourceRadius.X), NormalizeShadowSourceRadius(Settings.mSourceRadius.Y) };
	Parameters.mSourceAngleTanHalf = std::tan(FMath::DegreesToRadians(NormalizeShadowSourceAngle(Settings.mSourceAngle)) * 0.5f);
	Parameters.mIsOrthographic = View.mIsOrthographic ? 1u : 0u;
	return Parameters;
}
