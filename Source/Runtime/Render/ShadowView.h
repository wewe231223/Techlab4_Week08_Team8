#pragma once

#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "ShadowMap.h"

class ULightComponent;
class FLightSceneProxy;
struct FBox;
struct FTransform;

// 그림자 맵 한 면을 렌더링할 가상 카메라 정보. 그림자 생성 대상 선별, LOD 선택, 깊이 렌더링에 사용한다.
struct FShadowView {
	// 월드 좌표를 그림자 카메라의 뷰 좌표로 변환한다. 렌더러는 이 행렬에서 LOD 계산용 전방 방향도 얻는다.
	FMatrix mView{ FMatrix::Identity };
	// 뷰 좌표를 클립 좌표로 변환하는 직교 또는 원근 투영 행렬. 투영 배율은 LOD의 화면상 크기 계산에도 사용한다.
	FMatrix mProjection{ FMatrix::Identity };
	// mView * mProjection. 프러스텀 컬링, 그림자 깊이 렌더링, 조명 셰이더의 그림자 좌표 계산에 사용한다.
	FMatrix mViewProjection{ FMatrix::Identity };
	// 그림자 카메라의 월드 위치. LOD 거리와 방향 계산의 기준이며, 방향성 및 사각형 조명에서는 실제 광원 위치와 다를 수 있다.
	FVector mWorldPosition{};
	// 그림자 카메라에서 근평면까지의 전방 거리. 투영 행렬의 깊이 범위와 일치해야 하며, LOD 계산과 뷰 유효성 검사에 사용한다.
	float mNearPlane{};
	float mFarPlane{};
	// LOD 계산에 직교 투영 방식을 사용할지 여부. 방향성 조명은 true, 나머지는 false이며, 이 값 자체가 투영 행렬을 변경하지는 않는다.
	bool mIsOrthographic{ true };
	// 포인트 라이트의 전방향 그림자를 구성하는 뷰인지 여부. LOD 계산 시 각 면의 전방 대신 광원에서 물체로 향하는 방향을 사용한다.
	bool mIsOmnidirectional{ false };
	// 깊이 렌더링에서 뒷면을 제거할지 여부. false이면 양면을 그리며, 사각형 조명은 false를 사용한다. 요청의 첫 뷰 값이 모든 면에 적용된다.
	bool mCullBackFaces{ true };
};

// 조명 하나의 그림자 생성 요청. 렌더러가 뷰마다 아틀라스 타일을 할당하고, 렌더링 결과를 해당 조명과 연결한다.
struct FShadowMapRequest {
	// 면별 그림자 카메라. 일반 조명은 [0]만 사용하고, 포인트 라이트는 +X, -X, +Y, -Y, +Z, -Z 순서로 6개를 사용한다.
	FShadowView mViews[PointShadowFaceCount]{};
	// mViews에서 실제로 사용할 뷰 수이자 필요한 아틀라스 타일 수. 현재 렌더러는 1 또는 PointShadowFaceCount만 허용한다.
	uint32 mViewCount{ 1 };
	// 조명에서 복사한 그림자 설정. 생성 여부, 면별 요청 해상도, 깊이 및 기울기 기반 바이어스, 법선 방향 보정량을 결정한다.
	FLightShadowSettings mSettings{};
	// 요청을 만든 조명을 식별하는 비소유 포인터. 생성된 그림자 데이터의 시작 인덱스와 뷰 수를 조명별로 저장하고 조회하는 키로 사용한다.
	const ULightComponent* mLightComponent{};
	// 맵별 Caster 목록과 재사용 조건 평가를 담당하는 비소유 프록시.
	FLightSceneProxy* mLightProxy{};
};

struct FShadowCameraContext {
	float mNearZ{};
	float mFarZ{};
	FVector mCameraPosition{};
	FMatrix mViewProjection{ FMatrix::Identity };
};

struct FCascadeSliceBounds {
	FVector mCascadeSliceNear[4]{};
	FVector mCascadeSliceFar[4]{};
};

bool BuildDirectionalShadowView(const FTransform& LightTransform, const FBox& CasterBounds, int32 Resolution, FShadowView& OutView);
bool BuildDirectionalCascadeShadowView(const FTransform& LightTransform, const FBox& CasterBounds, const FCascadeSliceBounds& SliceBounds, int32 Resolution, FShadowView& OutView, FVector* OutSphereCenter = nullptr, float* OutSphereRadius = nullptr);
bool BuildSpotShadowView(const FTransform& LightTransform, float OuterConeAngle, float AttenuationRadius, FShadowView& OutView);
bool BuildRectShadowView(const FTransform& LightTransform, float HalfWidth, float HalfHeight, float AttenuationRadius, FShadowView& OutView);
bool BuildPointShadowViews(const FTransform& LightTransform, float AttenuationRadius, FShadowView (&OutViews)[PointShadowFaceCount]);
