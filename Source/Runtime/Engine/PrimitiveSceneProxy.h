#pragma once

#include "Math/Frustum.h"
#include "Render/MeshRenderData.h"

class FScene;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterial;

class FPrimitiveSceneProxy {
public:
    explicit FPrimitiveSceneProxy(UPrimitiveComponent* InComponent);
    ~FPrimitiveSceneProxy();

public:
    void UpdateTransform();
    void UpdateRenderState();
    void OnMeshRenderDataChanged();
    void OnMeshDestroyed();

    UPrimitiveComponent* GetComponent() const;
    const FAABB& GetBounds() const;
    const FMatrix& GetLocalToWorld() const;
    const FMatrix& GetWorldToLocal() const;
    const FLODSphere& GetLODSphere() const;
    UStaticMesh* GetMesh() const;
    bool IsVisible() const;
    bool CastsShadows() const;
    bool ReceivesShadows() const;
    uint8 GetLODCount() const;
    const FMeshRenderState* GetRenderState() const;
    const float* GetLODThresholdsSq() const;
    const FCachedMeshLOD& GetLOD(uint32 Index) const;
    const FCachedMeshSection& GetSection(uint32 Index) const;
    FScene* GetScene() const;
private:
    friend class FScene;
    FScene* Scene = nullptr;
    UPrimitiveComponent* Component = nullptr;
    UStaticMesh* Mesh = nullptr;
    TSharedPtr<const FMeshRenderState> RenderState;
    FMatrix LocalToWorld;
    FMatrix WorldToLocal;
    FAABB Bounds;
    FLODSphere LODSphere;
    int32 PackedIndex = INDEX_NONE;
    bool bQueuedForUpdate = false;
    bool bRenderStateQueued = false;
    bool bVisible = true;
    bool mCastShadows{ true };
    bool mReceiveShadows{ true };
};
