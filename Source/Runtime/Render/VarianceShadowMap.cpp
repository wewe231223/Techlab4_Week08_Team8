#include "EnginePCH.h"
#include "VarianceShadowMap.h"
#include "RenderCommand.h"

#include <d3dcompiler.h>
#include <limits>

namespace {
	struct FBuildConstants {
		uint32 TileX, TileY, Resolution, SatOffset;
		uint32 MinMaxOffset, SourceOffset, SourceResolution, DestinationOffset;
		float NearPlane, FarPlane;
		uint32 Orthographic, Padding;
	};
	static_assert(sizeof(FBuildConstants) == 48);

	uint32 HierarchySize(uint32 Resolution) {
		uint32 Count{};
		for (uint32 Size{ Resolution / 16 }; Size; Size /= 2) Count += Size * Size;
		return Count;
	}
}

bool FVarianceShadowMap::InitializeShaders() {
	if (mConstants && mScanRows && mScanColumns && mBuildMinMax && mReduceMinMax) return true;
	ID3D11Device* Device{ RenderCommand::GetDevice() };
	auto Compile = [Device](const char* Entry, Microsoft::WRL::ComPtr<ID3D11ComputeShader>& Shader) {
		Microsoft::WRL::ComPtr<ID3DBlob> Code, Errors;
		const HRESULT Result{ D3DCompileFromFile(L"Resources/Shader/VssmPreprocess.hlsli", nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE, Entry, "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
			0, Code.GetAddressOf(), Errors.GetAddressOf()) };
		if (FAILED(Result)) {
			HTR_LOG(Error, "[VSSM] {} compile failed: {}", Entry, Errors ? static_cast<const char*>(Errors->GetBufferPointer()) : "file unavailable");
			return false;
		}
		return SUCCEEDED(Device->CreateComputeShader(Code->GetBufferPointer(), Code->GetBufferSize(), nullptr, Shader.ReleaseAndGetAddressOf()));
	};
	if (!Compile("ScanRows", mScanRows) || !Compile("ScanColumns", mScanColumns) ||
		!Compile("BuildMinMax", mBuildMinMax) || !Compile("ReduceMinMax", mReduceMinMax)) return false;
	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = sizeof(FBuildConstants);
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, mConstants.ReleaseAndGetAddressOf()));
}

bool FVarianceShadowMap::EnsureStorage(FStorage& Storage, uint32 Count, uint32 Stride) {
	if (Storage.Capacity >= Count && Storage.Buffer && Storage.SRV && Storage.UAV) return true;
	if (!Count || Count > std::numeric_limits<uint32>::max() / Stride) return false;
	FStorage Replacement;
	D3D11_BUFFER_DESC Desc{};
	Desc.ByteWidth = Count * Stride;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = Stride;
	ID3D11Device* Device{ RenderCommand::GetDevice() };
	if (FAILED(Device->CreateBuffer(&Desc, nullptr, Replacement.Buffer.GetAddressOf()))) return false;
	D3D11_SHADER_RESOURCE_VIEW_DESC SRV{};
	SRV.Format = DXGI_FORMAT_UNKNOWN;
	SRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRV.Buffer.NumElements = Count;
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAV{};
	UAV.Format = DXGI_FORMAT_UNKNOWN;
	UAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAV.Buffer.NumElements = Count;
	if (FAILED(Device->CreateShaderResourceView(Replacement.Buffer.Get(), &SRV, Replacement.SRV.GetAddressOf())) ||
		FAILED(Device->CreateUnorderedAccessView(Replacement.Buffer.Get(), &UAV, Replacement.UAV.GetAddressOf()))) return false;
	Replacement.Capacity = Count;
	Storage = std::move(Replacement);
	return true;
}

void FVarianceShadowMap::ReleaseBuffers() {
	mReady = false;
	mFailedCount = 0;
	mRows = {};
	mSat = {};
	mMinMax = {};
}

