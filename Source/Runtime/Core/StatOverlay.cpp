#include "EnginePCH.h"
#include "StatOverlay.h"

#include "EngineStatics.h"
#include "Render/RenderCommand.h"
#include "Render/GraphicsMemoryStats.h"

#include <cctype>
#include <dxgi1_4.h>
#include <format>
#include <psapi.h>

#pragma comment(lib, "psapi")

namespace {
	// 콘솔 입력은 대소문자를 구분하지 않는다. UE도 "STAT FPS"와 "stat fps"를 같게 다룬다.
	bool EqualsIgnoreCase(const FString& A, const char* B) {
		uint64 Index{};
		for (; Index < A.size() && B[Index] != '\0'; ++Index) {
			const int Left{ std::tolower(static_cast<unsigned char>(A[Index])) };
			const int Right{ std::tolower(static_cast<unsigned char>(B[Index])) };
			if (Left != Right)
				return false;
		}
		return Index == A.size() && B[Index] == '\0';
	}

	// 공백 구분 토큰으로 나눈다. 연속 공백과 앞뒤 공백은 무시한다.
	void Tokenize(const FString& Line, TArray<FString>& OutTokens) {
		OutTokens.Reset();
		uint64 Cursor{};
		while (Cursor < Line.size()) {
			while (Cursor < Line.size() && std::isspace(static_cast<unsigned char>(Line[Cursor])))
				++Cursor;
			const uint64 Start{ Cursor };
			while (Cursor < Line.size() && !std::isspace(static_cast<unsigned char>(Line[Cursor])))
				++Cursor;
			if (Cursor > Start)
				OutTokens.Add(Line.substr(Start, Cursor - Start));
		}
	}
}

// 샘플 구간이 찰 때만 표시 수치를 바꿔 숫자가 매 프레임 튀지 않게 한다.
void FStatOverlay::Tick(const float DeltaTime) {
	mAccumulatedTime += DeltaTime;
	++mAccumulatedFrames;

	if (mAccumulatedTime < mSampleInterval || mAccumulatedFrames <= 0)
		return;

	mDisplayFPS = static_cast<float>(mAccumulatedFrames) / mAccumulatedTime;
	mDisplayFrameTimeMs = (mAccumulatedTime / static_cast<float>(mAccumulatedFrames)) * 1000.0f;

	if (IsEnabled(EStatFlags::GPUMemory) || IsEnabled(EStatFlags::Memory)) {
		UpdateGPUMemoryStats();
	}

	mAccumulatedTime = 0.0f;
	mAccumulatedFrames = 0;
}

// stat 명령만 소비하고 나머지는 호출자에게 되돌린다.
bool FStatOverlay::ExecCommand(const FString& CommandLine, FString& OutMessage) {
	TArray<FString> Tokens{};
	Tokenize(CommandLine, Tokens);

	if (Tokens.Num() == 0 || !EqualsIgnoreCase(Tokens[0], "stat"))
		return false;

	if (Tokens.Num() < 2) {
		OutMessage = "Usage: stat <fps|memory|gpu|profile|csm|all|none> | stat gpu dump";
		return true;
	}

	const FString& Arg{ Tokens[1] };

	if ((EqualsIgnoreCase(Arg, "gpu") || EqualsIgnoreCase(Arg, "gpumemory")) && Tokens.Num() > 2 && EqualsIgnoreCase(Tokens[2], "dump")) {
		OutMessage = FGraphicsMemoryStats::GetResourceReport();
		return true;
	}

	if (EqualsIgnoreCase(Arg, "none")) {
		mFlags = EStatFlags::None;
		OutMessage = "stat none: all overlays disabled";
		return true;
	}

	if (EqualsIgnoreCase(Arg, "all")) {
		mFlags = EStatFlags::FPS | EStatFlags::Memory | EStatFlags::Profile | EStatFlags::GPUMemory | EStatFlags::CSM;
		OutMessage = "stat all: all overlays enabled";
		return true;
	}

	EStatFlags Target{ EStatFlags::None };
	if (EqualsIgnoreCase(Arg, "fps"))
		Target = EStatFlags::FPS;
	else if (EqualsIgnoreCase(Arg, "memory"))
		Target = EStatFlags::Memory;
	else if (EqualsIgnoreCase(Arg, "gpu") || EqualsIgnoreCase(Arg, "gpumemory"))
		Target = EStatFlags::GPUMemory;
	else if (EqualsIgnoreCase(Arg, "profile"))
		Target = EStatFlags::Profile;
	else if (EqualsIgnoreCase(Arg, "csm"))
		Target = EStatFlags::CSM;

	if (Target == EStatFlags::None) {
		OutMessage = std::format("stat: unknown stat '{}'. Available: fps, memory, gpu, profile, csm, all, none", Arg);
		return true;
	}

	// UE와 같이 같은 명령을 다시 입력하면 해당 항목만 꺼진다.
	const bool Enable{ !HasFlag(mFlags, Target) };
	if (Enable)
		mFlags |= Target;
	else
		mFlags &= ~Target;

	OutMessage = std::format("stat {}: {}", Arg, Enable ? "on" : "off");
	return true;
}

