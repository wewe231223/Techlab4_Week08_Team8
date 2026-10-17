#include "EnginePCH.h"
#include "DirectionalLightComponent.h"

#include "Engine/DirectionalLightSceneProxy.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{

}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

FLightSceneProxy* UDirectionalLightComponent::CreateSceneProxy() const
{
    return new FDirectionalLightSceneProxy(this);
}

FMatrix UDirectionalLightComponent::GetShadowViewPorjection(FVector Center, float Radius, int index) const
{
    if (Radius <= 0.f)
        return FMatrix();

    // 셰이더에 넘기는 방향(프록시)과 같은 월드 회전을 써야 조명과 그림자 방향이 맞는다.
    FTransform Transform;
    Transform.Rotation = GetWorldRotation();
    const FVector Forward = Transform.GetForward();   // 빛이 나가는 방향 = View 공간 X(깊이)
    const FVector Right = Transform.GetRight();
    const FVector Up = Transform.GetUp();

    const FVector Eye = Center - Forward * Radius;
    const float Near = 0.0f;
    const float Far = 2.0f * Radius;
    const float Width = 2.0f * Radius;
    const float Height = 2.0f * Radius;

    const FMatrix View(
        Forward.X, Right.X, Up.X, 0.0f,
        Forward.Y, Right.Y, Up.Y, 0.0f,
        Forward.Z, Right.Z, Up.Z, 0.0f,
        -FVector::Dot(Forward, Eye), -FVector::Dot(Right, Eye), -FVector::Dot(Up, Eye), 1.0f);

    const FMatrix Projection(
        0.0f, 0.0f, 1.0f / (Far - Near), 0.0f,
        2.0f / Width, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / Height, 0.0f, 0.0f,
        0.0f, 0.0f, -Near / (Far - Near), 1.0f);

    return View * Projection;
}
