#include "EnginePCH.h"
#include "PrimitiveSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Render/Mesh.h"
#include "Engine/Scene.h"
#include "Math/EpsilonCompare.h"

#include <atomic>

bool FPrimitiveSceneProxy::CastsShadows() const {
    return mCastShadows;
}

bool FPrimitiveSceneProxy::ReceivesShadows() const {
    return mReceiveShadows;
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

uint64 FPrimitiveSceneProxy::GetShadowRevision() const {
    return mShadowRevision;
}

const FMatrix& FPrimitiveSceneProxy::GetShadowLocalToWorld() const {
    return mShadowTransform;
}

void FPrimitiveSceneProxy::UpdateShadowRevision(uint64 UpdateSerial) {
    if (UpdateSerial != 0 && mShadowUpdateSerial == UpdateSerial) {
        return;
    }
    mShadowUpdateSerial = UpdateSerial;

    const EpsilonCompare<FMatrix> CurrentTransform{ LocalToWorld };
    const EpsilonCompare<FMatrix> PreviousTransform{ mShadowTransform };
    const uint64 MeshRevision{ Mesh ? Mesh->GetRenderDataRevision() : 0 };
    const uint32 MeshId{ Mesh ? Mesh->GetUUID() : 0 };
    if (mShadowRevision != 0 && CurrentTransform == PreviousTransform && Mesh == mShadowMesh && MeshId == mShadowMeshId && MeshRevision == mShadowMeshRevision && bVisible == mShadowVisible && mCastShadows == mShadowCastShadows && !HasShadowGeometryChanged()) {
        return;
    }

    // 프록시 주소가 재사용되어도 이전 상태와 구분되도록 모든 Primitive가 공유한다.
    static std::atomic<uint64> RevisionCounter{};
    mShadowRevision = RevisionCounter.fetch_add(1, std::memory_order_relaxed) + 1;
    mShadowTransform = LocalToWorld;
    mShadowMesh = Mesh;
    mShadowMeshId = MeshId;
    mShadowMeshRevision = MeshRevision;
    mShadowVisible = bVisible;
    mShadowCastShadows = mCastShadows;
    CaptureShadowGeometry();
}

void FPrimitiveSceneProxy::ResetShadowTracking() {
    mShadowRevision = 0;
    mShadowUpdateSerial = 0;
    mShadowMesh = nullptr;
    mShadowSections.Empty();
}

bool FPrimitiveSceneProxy::HasShadowGeometryChanged() const {
    uint32 StateIndex{};
    if (Mesh && RenderState) {
        for (uint32 LODIndex{}; LODIndex < RenderState->LODCount; ++LODIndex) {
            const FVertexBuffer* VertexBuffer{ Mesh->GetVertexBuffer(LODIndex) };
            const FIndexBuffer* IndexBuffer{ Mesh->GetIndexBuffer(LODIndex) };
            const void* VertexResource{ VertexBuffer ? VertexBuffer->GetBuffer() : nullptr };
            const void* IndexResource{ IndexBuffer ? IndexBuffer->GetBuffer() : nullptr };
            const uint32 BufferIndexCount{ IndexBuffer ? IndexBuffer->GetIndexCount() : 0 };
            const FCachedMeshLOD& LOD{ RenderState->LODs[LODIndex] };
            for (uint32 SectionIndex{}; SectionIndex < LOD.NumSections; ++SectionIndex) {
                const FCachedMeshSection& Section{ RenderState->Sections[LOD.FirstSection + SectionIndex] };
                if (!Section.Material || Section.Material->BlendState != EBlendState::Opaque || Section.IndexCount == 0) {
                    continue;
                }

                if (StateIndex >= static_cast<uint32>(mShadowSections.Num())) {
                    return true;
                }
                const FShadowSectionState& State{ mShadowSections[StateIndex++] };
                if (State.mVertexBuffer != VertexResource || State.mIndexBuffer != IndexResource || State.mBufferIndexCount != BufferIndexCount || State.mLODIndex != LODIndex || State.mStartIndex != Section.StartIndex || State.mIndexCount != Section.IndexCount) {
                    return true;
                }
            }
        }
    }

    return StateIndex != static_cast<uint32>(mShadowSections.Num());
}

void FPrimitiveSceneProxy::CaptureShadowGeometry() {
    mShadowSections.Reset();
    if (!Mesh || !RenderState) {
        return;
    }

    for (uint32 LODIndex{}; LODIndex < RenderState->LODCount; ++LODIndex) {
        const FVertexBuffer* VertexBuffer{ Mesh->GetVertexBuffer(LODIndex) };
        const FIndexBuffer* IndexBuffer{ Mesh->GetIndexBuffer(LODIndex) };
        const void* VertexResource{ VertexBuffer ? VertexBuffer->GetBuffer() : nullptr };
        const void* IndexResource{ IndexBuffer ? IndexBuffer->GetBuffer() : nullptr };
        const uint32 BufferIndexCount{ IndexBuffer ? IndexBuffer->GetIndexCount() : 0 };
        const FCachedMeshLOD& LOD{ RenderState->LODs[LODIndex] };
        for (uint32 SectionIndex{}; SectionIndex < LOD.NumSections; ++SectionIndex) {
            const FCachedMeshSection& Section{ RenderState->Sections[LOD.FirstSection + SectionIndex] };
            if (Section.Material && Section.Material->BlendState == EBlendState::Opaque && Section.IndexCount != 0) {
                mShadowSections.Add(FShadowSectionState{ VertexResource, IndexResource, LODIndex, Section.StartIndex, Section.IndexCount, BufferIndexCount });
            }
        }
    }
}
