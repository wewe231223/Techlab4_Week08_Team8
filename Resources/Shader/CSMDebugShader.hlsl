#pragma pack_matrix(row_major)

Texture2D<float> SceneDepth : register(t0);

cbuffer FCSMDebugConstants : register(b0) {
	float4x4 mInverseViewProjection;
	float4x4 mViewProjection;
	float4 mCascadeSplits;
	float mCascadeTransitionFraction;
	uint mCascadeCount;
	float2 mPadding;
};

struct PSInput {
	float4 mPosition : SV_POSITION;
	float2 mUV : TEXCOORD0;
};

PSInput mainVS(uint VertexID : SV_VertexID) {
	PSInput Output = { (PSInput) 0 };
	const float2 UV = { float2((VertexID << 1) & 2, VertexID & 2) };
	Output.mPosition = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	Output.mUV = UV;
	return Output;
}

float4 mainPS(PSInput Input) : SV_Target {
	const float Depth = { SceneDepth.Load(int3(Input.mPosition.xy, 0)) };
	if (Depth >= 1.0f || mCascadeCount <= 1) {
		discard;
	}

	const float2 NdcPosition = { Input.mUV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f) };
	float4 WorldPosition = { mul(float4(NdcPosition, Depth, 1.0f), mInverseViewProjection) };
	WorldPosition /= WorldPosition.w;
	const float ViewDepth = { mul(float4(WorldPosition.xyz, 1.0f), mViewProjection).w };
	static const float3 CascadeColors[4] = {
		float3(1.0f, 0.25f, 0.25f),
		float3(0.25f, 1.0f, 0.25f),
		float3(0.25f, 0.25f, 1.0f),
		float3(1.0f, 1.0f, 0.25f)
	};

	int CascadeIndex = { -1 };
	float NearSplit = { 0.0f };
	float FarSplit = { 0.0f };
	for (uint Index = { 0 }; Index < min(mCascadeCount, 4u); ++Index) {
		if (ViewDepth < mCascadeSplits[Index]) {
			CascadeIndex = (int) Index;
			FarSplit = mCascadeSplits[Index];
			break;
		}
		NearSplit = mCascadeSplits[Index];
	}

	if (CascadeIndex < 0) {
		discard;
	}

	float3 Color = { CascadeColors[CascadeIndex] };
	if (CascadeIndex < (int) min(mCascadeCount, 4u) - 1 && mCascadeTransitionFraction > 0.0f) {
		const float CascadeRange = { FarSplit - NearSplit };
		const float DistanceToSplit = { FarSplit - ViewDepth };
		const float BlendRange = { CascadeRange * mCascadeTransitionFraction };
		if (DistanceToSplit < BlendRange && BlendRange > 0.0f) {
			const float BlendRatio = { saturate(1.0f - DistanceToSplit / BlendRange) };
			Color = lerp(Color, CascadeColors[CascadeIndex + 1], BlendRatio);
		}
	}

	return float4(Color, 1.0f);
}
