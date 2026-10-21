#include "EnginePCH.h"
#include "RenderResourceManager.h"

#include "RenderCommand.h"

#include "RenderUtil.h"

namespace
{
	FString MakeShaderKey(const FString& Path, const D3D_SHADER_MACRO* Macros)
	{
		FString Key = Path;
		while (Macros && Macros->Name)
		{
			Key += "_" + FString(Macros->Name) + "_" + FString(Macros->Definition ? Macros->Definition : "");
			++Macros;
		}
		return Key;
	}
}


void FRenderResourceManager::ScanShaders(const fs::path& ShaderRoot)
{
	std::error_code ErrorCode;

	if (!fs::exists(ShaderRoot, ErrorCode))
	{
		HTR_LOG(Error, "[Shader] Scan Root Not Found : {}", ShaderRoot.generic_string());
		return;
	}

	for (const fs::directory_entry& Entry : fs::recursive_directory_iterator(ShaderRoot))
	{
		if (!Entry.is_regular_file()) continue;
		if (Entry.path().extension() != ".hlsl") continue;
		if (Entry.path().stem() == "UberLit") continue;

		FString Path = Entry.path().generic_string();
		LoadOrCompileShader(Path);
	}
	LoadUberLit(ShaderRoot);
}

void FRenderResourceManager::LoadUberLit(const fs::path& ShaderRoot) {
	const FString Path{ (ShaderRoot / "UberLit.hlsl").generic_string() };

	// 같은 UberLit.hlsl을 매크로 조합별로 따로 컴파일한다.
	// 매크로 없는 기본 키("Resources/Shader/UberLit.hlsl")는 기존 머티리얼이 쓰므로 Lambert로 남겨 둔다. (셰이더 쪽 기본값)

	LoadOrCompileShader(Path);	// 기본 (Lambert)
	TUniquePtr<FShaderProgram>* DefaultProgram{ ShaderProgramMap.Find(MakeShaderKey(Path, nullptr)) };
	UberLitDefault = DefaultProgram ? DefaultProgram->get() : nullptr;
	const char* ModelDefines[]{ "LIGHTING_MODEL_GOURAUD", "LIGHTING_MODEL_LAMBERT", "LIGHTING_MODEL_BLINN_PHONG", "LIGHTING_MODEL_UNLIT", "WORLD_NORMAL" };
	static_assert(std::size(ModelDefines) == static_cast<size_t>(EViewMode::Count));
	for (int32 FilterIndex{}; FilterIndex < static_cast<int32>(EShadowFilterMode::Count); ++FilterIndex) {
		const EShadowFilterMode FilterMode{ static_cast<EShadowFilterMode>(FilterIndex) };
		for (int32 ModeIndex{}; ModeIndex < static_cast<int32>(EViewMode::Count); ++ModeIndex) {
			const D3D_SHADER_MACRO Macros[]{
				{ ModelDefines[ModeIndex], "1" },
				{ "SHADOW_PCF", FilterMode == EShadowFilterMode::PCF ? "1" : "0" },
				{ "SHADOW_PCSS", FilterMode == EShadowFilterMode::PCSS ? "1" : "0" },
				{ nullptr, nullptr }
			};
			LoadOrCompileShader(Path, Macros);
			TUniquePtr<FShaderProgram>* Program{ ShaderProgramMap.Find(MakeShaderKey(Path, Macros)) };
			mUberLitPrograms[FilterIndex][ModeIndex] = Program ? Program->get() : nullptr;
		}
	}
}

FShaderProgram* FRenderResourceManager::GetUberLitProgram(EViewMode Mode, EShadowFilterMode ShadowFilterMode) {
	const int32 Index{ static_cast<int32>(Mode) };
	if (Index < 0 || Index >= static_cast<int32>(EViewMode::Count)) {
		return nullptr;
	}
	const int32 FilterIndex{ static_cast<int32>(NormalizeShadowFilterMode(static_cast<int32>(ShadowFilterMode))) };
	return Get().mUberLitPrograms[FilterIndex][Index];
}

bool FRenderResourceManager::IsUberLitProgram(const FShaderProgram* Program)
{
	if (!Program)
		return false;

	const FRenderResourceManager& Self = Get();
	if (Program == Self.UberLitDefault)
		return true;
	for (const auto& Variants : Self.mUberLitPrograms) {
		for (const FShaderProgram* Variant : Variants) {
			if (Program == Variant) {
				return true;
			}
		}
	}
	return false;
}

