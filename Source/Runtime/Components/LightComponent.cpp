#include "EnginePCH.h"
#include "LightComponent.h"

#include "Engine/Scene.h"
#include "Engine/World.h"

#include <bit>

ULightComponent::ULightComponent() {}
ULightComponent::~ULightComponent() {}
void ULightComponent::DrawDebug(FLineBatcher* LineBatcher) const {}

namespace
{
	uint32 ClampShadowResolution(uint32 Resolution)
	{
		return std::clamp(Resolution, ULightComponent::MinShadowResolution, ULightComponent::MaxShadowResolution);
	}
}

uint32 ULightComponent::GetShadowResolution() const
{
	// 거듭제곱이 아닌 값은 아래로 내려 쓴다 (메모리를 늘리지 않는 쪽).
	return std::bit_floor(ClampShadowResolution(ShadowResolution));
}

void ULightComponent::SetShadowResolution(uint32 InResolution)
{
	ShadowResolution = std::bit_floor(ClampShadowResolution(InResolution));
}

void ULightComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name != "ShadowResolution")
		return;

	// DragInt는 int로 쓰므로 음수가 들어오면 uint32에서 아주 큰 값이 된다. 최소값으로 돌린다.
	const uint32 Requested = static_cast<int32>(ShadowResolution) < 0 ? MinShadowResolution : ShadowResolution;

	uint32 Snapped = Requested;
	if (Requested > 0 && !std::has_single_bit(Requested))
	{
		const uint32 Lower = std::bit_floor(Requested);
		const uint32 Upper = Lower * 2;
		// DragInt는 1씩 움직이므로 거듭제곱 바로 위(1025)면 올린 것, 바로 아래(1023)면 내린 것으로 보고 한 단계 이동한다.
		// 직접 입력한 값(3000 등)은 가까운 쪽으로 맞춘다.
		if (Requested - Lower == 1)       Snapped = Upper;
		else if (Upper - Requested == 1)  Snapped = Lower;   // 1024에서 내린 1023 → bit_floor는 512
		else                              Snapped = (Requested - Lower < Upper - Requested) ? Lower : Upper;
	}

	SetShadowResolution(Snapped);
}

void ULightComponent::CreateRenderState()
{
	Super::CreateRenderState();
	GetWorld()->GetScene().AddLight(this);
}

void ULightComponent::DestroyRenderState()
{
	GetWorld()->GetScene().RemoveLight(this);
	Super::DestroyRenderState();
}
