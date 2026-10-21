#pragma once

#include "Types.h"
#include "EngineString.h"
#include "Math/Vector.h"

struct FCascadeDebugInfo {
	float mNear{};
	float mFar{};
	FVector mSphereCenter{};
	float mSphereRadius{};
};
struct FCSMDebugSnapshot {
	bool mEnabled{};
	uint32 mCascadeCount{};
	FCascadeDebugInfo mCascades[4]{};
};
// Overlay로 표시할 Stat 항목. 여러 항목을 동시에 켤 수 있어 비트 플래그로 둔다.
enum class EStatFlags : uint32 {
	None      = 0,
	FPS       = 1 << 0,
	Memory    = 1 << 1,
	Profile   = 1 << 2,
	GPUMemory = 1 << 3,
	CSM       = 1 << 4,
};
DEFINE_ENUM_OPERATORS(EStatFlags)

// 콘솔 stat 명령의 상태와 표시용 수치만 보관한다.
// 그리기는 Viewport를 소유한 에디터 쪽이 맡아 엔진이 ImGui에 의존하지 않게 한다.
class FStatOverlay {
public:
	// 프레임마다 호출해 FPS 평균 창을 누적하고 표시 수치를 갱신한다.
	static void Tick(float DeltaTime);

	// "stat ..." 명령을 해석한다.
	// stat 명령이 아니면 false를 반환해 호출자가 나머지 명령 처리를 이어가게 한다.
	static bool ExecCommand(const FString& CommandLine, FString& OutMessage);

	static EStatFlags GetFlags();
	static bool IsEnabled(EStatFlags Flag);
	static bool IsAnyEnabled();

	// 샘플 구간 평균 FPS와 프레임 시간(ms). 매 프레임 값은 흔들려서 평균만 노출한다.
	static float GetFPS();
	static float GetFrameTimeMs();

	// UObject::operator new/delete가 추적한 누적 할당량.
	static uint64 GetObjectAllocationBytes();
	static uint32 GetObjectAllocationCount();
	// 프로세스 전체가 점유한 물리 메모리(Working Set).
	static uint64 GetProcessWorkingSetBytes();

	static bool HasGPULocalMemoryInfo();
	static bool HasGPUNonLocalMemoryInfo();
	static uint64 GetGPULocalUsageBytes();
	static uint64 GetGPULocalBudgetBytes();
	static uint64 GetGPUNonLocalUsageBytes();
	static uint64 GetGPUNonLocalBudgetBytes();
	static void UpdateCSMStats(const FCSMDebugSnapshot& Snapshot);
	static const FCSMDebugSnapshot& GetCSMStats();

private:
	static void UpdateGPUMemoryStats();

private:
	// 표시 수치 갱신 주기(초). 너무 짧으면 숫자가 읽히지 않는다.
	static constexpr float mSampleInterval{ 0.25f };

	inline static EStatFlags mFlags{ EStatFlags::None };
	inline static float mAccumulatedTime{};
	inline static int32 mAccumulatedFrames{};
	inline static float mDisplayFPS{};
	inline static float mDisplayFrameTimeMs{};
	inline static bool mHasGPULocalMemoryInfo{};
	inline static bool mHasGPUNonLocalMemoryInfo{};
	inline static uint64 mGPULocalUsageBytes{};
	inline static uint64 mGPULocalBudgetBytes{};
	inline static uint64 mGPUNonLocalUsageBytes{};
	inline static uint64 mGPUNonLocalBudgetBytes{};
	inline static FCSMDebugSnapshot mCSMStats{};
};