void FRenderResourceManager::Shutdown()
{
	Get().VertexShaderMap.Empty();
	Get().PixelShaderMap.Empty();
	Get().ComputeShaderMap.Empty();
	Get().ShaderProgramMap.Empty();
	Get().UberLitDefault = nullptr;
	for (auto& Variants : Get().mUberLitPrograms) {
		for (FShaderProgram*& Program : Variants) {
			Program = nullptr;
		}
	}
}

FShaderProgram* FRenderResourceManager::GetShaderProgram(const FString& InPath, const D3D_SHADER_MACRO* Macro)
{
	const FString Key = MakeShaderKey(InPath, Macro);

	if (TUniquePtr<FShaderProgram>* Found = Get().ShaderProgramMap.Find(Key))
		return Found->get();

	HTR_LOG(Error, "[Shader] not found: {}", Key);

	if (TUniquePtr<FShaderProgram>* Fallback =
		Get().ShaderProgramMap.Find("Resources/Shader/DefaultShader.hlsl"))
		return Fallback->get();

	return nullptr;
}

FComputeShader* FRenderResourceManager::GetComputeShader(const FString& InPath)
{
	if (TUniquePtr<FComputeShader>* Found = Get().ComputeShaderMap.Find(InPath))
		return Found->get();
	return nullptr;
}
void FRenderResourceManager::LoadOrCompileShader(const FString& Path, const D3D_SHADER_MACRO* Macro)
{
	std::ifstream In(Path);
	const std::string Source((std::istreambuf_iterator<char>(In)), std::istreambuf_iterator<char>());

	const bool bHasVS = Source.find("mainVS") != std::string::npos;
	const bool bHasPS = Source.find("mainPS") != std::string::npos;
	const bool bHasCS = Source.find("mainCS") != std::string::npos;
	const bool HasGraphicsInclude{ !bHasCS && Source.find("#include") != std::string::npos };

	if (bHasVS || bHasPS || HasGraphicsInclude) {
		const bool HasPixelShader{ bHasPS || HasGraphicsInclude };
		FString VSCSOPath;
		FString PSCSOPath;
		FShaderByteCode VSCode = RenderUtil::GetOrCompile(Path, "mainVS", EShaderType::Vertex, VSCSOPath, Macro);
		FShaderByteCode PSCode{};
		if (HasPixelShader) {
			PSCode = RenderUtil::GetOrCompile(Path, "mainPS", EShaderType::Pixel, PSCSOPath, Macro);
		}
		const FString Key = MakeShaderKey(Path, Macro);




		if (!VSCode.IsValid() || (HasPixelShader && !PSCode.IsValid()))
		{
			HTR_LOG(Error, "[Shader] compile failed: {}", Key);
			return;
		}

		TUniquePtr<FVertexShader> Vs = RenderCommand::CreateVertexShader(VSCode);
		TUniquePtr<FPixelShader>  Ps = HasPixelShader ? RenderCommand::CreatePixelShader(PSCode) : nullptr;

		if (!Vs || !Vs->IsValid() || (HasPixelShader && (!Ps || !Ps->IsValid())))
		{
			HTR_LOG(Error, "[Shader] device create failed: {}", Key);
			return;
		}

		FVertexShader* VsRaw = Vs.get();
		FPixelShader* PsRaw = Ps.get();

		VertexShaderMap[VSCSOPath] = std::move(Vs);
		if (Ps) {
			PixelShaderMap[PSCSOPath] = std::move(Ps);
		}

		ShaderProgramMap[Key] = MakeUnique<FShaderProgram>(VsRaw, PsRaw);

		HTR_LOG(Info, "[Shader] Loaded Shader: {}", Path);
	}

	if (bHasCS)
	{
		FString CSOPath;
		FShaderByteCode CSCode = RenderUtil::GetOrCompile(Path, "mainCS", EShaderType::Compute, CSOPath);
		if (!CSCode.IsValid()) { HTR_LOG(Error, "[Shader] compute compile failed: {}", Path); return; }   // 진짜 에러

		ComputeShaderMap[Path] = RenderCommand::CreateComputeShader(CSCode);

		HTR_LOG(Info, "[Shader] Loaded ComputeShader: {}", Path);

	}

}
