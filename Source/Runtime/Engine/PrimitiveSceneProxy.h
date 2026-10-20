#pragma once

#include "Math/Frustum.h"
#include "Render/MeshRenderData.h"

class FScene;
class UPrimitiveComponent;
class UStaticMesh;
class UMaterial;

class FPrimitiveSceneProxy
{
private:
    // 그림자 지오메트리 변경 감지를 위해 기록하는 불투명 섹션 상태.
    struct FShadowSectionState {
        const void* mVertexBuffer{};
        const void* mIndexBuffer{};
        uint32 mLODIndex{};
        uint32 mStartIndex{};
        uint32 mIndexCount{};
        uint32 mBufferIndexCount{};
    };

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
    void UpdateShadowRevision(uint64 UpdateSerial);
    void ResetShadowTracking();
    uint64 GetShadowRevision() const;
    const FMatrix& GetShadowLocalToWorld() const;

private:
    bool HasShadowGeometryChanged() const;
    void CaptureShadowGeometry();

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
    // 그림자 관련 상태가 바뀌었을 때 새로 부여하는 버전.
    uint64 mShadowRevision{};
    // 이 객체의 변경 검사를 마지막으로 수행한 그림자 렌더 호출 번호.
    uint64 mShadowUpdateSerial{};
    // 현재 Revision에 대응하는 변환. 캐시 경로의 깊이 렌더링에도 같은 값을 사용한다.
    FMatrix mShadowTransform{ FMatrix::Identity };
    // 마지막 Revision 갱신 시점의 메시, 표시 여부, 그림자 설정과 섹션 상태.
    const UStaticMesh* mShadowMesh{};
    uint32 mShadowMeshId{};
    // Mesh의 RebuildRenderData 버전. 같은 Mesh 객체의 지오메트리 변경도 감지한다.
    uint64 mShadowMeshRevision{};
    bool mShadowVisible{};
    bool mShadowCastShadows{};
    TArray<FShadowSectionState> mShadowSections{};
};