bool FVarianceShadowMap::Build(ID3D11ShaderResourceView* DepthAtlas, TArray<FShadowMapRenderParameters>& Parameters) {
	mReady = false;
	ID3D11DeviceContext* Context{ RenderCommand::GetContext() };
	ID3D11ShaderResourceView* NullSRVs[2]{};
	ID3D11UnorderedAccessView* NullUAVs[2]{};
	Context->PSSetShaderResources(16, 2, NullSRVs);
	uint32 SatCount{}, MinMaxCount{};
	for (FShadowMapRenderParameters& P : Parameters) {
		P.mVssmResolution = 0;
		const uint32 Resolution{ static_cast<uint32>(std::lround(P.mUvScaleBias.X / P.mTexelSize.X)) };
		SatCount += Resolution * Resolution;
		MinMaxCount += HierarchySize(Resolution);
	}
	if (SatCount != 0 && SatCount == mFailedCount) return false;
	if (!DepthAtlas || !SatCount || !InitializeShaders() || !EnsureStorage(mRows, SatCount, 16) ||
		!EnsureStorage(mSat, SatCount, 16) || !EnsureStorage(mMinMax, MinMaxCount, 8)) {
		mFailedCount = SatCount;
		mRows = {};
		mSat = {};
		mMinMax = {};
		HTR_LOG(Error, "[VSSM] statistics unavailable; falling back to PCSS");
		return false;
	}
	Context->CSSetConstantBuffers(0, 1, mConstants.GetAddressOf());
	uint32 SatOffset{}, MinMaxOffset{};
	for (FShadowMapRenderParameters& P : Parameters) {
		const uint32 Resolution{ static_cast<uint32>(std::lround(P.mUvScaleBias.X / P.mTexelSize.X)) };
		FBuildConstants Constants{
			static_cast<uint32>(std::lround(P.mUvScaleBias.Z / P.mTexelSize.X)),
			static_cast<uint32>(std::lround(P.mUvScaleBias.W / P.mTexelSize.Y)), Resolution, SatOffset,
			MinMaxOffset, 0, 0, 0, P.NearFar.X, P.NearFar.Y, P.mIsOrthographic, 0 };
		Context->UpdateSubresource(mConstants.Get(), 0, nullptr, &Constants, 0, 0);
		Context->CSSetShaderResources(0, 1, &DepthAtlas);
		Context->CSSetUnorderedAccessViews(0, 1, mRows.UAV.GetAddressOf(), nullptr);
		Context->CSSetShader(mScanRows.Get(), nullptr, 0);
		Context->Dispatch(Resolution, 1, 1);
		Context->CSSetUnorderedAccessViews(0, 1, NullUAVs, nullptr);
		Context->CSSetShaderResources(1, 1, mRows.SRV.GetAddressOf());
		Context->CSSetUnorderedAccessViews(0, 1, mSat.UAV.GetAddressOf(), nullptr);
		Context->CSSetShader(mScanColumns.Get(), nullptr, 0);
		Context->Dispatch(Resolution, 1, 1);
		Context->CSSetUnorderedAccessViews(0, 1, NullUAVs, nullptr);
		Context->CSSetShaderResources(1, 1, NullSRVs);

		Context->CSSetUnorderedAccessViews(1, 1, mMinMax.UAV.GetAddressOf(), nullptr);
		Context->CSSetShader(mBuildMinMax.Get(), nullptr, 0);
		Context->Dispatch((Resolution / 16 + 7) / 8, (Resolution / 16 + 7) / 8, 1);
		Context->CSSetUnorderedAccessViews(1, 1, NullUAVs, nullptr);
		Constants.SourceResolution = Resolution / 16;
		while (Constants.SourceResolution > 1) {
			Constants.DestinationOffset = Constants.SourceOffset + Constants.SourceResolution * Constants.SourceResolution;
			Context->UpdateSubresource(mConstants.Get(), 0, nullptr, &Constants, 0, 0);
			Context->CSSetUnorderedAccessViews(1, 1, mMinMax.UAV.GetAddressOf(), nullptr);
			Context->CSSetShader(mReduceMinMax.Get(), nullptr, 0);
			const uint32 Size{ Constants.SourceResolution / 2 };
			Context->Dispatch((Size + 7) / 8, (Size + 7) / 8, 1);
			Context->CSSetUnorderedAccessViews(1, 1, NullUAVs, nullptr);
			Constants.SourceOffset = Constants.DestinationOffset;
			Constants.SourceResolution = Size;
		}
		P.mVssmSatOffset = SatOffset;
		P.mVssmMinMaxOffset = MinMaxOffset;
		P.mVssmResolution = Resolution;
		SatOffset += Resolution * Resolution;
		MinMaxOffset += HierarchySize(Resolution);
	}
	Context->CSSetShaderResources(0, 2, NullSRVs);
	Context->CSSetShader(nullptr, nullptr, 0);
	ID3D11Buffer* NullCB{};
	Context->CSSetConstantBuffers(0, 1, &NullCB);
	mReady = true;
	mFailedCount = 0;
	return true;
}

void FVarianceShadowMap::Bind() {
	ID3D11ShaderResourceView* Views[]{ mReady ? mSat.SRV.Get() : nullptr, mReady ? mMinMax.SRV.Get() : nullptr };
	RenderCommand::GetContext()->PSSetShaderResources(16, 2, Views);
}
