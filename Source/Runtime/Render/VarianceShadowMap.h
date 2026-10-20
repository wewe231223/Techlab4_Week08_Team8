#pragma once

#include "Containers/Array.h"
#include "ShadowMap.h"
#include <d3d11.h>
#include <wrl/client.h>

// Packed per-view SATs and a conservative min/max hierarchy. Built only while
// VSSM is selected; the original depth atlas and PCSS path remain available.
class FVarianceShadowMap {
public:
	bool Build(ID3D11ShaderResourceView* DepthAtlas, TArray<FShadowMapRenderParameters>& Parameters);
	void Bind();
	void ReleaseBuffers();

private:
	struct FStorage {
		Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> UAV;
		uint32 Capacity{};
	};
	bool InitializeShaders();
	static bool EnsureStorage(FStorage& Storage, uint32 Count, uint32 Stride);

	FStorage mRows;
	FStorage mSat;
	FStorage mMinMax;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> mScanRows;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> mScanColumns;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> mBuildMinMax;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> mReduceMinMax;
	Microsoft::WRL::ComPtr<ID3D11Buffer> mConstants;
	bool mReady{};
	uint32 mFailedCount{};
};