EStatFlags FStatOverlay::GetFlags() {
	return mFlags;
}

bool FStatOverlay::IsEnabled(EStatFlags Flag) {
	return HasFlag(mFlags, Flag);
}

bool FStatOverlay::IsAnyEnabled() {
	return mFlags != EStatFlags::None;
}

float FStatOverlay::GetFPS() {
	return mDisplayFPS;
}

float FStatOverlay::GetFrameTimeMs() {
	return mDisplayFrameTimeMs;
}

uint64 FStatOverlay::GetObjectAllocationBytes() {
	return FEngineStatics::TotalAllocationBytes;
}

uint32 FStatOverlay::GetObjectAllocationCount() {
	return FEngineStatics::TotalAllocationCount;
}

// UObject 추적치는 엔진이 직접 센 값이라 텍스처·버퍼 같은 다른 할당이 빠진다.
// 전체 사용량은 OS에 직접 묻는다.
uint64 FStatOverlay::GetProcessWorkingSetBytes() {
	PROCESS_MEMORY_COUNTERS Counters{};
	if (!GetProcessMemoryInfo(GetCurrentProcess(), &Counters, sizeof(Counters)))
		return 0;
	return static_cast<uint64>(Counters.WorkingSetSize);
}

bool FStatOverlay::HasGPULocalMemoryInfo() {
	return mHasGPULocalMemoryInfo;
}

bool FStatOverlay::HasGPUNonLocalMemoryInfo() {
	return mHasGPUNonLocalMemoryInfo;
}

uint64 FStatOverlay::GetGPULocalUsageBytes() {
	return mGPULocalUsageBytes;
}

uint64 FStatOverlay::GetGPULocalBudgetBytes() {
	return mGPULocalBudgetBytes;
}

uint64 FStatOverlay::GetGPUNonLocalUsageBytes() {
	return mGPUNonLocalUsageBytes;
}

uint64 FStatOverlay::GetGPUNonLocalBudgetBytes() {
	return mGPUNonLocalBudgetBytes;
}

void FStatOverlay::UpdateCSMStats(const FCSMDebugSnapshot& Snapshot) {
	mCSMStats = Snapshot;
}

const FCSMDebugSnapshot& FStatOverlay::GetCSMStats() {
	return mCSMStats;
}

void FStatOverlay::UpdateGPUMemoryStats() {
	mHasGPULocalMemoryInfo = false;
	mHasGPUNonLocalMemoryInfo = false;
	mGPULocalUsageBytes = 0;
	mGPULocalBudgetBytes = 0;
	mGPUNonLocalUsageBytes = 0;
	mGPUNonLocalBudgetBytes = 0;

	ID3D11Device* Device{ RenderCommand::GetDevice() };
	if (!Device) {
		return;
	}

	Microsoft::WRL::ComPtr<IDXGIDevice> DXGIDevice{};
	if (FAILED(Device->QueryInterface(IID_PPV_ARGS(DXGIDevice.GetAddressOf())))) {
		return;
	}

	Microsoft::WRL::ComPtr<IDXGIAdapter> Adapter{};
	if (FAILED(DXGIDevice->GetAdapter(Adapter.GetAddressOf()))) {
		return;
	}

	Microsoft::WRL::ComPtr<IDXGIAdapter3> MemoryAdapter{};
	if (FAILED(Adapter.As(&MemoryAdapter))) {
		return;
	}

	DXGI_QUERY_VIDEO_MEMORY_INFO LocalInfo{};
	if (SUCCEEDED(MemoryAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &LocalInfo))) {
		mHasGPULocalMemoryInfo = true;
		mGPULocalUsageBytes = LocalInfo.CurrentUsage;
		mGPULocalBudgetBytes = LocalInfo.Budget;
	}

	DXGI_QUERY_VIDEO_MEMORY_INFO NonLocalInfo{};
	if (SUCCEEDED(MemoryAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &NonLocalInfo))) {
		mHasGPUNonLocalMemoryInfo = true;
		mGPUNonLocalUsageBytes = NonLocalInfo.CurrentUsage;
		mGPUNonLocalBudgetBytes = NonLocalInfo.Budget;
	}
}
