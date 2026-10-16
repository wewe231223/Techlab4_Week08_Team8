#include "EnginePCH.h"
#include "PrimitiveSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Render/Mesh.h"
#include "Engine/Scene.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
    : Component{ InComponent } {
}

UPrimitiveComponent* FPrimitiveSceneProxy::GetComponent() const {
    return Component;
}

const FAABB& FPrimitiveSceneProxy::GetBounds() const {
    return Bounds;
}

const FMatrix& FPrimitiveSceneProxy::GetLocalToWorld() const {
    return LocalToWorld;
}

const FMatrix& FPrimitiveSceneProxy::GetWorldToLocal() const {
    return WorldToLocal;
}

const FLODSphere& FPrimitiveSceneProxy::GetLODSphere() const {
    return LODSphere;
}

UStaticMesh* FPrimitiveSceneProxy::GetMesh() const {
    return Mesh;
}

bool FPrimitiveSceneProxy::IsVisible() const {
    return bVisible;
}

bool FPrimitiveSceneProxy::CastsShadows() const {
    return mCastShadows;
}

bool FPrimitiveSceneProxy::ReceivesShadows() const {
    return mReceiveShadows;
}

uint8 FPrimitiveSceneProxy::GetLODCount() const {
    return RenderState ? RenderState->LODCount : 1;
}

const FMeshRenderState* FPrimitiveSceneProxy::GetRenderState() const {
    return RenderState.get();
}

const float* FPrimitiveSceneProxy::GetLODThresholdsSq() const {
    return RenderState->LODThresholdSq;
}

const FCachedMeshLOD& FPrimitiveSceneProxy::GetLOD(uint32 Index) const {
    return RenderState->LODs[Index];
}

const FCachedMeshSection& FPrimitiveSceneProxy::GetSection(uint32 Index) const {
    return RenderState->Sections[Index];
}

FScene* FPrimitiveSceneProxy::GetScene() const {
    return Scene;
}

FPrimitiveSceneProxy::~FPrimitiveSceneProxy()
{
    if (Mesh) Mesh->UnregisterProxy(this);
}

void FPrimitiveSceneProxy::OnMeshRenderDataChanged()
{
    if (Scene)
    {
        Scene->MarkRenderStateDirty(this);
        Scene->MarkDirty(this);
    }
}

void FPrimitiveSceneProxy::OnMeshDestroyed()
{
    Mesh = nullptr;
    RenderState.reset();
}

void FPrimitiveSceneProxy::UpdateTransform()
{
    LocalToWorld = Component->GetWorldMatrix();
    WorldToLocal = LocalToWorld.Inverse();
    const FBox LocalBounds = Mesh ? Mesh->GetRenderBounds() : Component->CalcLocalBounds();
    Bounds = MakeWorldBounds(LocalBounds.GetWorldAABB(LocalToWorld));
    LODSphere = Mesh ? Mesh->GetLocalLODSphere().Transform(LocalToWorld) : FLODSphere{};
}

void FPrimitiveSceneProxy::UpdateRenderState()
{
    UStaticMesh* PreviousMesh = Mesh;
    Mesh = nullptr;
    RenderState.reset();
    bVisible = Component->IsVisible();
    mCastShadows = Component->CastsShadows();
    mReceiveShadows = Component->ReceivesShadows();
    UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component);
    if (StaticMeshComponent) Mesh = StaticMeshComponent->GetStaticMesh();
    if (Mesh != PreviousMesh)
    {
        if (PreviousMesh) PreviousMesh->UnregisterProxy(this);
        if (Mesh) Mesh->RegisterProxy(this);
        if (Scene) Scene->MarkDirty(this);
    }
    if (!Mesh) return;
    std::vector<UMaterial*> Materials;
    Materials.reserve(Mesh->MeshData.MaterialSlots.Num());
    for (uint32 Slot = 0; Slot < static_cast<uint32>(Mesh->MeshData.MaterialSlots.Num()); ++Slot)
        Materials.push_back(StaticMeshComponent->GetMaterial(Slot));
    RenderState = Mesh->GetRenderState(Materials);
}
