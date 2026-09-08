#include "TRIADSensorFusionScenarioManager.h"

#include "CesiumGeoreference.h"
#include "OriginPlacement.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Interfaces/IPluginManager.h"
#include "Json.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "TRIADDemoDroneActor.h"
#include "TRIADGeodesy.h"
#include "TRIADLongRangeSensorModel.h"
#include "TRIADIstanaRuntimePolicyActor.h"
#include "TRIADOperatorObserverActor.h"
#include "TRIADRFEmitterComponent.h"
#include "TRIADSensorNodeActor.h"
#include "TRIADSensorNodeComponent.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "Weather/WeatherLib.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
    bool IsSha256String(const FString& Value)
    {
        if (Value.Len() != 64)
        {
            return false;
        }
        for (const TCHAR Character : Value)
        {
            if (!FChar::IsHexDigit(Character))
            {
                return false;
            }
        }
        return true;
    }

    FString NormalizePIEWorldPackageName(const FString& PackageName)
    {
        const FString Directory = FPaths::GetPath(PackageName);
        FString ShortName = FPaths::GetCleanFilename(PackageName);
        static const FString PIEPrefix = TEXT("UEDPIE_");
        if (ShortName.StartsWith(PIEPrefix, ESearchCase::CaseSensitive))
        {
            const int32 MapNameDelimiter = ShortName.Find(
                TEXT("_"),
                ESearchCase::CaseSensitive,
                ESearchDir::FromStart,
                PIEPrefix.Len());
            if (MapNameDelimiter != INDEX_NONE && MapNameDelimiter + 1 < ShortName.Len())
            {
                ShortName.RightChopInline(MapNameDelimiter + 1, EAllowShrinking::No);
            }
        }
        return Directory.IsEmpty() ? ShortName : FPaths::Combine(Directory, ShortName);
    }

    FString RFPathKindToString(ETRIADRFPathKind Kind)
    {
        switch (Kind)
        {
        case ETRIADRFPathKind::Direct:
            return TEXT("DIRECT");
        case ETRIADRFPathKind::Transmitted:
            return TEXT("STRAIGHT_TRANSMISSION");
        case ETRIADRFPathKind::SingleReflection:
            return TEXT("SINGLE_REFLECTION_NOT_ADMITTED_BY_SCENARIO_BINDING");
        default:
            return TEXT("UNKNOWN");
        }
    }

    FString RFInteractionKindToString(ETRIADRFInteractionKind Kind)
    {
        return Kind == ETRIADRFInteractionKind::Transmission
            ? TEXT("TRANSMISSION")
            : TEXT("REFLECTION_NOT_ADMITTED_BY_SCENARIO_BINDING");
    }

    FString RFCalibrationStateToString(ETRIADRFMaterialCalibrationState State)
    {
        switch (State)
        {
        case ETRIADRFMaterialCalibrationState::NotApplicable:
            return TEXT("NOT_APPLICABLE");
        case ETRIADRFMaterialCalibrationState::Uncalibrated:
            return TEXT("UNCALIBRATED_ASSUMPTION");
        case ETRIADRFMaterialCalibrationState::Calibrated:
            return TEXT("CALIBRATED_COEFFICIENT_PROVENANCE_ONLY_NOT_SURVEY_TRUTH");
        default:
            return TEXT("UNKNOWN");
        }
    }

    FString MakeOperatorRFCueTrackId(
        const AActor* Target,
        const UTRIADRFEmitterComponent* Emitter)
    {
        // The operator identifier is stable for the authored target/emitter pair
        // but never exposes descriptive scenario names such as hostile/friendly.
        const FString TargetSeed = Target ? Target->GetName() : TEXT("unknown-target");
        const FString EmitterSeed = Emitter ? Emitter->Definition.EmitterId : TEXT("unknown-emitter");
        const FString Seed = TargetSeed + TEXT("|") + EmitterSeed;
        const FString SaltedSeed = TEXT("triad-rf-cue-v1|") + Seed;
        const uint32 First = FCrc::StrCrc32(*Seed);
        const uint32 Second = FCrc::StrCrc32(*SaltedSeed);
        return FString::Printf(TEXT("RF-CUE-%08X-%08X"), First, Second);
    }

    FString MakeSearchRadarTrackId(const AActor* Target)
    {
        const FString TargetSeed = Target ? Target->GetName() : TEXT("unknown-target");
        const uint32 First = FCrc::StrCrc32(*TargetSeed);
        const uint32 Second = FCrc::StrCrc32(*(TEXT("triad-search-radar-v1|") + TargetSeed));
        return FString::Printf(TEXT("RADAR-%08X-%08X"), First, Second);
    }

    FString MakeSensorContactIdFromTargetSeed(const FString& TargetSeed)
    {
        // One opaque identifier joins RF, radar, PTZ, observer and downstream C2
        // records without exposing the authored target name to the operator.
        const uint32 First = FCrc::StrCrc32(*TargetSeed);
        const uint32 Second = FCrc::StrCrc32(*(TEXT("triad-sensor-contact-v1|") + TargetSeed));
        return FString::Printf(TEXT("CONTACT-%08X-%08X"), First, Second);
    }

    FString MakeSensorContactId(const AActor* Target)
    {
        return MakeSensorContactIdFromTargetSeed(
            Target ? Target->GetName() : TEXT("unknown-target"));
    }

    struct FResolvedSingaporeWeather
    {
        FString Name;
        float RainAmount = 0.0f;
        float RoadWetnessAmount = 0.0f;
        float FogAmount = 0.0f;
        float DustAmount = 0.0f;
        FVector Wind = FVector::ZeroVector;
        double RainRateMillimetersPerHour = 0.0;
        double VisibilityMeters = 30000.0;
        double RFSpecificAttenuationDbPerKmAt2_4GHz = 0.0;
        double RFSpecificAttenuationDbPerKmAt5_8GHz = 0.0;
    };

    FResolvedSingaporeWeather ResolveSingaporeWeather(const FTRIADSingaporeWeatherSettings& Settings)
    {
        // These small coefficients are transparent, repeatable scenario-test inputs.
        // They deliberately remain separate from FSPL and must be replaced/calibrated
        // by a validated propagation model for quantitative RF performance claims.
        FResolvedSingaporeWeather Result;
        switch (Settings.Profile)
        {
        case ETRIADSingaporeWeatherProfile::LightRain:
            Result.Name = TEXT("Light Rain");
            Result.RainAmount = 0.30f;
            Result.RoadWetnessAmount = 0.40f;
            Result.FogAmount = 0.04f;
            Result.Wind = FVector(0.18, 0.05, 0.0);
            Result.RainRateMillimetersPerHour = 5.0;
            Result.VisibilityMeters = 12000.0;
            Result.RFSpecificAttenuationDbPerKmAt2_4GHz = 0.001;
            Result.RFSpecificAttenuationDbPerKmAt5_8GHz = 0.008;
            break;
        case ETRIADSingaporeWeatherProfile::Monsoon:
            Result.Name = TEXT("Monsoon");
            Result.RainAmount = 0.90f;
            Result.RoadWetnessAmount = 0.95f;
            Result.FogAmount = 0.14f;
            Result.Wind = FVector(0.72, 0.22, 0.0);
            Result.RainRateMillimetersPerHour = 80.0;
            Result.VisibilityMeters = 3000.0;
            Result.RFSpecificAttenuationDbPerKmAt2_4GHz = 0.010;
            Result.RFSpecificAttenuationDbPerKmAt5_8GHz = 0.080;
            break;
        case ETRIADSingaporeWeatherProfile::Haze:
            Result.Name = TEXT("Haze");
            Result.FogAmount = 0.48f;
            Result.DustAmount = 0.28f;
            Result.Wind = FVector(0.06, 0.02, 0.0);
            Result.VisibilityMeters = 2500.0;
            Result.RFSpecificAttenuationDbPerKmAt2_4GHz = 0.0005;
            Result.RFSpecificAttenuationDbPerKmAt5_8GHz = 0.003;
            break;
        case ETRIADSingaporeWeatherProfile::Custom:
            Result.Name = TEXT("Custom");
            Result.RainAmount = FMath::Clamp(Settings.CustomRainAmount, 0.0f, 1.0f);
            Result.RoadWetnessAmount = FMath::Clamp(Settings.CustomRoadWetnessAmount, 0.0f, 1.0f);
            Result.FogAmount = FMath::Clamp(Settings.CustomFogAmount, 0.0f, 1.0f);
            Result.DustAmount = FMath::Clamp(Settings.CustomDustAmount, 0.0f, 1.0f);
            Result.Wind = Settings.CustomWind.BoundToCube(1.0);
            Result.RainRateMillimetersPerHour = FMath::Max(Settings.CustomRainRateMillimetersPerHour, 0.0);
            Result.VisibilityMeters = FMath::Max(Settings.CustomVisibilityMeters, 1.0);
            Result.RFSpecificAttenuationDbPerKmAt2_4GHz =
                FMath::Max(Settings.CustomRFSpecificAttenuationDbPerKmAt2_4GHz, 0.0);
            Result.RFSpecificAttenuationDbPerKmAt5_8GHz =
                FMath::Max(Settings.CustomRFSpecificAttenuationDbPerKmAt5_8GHz, 0.0);
            break;
        case ETRIADSingaporeWeatherProfile::Clear:
        default:
            Result.Name = TEXT("Clear");
            Result.Wind = FVector(0.05, 0.0, 0.0);
            break;
        }
        return Result;
    }

    bool MatchesRegex(const FString& Pattern, const FString& Candidate)
    {
        if (Pattern.IsEmpty())
        {
            return false;
        }
        const FRegexPattern RegexPattern(Pattern);
        FRegexMatcher Matcher(RegexPattern, Candidate);
        return Matcher.FindNext();
    }

    FString EscapeCsv(const FString& Value)
    {
        FString Escaped = Value;
        Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
        return FString::Printf(TEXT("\"%s\""), *Escaped);
    }

    int64 Utf8Length(const FString& Value)
    {
        return FTCHARToUTF8(*Value).Length();
    }

    bool AtomicallyPublishFile(const FString& TemporaryPath, const FString& DestinationPath)
    {
#if PLATFORM_WINDOWS
        // The files are siblings on one NTFS volume. MoveFileEx publishes the new
        // complete file as one replace operation without deleting the old path first.
        return ::MoveFileExW(
            *TemporaryPath,
            *DestinationPath,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        // The current simulation target is Win64. This remains a best-effort
        // same-directory fallback for any future unsupported platform port.
        return IFileManager::Get().Move(*DestinationPath, *TemporaryPath, true, true);
#endif
    }
}

ATRIADSensorFusionScenarioManager::ATRIADSensorFusionScenarioManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

FString ATRIADSensorFusionScenarioManager::GetScenarioConfigPath()
{
    FString OverridePath;
    if (FParse::Value(FCommandLine::Get(), TEXT("-TRIADScenarioConfig="), OverridePath) &&
        !OverridePath.IsEmpty())
    {
        return FPaths::ConvertRelativePathToFull(OverridePath);
    }
    return FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("SingaporeSensorFusion.json"));
}

bool ATRIADSensorFusionScenarioManager::LoadScenarioConfig(
    FTRIADSensorFusionScenarioConfig& OutConfig,
    FString& OutError)
{
    const FString ConfigPath = GetScenarioConfigPath();
    if (!IFileManager::Get().FileExists(*ConfigPath))
    {
        OutError = FString::Printf(TEXT("Config file does not exist: %s"), *ConfigPath);
        return false;
    }

    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *ConfigPath))
    {
        OutError = FString::Printf(TEXT("Could not read config file: %s"), *ConfigPath);
        return false;
    }

    FText FailureReason;
    if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonText, &OutConfig, 0, 0, false, &FailureReason))
    {
        OutError = FString::Printf(TEXT("Invalid config '%s': %s"), *ConfigPath, *FailureReason.ToString());
        return false;
    }
    FString PerimeterError;
    if (!TRIAD::Geodesy::ValidatePerimeter(OutConfig.SimulationPerimeter, PerimeterError))
    {
        OutError = FString::Printf(
            TEXT("Invalid SimulationPerimeter in '%s': %s"),
            *ConfigPath,
            *PerimeterError);
        return false;
    }
    return true;
}

bool ATRIADSensorFusionScenarioManager::ResolveDedicatedRFResourcePath(
    const FString& ResourceSpecification,
    FString& OutResolvedPath,
    FString& OutError) const
{
    OutResolvedPath.Reset();
    FString Specification = ResourceSpecification;
    Specification.TrimStartAndEndInline();
    Specification.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (Specification.IsEmpty() || Specification.Contains(TEXT("//")) ||
        !FPaths::IsRelative(Specification))
    {
        OutError = TEXT("Dedicated RF resources must use a relative Plugin/... or Project/... specification.");
        return false;
    }

    FString Root;
    FString Relative;
    if (Specification.StartsWith(TEXT("Plugin/"), ESearchCase::IgnoreCase))
    {
        const TSharedPtr<IPlugin> Plugin =
            IPluginManager::Get().FindPlugin(TEXT("TRIADSensorFusion"));
        if (!Plugin.IsValid())
        {
            OutError = TEXT("Could not resolve the installed TRIADSensorFusion plugin base directory.");
            return false;
        }
        Root = Plugin->GetBaseDir();
        Relative = Specification.RightChop(7);
    }
    else if (Specification.StartsWith(TEXT("Project/"), ESearchCase::IgnoreCase))
    {
        Root = FPaths::ProjectDir();
        Relative = Specification.RightChop(8);
    }
    else
    {
        OutError = TEXT("Dedicated RF resource specification must begin with Plugin/ or Project/.");
        return false;
    }

    TArray<FString> Components;
    Relative.ParseIntoArray(Components, TEXT("/"), false);
    if (Components.IsEmpty())
    {
        OutError = TEXT("Dedicated RF resource specification has no relative file component.");
        return false;
    }
    for (const FString& Component : Components)
    {
        if (Component.IsEmpty() || Component == TEXT(".") || Component == TEXT("..") ||
            Component.Contains(TEXT(":")))
        {
            OutError = TEXT("Dedicated RF resource specification contains an unsafe path component.");
            return false;
        }
    }

    FString NormalizedRoot = FPaths::ConvertRelativePathToFull(Root);
    FString Candidate = FPaths::ConvertRelativePathToFull(FPaths::Combine(Root, Relative));
    FPaths::NormalizeDirectoryName(NormalizedRoot);
    FPaths::NormalizeFilename(Candidate);
    const FString RootPrefix = NormalizedRoot.EndsWith(TEXT("/"))
        ? NormalizedRoot
        : NormalizedRoot + TEXT("/");
    if (!Candidate.StartsWith(RootPrefix, ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Dedicated RF resource resolved outside its permitted root.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*Candidate))
    {
        OutError = FString::Printf(TEXT("Dedicated RF resource does not exist: %s"), *Candidate);
        return false;
    }
    OutResolvedPath = MoveTemp(Candidate);
    return true;
}

bool ATRIADSensorFusionScenarioManager::InitializeDedicatedRFPropagation(FString& OutError)
{
    DedicatedRFGeometryQuery.Reset();
    DedicatedRFInteractionModel.Reset();
    DedicatedRFMetadata = FTRIADRFIndexedGeometryMetadata();
    DedicatedRFGeometrySha256.Reset();
    DedicatedRFMaterialCatalogSha256.Reset();
    DedicatedRFSceneContractSha256.Reset();
    DedicatedRFFailureReason.Reset();
    bDedicatedRFReady = false;
    bDedicatedRFDegraded = false;
    ActiveRFPropagationMode = TEXT("LEGACY_VISIBILITY_BINARY_NLOS");
    DedicatedRFReadiness = TEXT("DEDICATED_RF_DISABLED_LEGACY_MODE");
    OutError.Reset();

    const auto Fail = [this, &OutError](const FString& Reason)
    {
        DedicatedRFGeometryQuery.Reset();
        DedicatedRFInteractionModel.Reset();
        DedicatedRFMetadata = FTRIADRFIndexedGeometryMetadata();
        DedicatedRFGeometrySha256.Reset();
        DedicatedRFMaterialCatalogSha256.Reset();
        DedicatedRFSceneContractSha256.Reset();
        bDedicatedRFReady = false;
        bDedicatedRFDegraded = true;
        ActiveRFPropagationMode = ScenarioConfig.bRequireDedicatedRFReady
            ? TEXT("DEDICATED_RF_REQUIRED_UNAVAILABLE_FAIL_CLOSED")
            : TEXT("LEGACY_VISIBILITY_BINARY_NLOS_DEGRADED");
        DedicatedRFReadiness = ScenarioConfig.bRequireDedicatedRFReady
            ? TEXT("DEDICATED_RF_REQUIRED_LOAD_FAILED_MANAGER_STOPPED")
            : TEXT("DEDICATED_RF_LOAD_FAILED_OPTIONAL_LEGACY_FALLBACK");
        DedicatedRFFailureReason = Reason;
        OutError = Reason;
        return false;
    };

    if (!ScenarioConfig.bUseDedicatedRFPropagation)
    {
        if (ScenarioConfig.bRequireDedicatedRFReady)
        {
            return Fail(TEXT("bRequireDedicatedRFReady cannot be true while dedicated RF propagation is disabled."));
        }
        return true;
    }
    if (!ScenarioConfig.bDedicatedRFFrameIsWorldOriginIdentity)
    {
        return Fail(TEXT(
            "Dedicated RF requires an explicit authored world-frame assertion; unbound arbitrary offsets are unsupported."));
    }
    if (ScenarioConfig.DedicatedRFExpectedWorldPackageName.IsEmpty() || !GetWorld())
    {
        return Fail(TEXT("Dedicated RF requires an exact expected world package name."));
    }
    const FString CurrentWorldPackage = NormalizePIEWorldPackageName(GetWorld()->GetOutermost()->GetName());
    const FString ExpectedWorldPackage = NormalizePIEWorldPackageName(
        ScenarioConfig.DedicatedRFExpectedWorldPackageName);
    if (!CurrentWorldPackage.Equals(ExpectedWorldPackage, ESearchCase::CaseSensitive))
    {
        return Fail(FString::Printf(
            TEXT("Dedicated RF frame is bound to world '%s', but '%s' is loaded."),
            *ExpectedWorldPackage,
            *CurrentWorldPackage));
    }
    if (!IsSha256String(ScenarioConfig.DedicatedRFExpectedGeometrySha256) ||
        !IsSha256String(ScenarioConfig.DedicatedRFExpectedMaterialCatalogSha256) ||
        !IsSha256String(ScenarioConfig.DedicatedRFExpectedSceneContractSha256))
    {
        return Fail(TEXT("Dedicated RF expected SHA-256 values must be complete 64-digit hex strings."));
    }
    if (ScenarioConfig.DedicatedRFExpectedGeometryQueryId.IsEmpty() ||
        ScenarioConfig.DedicatedRFExpectedMaterialCatalogId.IsEmpty() ||
        ScenarioConfig.DedicatedRFExpectedModeledCoverageId.IsEmpty() ||
        ScenarioConfig.DedicatedRFExpectedModeledCoverageScope.IsEmpty())
    {
        return Fail(TEXT("Dedicated RF expected semantic IDs must be non-empty."));
    }

    FString GeometryPath;
    FString CatalogPath;
    FString ContractPath;
    FString ResolveError;
    if (!ResolveDedicatedRFResourcePath(
            ScenarioConfig.DedicatedRFGeometryResourcePath,
            GeometryPath,
            ResolveError))
    {
        return Fail(ResolveError);
    }
    if (!ResolveDedicatedRFResourcePath(
            ScenarioConfig.DedicatedRFMaterialCatalogResourcePath,
            CatalogPath,
            ResolveError))
    {
        return Fail(ResolveError);
    }
    if (!ResolveDedicatedRFResourcePath(
            ScenarioConfig.DedicatedRFSceneContractResourcePath,
            ContractPath,
            ResolveError))
    {
        return Fail(ResolveError);
    }

    constexpr int64 MaximumSceneContractBytes = 1024 * 1024;
    const int64 ContractFileBytes = IFileManager::Get().FileSize(*ContractPath);
    if (ContractFileBytes <= 0 || ContractFileBytes > MaximumSceneContractBytes)
    {
        return Fail(FString::Printf(
            TEXT("Dedicated RF scene contract must contain 1..%lld bytes; '%s' contains %lld."),
            MaximumSceneContractBytes,
            *ContractPath,
            ContractFileBytes));
    }
    FString ContractJson;
    if (!FFileHelper::LoadFileToString(ContractJson, *ContractPath) ||
        ContractJson.IsEmpty())
    {
        return Fail(TEXT("Dedicated RF scene-contract single-read load failed."));
    }
    FString ContractHash;
    FString ContractHashError;
    if (!FTRIADRFIndexedGeometryQuery::ComputeCanonicalJsonSha256(
            ContractJson,
            ContractHash,
            ContractHashError))
    {
        return Fail(FString::Printf(
            TEXT("Dedicated RF scene-contract UTF-8 hash failed: %s"),
            *ContractHashError));
    }
    if (!ContractHash.Equals(
            ScenarioConfig.DedicatedRFExpectedSceneContractSha256,
            ESearchCase::IgnoreCase))
    {
        return Fail(FString::Printf(
            TEXT("Dedicated RF scene-contract hash mismatch: actual=%s expected=%s."),
            *ContractHash,
            *ScenarioConfig.DedicatedRFExpectedSceneContractSha256));
    }

    TUniquePtr<FTRIADRFIndexedGeometryQuery> CandidateQuery =
        MakeUnique<FTRIADRFIndexedGeometryQuery>();
    TUniquePtr<FTRIADDeterministicRFInteractionModel> CandidateModel =
        MakeUnique<FTRIADDeterministicRFInteractionModel>();
    FString LoadError;
    if (!CandidateQuery->LoadFromJsonFiles(
            GeometryPath,
            CatalogPath,
            ScenarioConfig.DedicatedRFExpectedGeometrySha256,
            ScenarioConfig.DedicatedRFExpectedMaterialCatalogSha256,
            LoadError) ||
        !CandidateQuery->IsReady())
    {
        return Fail(FString::Printf(TEXT("Dedicated RF transactional load failed: %s"), *LoadError));
    }

    const FTRIADRFIndexedGeometryMetadata& Metadata = CandidateQuery->GetMetadata();
    if (!Metadata.GeometryQueryId.Equals(
            ScenarioConfig.DedicatedRFExpectedGeometryQueryId,
            ESearchCase::CaseSensitive) ||
        !Metadata.MaterialCatalogId.Equals(
            ScenarioConfig.DedicatedRFExpectedMaterialCatalogId,
            ESearchCase::CaseSensitive) ||
        !Metadata.GeometrySha256.Equals(
            ScenarioConfig.DedicatedRFExpectedGeometrySha256,
            ESearchCase::IgnoreCase) ||
        !Metadata.MaterialCatalogSha256.Equals(
            ScenarioConfig.DedicatedRFExpectedMaterialCatalogSha256,
            ESearchCase::IgnoreCase) ||
        !Metadata.ContractSha256.Equals(
            ContractHash,
            ESearchCase::IgnoreCase) ||
        !Metadata.ModeledCoverageId.Equals(
            ScenarioConfig.DedicatedRFExpectedModeledCoverageId,
            ESearchCase::CaseSensitive) ||
        !Metadata.ModeledCoverageScope.Equals(
            ScenarioConfig.DedicatedRFExpectedModeledCoverageScope,
            ESearchCase::CaseSensitive) ||
        Metadata.bModeledCoverageCoversOneKilometreAoi !=
            ScenarioConfig.bDedicatedRFExpectedCoverageCoversOneKilometreAoi ||
        Metadata.bModeledCoverageCoversSurroundings !=
            ScenarioConfig.bDedicatedRFExpectedCoverageCoversSurroundings)
    {
        return Fail(TEXT("Dedicated RF semantic IDs, independently verified contract, modeled domain, or consumed resource hashes do not match the scenario contract."));
    }

    if (Metadata.bHasClosedWgs84GeodesicCircleStudyDomain)
    {
        const FTRIADSimulationPerimeter& Perimeter = ScenarioConfig.SimulationPerimeter;
        int32 GeoreferenceCount = 0;
        ACesiumGeoreference* UniqueGeoreference = nullptr;
        for (TActorIterator<ACesiumGeoreference> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It))
            {
                ++GeoreferenceCount;
                UniqueGeoreference = *It;
            }
        }
        const FVector GeoreferenceOrigin = Georeference
            ? Georeference->GetOriginLongitudeLatitudeHeight()
            : FVector(std::numeric_limits<double>::quiet_NaN());
        FVector2D ProjectedStudyOrigin;
        FString ProjectionBindingError;
        const bool bProjectedStudyOriginValid =
            TRIAD::Geodesy::Wgs84ToSvy21Meters(
                Metadata.StudyDomainCenterWgs84Degrees.X,
                Metadata.StudyDomainCenterWgs84Degrees.Y,
                ProjectedStudyOrigin,
                ProjectionBindingError);
        if (ScenarioConfig.DedicatedRFExpectedStudyDomainId.IsEmpty() ||
            ScenarioConfig.DedicatedRFExpectedStudyDomainType != TEXT("CLOSED_WGS84_GEODESIC_CIRCLE") ||
            Metadata.StudyDomainId != ScenarioConfig.DedicatedRFExpectedStudyDomainId ||
            Metadata.StudyDomainType != ScenarioConfig.DedicatedRFExpectedStudyDomainType ||
            !Metadata.bStudyDomainConfigurationEnabled ||
            Metadata.StudyDomainConfigurationReferenceName != Perimeter.ReferenceName ||
            Metadata.StudyDomainConfigurationShape != Perimeter.Shape ||
            Metadata.StudyDomainCenterWgs84Degrees.X != ScenarioConfig.DedicatedRFExpectedStudyCenterLongitudeDegrees ||
            Metadata.StudyDomainCenterWgs84Degrees.Y != ScenarioConfig.DedicatedRFExpectedStudyCenterLatitudeDegrees ||
            Metadata.StudyDomainRadiusMeters != ScenarioConfig.DedicatedRFExpectedStudyRadiusMeters ||
            Metadata.StudyDomainPerimeterSampleCount != ScenarioConfig.DedicatedRFExpectedStudyPerimeterSampleCount ||
            Metadata.StudyDomainPerimeterStartAzimuthDegrees != ScenarioConfig.DedicatedRFExpectedStudyPerimeterStartAzimuthDegrees ||
            Metadata.StudyDomainPerimeterStepDegrees != ScenarioConfig.DedicatedRFExpectedStudyPerimeterStepDegrees ||
            Metadata.StudyDomainSourceGeodeticCrs != ScenarioConfig.DedicatedRFExpectedStudySourceGeodeticCrs ||
            Metadata.StudyDomainProjectedConstructionCrs != ScenarioConfig.DedicatedRFExpectedStudyProjectedConstructionCrs ||
            Metadata.StudyDomainLogicalSystem != ScenarioConfig.DedicatedRFExpectedStudyLogicalSystem ||
            ScenarioConfig.DedicatedRFExpectedProjectionId != TEXT("EPSG:3414") ||
            ScenarioConfig.DedicatedRFExpectedProjectedOriginEastingMeters != 29064.15860639389 ||
            ScenarioConfig.DedicatedRFExpectedProjectedOriginNorthingMeters != 32157.571268641685 ||
            !bProjectedStudyOriginValid ||
            !ProjectedStudyOrigin.Equals(
                FVector2D(
                    ScenarioConfig.DedicatedRFExpectedProjectedOriginEastingMeters,
                    ScenarioConfig.DedicatedRFExpectedProjectedOriginNorthingMeters),
                0.0005) ||
            ScenarioConfig.DedicatedRFExpectedGeometryAxisPolicy != TEXT("X_EASTING_DELTA_Y_NEGATED_NORTHING_DELTA") ||
            ScenarioConfig.DedicatedRFExpectedGeometryVerticalPolicy != TEXT("ELLIPSOID_HEIGHT_MINUS_PINNED_ORIGIN_HEIGHT") ||
            GeoreferenceCount != 1 || UniqueGeoreference != Georeference.Get() ||
            !ScenarioConfig.bDedicatedRFExpectedGeoreferenceCartographicOrigin ||
            Georeference->GetOriginPlacement() != EOriginPlacement::CartographicOrigin ||
            !ScenarioConfig.bDedicatedRFExpectedGeoreferenceActorTransformIdentity ||
            !Georeference->GetActorTransform().Equals(FTransform::Identity, 1.0e-9) ||
            GeoreferenceOrigin.Z != ScenarioConfig.DedicatedRFExpectedGeoreferenceOriginHeightMeters ||
            Georeference->GetScale() != ScenarioConfig.DedicatedRFExpectedGeoreferenceScaleCentimetersPerMeter ||
            !Perimeter.bEnabled || !TRIAD::Geodesy::IsCircle(Perimeter) ||
            Perimeter.ReferenceName != Metadata.StudyDomainId ||
            Perimeter.CenterLongitudeDegrees != Metadata.StudyDomainCenterWgs84Degrees.X ||
            Perimeter.CenterLatitudeDegrees != Metadata.StudyDomainCenterWgs84Degrees.Y ||
            Perimeter.RadiusMeters != Metadata.StudyDomainRadiusMeters ||
            !FMath::IsFinite(GeoreferenceOrigin.X) || !FMath::IsFinite(GeoreferenceOrigin.Y) ||
            GeoreferenceOrigin.X != Metadata.StudyDomainCenterWgs84Degrees.X ||
            GeoreferenceOrigin.Y != Metadata.StudyDomainCenterWgs84Degrees.Y ||
            !Metadata.bModeledCoverageCoversOneKilometreAoi ||
            !Metadata.bModeledCoverageCoversSurroundings)
        {
            return Fail(TEXT("Dedicated RF closed WGS84 circle, perimeter, georeference, frame, or coverage flags do not exactly match the scenario-pinned OneKilometreV2 contract."));
        }
    }
    else if (!ScenarioConfig.DedicatedRFExpectedStudyDomainId.IsEmpty() ||
             !ScenarioConfig.DedicatedRFExpectedStudyDomainType.IsEmpty())
    {
        return Fail(TEXT("Dedicated RF scenario requires a study domain but the loaded geometry has none."));
    }

    DedicatedRFMetadata = Metadata;
    DedicatedRFGeometrySha256 = Metadata.GeometrySha256;
    DedicatedRFMaterialCatalogSha256 = Metadata.MaterialCatalogSha256;
    DedicatedRFSceneContractSha256 = ContractHash.ToLower();
    DedicatedRFGeometryQuery = MoveTemp(CandidateQuery);
    DedicatedRFInteractionModel = MoveTemp(CandidateModel);
    bDedicatedRFReady = true;
    bDedicatedRFDegraded = false;
    ActiveRFPropagationMode = Metadata.bHasClosedWgs84GeodesicCircleStudyDomain
        ? TEXT("DEDICATED_RF_INDEXED_GEOMETRY_CLOSED_WGS84_CIRCLE_DIRECT_OR_STRAIGHT_TRANSMISSION_V2")
        : TEXT("DEDICATED_RF_INDEXED_GEOMETRY_DIRECT_OR_STRAIGHT_TRANSMISSION_V1");
    DedicatedRFReadiness = TEXT("SIMULATION_READY_ASSUMPTION_BOUND_RUNTIME_HASH_BOUND_NOT_FIELD_VALIDATED");
    UE_LOG(
        LogTemp,
        Display,
        TEXT("TRIAD dedicated RF ready: %s, %d vertices, %d triangles; coverage=%s (%s, coversOneKilometreAoi=%s, coversSurroundings=%s); simulation assumptions only."),
        *DedicatedRFMetadata.GeometryQueryId,
        DedicatedRFGeometryQuery->GetVertexCount(),
        DedicatedRFGeometryQuery->GetTriangleCount(),
        *DedicatedRFMetadata.ModeledCoverageId,
        *DedicatedRFMetadata.ModeledCoverageScope,
        DedicatedRFMetadata.bModeledCoverageCoversOneKilometreAoi
            ? TEXT("true")
            : TEXT("false"),
        DedicatedRFMetadata.bModeledCoverageCoversSurroundings
            ? TEXT("true")
            : TEXT("false"));
    return true;
}

bool ATRIADSensorFusionScenarioManager::EvaluateDedicatedRFPath(
    const FVector& TransmitterWorldCentimeters,
    const FVector& ReceiverWorldCentimeters,
    double FrequencyGHz,
    FRFPropagationSample& OutSample) const
{
    OutSample = FRFPropagationSample();
    OutSample.bDedicated = true;
    OutSample.bDegraded = bDedicatedRFDegraded;
    OutSample.PropagationMode = ActiveRFPropagationMode;
    OutSample.Readiness = DedicatedRFReadiness;
    if (!bDedicatedRFReady || !DedicatedRFGeometryQuery || !DedicatedRFInteractionModel)
    {
        OutSample.FailureReason = DedicatedRFFailureReason.IsEmpty()
            ? TEXT("DEDICATED_RF_RUNTIME_NOT_READY")
            : DedicatedRFFailureReason;
        return false;
    }

    OutSample.bAoiAdmissionRequired =
        DedicatedRFMetadata.bHasClosedWgs84GeodesicCircleStudyDomain;
    OutSample.AoiAdmissionDomainId = DedicatedRFMetadata.StudyDomainId.Left(256);
    FVector GeometryTransmitterCentimeters = TransmitterWorldCentimeters;
    FVector GeometryReceiverCentimeters = ReceiverWorldCentimeters;
    OutSample.GeometryTransmitterCentimeters = GeometryTransmitterCentimeters;
    OutSample.GeometryReceiverCentimeters = GeometryReceiverCentimeters;
    OutSample.bGeometryFrameTransformValid = true;
    if (OutSample.bAoiAdmissionRequired)
    {
        if (!Georeference ||
            !FMath::IsFinite(TransmitterWorldCentimeters.X) ||
            !FMath::IsFinite(TransmitterWorldCentimeters.Y) ||
            !FMath::IsFinite(TransmitterWorldCentimeters.Z) ||
            !FMath::IsFinite(ReceiverWorldCentimeters.X) ||
            !FMath::IsFinite(ReceiverWorldCentimeters.Y) ||
            !FMath::IsFinite(ReceiverWorldCentimeters.Z))
        {
            OutSample.FailureReason =
                TEXT("DEDICATED_RF_AOI_ADMISSION_FAILED_CLOSED: finite UE endpoints and an exact georeference are required; no direct or legacy fallback is allowed.");
            return false;
        }
        const FVector TransmitterLlh =
            Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(
                TransmitterWorldCentimeters);
        const FVector ReceiverLlh =
            Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(
                ReceiverWorldCentimeters);
        FVector2D TransmitterSvy21;
        FVector2D ReceiverSvy21;
        FString ProjectionError;
        if (!TRIAD::Geodesy::Wgs84ToSvy21Meters(
                TransmitterLlh.X, TransmitterLlh.Y,
                TransmitterSvy21, ProjectionError) ||
            !TRIAD::Geodesy::Wgs84ToSvy21Meters(
                ReceiverLlh.X, ReceiverLlh.Y,
                ReceiverSvy21, ProjectionError))
        {
            OutSample.FailureReason =
                TEXT("DEDICATED_RF_AOI_FRAME_TRANSFORM_FAILED_CLOSED: ") +
                ProjectionError.Left(512);
            return false;
        }
        GeometryTransmitterCentimeters = FVector(
            (TransmitterSvy21.X - ScenarioConfig.DedicatedRFExpectedProjectedOriginEastingMeters) * 100.0,
            -(TransmitterSvy21.Y - ScenarioConfig.DedicatedRFExpectedProjectedOriginNorthingMeters) * 100.0,
            (TransmitterLlh.Z - ScenarioConfig.DedicatedRFExpectedGeoreferenceOriginHeightMeters) * 100.0);
        GeometryReceiverCentimeters = FVector(
            (ReceiverSvy21.X - ScenarioConfig.DedicatedRFExpectedProjectedOriginEastingMeters) * 100.0,
            -(ReceiverSvy21.Y - ScenarioConfig.DedicatedRFExpectedProjectedOriginNorthingMeters) * 100.0,
            (ReceiverLlh.Z - ScenarioConfig.DedicatedRFExpectedGeoreferenceOriginHeightMeters) * 100.0);
        OutSample.GeometryTransmitterCentimeters = GeometryTransmitterCentimeters;
        OutSample.GeometryReceiverCentimeters = GeometryReceiverCentimeters;
        OutSample.bGeometryFrameTransformValid =
            FMath::IsFinite(GeometryTransmitterCentimeters.X) &&
            FMath::IsFinite(GeometryTransmitterCentimeters.Y) &&
            FMath::IsFinite(GeometryTransmitterCentimeters.Z) &&
            FMath::IsFinite(GeometryReceiverCentimeters.X) &&
            FMath::IsFinite(GeometryReceiverCentimeters.Y) &&
            FMath::IsFinite(GeometryReceiverCentimeters.Z);
        if (!OutSample.bGeometryFrameTransformValid)
        {
            OutSample.FailureReason = TEXT("DEDICATED_RF_AOI_FRAME_TRANSFORM_FAILED_CLOSED: transformed geometry endpoints are non-finite.");
            return false;
        }
        FString AdmissionError;
        if (!DedicatedRFGeometryQuery->IsFiniteSegmentWithinAdmittedStudyDomain(
                GeometryTransmitterCentimeters,
                GeometryReceiverCentimeters,
                TransmitterLlh,
                ReceiverLlh,
                OutSample.TransmitterAoiSignedDistanceMeters,
                OutSample.ReceiverAoiSignedDistanceMeters,
                AdmissionError))
        {
            OutSample.FailureReason =
                TEXT("DEDICATED_RF_AOI_ADMISSION_FAILED_CLOSED: ") +
                AdmissionError.Left(512);
            return false;
        }
        OutSample.bAoiEndpointsAdmitted = true;
    }

    TArray<FTRIADRFPathCandidate> Candidates;
    FString QueryError;
    const FTRIADRFModelLimits& Limits = DedicatedRFInteractionModel->GetLimits();
    if (!DedicatedRFGeometryQuery->BuildPathCandidates(
            GeometryTransmitterCentimeters,
            GeometryReceiverCentimeters,
            FrequencyGHz,
            Limits,
            Candidates,
            QueryError))
    {
        OutSample.FailureReason = FString::Printf(
            TEXT("DEDICATED_RF_GEOMETRY_QUERY_FAILED_CLOSED: %s"),
            *QueryError);
        return false;
    }
    if (Candidates.IsEmpty())
    {
        OutSample.FailureReason =
            TEXT("OPAQUE_STRAIGHT_PATH_NO_ADMITTED_DIRECT_OR_TRANSMISSION_CANDIDATE");
        return false;
    }

    TArray<FTRIADRFPathCandidate> AdmittedCandidates;
    AdmittedCandidates.Reserve(Candidates.Num());
    for (FTRIADRFPathCandidate& Candidate : Candidates)
    {
        if (Candidate.Kind == ETRIADRFPathKind::Direct ||
            Candidate.Kind == ETRIADRFPathKind::Transmitted)
        {
            AdmittedCandidates.Add(MoveTemp(Candidate));
        }
    }
    if (AdmittedCandidates.IsEmpty())
    {
        OutSample.FailureReason =
            TEXT("NO_ADMITTED_DIRECT_OR_STRAIGHT_TRANSMISSION_PATH_REFLECTION_IS_NOT_IMPLEMENTED");
        return false;
    }

    TArray<FTRIADRFPathEvaluation> Evaluations;
    FString EvaluationError;
    if (!DedicatedRFInteractionModel->EvaluatePaths(
            AdmittedCandidates,
            FrequencyGHz,
            Evaluations,
            EvaluationError))
    {
        OutSample.FailureReason = FString::Printf(
            TEXT("DEDICATED_RF_PATH_EVALUATION_FAILED_CLOSED: %s"),
            *EvaluationError);
        return false;
    }

    const FTRIADRFPathEvaluation* Best = nullptr;
    FString FirstInvalidEvaluationReason;
    for (const FTRIADRFPathEvaluation& Evaluation : Evaluations)
    {
        if (!Evaluation.bValid || !FMath::IsFinite(Evaluation.TotalPropagationLossDb) ||
            (Evaluation.PathKind != ETRIADRFPathKind::Direct &&
                Evaluation.PathKind != ETRIADRFPathKind::Transmitted))
        {
            if (FirstInvalidEvaluationReason.IsEmpty() && !Evaluation.Error.IsEmpty())
            {
                FirstInvalidEvaluationReason = FString::Printf(
                    TEXT("%s: %s"),
                    *Evaluation.PathId,
                    *Evaluation.Error);
            }
            continue;
        }
        if (!Best ||
            Evaluation.TotalPropagationLossDb < Best->TotalPropagationLossDb - UE_DOUBLE_SMALL_NUMBER ||
            (FMath::IsNearlyEqual(
                    Evaluation.TotalPropagationLossDb,
                    Best->TotalPropagationLossDb,
                    UE_DOUBLE_SMALL_NUMBER) &&
                Evaluation.PathId < Best->PathId))
        {
            Best = &Evaluation;
        }
    }
    if (!Best)
    {
        OutSample.FailureReason = FirstInvalidEvaluationReason.IsEmpty()
            ? TEXT("NO_VALID_DETERMINISTIC_RF_PATH_EVALUATION")
            : FString::Printf(
                TEXT("NO_VALID_DETERMINISTIC_RF_PATH_EVALUATION: %s"),
                *FirstInvalidEvaluationReason);
        return false;
    }

    OutSample.bPathValid = true;
    OutSample.PathEvaluation = *Best;
    return true;
}

void ATRIADSensorFusionScenarioManager::AddRFPropagationTelemetryFields(
    const TSharedRef<FJsonObject>& Json,
    const FRFPropagationSample& Propagation,
    double SystemLossDb,
    double WeatherLossDb) const
{
    Json->SetStringField(TEXT("rfPropagationSchemaVersion"), TEXT("triad.rf_link_propagation.v1"));
    Json->SetStringField(TEXT("propagationMode"), Propagation.PropagationMode);
    Json->SetStringField(TEXT("rfPropagationReadiness"), Propagation.Readiness);
    Json->SetBoolField(TEXT("dedicatedRFRequested"), ScenarioConfig.bUseDedicatedRFPropagation);
    Json->SetBoolField(TEXT("dedicatedRFReady"), bDedicatedRFReady);
    Json->SetBoolField(TEXT("propagationDegraded"), Propagation.bDegraded);
    Json->SetBoolField(TEXT("propagationPathValid"), Propagation.bPathValid);
    Json->SetBoolField(TEXT("rfAoiAdmissionRequired"), Propagation.bAoiAdmissionRequired);
    Json->SetBoolField(TEXT("rfAoiEndpointsAdmitted"), Propagation.bAoiEndpointsAdmitted);
    Json->SetStringField(TEXT("rfAoiAdmissionDomainId"), Propagation.AoiAdmissionDomainId.Left(256));
    Json->SetBoolField(TEXT("rfGeometryFrameTransformValid"), Propagation.bGeometryFrameTransformValid);
    Json->SetStringField(
        TEXT("rfGeometryEndpointFrame"),
        Propagation.bAoiAdmissionRequired
            ? TEXT("EPSG3414_DELTA_EAST_NEGATED_NORTH_HEIGHT_MINUS_ORIGIN_CENTIMETERS")
            : TEXT("TIGHT_V1_WORLD_ORIGIN_IDENTITY_CENTIMETERS"));
    if (Propagation.bGeometryFrameTransformValid)
    {
        Json->SetStringField(TEXT("rfGeometryTransmitterCentimeters"),
            Propagation.GeometryTransmitterCentimeters.ToCompactString().Left(256));
        Json->SetStringField(TEXT("rfGeometryReceiverCentimeters"),
            Propagation.GeometryReceiverCentimeters.ToCompactString().Left(256));
    }
    if (FMath::IsFinite(Propagation.TransmitterAoiSignedDistanceMeters))
    {
        Json->SetNumberField(TEXT("rfTransmitterAoiSignedDistanceMeters"), Propagation.TransmitterAoiSignedDistanceMeters);
    }
    else
    {
        Json->SetField(TEXT("rfTransmitterAoiSignedDistanceMeters"), MakeShared<FJsonValueNull>());
    }
    if (FMath::IsFinite(Propagation.ReceiverAoiSignedDistanceMeters))
    {
        Json->SetNumberField(TEXT("rfReceiverAoiSignedDistanceMeters"), Propagation.ReceiverAoiSignedDistanceMeters);
    }
    else
    {
        Json->SetField(TEXT("rfReceiverAoiSignedDistanceMeters"), MakeShared<FJsonValueNull>());
    }
    Json->SetBoolField(TEXT("visibilityLineOfSightControlsRF"), !Propagation.bDedicated);
    Json->SetBoolField(TEXT("readyForSurveyTruth"), false);
    Json->SetBoolField(TEXT("fieldValidated"), false);
    Json->SetBoolField(
        TEXT("externalAcceptanceContextBound"),
        Propagation.bPathValid && Propagation.PathEvaluation.bExternalAcceptanceContextBound);
    Json->SetStringField(TEXT("rfGeometryQueryId"), DedicatedRFMetadata.GeometryQueryId);
    Json->SetStringField(TEXT("rfGeometryRevision"), DedicatedRFMetadata.GeometryRevision);
    Json->SetStringField(TEXT("rfGeometryStatus"), DedicatedRFMetadata.GeometryStatus);
    Json->SetStringField(TEXT("rfGeometrySchemaVersion"), DedicatedRFMetadata.GeometrySchemaVersion);
    Json->SetStringField(TEXT("rfGeometrySha256"), DedicatedRFGeometrySha256);
    Json->SetStringField(
        TEXT("rfResourceHashSemantics"),
        TEXT("SINGLE_READ_STRICT_UTF8_BUFFERS_HASHED_AND_PARSED_TRANSACTIONALLY"));
    Json->SetStringField(TEXT("rfContractSha256"), DedicatedRFMetadata.ContractSha256);
    Json->SetStringField(
        TEXT("rfVerifiedSceneContractSha256"),
        DedicatedRFSceneContractSha256);
    Json->SetStringField(
        TEXT("rfContractHashSemantics"),
        TEXT("INDEPENDENT_SINGLE_READ_SCENE_CONTRACT_HASH_MATCHES_SCENARIO_AND_GEOMETRY_BINDING"));
    Json->SetStringField(TEXT("rfMaterialCatalogId"), DedicatedRFMetadata.MaterialCatalogId);
    Json->SetStringField(TEXT("rfMaterialCatalogSchemaVersion"), DedicatedRFMetadata.MaterialCatalogSchemaVersion);
    Json->SetStringField(TEXT("rfMaterialCatalogSha256"), DedicatedRFMaterialCatalogSha256);
    Json->SetStringField(TEXT("rfCoordinateSemantics"), DedicatedRFMetadata.CoordinateSemantics);
    Json->SetStringField(TEXT("rfRuntimeSemantics"), DedicatedRFMetadata.RuntimeSemantics);
    Json->SetStringField(TEXT("rfModeledCoverageId"), DedicatedRFMetadata.ModeledCoverageId);
    Json->SetStringField(TEXT("rfModeledCoverageScope"), DedicatedRFMetadata.ModeledCoverageScope);
    Json->SetStringField(TEXT("rfModeledCoverageShape"), DedicatedRFMetadata.ModeledCoverageShape);
    Json->SetStringField(
        TEXT("rfModeledCoverageFiniteSegmentPolicy"),
        DedicatedRFMetadata.ModeledCoverageFiniteSegmentPolicy);
    Json->SetStringField(
        TEXT("rfModeledCoverageOutsideDomainPolicy"),
        DedicatedRFMetadata.ModeledCoverageOutsideDomainPolicy);
    Json->SetBoolField(
        TEXT("rfModeledCoverageCoversOneKilometreAoi"),
        DedicatedRFMetadata.bModeledCoverageCoversOneKilometreAoi);
    Json->SetBoolField(
        TEXT("rfModeledCoverageCoversSurroundings"),
        DedicatedRFMetadata.bModeledCoverageCoversSurroundings);
    Json->SetBoolField(
        TEXT("rfModeledCoverageSurveyControlled"),
        DedicatedRFMetadata.bModeledCoverageSurveyControlled);
    Json->SetBoolField(
        TEXT("rfModeledCoverageFieldValidated"),
        DedicatedRFMetadata.bModeledCoverageFieldValidated);
    Json->SetStringField(TEXT("pathId"), Propagation.PathEvaluation.PathId);
    Json->SetStringField(
        TEXT("pathKind"),
        Propagation.bPathValid
            ? RFPathKindToString(Propagation.PathEvaluation.PathKind)
            : TEXT("NO_ADMITTED_PATH"));
    Json->SetStringField(
        TEXT("materialCalibrationState"),
        RFCalibrationStateToString(Propagation.PathEvaluation.MaterialCalibrationState));
    Json->SetBoolField(
        TEXT("friisFarFieldApplicabilityValidated"),
        Propagation.bPathValid && Propagation.PathEvaluation.bFriisFarFieldApplicabilityValidated);
    Json->SetStringField(TEXT("propagationFailureReason"), Propagation.FailureReason);
    Json->SetNumberField(TEXT("systemLossDb"), SystemLossDb);
    Json->SetNumberField(TEXT("weatherRFLossDb"), WeatherLossDb);

    if (Propagation.bPathValid)
    {
        Json->SetNumberField(TEXT("freeSpacePathLossDb"), Propagation.PathEvaluation.FreeSpacePathLossDb);
        Json->SetNumberField(TEXT("interactionLossDb"), Propagation.PathEvaluation.InteractionLossDb);
        Json->SetNumberField(TEXT("totalPropagationLossDb"), Propagation.PathEvaluation.TotalPropagationLossDb);
        Json->SetNumberField(
            TEXT("totalLinkLossDb"),
            Propagation.PathEvaluation.TotalPropagationLossDb + SystemLossDb + WeatherLossDb);
        Json->SetStringField(TEXT("rfModelSemantics"), Propagation.PathEvaluation.ModelSemantics);
        Json->SetStringField(TEXT("rfReadinessSemantics"), Propagation.PathEvaluation.ReadinessSemantics);
    }
    else
    {
        Json->SetField(TEXT("freeSpacePathLossDb"), MakeShared<FJsonValueNull>());
        Json->SetField(TEXT("interactionLossDb"), MakeShared<FJsonValueNull>());
        Json->SetField(TEXT("totalPropagationLossDb"), MakeShared<FJsonValueNull>());
        Json->SetField(TEXT("totalLinkLossDb"), MakeShared<FJsonValueNull>());
    }

    TArray<TSharedPtr<FJsonValue>> ClearWitnessValues;
    for (const FString& WitnessId : Propagation.PathEvaluation.ClearSegmentWitnessIds)
    {
        ClearWitnessValues.Add(MakeShared<FJsonValueString>(WitnessId));
    }
    Json->SetArrayField(TEXT("rfClearSegmentWitnessIds"), MoveTemp(ClearWitnessValues));

    TArray<TSharedPtr<FJsonValue>> CalibrationProvenanceValues;
    for (const FString& ProvenanceId : Propagation.PathEvaluation.CalibrationProvenanceIds)
    {
        CalibrationProvenanceValues.Add(MakeShared<FJsonValueString>(ProvenanceId));
    }
    Json->SetArrayField(TEXT("rfCalibrationProvenanceIds"), MoveTemp(CalibrationProvenanceValues));

    Json->SetStringField(
        TEXT("rfInteractionTraceSchemaVersion"),
        TEXT("triad.rf_interaction_trace.v3"));
    TArray<TSharedPtr<FJsonValue>> InteractionValues;
    for (const FTRIADRFInteractionEvaluation& Interaction :
        Propagation.PathEvaluation.InteractionEvaluations)
    {
        TSharedRef<FJsonObject> InteractionJson = MakeShared<FJsonObject>();
        InteractionJson->SetStringField(TEXT("kind"), RFInteractionKindToString(Interaction.Kind));
        InteractionJson->SetStringField(TEXT("surfaceId"), Interaction.SurfaceId);
        InteractionJson->SetStringField(TEXT("solidId"), Interaction.SolidId);
        InteractionJson->SetStringField(TEXT("materialId"), Interaction.MaterialId);
        InteractionJson->SetStringField(TEXT("profileId"), Interaction.ProfileId);
        InteractionJson->SetStringField(TEXT("sourceClass"), Interaction.SourceClass);
        InteractionJson->SetStringField(TEXT("uncertaintyClass"), Interaction.UncertaintyClass);
        InteractionJson->SetStringField(
            TEXT("coefficientSelectionSemantics"),
            Interaction.CoefficientSelectionSemantics);
        TArray<TSharedPtr<FJsonValue>> ContributorValues;
        ContributorValues.Reserve(Interaction.Contributors.Num());
        for (const FTRIADRFInteractionContributor& Contributor :
             Interaction.Contributors)
        {
            const auto MakePointJson = [](const FVector& Point)
            {
                TSharedRef<FJsonObject> PointJson = MakeShared<FJsonObject>();
                PointJson->SetNumberField(TEXT("x"), Point.X);
                PointJson->SetNumberField(TEXT("y"), Point.Y);
                PointJson->SetNumberField(TEXT("z"), Point.Z);
                return PointJson;
            };
            TSharedRef<FJsonObject> ContributorJson = MakeShared<FJsonObject>();
            ContributorJson->SetStringField(TEXT("solidId"), Contributor.SolidId);
            ContributorJson->SetStringField(
                TEXT("entrySurfaceId"),
                Contributor.EntrySurfaceId);
            ContributorJson->SetStringField(
                TEXT("exitSurfaceId"),
                Contributor.ExitSurfaceId);
            ContributorJson->SetStringField(
                TEXT("sourceClass"),
                Contributor.SourceClass);
            ContributorJson->SetStringField(
                TEXT("uncertaintyClass"),
                Contributor.UncertaintyClass);
            ContributorJson->SetObjectField(
                TEXT("entryPointCentimeters"),
                MakePointJson(Contributor.EntryPointCentimeters));
            ContributorJson->SetObjectField(
                TEXT("exitPointCentimeters"),
                MakePointJson(Contributor.ExitPointCentimeters));
            ContributorValues.Add(
                MakeShared<FJsonValueObject>(MoveTemp(ContributorJson)));
        }
        InteractionJson->SetArrayField(
            TEXT("contributors"),
            MoveTemp(ContributorValues));
        InteractionJson->SetStringField(
            TEXT("calibrationState"),
            RFCalibrationStateToString(Interaction.CalibrationState));
        InteractionJson->SetStringField(
            TEXT("calibrationProvenanceId"),
            Interaction.CalibrationProvenanceId);
        InteractionJson->SetNumberField(TEXT("segmentIndex"), Interaction.SegmentIndex);
        InteractionJson->SetNumberField(TEXT("vertexIndex"), Interaction.VertexIndex);
        InteractionJson->SetNumberField(
            TEXT("minimumFrequencyGHz"),
            Interaction.MinimumFrequencyGHz);
        InteractionJson->SetNumberField(
            TEXT("maximumFrequencyGHz"),
            Interaction.MaximumFrequencyGHz);
        InteractionJson->SetNumberField(
            TEXT("minimumIncidenceCosine"),
            Interaction.MinimumIncidenceCosine);
        InteractionJson->SetNumberField(
            TEXT("maximumIncidenceCosine"),
            Interaction.MaximumIncidenceCosine);
        InteractionJson->SetNumberField(
            TEXT("pairedBoundaryTransmissionLossDb"),
            Interaction.PairedBoundaryTransmissionLossDb);
        InteractionJson->SetNumberField(
            TEXT("bulkAttenuationDbPerMeter"),
            Interaction.BulkAttenuationDbPerMeter);
        InteractionJson->SetNumberField(
            TEXT("reflectionLossDb"),
            Interaction.ReflectionLossDb);
        InteractionJson->SetNumberField(
            TEXT("empiricalGrazingReflectionLossDb"),
            Interaction.EmpiricalGrazingReflectionLossDb);
        InteractionJson->SetNumberField(TEXT("normalThicknessMeters"), Interaction.NormalThicknessMeters);
        InteractionJson->SetNumberField(TEXT("traversalDistanceMeters"), Interaction.TraversalDistanceMeters);
        InteractionJson->SetNumberField(TEXT("incidenceCosine"), Interaction.IncidenceCosine);
        InteractionJson->SetNumberField(TEXT("lossDb"), Interaction.LossDb);
        InteractionValues.Add(MakeShared<FJsonValueObject>(MoveTemp(InteractionJson)));
    }
    Json->SetArrayField(TEXT("rfInteractions"), MoveTemp(InteractionValues));
}

TSharedRef<FJsonObject> ATRIADSensorFusionScenarioManager::MakeRFPropagationStatusJson() const
{
    TSharedRef<FJsonObject> Status = MakeShared<FJsonObject>();
    Status->SetStringField(TEXT("schemaVersion"), TEXT("triad.rf_propagation_status.v1"));
    Status->SetBoolField(TEXT("dedicatedRFRequested"), ScenarioConfig.bUseDedicatedRFPropagation);
    Status->SetBoolField(TEXT("dedicatedRFRequired"), ScenarioConfig.bRequireDedicatedRFReady);
    Status->SetBoolField(TEXT("dedicatedRFReady"), bDedicatedRFReady);
    Status->SetBoolField(TEXT("degraded"), bDedicatedRFDegraded);
    Status->SetStringField(TEXT("mode"), ActiveRFPropagationMode);
    Status->SetStringField(TEXT("readiness"), DedicatedRFReadiness);
    Status->SetStringField(TEXT("failureReason"), DedicatedRFFailureReason);
    Status->SetStringField(
        TEXT("expectedWorldPackageName"),
        ScenarioConfig.DedicatedRFExpectedWorldPackageName);
    Status->SetBoolField(
        TEXT("worldOriginIdentityFrameAsserted"),
        ScenarioConfig.bDedicatedRFFrameIsWorldOriginIdentity);
    Status->SetStringField(TEXT("geometryQueryId"), DedicatedRFMetadata.GeometryQueryId);
    Status->SetStringField(TEXT("geometryRevision"), DedicatedRFMetadata.GeometryRevision);
    Status->SetStringField(TEXT("geometryStatus"), DedicatedRFMetadata.GeometryStatus);
    Status->SetStringField(TEXT("geometrySchemaVersion"), DedicatedRFMetadata.GeometrySchemaVersion);
    Status->SetStringField(TEXT("geometrySha256"), DedicatedRFGeometrySha256);
    Status->SetStringField(
        TEXT("resourceHashSemantics"),
        TEXT("SINGLE_READ_STRICT_UTF8_BUFFERS_HASHED_AND_PARSED_TRANSACTIONALLY"));
    Status->SetStringField(TEXT("contractSha256"), DedicatedRFMetadata.ContractSha256);
    Status->SetStringField(
        TEXT("verifiedSceneContractSha256"),
        DedicatedRFSceneContractSha256);
    Status->SetStringField(
        TEXT("contractHashSemantics"),
        TEXT("INDEPENDENT_SINGLE_READ_SCENE_CONTRACT_HASH_MATCHES_SCENARIO_AND_GEOMETRY_BINDING"));
    Status->SetStringField(TEXT("materialCatalogId"), DedicatedRFMetadata.MaterialCatalogId);
    Status->SetStringField(TEXT("materialCatalogSchemaVersion"), DedicatedRFMetadata.MaterialCatalogSchemaVersion);
    Status->SetStringField(TEXT("materialCatalogSha256"), DedicatedRFMaterialCatalogSha256);
    Status->SetStringField(TEXT("coordinateSemantics"), DedicatedRFMetadata.CoordinateSemantics);
    Status->SetStringField(TEXT("runtimeSemantics"), DedicatedRFMetadata.RuntimeSemantics);
    Status->SetStringField(TEXT("modeledCoverageId"), DedicatedRFMetadata.ModeledCoverageId);
    Status->SetStringField(TEXT("modeledCoverageScope"), DedicatedRFMetadata.ModeledCoverageScope);
    Status->SetStringField(TEXT("modeledCoverageShape"), DedicatedRFMetadata.ModeledCoverageShape);
    Status->SetStringField(
        TEXT("modeledCoverageFiniteSegmentPolicy"),
        DedicatedRFMetadata.ModeledCoverageFiniteSegmentPolicy);
    Status->SetStringField(
        TEXT("modeledCoverageOutsideDomainPolicy"),
        DedicatedRFMetadata.ModeledCoverageOutsideDomainPolicy);
    Status->SetBoolField(
        TEXT("modeledCoverageCoversOneKilometreAoi"),
        DedicatedRFMetadata.bModeledCoverageCoversOneKilometreAoi);
    Status->SetBoolField(
        TEXT("modeledCoverageCoversSurroundings"),
        DedicatedRFMetadata.bModeledCoverageCoversSurroundings);
    Status->SetStringField(
        TEXT("frameBindingSemantics"),
        TEXT("EXACT_EXPECTED_WORLD_PLUS_EXPLICIT_WORLD_ORIGIN_IDENTITY_ASSERTION_NO_OFFSET_ROTATION_OR_SCALE"));
    Status->SetBoolField(TEXT("readyForSurveyTruth"), false);
    Status->SetBoolField(TEXT("fieldValidated"), false);
    Status->SetBoolField(TEXT("externalAcceptanceContextBound"), false);
    Status->SetBoolField(TEXT("diffractionImplemented"), false);
    Status->SetBoolField(TEXT("phaseImplemented"), false);
    Status->SetBoolField(TEXT("polarizationImplemented"), false);
    Status->SetBoolField(TEXT("reflectionCandidateEnumerationImplemented"), false);
    return Status;
}

void ATRIADSensorFusionScenarioManager::BeginPlay()
{
    Super::BeginPlay();

    FString ConfigError;
    if (!LoadScenarioConfig(ScenarioConfig, ConfigError) || !ScenarioConfig.bEnabled)
    {
        UE_LOG(LogTemp, Warning, TEXT("TRIAD Singapore sensor fusion did not start: %s"), *ConfigError);
        Destroy();
        return;
    }

    Georeference = FindGeoreference();
    if (!Georeference)
    {
        UE_LOG(LogTemp, Error, TEXT("TRIAD Singapore sensor fusion requires an existing CesiumGeoreference actor."));
        Destroy();
        return;
    }

    FString DedicatedRFError;
    if (!InitializeDedicatedRFPropagation(DedicatedRFError))
    {
        if (ScenarioConfig.bRequireDedicatedRFReady)
        {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("TRIAD Singapore sensor fusion stopped because required dedicated RF did not load: %s"),
                *DedicatedRFError);
            Destroy();
            return;
        }
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("TRIAD dedicated RF did not load; using explicit degraded legacy propagation: %s"),
            *DedicatedRFError);
    }

    if (ScenarioConfig.SimulationPerimeter.bEnabled)
    {
        int32 NodesOutsideReference = 0;
        for (const FTRIADGeodeticSensorNode& Node : ScenarioConfig.SensorNodes)
        {
            NodesOutsideReference += ComputeSignedDistanceToSimulationPerimeterMeters(
                Node.LongitudeDegrees,
                Node.LatitudeDegrees) > 0.0 ? 1 : 0;
        }
        const FTRIADSimulationPerimeter& Perimeter = ScenarioConfig.SimulationPerimeter;
        const FString GeometryDescription = TRIAD::Geodesy::IsCircle(Perimeter)
            ? FString::Printf(
                TEXT("WGS84 geodesic circle centered at (%.8f, %.8f), radius %.1f m"),
                Perimeter.CenterLongitudeDegrees,
                Perimeter.CenterLatitudeDegrees,
                Perimeter.RadiusMeters)
            : FString::Printf(
                TEXT("WGS84 rectangle [%.4f, %.4f] lon x [%.4f, %.4f] lat"),
                FMath::Min(Perimeter.MinimumLongitudeDegrees, Perimeter.MaximumLongitudeDegrees),
                FMath::Max(Perimeter.MinimumLongitudeDegrees, Perimeter.MaximumLongitudeDegrees),
                FMath::Min(Perimeter.MinimumLatitudeDegrees, Perimeter.MaximumLatitudeDegrees),
                FMath::Max(Perimeter.MinimumLatitudeDegrees, Perimeter.MaximumLatitudeDegrees));
        const FString PerimeterStatus = FString::Printf(
            TEXT("TRIAD approach reference '%s' is a non-legal %s; %d configured node(s) outside."),
            *Perimeter.ReferenceName,
            *GeometryDescription,
            NodesOutsideReference);
        if (NodesOutsideReference == 0)
        {
            UE_LOG(LogTemp, Display, TEXT("%s"), *PerimeterStatus);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("%s"), *PerimeterStatus);
        }
    }

    InitializeTelemetry();
    SpawnSensorNodes();
    SpawnDemoTargets();
    SpawnOperatorObserver();
    ApplyActiveWeatherProfile();

    if (ScenarioConfig.Weather.bCycleProfiles && ScenarioConfig.Weather.CycleProfiles.Num() > 1)
    {
        WeatherCycleIndex = ScenarioConfig.Weather.CycleProfiles.IndexOfByKey(ScenarioConfig.Weather.Profile);
        if (WeatherCycleIndex == INDEX_NONE)
        {
            WeatherCycleIndex = 0;
        }
        GetWorldTimerManager().SetTimer(
            WeatherCycleTimerHandle,
            this,
            &ATRIADSensorFusionScenarioManager::CycleWeatherProfile,
            FMath::Max(ScenarioConfig.Weather.CycleIntervalSeconds, 1.0f),
            true);
    }

    GetWorldTimerManager().SetTimer(
        SampleTimerHandle,
        this,
        &ATRIADSensorFusionScenarioManager::SampleScenarioNow,
        FMath::Max(ScenarioConfig.SampleCadenceSeconds, 0.01f),
        true,
        0.25f);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("TRIAD Singapore sensor fusion started with %d sensor nodes and %d optional demo targets."),
        SpawnedSensorNodes.Num(),
        SpawnedDemoTargets.Num());
}

void ATRIADSensorFusionScenarioManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(SampleTimerHandle);
    GetWorldTimerManager().ClearTimer(WeatherCycleTimerHandle);
    FlushTelemetryBuffers();
    DedicatedRFInteractionModel.Reset();
    DedicatedRFGeometryQuery.Reset();
    bDedicatedRFReady = false;
    Super::EndPlay(EndPlayReason);
}

void ATRIADSensorFusionScenarioManager::SetWeatherProfile(ETRIADSingaporeWeatherProfile NewProfile)
{
    ScenarioConfig.Weather.Profile = NewProfile;
    ApplyActiveWeatherProfile();
}

void ATRIADSensorFusionScenarioManager::ApplyActiveWeatherProfile()
{
    const FResolvedSingaporeWeather Weather = ResolveSingaporeWeather(ScenarioConfig.Weather);
    ActiveWeatherProfileName = Weather.Name;
    ActiveWeatherRainAmount = Weather.RainAmount;
    ActiveWeatherRoadWetnessAmount = Weather.RoadWetnessAmount;
    ActiveWeatherFogAmount = Weather.FogAmount;
    ActiveWeatherDustAmount = Weather.DustAmount;
    ActiveWeatherWind = Weather.Wind;
    ActiveWeatherRainRateMillimetersPerHour = Weather.RainRateMillimetersPerHour;
    ActiveWeatherVisibilityMeters = Weather.VisibilityMeters;
    ActiveWeatherRFSpecificAttenuationDbPerKmAt2_4GHz = Weather.RFSpecificAttenuationDbPerKmAt2_4GHz;
    ActiveWeatherRFSpecificAttenuationDbPerKmAt5_8GHz = Weather.RFSpecificAttenuationDbPerKmAt5_8GHz;
    bAirSimVisualWeatherApplied = false;

    UWorld* World = GetWorld();
    const bool bMapSuppressesAirSimVisualWeather =
        ATRIADIstanaRuntimePolicyActor::ShouldSuppressAirSimVisualWeather(World);
    if (World && bMapSuppressesAirSimVisualWeather)
    {
        // The Istana runtime map keeps the inherited authored Singapore sky.
        // Do not instantiate the attachment-based AirSim WeatherActor here: in
        // PIE it can obscure the scene and depends on AirSim actors that are not
        // present in ordinary editor viewports.
        UWeatherLib::setWeatherEnabled(World, false);
        bAirSimWeatherActorsVerified = false;
    }
    else if (World && ScenarioConfig.Weather.bApplyAirSimVisualWeather)
    {
        if (!bAirSimWeatherInitialized)
        {
            TArray<AActor*> WeatherAttachmentActors;
            for (ATRIADSensorNodeActor* Node : SpawnedSensorNodes)
            {
                if (IsValid(Node))
                {
                    WeatherAttachmentActors.Add(Node);
                }
            }

            // Also attach weather around any playable camera pawn so the operator view
            // and the fixed sensor captures can see the same scenario conditions.
            TArray<AActor*> Pawns;
            UGameplayStatics::GetAllActorsOfClass(World, APawn::StaticClass(), Pawns);
            for (AActor* Pawn : Pawns)
            {
                WeatherAttachmentActors.AddUnique(Pawn);
            }

            UClass* WeatherActorClass = FSoftClassPath(
                TEXT("AActor'/AirSimTriadRuntime/Weather/WeatherFX/WeatherActor.WeatherActor_C'"))
                .TryLoadClass<AActor>();
            if (WeatherActorClass && WeatherAttachmentActors.Num() > 0)
            {
                UWeatherLib::initWeather(World, WeatherAttachmentActors);
                TArray<AActor*> SpawnedWeatherActors;
                UGameplayStatics::GetAllActorsOfClass(World, WeatherActorClass, SpawnedWeatherActors);
                bAirSimWeatherActorsVerified = SpawnedWeatherActors.Num() >= WeatherAttachmentActors.Num();
            }
            else
            {
                bAirSimWeatherActorsVerified = false;
            }
            bAirSimWeatherInitialized = true;
        }

        UWeatherLib::setWeatherEnabled(World, true);
        UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_RAIN, ActiveWeatherRainAmount);
        UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADWETNESS, ActiveWeatherRoadWetnessAmount);
        UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_FOG, ActiveWeatherFogAmount);
        UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_DUST, ActiveWeatherDustAmount);
        UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_SNOW, 0.0f);
        UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADSNOW, 0.0f);
        UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_MAPLELEAF, 0.0f);
        UWeatherLib::setWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_ROADLEAF, 0.0f);
        UWeatherLib::setWeatherWindDirection(World, ActiveWeatherWind);

        // Read back the material-collection state. This is intentionally stricter
        // than assuming a void API call succeeded.
        bAirSimVisualWeatherApplied = bAirSimWeatherActorsVerified && UWeatherLib::getIsWeatherEnabled(World) &&
            FMath::IsNearlyEqual(
                UWeatherLib::getWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_RAIN),
                ActiveWeatherRainAmount,
                0.001f) &&
            FMath::IsNearlyEqual(
                UWeatherLib::getWeatherParamScalar(World, EWeatherParamScalar::WEATHER_PARAM_SCALAR_FOG),
                ActiveWeatherFogAmount,
                0.001f);
    }
    else if (World && bAirSimWeatherInitialized)
    {
        UWeatherLib::setWeatherEnabled(World, false);
    }

    for (ATRIADSensorNodeActor* Node : SpawnedSensorNodes)
    {
        if (IsValid(Node))
        {
            Node->SetWeatherMetadata(
                ActiveWeatherProfileName,
                bAirSimVisualWeatherApplied,
                ActiveWeatherRainRateMillimetersPerHour,
                ActiveWeatherVisibilityMeters);
        }
    }

    const FString VisualStatus = bAirSimVisualWeatherApplied
        ? TEXT("AirSim visual weather verified")
        : bMapSuppressesAirSimVisualWeather
            ? TEXT("visual weather suppressed by map runtime policy")
        : ScenarioConfig.Weather.bApplyAirSimVisualWeather
            ? TEXT("visual weather NOT verified; metadata/RF profile only")
            : TEXT("metadata/RF profile only by configuration");
    const FString WeatherStatus = FString::Printf(
        TEXT("WEATHER %s | rain %.0f mm/h | visibility %.1f km | %s"),
        *ActiveWeatherProfileName,
        ActiveWeatherRainRateMillimetersPerHour,
        ActiveWeatherVisibilityMeters / 1000.0,
        *VisualStatus);
    UE_LOG(LogTemp, Display, TEXT("TRIAD %s."), *WeatherStatus);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(0x54525758ULL, 8.0f, FColor(120, 210, 255), WeatherStatus);
    }
}

void ATRIADSensorFusionScenarioManager::CycleWeatherProfile()
{
    if (ScenarioConfig.Weather.CycleProfiles.Num() == 0)
    {
        return;
    }
    WeatherCycleIndex = (WeatherCycleIndex + 1) % ScenarioConfig.Weather.CycleProfiles.Num();
    SetWeatherProfile(ScenarioConfig.Weather.CycleProfiles[WeatherCycleIndex]);
}

double ATRIADSensorFusionScenarioManager::GetWeatherSpecificAttenuationDbPerKm(double FrequencyGHz) const
{
    const double Alpha = FMath::Clamp((FrequencyGHz - 2.4) / (5.8 - 2.4), 0.0, 1.0);
    return FMath::Lerp(
        ActiveWeatherRFSpecificAttenuationDbPerKmAt2_4GHz,
        ActiveWeatherRFSpecificAttenuationDbPerKmAt5_8GHz,
        Alpha);
}

void ATRIADSensorFusionScenarioManager::SampleScenarioNow()
{
    // This collection represents only the current sample. It is intentionally
    // independent of the append-only session telemetry streams and their caps.
    CurrentDetectedRFSnapshotLinks.Reset();
    CurrentSearchRadarDetections.Reset();

    TArray<AActor*> Targets;
    DiscoverTargets(Targets);
    UpdateTargetApproachTelemetry(Targets);

    const double RadarSampleSimulationSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    TMap<AActor*, FVector> TargetVelocityWorldCentimetersPerSecond;
    for (AActor* Target : Targets)
    {
        if (!IsValid(Target))
        {
            continue;
        }
        FVector EstimatedVelocity = Target->GetVelocity();
        if (const FPreviousRadarTargetSample* Previous = PreviousRadarTargetSampleByActor.Find(Target->GetName()))
        {
            const double DeltaSeconds = RadarSampleSimulationSeconds - Previous->SimulationSeconds;
            if (DeltaSeconds > UE_DOUBLE_SMALL_NUMBER)
            {
                EstimatedVelocity = (Target->GetActorLocation() - Previous->WorldLocation) / DeltaSeconds;
            }
        }
        TargetVelocityWorldCentimetersPerSecond.Add(Target, EstimatedVelocity);
    }

    for (ATRIADSensorNodeActor* Node : SpawnedSensorNodes)
    {
        if (IsValid(Node))
        {
            Node->BeginRFVisualizationSample();
        }
    }

    // Emitter discovery and alert evaluation deliberately continue after RF telemetry
    // reaches its file/count limits. Alerts are a separate bounded output path.
    TMap<AActor*, UTRIADRFEmitterComponent*> TargetEmitters;
    for (AActor* Target : Targets)
    {
        if (UTRIADRFEmitterComponent* Emitter = FindOrAttachEmitter(Target))
        {
            if (Emitter->IsEmitting())
            {
                TargetEmitters.Add(Target, Emitter);
            }
        }
    }

    // One entry per confirming node. The stored value is the same 3D slant range
    // used by the RF link budget, so alert distance cannot drift from its evidence.
    TMap<AActor*, TMap<FString, double>> ConfirmingNodeDistancesByTarget;
    TMap<AActor*, double> MaximumDetectedSnrByTarget;
    TMap<AActor*, FTRIADOperatorObservedContact> CurrentObserverContactsByTarget;
    for (ATRIADSensorNodeActor* Node : SpawnedSensorNodes)
    {
        if (!IsValid(Node) || !Node->GetNodeDefinition().bEnabled)
        {
            continue;
        }

        const FTRIADGeodeticSensorNode& NodeDefinition = Node->GetNodeDefinition();
        AActor* NearestRFDetectedTarget = nullptr;
        double NearestRFDetectedDistanceSquared = TNumericLimits<double>::Max();
        AActor* SelectedRadarCueTarget = nullptr;
        FString SelectedRadarTrackId;
        double SelectedRadarRangeMeters = 0.0;
        double SelectedRadarBearingDegrees = 0.0;
        double SelectedRadarElevationDegrees = 0.0;
        double SelectedRadarConfidence = -1.0;

        for (AActor* Target : Targets)
        {
            FString RadarTrackId;
            double RadarRangeMeters = 0.0;
            double RadarBearingDegrees = 0.0;
            double RadarElevationDegrees = 0.0;
            double RadarConfidence = 0.0;
            const bool bDetectedBySearchRadar = SampleNodeTargetSearchRadar(
                Node,
                Target,
                TargetVelocityWorldCentimetersPerSecond.FindRef(Target),
                RadarTrackId,
                RadarRangeMeters,
                RadarBearingDegrees,
                RadarElevationDegrees,
                RadarConfidence);
            // Never use noisy measured range to break a symmetric-swarm tie.
            // Confidence is derived from the true analytic geometry; when it is
            // effectively equal, a stable track-id ordering prevents cue flapping
            // from repeatedly resetting the PTZ slew/settle state.
            const bool bStrongerRadarCue = RadarConfidence > SelectedRadarConfidence;
            const bool bStableEqualConfidenceTieBreak =
                FMath::IsNearlyEqual(RadarConfidence, SelectedRadarConfidence, 0.000001) &&
                (SelectedRadarTrackId.IsEmpty() ||
                    RadarTrackId.Compare(SelectedRadarTrackId, ESearchCase::CaseSensitive) < 0);
            if (bDetectedBySearchRadar && (bStrongerRadarCue || bStableEqualConfidenceTieBreak))
            {
                SelectedRadarCueTarget = Target;
                SelectedRadarTrackId = RadarTrackId;
                SelectedRadarRangeMeters = RadarRangeMeters;
                SelectedRadarBearingDegrees = RadarBearingDegrees;
                SelectedRadarElevationDegrees = RadarElevationDegrees;
                SelectedRadarConfidence = RadarConfidence;
            }

            double MaximumDetectedSnrDb = -TNumericLimits<double>::Max();
            double SlantDistanceMeters = -1.0;
            bool bDetectedByNode = false;
            UTRIADRFEmitterComponent* Emitter = TargetEmitters.FindRef(Target);
            if (Emitter)
            {
                bDetectedByNode = SampleNodeTargetLink(
                    Node,
                    Target,
                    Emitter,
                    MaximumDetectedSnrDb,
                    SlantDistanceMeters);
            }
            // Every model-derived RF detection can corroborate an early-warning cue.
            // Authored scenario truth remains available for offline scoring only and
            // must never gate, promote, or suppress operator-facing detection cues.
            if (bDetectedByNode)
            {
                // This simulation uses the detected actor's exact scene position
                // as an idealized RF cue-to-slew location. No RF detection means
                // no camera retarget. Bearing estimation and slew latency are not
                // modeled and must not be inferred from this convenience path.
                if (NodeDefinition.bCaptureCameraFrames && NodeDefinition.bTrackNearestTarget)
                {
                    const double DistanceSquared = FVector::DistSquared(
                        Node->GetActorLocation(),
                        Target->GetActorLocation());
                    if (DistanceSquared < NearestRFDetectedDistanceSquared)
                    {
                        NearestRFDetectedDistanceSquared = DistanceSquared;
                        NearestRFDetectedTarget = Target;
                    }
                }
                ConfirmingNodeDistancesByTarget.FindOrAdd(Target).Add(
                    NodeDefinition.NodeId,
                    SlantDistanceMeters);
                if (double* ExistingMaximum = MaximumDetectedSnrByTarget.Find(Target))
                {
                    *ExistingMaximum = FMath::Max(*ExistingMaximum, MaximumDetectedSnrDb);
                }
                else
                {
                    MaximumDetectedSnrByTarget.Add(Target, MaximumDetectedSnrDb);
                }
            }

            // The operator observer is downstream of positive sensor gates. It may
            // use actor truth to position its explicitly labelled presentation camera,
            // but it cannot create a contact from SpawnedDemoTargets/scenario truth.
            if (bDetectedBySearchRadar || bDetectedByNode)
            {
                FTRIADOperatorObservedContact& Contact = CurrentObserverContactsByTarget.FindOrAdd(Target);
                Contact.TargetActor = Target;
                Contact.ContactId = MakeSensorContactId(Target);
                Contact.TargetActorName = Target->GetName();
                Contact.WeatherProfile = ActiveWeatherProfileName;
                Contact.ObservationSimulationSeconds = RadarSampleSimulationSeconds;
                Contact.bDetectedBySearchRadar = Contact.bDetectedBySearchRadar || bDetectedBySearchRadar;
                Contact.bDetectedByRF = Contact.bDetectedByRF || bDetectedByNode;
                if (bDetectedByNode && FMath::IsFinite(MaximumDetectedSnrDb))
                {
                    Contact.MaximumRFSnrDb = FMath::Max(Contact.MaximumRFSnrDb, MaximumDetectedSnrDb);
                }

                if (const FTargetApproachStatus* Approach = CurrentTargetApproachStatusByActor.Find(Target->GetName()))
                {
                    Contact.IngressCorridorId = Approach->IngressCorridorId;
                    Contact.AirspaceState = Approach->AirspaceState;
                    Contact.DistanceToPerimeterMeters = Approach->DistanceToPerimeterMeters;
                    Contact.ApproachRateMetersPerSecond = Approach->ApproachRateMetersPerSecond;
                    Contact.HeadingDegrees = Approach->HeadingDegrees;
                    Contact.SpeedMetersPerSecond = Approach->SpeedMetersPerSecond;
                    Contact.bOutsideSimulationPerimeter = Approach->bOutsideSimulationPerimeter;
                    Contact.bInboundApproachScenario = Approach->bInboundApproachScenario;
                    // Displayed only as authored exercise context; it never affects
                    // contact creation, ranking, colour, or a fusion decision.
                    Contact.bAuthoredAttackScenario = Approach->bHostileScenarioTruth;
                }

                const double ThisNodeReportedRangeMeters = bDetectedBySearchRadar && bDetectedByNode
                    ? FMath::Min(RadarRangeMeters, SlantDistanceMeters)
                    : bDetectedBySearchRadar
                        ? RadarRangeMeters
                        : SlantDistanceMeters;
                const bool bFirstReportingNode = !Contact.ReportingNode.IsValid();
                const bool bCloserReportingNode =
                    ThisNodeReportedRangeMeters + 0.001 < Contact.ReportedRangeMeters;
                const bool bStableReportingNodeTie =
                    FMath::IsNearlyEqual(ThisNodeReportedRangeMeters, Contact.ReportedRangeMeters, 0.001) &&
                    (Contact.ReportingNodeId.IsEmpty() || NodeDefinition.NodeId < Contact.ReportingNodeId);
                if (bFirstReportingNode || bCloserReportingNode || bStableReportingNodeTie)
                {
                    Contact.ReportingNode = Node;
                    Contact.ReportingNodeId = NodeDefinition.NodeId;
                    Contact.ReportedRangeMeters = FMath::Max(ThisNodeReportedRangeMeters, 0.0);
                    Contact.SensorSummary = bDetectedBySearchRadar && bDetectedByNode
                        ? TEXT("SEARCH RADAR + WIDEBAND RF")
                        : bDetectedBySearchRadar
                            ? TEXT("SEARCH RADAR")
                            : TEXT("WIDEBAND RF");
                }

                if (bDetectedBySearchRadar &&
                    (!Contact.bHasRadarPositionEstimate || RadarConfidence > Contact.RadarConfidence))
                {
                    Contact.RadarTrackId = RadarTrackId;
                    Contact.RadarConfidence = RadarConfidence;
                    const double BearingRadians = FMath::DegreesToRadians(RadarBearingDegrees);
                    const double ElevationRadians = FMath::DegreesToRadians(RadarElevationDegrees);
                    const double CosElevation = FMath::Cos(ElevationRadians);
                    const FVector LocalMeasuredDirection(
                        FMath::Sin(BearingRadians) * CosElevation,
                        -FMath::Cos(BearingRadians) * CosElevation,
                        FMath::Sin(ElevationRadians));
                    const FVector WorldMeasuredDirection =
                        Node->GetActorTransform().TransformVectorNoScale(LocalMeasuredDirection).GetSafeNormal();
                    if (Georeference && !WorldMeasuredDirection.IsNearlyZero())
                    {
                        const FVector EstimatedWorldLocation = Node->GetActorLocation() +
                            WorldMeasuredDirection * RadarRangeMeters * 100.0;
                        const FVector EstimatedLongitudeLatitudeHeight =
                            Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(EstimatedWorldLocation);
                        Contact.EstimatedLongitudeDegrees = EstimatedLongitudeLatitudeHeight.X;
                        Contact.EstimatedLatitudeDegrees = EstimatedLongitudeLatitudeHeight.Y;
                        Contact.EstimatedHeightMeters = EstimatedLongitudeLatitudeHeight.Z;
                        Contact.bHasRadarPositionEstimate = true;
                    }
                }
            }
        }

        if (NearestRFDetectedTarget)
        {
            Node->AimCamerasAtWorldLocation(NearestRFDetectedTarget->GetActorLocation());
        }
        if (SelectedRadarCueTarget)
        {
            Node->CueLongRangeSensors(
                SelectedRadarCueTarget,
                MakeSensorContactId(SelectedRadarCueTarget),
                SelectedRadarTrackId,
                SelectedRadarRangeMeters,
                SelectedRadarBearingDegrees,
                SelectedRadarElevationDegrees,
                SelectedRadarConfidence,
                RadarSampleSimulationSeconds);
            Node->RecordSearchRadarDetection(
                SelectedRadarCueTarget->GetActorLocation(),
                SelectedRadarTrackId,
                SelectedRadarRangeMeters,
                SelectedRadarConfidence,
                NodeDefinition.LongRangeVisualizationSeconds);
        }
        else
        {
            Node->ClearLongRangeCue();
        }
    }

    if (OperatorObserver)
    {
        TArray<FTRIADOperatorObservedContact> ObserverContacts;
        CurrentObserverContactsByTarget.GenerateValueArray(ObserverContacts);
        OperatorObserver->UpdateDetectedContacts(ObserverContacts, RadarSampleSimulationSeconds);
    }

    PreviousRadarTargetSampleByActor.Reset();
    for (AActor* Target : Targets)
    {
        if (!IsValid(Target))
        {
            continue;
        }
        FPreviousRadarTargetSample& Previous = PreviousRadarTargetSampleByActor.FindOrAdd(Target->GetName());
        Previous.WorldLocation = Target->GetActorLocation();
        Previous.SimulationSeconds = RadarSampleSimulationSeconds;
    }

    if (ScenarioConfig.bEnableThreatAlerts)
    {
        const int32 MinimumConfirmingNodes = FMath::Max(ScenarioConfig.AlertMinimumConfirmingNodes, 1);
        for (AActor* Target : Targets)
        {
            UTRIADRFEmitterComponent* Emitter = TargetEmitters.FindRef(Target);
            const TMap<FString, double>* ConfirmingNodeDistances = ConfirmingNodeDistancesByTarget.Find(Target);
            const double* MaximumSnrDb = MaximumDetectedSnrByTarget.Find(Target);
            if (Emitter && ConfirmingNodeDistances && MaximumSnrDb &&
                ConfirmingNodeDistances->Num() >= MinimumConfirmingNodes)
            {
                for (ATRIADSensorNodeActor* Node : SpawnedSensorNodes)
                {
                    if (IsValid(Node) && ConfirmingNodeDistances->Contains(Node->GetNodeDefinition().NodeId))
                    {
                        Node->MarkRFMultinodePreliminaryCue();
                    }
                }
                EmitRFEarlyWarningCue(Target, *ConfirmingNodeDistances, *MaximumSnrDb);
            }
        }
    }

    for (ATRIADSensorNodeActor* Node : SpawnedSensorNodes)
    {
        if (IsValid(Node))
        {
            Node->FinalizeRFVisualizationSample(
                ScenarioConfig.bShowNodeRFStatusOnScreen,
                FMath::Max(ScenarioConfig.SampleCadenceSeconds * 1.75f, 0.5f));
        }
    }

    WriteLatestRFSnapshot();

    // Reopen each output file only once per simulation sample instead of once per link/alert.
    FlushTelemetryBuffers();
}

double ATRIADSensorFusionScenarioManager::ComputeSignedDistanceToSimulationPerimeterMeters(
    double LongitudeDegrees,
    double LatitudeDegrees) const
{
    return TRIAD::Geodesy::SignedDistanceToPerimeterMeters(
        ScenarioConfig.SimulationPerimeter,
        LongitudeDegrees,
        LatitudeDegrees);
}

void ATRIADSensorFusionScenarioManager::UpdateTargetApproachTelemetry(const TArray<AActor*>& Targets)
{
    CurrentTargetApproachStatusByActor.Reset();
    if (!Georeference)
    {
        return;
    }

    const double SimulationSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    constexpr double Wgs84EquatorialRadiusMeters = 6378137.0;
    constexpr double SecondsForAnalyticRateProjection = 1.0;

    for (AActor* Target : Targets)
    {
        if (!IsValid(Target))
        {
            continue;
        }

        const FVector LongitudeLatitudeHeight =
            Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(Target->GetActorLocation());
        FTargetApproachStatus Status;
        Status.TargetActor = Target->GetName();
        Status.LongitudeDegrees = LongitudeLatitudeHeight.X;
        Status.LatitudeDegrees = LongitudeLatitudeHeight.Y;
        Status.HeightMeters = LongitudeLatitudeHeight.Z;
        if (const UTRIADRFEmitterComponent* ExistingEmitter =
                Target->FindComponentByClass<UTRIADRFEmitterComponent>())
        {
            Status.bHostileScenarioTruth = ExistingEmitter->Definition.bHostileScenarioTruth;
        }
        Status.DistanceToPerimeterMeters = ComputeSignedDistanceToSimulationPerimeterMeters(
            Status.LongitudeDegrees,
            Status.LatitudeDegrees);
        Status.bOutsideSimulationPerimeter = Status.DistanceToPerimeterMeters > 0.0;

        if (const ATRIADDemoDroneActor* DemoTarget = Cast<ATRIADDemoDroneActor>(Target))
        {
            Status.IngressCorridorId = DemoTarget->GetIngressCorridorId();
            Status.bInboundApproachScenario = DemoTarget->IsInboundApproachScenarioTarget();
            Status.HeadingDegrees = DemoTarget->GetCurrentHeadingDegrees();
            Status.SpeedMetersPerSecond = DemoTarget->GetCurrentSpeedMetersPerSecond();
            Status.bKinematicsAvailable = true;

            const double HeadingRadians = FMath::DegreesToRadians(Status.HeadingDegrees);
            const double EastMeters = FMath::Sin(HeadingRadians) * Status.SpeedMetersPerSecond * SecondsForAnalyticRateProjection;
            const double NorthMeters = FMath::Cos(HeadingRadians) * Status.SpeedMetersPerSecond * SecondsForAnalyticRateProjection;
            const double LatitudeRadians = FMath::DegreesToRadians(Status.LatitudeDegrees);
            const double FutureLatitude = Status.LatitudeDegrees +
                FMath::RadiansToDegrees(NorthMeters / Wgs84EquatorialRadiusMeters);
            const double FutureLongitude = Status.LongitudeDegrees + FMath::RadiansToDegrees(
                EastMeters /
                (Wgs84EquatorialRadiusMeters * FMath::Max(FMath::Abs(FMath::Cos(LatitudeRadians)), 0.000001)));
            Status.ApproachRateMetersPerSecond = Status.DistanceToPerimeterMeters -
                ComputeSignedDistanceToSimulationPerimeterMeters(FutureLongitude, FutureLatitude);
        }
        else if (const FPreviousTargetApproachSample* Previous =
                     PreviousTargetApproachSampleByActor.Find(Status.TargetActor))
        {
            const double DeltaSeconds = SimulationSeconds - Previous->SimulationSeconds;
            if (DeltaSeconds > 0.001)
            {
                const double MeanLatitudeRadians = FMath::DegreesToRadians(
                    (Previous->LatitudeDegrees + Status.LatitudeDegrees) * 0.5);
                const double EastMeters = FMath::DegreesToRadians(
                    Status.LongitudeDegrees - Previous->LongitudeDegrees) *
                    Wgs84EquatorialRadiusMeters * FMath::Cos(MeanLatitudeRadians);
                const double NorthMeters = FMath::DegreesToRadians(
                    Status.LatitudeDegrees - Previous->LatitudeDegrees) * Wgs84EquatorialRadiusMeters;
                const double HorizontalDistanceMeters = FMath::Sqrt(EastMeters * EastMeters + NorthMeters * NorthMeters);
                Status.SpeedMetersPerSecond = HorizontalDistanceMeters / DeltaSeconds;
                Status.HeadingDegrees = HorizontalDistanceMeters > UE_DOUBLE_SMALL_NUMBER
                    ? FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(EastMeters, NorthMeters)) + 360.0, 360.0)
                    : 0.0;
                Status.ApproachRateMetersPerSecond =
                    (Previous->SignedDistanceToPerimeterMeters - Status.DistanceToPerimeterMeters) / DeltaSeconds;
                Status.bKinematicsAvailable = true;
            }
        }

        if (!ScenarioConfig.SimulationPerimeter.bEnabled)
        {
            Status.AirspaceState = TEXT("NOT_EVALUATED");
        }
        else if (!Status.bOutsideSimulationPerimeter)
        {
            Status.AirspaceState = TEXT("INSIDE");
        }
        else
        {
            const double Deadband = FMath::Max(
                ScenarioConfig.SimulationPerimeter.PhaseRateDeadbandMetersPerSecond,
                0.0);
            Status.AirspaceState = Status.ApproachRateMetersPerSecond > Deadband
                ? TEXT("APPROACHING")
                : Status.ApproachRateMetersPerSecond < -Deadband
                    ? TEXT("DEPARTING")
                    : TEXT("OUTSIDE");
        }

        FPreviousTargetApproachSample& Previous = PreviousTargetApproachSampleByActor.FindOrAdd(Status.TargetActor);
        Previous.LongitudeDegrees = Status.LongitudeDegrees;
        Previous.LatitudeDegrees = Status.LatitudeDegrees;
        Previous.SignedDistanceToPerimeterMeters = Status.DistanceToPerimeterMeters;
        Previous.SimulationSeconds = SimulationSeconds;
        CurrentTargetApproachStatusByActor.Add(Status.TargetActor, MoveTemp(Status));
    }
}

ACesiumGeoreference* ATRIADSensorFusionScenarioManager::FindGeoreference() const
{
    if (const UWorld* World = GetWorld())
    {
        for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
        {
            return *It;
        }
    }
    return nullptr;
}

void ATRIADSensorFusionScenarioManager::SpawnSensorNodes()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FString FramesRoot = FPaths::Combine(TelemetryDirectory, TEXT("frames"));
    for (const FTRIADGeodeticSensorNode& Definition : ScenarioConfig.SensorNodes)
    {
        if (!Definition.bEnabled)
        {
            continue;
        }

        FString RequestedName = FString::Printf(TEXT("TRIAD_Sensor_%s"), *Definition.NodeId);
        RequestedName.ReplaceInline(TEXT(" "), TEXT("_"));
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Name = MakeUniqueObjectName(World, ATRIADSensorNodeActor::StaticClass(), FName(*RequestedName));
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ATRIADSensorNodeActor* Node = World->SpawnActor<ATRIADSensorNodeActor>(
                ATRIADSensorNodeActor::StaticClass(), FTransform::Identity, SpawnParameters))
        {
            Node->ConfigureNode(
                Definition,
                Georeference,
                FramesRoot,
                ScenarioConfig.MaxFramesPerNode,
                ScenarioConfig.MaxFrameBytesPerNode);
            SpawnedSensorNodes.Add(Node);
        }
    }
}

void ATRIADSensorFusionScenarioManager::SpawnDemoTargets()
{
    if (!ScenarioConfig.bSpawnDemoTargets)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    constexpr int32 MaximumDemoTargetActors = 64;
    constexpr double Wgs84EquatorialRadiusMeters = 6378137.0;
    int64 RequestedTargetCount = 0;

    for (const FTRIADDemoTargetDefinition& Definition : ScenarioConfig.DemoTargets)
    {
        if (!Definition.bEnabled)
        {
            continue;
        }

        const int32 DefinitionSpawnCount = FMath::Clamp(Definition.SpawnCount, 1, MaximumDemoTargetActors);
        RequestedTargetCount += FMath::Max<int64>(Definition.SpawnCount, 1);
        const int32 FormationColumns = FMath::Clamp(Definition.FormationColumns, 1, DefinitionSpawnCount);
        const int32 FormationRows = FMath::DivideAndRoundUp(DefinitionSpawnCount, FormationColumns);
        const double FormationSpacingMeters = FMath::Max(Definition.FormationSpacingMeters, 0.0);

        for (int32 FormationIndex = 0;
             FormationIndex < DefinitionSpawnCount && SpawnedDemoTargets.Num() < MaximumDemoTargetActors;
             ++FormationIndex)
        {
            const int32 Row = FormationIndex / FormationColumns;
            const int32 Column = FormationIndex % FormationColumns;
            const int32 FirstIndexInRow = Row * FormationColumns;
            const int32 TargetsInRow = FMath::Min(FormationColumns, DefinitionSpawnCount - FirstIndexInRow);
            const double EastOffsetMeters =
                (static_cast<double>(Column) - static_cast<double>(TargetsInRow - 1) * 0.5) * FormationSpacingMeters;
            const double NorthOffsetMeters =
                (static_cast<double>(Row) - static_cast<double>(FormationRows - 1) * 0.5) * FormationSpacingMeters;

            FTRIADDemoTargetDefinition ExpandedDefinition = Definition;
            ExpandedDefinition.SpawnCount = 1;
            const FString BaseActorName = Definition.ActorName.IsEmpty() ? TEXT("Drone1") : Definition.ActorName;
            ExpandedDefinition.ActorName = DefinitionSpawnCount > 1
                ? FString::Printf(TEXT("%s_%02d"), *BaseActorName, FormationIndex + 1)
                : BaseActorName;

            const double StartLatitudeRadians = FMath::DegreesToRadians(Definition.StartLatitudeDegrees);
            const double CosLatitude = FMath::Max(FMath::Abs(FMath::Cos(StartLatitudeRadians)), 0.000001);
            ExpandedDefinition.StartLatitudeDegrees = Definition.StartLatitudeDegrees +
                FMath::RadiansToDegrees(NorthOffsetMeters / Wgs84EquatorialRadiusMeters);
            ExpandedDefinition.StartLongitudeDegrees = Definition.StartLongitudeDegrees +
                FMath::RadiansToDegrees(EastOffsetMeters / (Wgs84EquatorialRadiusMeters * CosLatitude));

            FString RequestedName = ExpandedDefinition.ActorName;
            RequestedName.ReplaceInline(TEXT(" "), TEXT("_"));
            FActorSpawnParameters SpawnParameters;
            SpawnParameters.Name = MakeUniqueObjectName(World, ATRIADDemoDroneActor::StaticClass(), FName(*RequestedName));
            SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if (ATRIADDemoDroneActor* DemoTarget = World->SpawnActor<ATRIADDemoDroneActor>(
                    ATRIADDemoDroneActor::StaticClass(), FTransform::Identity, SpawnParameters))
            {
                DemoTarget->ConfigureDemoTarget(ExpandedDefinition, Georeference);
                DemoTarget->Tags.AddUnique(ScenarioConfig.TargetActorTag);
                SpawnedDemoTargets.Add(DemoTarget);
            }
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT("TRIAD demo target formation expansion requested %lld actor(s), spawned %d (hard session cap %d)."),
        RequestedTargetCount,
        SpawnedDemoTargets.Num(),
        MaximumDemoTargetActors);
}

void ATRIADSensorFusionScenarioManager::SpawnOperatorObserver()
{
    if (!ScenarioConfig.OperatorObserver.bEnabled || !GetWorld())
    {
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIAD_LiveInboundTrackViewer");
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    OperatorObserver = GetWorld()->SpawnActor<ATRIADOperatorObserverActor>(
        ATRIADOperatorObserverActor::StaticClass(),
        FTransform::Identity,
        SpawnParameters);
    if (OperatorObserver)
    {
        OperatorObserver->InitializeObserver(
            ScenarioConfig.OperatorObserver,
            ScenarioConfig.SimulationPerimeter,
            Georeference,
            SpawnedSensorNodes);
    }
}

void ATRIADSensorFusionScenarioManager::DiscoverTargets(TArray<AActor*>& OutTargets) const
{
    if (const UWorld* World = GetWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Candidate = *It;
            if (IsConfiguredTarget(Candidate))
            {
                OutTargets.Add(Candidate);
            }
        }
    }
}

bool ATRIADSensorFusionScenarioManager::IsConfiguredTarget(const AActor* Actor) const
{
    if (!IsValid(Actor) || Actor == this || Actor->IsA<ATRIADSensorNodeActor>() || Actor->IsA<ACesiumGeoreference>())
    {
        return false;
    }

    const FString ActorName = Actor->GetName();
    const bool bDrone1Match = ScenarioConfig.bAlwaysIncludeDrone1 && ActorName.Contains(TEXT("Drone1"), ESearchCase::IgnoreCase);
    const bool bRegexMatch = MatchesRegex(ScenarioConfig.TargetNameRegex, ActorName);
    const bool bTagMatch = !ScenarioConfig.TargetActorTag.IsNone() && Actor->ActorHasTag(ScenarioConfig.TargetActorTag);
    return bDrone1Match || bRegexMatch || bTagMatch;
}

UTRIADRFEmitterComponent* ATRIADSensorFusionScenarioManager::FindOrAttachEmitter(AActor* Target)
{
    if (!IsValid(Target))
    {
        return nullptr;
    }
    if (UTRIADRFEmitterComponent* Existing = Target->FindComponentByClass<UTRIADRFEmitterComponent>())
    {
        return Existing;
    }
    if (!ScenarioConfig.bAttachDefaultEmitterToDiscoveredTargets)
    {
        return nullptr;
    }

    UTRIADRFEmitterComponent* Emitter = NewObject<UTRIADRFEmitterComponent>(
        Target,
        MakeUniqueObjectName(Target, UTRIADRFEmitterComponent::StaticClass(), FName(TEXT("TRIADRFEmitter"))));
    if (Emitter)
    {
        Emitter->ConfigureEmitter(SelectEmitterDefinition(Target));
        Target->AddInstanceComponent(Emitter);
        Emitter->RegisterComponent();
    }
    return Emitter;
}

const FTRIADRFEmitterDefinition& ATRIADSensorFusionScenarioManager::SelectEmitterDefinition(const AActor* Target) const
{
    const FString ActorName = Target ? Target->GetName() : FString();
    for (const FTRIADRFEmitterDefinition& Profile : ScenarioConfig.RFEmitterProfiles)
    {
        if (!Profile.TargetActorNameRegex.IsEmpty() && MatchesRegex(Profile.TargetActorNameRegex, ActorName))
        {
            return Profile;
        }
    }
    return ScenarioConfig.DefaultEmitter;
}

bool ATRIADSensorFusionScenarioManager::SampleNodeTargetLink(
    ATRIADSensorNodeActor* Node,
    AActor* Target,
    UTRIADRFEmitterComponent* Emitter,
    double& OutMaxDetectedSnrDb,
    double& OutSlantDistanceMeters)
{
    OutMaxDetectedSnrDb = -TNumericLimits<double>::Max();
    OutSlantDistanceMeters = -1.0;
    if (!Node || !Target || !Emitter || !Georeference)
    {
        return false;
    }

    const FVector NodeLocation = Node->GetActorLocation();
    const FVector TargetLocation = Target->GetActorLocation();
    const FVector LinkVector = TargetLocation - NodeLocation;
    const double DistanceMeters = LinkVector.Length() / 100.0;
    OutSlantDistanceMeters = DistanceMeters;
    const FVector LocalLink = Node->GetActorTransform().InverseTransformVectorNoScale(LinkVector);
    const double HorizontalCentimeters = FVector2D(LocalLink.X, LocalLink.Y).Length();
    double AzimuthDegrees = FMath::RadiansToDegrees(FMath::Atan2(LocalLink.X, -LocalLink.Y));
    AzimuthDegrees = FMath::Fmod(AzimuthDegrees + 360.0, 360.0);
    const double ElevationDegrees = FMath::RadiansToDegrees(FMath::Atan2(LocalLink.Z, HorizontalCentimeters));

    FString BlockingActor;
    const bool bLineOfSight = ComputeLineOfSight(Node, Target, BlockingActor);
    const FVector TargetLongitudeLatitudeHeight = Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(TargetLocation);
    const FTRIADGeodeticSensorNode& NodeDefinition = Node->GetNodeDefinition();
    UTRIADSensorNodeComponent* Receiver = Node->SensorNode;
    const double NoiseFloorDbm = Receiver->ComputeNoiseFloorDbm();
    const FString TimestampUtc = FDateTime::UtcNow().ToIso8601();
    const double SimulationSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    bool bAnyFrequencyDetected = false;

    for (const double FrequencyGHz : Emitter->Definition.CenterFrequenciesGHz)
    {
        const bool bFrequencySupported = Receiver->SupportsFrequencyGHz(FrequencyGHz);
        const double WeatherSpecificAttenuationDbPerKm = GetWeatherSpecificAttenuationDbPerKm(FrequencyGHz);
        const double WeatherLossDb = WeatherSpecificAttenuationDbPerKm * (DistanceMeters / 1000.0);
        FRFPropagationSample Propagation;
        if (bDedicatedRFReady)
        {
            // TightV1 remains world-origin/identity. OneKilometreV2 performs
            // its separately pinned WGS84/EPSG:3414 transform inside the
            // dedicated evaluator immediately before the geometry query.
            EvaluateDedicatedRFPath(
                TargetLocation,
                NodeLocation,
                FrequencyGHz,
                Propagation);
        }
        else
        {
            const double LegacyFreeSpacePathLossDb =
                Receiver->ComputeFreeSpacePathLossDb(FrequencyGHz, DistanceMeters);
            const double LegacyObstructionLossDb = bLineOfSight
                ? 0.0
                : FMath::Max(ScenarioConfig.NonLineOfSightAdditionalLossDb, 0.0);
            Propagation.bPathValid = FMath::IsFinite(LegacyFreeSpacePathLossDb);
            Propagation.bDedicated = false;
            Propagation.bDegraded = bDedicatedRFDegraded;
            Propagation.PropagationMode = ActiveRFPropagationMode;
            Propagation.Readiness = DedicatedRFReadiness;
            Propagation.FailureReason = DedicatedRFFailureReason;
            Propagation.PathEvaluation.bValid = Propagation.bPathValid;
            Propagation.PathEvaluation.PathId = bLineOfSight
                ? TEXT("legacy-visibility-direct")
                : TEXT("legacy-visibility-binary-nlos-scalar");
            Propagation.PathEvaluation.PathKind = ETRIADRFPathKind::Direct;
            Propagation.PathEvaluation.MaterialCalibrationState =
                ETRIADRFMaterialCalibrationState::NotApplicable;
            Propagation.PathEvaluation.FrequencyGHz = FrequencyGHz;
            Propagation.PathEvaluation.PathLengthMeters = DistanceMeters;
            Propagation.PathEvaluation.FreeSpacePathLossDb = LegacyFreeSpacePathLossDb;
            Propagation.PathEvaluation.InteractionLossDb = LegacyObstructionLossDb;
            Propagation.PathEvaluation.TotalPropagationLossDb =
                LegacyFreeSpacePathLossDb + LegacyObstructionLossDb;
            Propagation.PathEvaluation.ModelSemantics =
                TEXT("LEGACY_FSPL_PLUS_BINARY_VISIBILITY_NLOS_SCALAR");
            Propagation.PathEvaluation.ReadinessSemantics =
                TEXT("BACKWARDS_COMPATIBLE_GENERIC_SIMULATION_NOT_DEDICATED_RF_GEOMETRY_OR_FIELD_VALIDATED");
        }
        const double FreeSpacePathLossDb = Propagation.bPathValid
            ? Propagation.PathEvaluation.FreeSpacePathLossDb
            : 0.0;
        const double ObstructionLossDb = Propagation.bPathValid
            ? Propagation.PathEvaluation.InteractionLossDb
            : 0.0;
        const double TotalPropagationLossDb = Propagation.bPathValid
            ? Propagation.PathEvaluation.TotalPropagationLossDb
            : 0.0;
        const double ReceivedPowerDbm = Propagation.bPathValid
            ? Emitter->Definition.TransmitPowerDbm +
                Emitter->Definition.TransmitAntennaGainDbi + NodeDefinition.ReceiveAntennaGainDbi -
                TotalPropagationLossDb - NodeDefinition.SystemLossDb - WeatherLossDb
            : -1000.0;
        const double SnrDb = ReceivedPowerDbm - NoiseFloorDbm;
        const bool bInRange = DistanceMeters <= FMath::Max(NodeDefinition.DetectionRangeMeters, 0.0);
        const bool bAboveSensitivity = Propagation.bPathValid &&
            ReceivedPowerDbm >= NodeDefinition.ReceiverSensitivityDbm;
        const bool bLegacyLineOfSightGateSatisfied = Propagation.bDedicated ||
            !ScenarioConfig.bRequireLineOfSightForDetection || bLineOfSight;
        const bool bDetected = Propagation.bPathValid && bFrequencySupported && bInRange &&
            bAboveSensitivity && bLegacyLineOfSightGateSatisfied;
        // Legacy field/config names retain JSON and Blueprint compatibility, but
        // cue evidence is now the sensor-derived RF result alone. Scenario truth
        // is deliberately excluded from operator-cue policy.
        const bool bPreliminaryCueEvidence = bDetected;
        if (bDetected)
        {
            bAnyFrequencyDetected = true;
            OutMaxDetectedSnrDb = FMath::Max(OutMaxDetectedSnrDb, SnrDb);
            Node->RecordRFDetection(
                TargetLocation,
                FrequencyGHz,
                bPreliminaryCueEvidence,
                SnrDb,
                ScenarioConfig.RFLinkVisualizationSeconds,
                ScenarioConfig.bVisualizeRFLinks,
                ScenarioConfig.MaxRFLinksVisualizedPerNode);

            if (ScenarioConfig.bWriteLatestRFSnapshot)
            {
                TSharedPtr<FJsonObject> SnapshotLink = MakeShared<FJsonObject>();
                SnapshotLink->SetStringField(TEXT("timestampUtc"), TimestampUtc);
                SnapshotLink->SetStringField(
                    TEXT("linkId"),
                    FString::Printf(TEXT("%s|%s|%.6f"), *NodeDefinition.NodeId, *Target->GetName(), FrequencyGHz));
                SnapshotLink->SetStringField(TEXT("nodeId"), NodeDefinition.NodeId);
                SnapshotLink->SetStringField(TEXT("contactId"), MakeSensorContactId(Target));
                SnapshotLink->SetStringField(TEXT("targetActor"), Target->GetName());
                SnapshotLink->SetStringField(TEXT("emitterId"), Emitter->Definition.EmitterId);
                SnapshotLink->SetBoolField(TEXT("detected"), true);
                SnapshotLink->SetBoolField(TEXT("hostileScenarioTruth"), Emitter->Definition.bHostileScenarioTruth);
                SnapshotLink->SetBoolField(TEXT("preliminaryCueEvidence"), bPreliminaryCueEvidence);
                SnapshotLink->SetBoolField(TEXT("alertEvidence"), bPreliminaryCueEvidence);
                SnapshotLink->SetNumberField(TEXT("frequencyGHz"), FrequencyGHz);
                SnapshotLink->SetStringField(
                    TEXT("frequencyBandLabel"),
                    FString::Printf(TEXT("%.1f GHz"), FrequencyGHz));
                SnapshotLink->SetNumberField(TEXT("nodeLongitudeDegrees"), NodeDefinition.LongitudeDegrees);
                SnapshotLink->SetNumberField(TEXT("nodeLatitudeDegrees"), NodeDefinition.LatitudeDegrees);
                SnapshotLink->SetNumberField(TEXT("nodeHeightMeters"), NodeDefinition.HeightMeters);
                SnapshotLink->SetNumberField(TEXT("targetLongitudeDegrees"), TargetLongitudeLatitudeHeight.X);
                SnapshotLink->SetNumberField(TEXT("targetLatitudeDegrees"), TargetLongitudeLatitudeHeight.Y);
                SnapshotLink->SetNumberField(TEXT("targetHeightMeters"), TargetLongitudeLatitudeHeight.Z);
                SnapshotLink->SetNumberField(TEXT("slantRangeMeters"), DistanceMeters);
                SnapshotLink->SetStringField(TEXT("rangeSemantics"), TEXT("sensor_to_target_3d_slant_range"));
                SnapshotLink->SetNumberField(TEXT("azimuthDegrees"), AzimuthDegrees);
                SnapshotLink->SetNumberField(TEXT("elevationDegrees"), ElevationDegrees);
                SnapshotLink->SetBoolField(TEXT("lineOfSight"), bLineOfSight);
                SnapshotLink->SetStringField(TEXT("blockingActor"), BlockingActor);
                SnapshotLink->SetNumberField(TEXT("receivedPowerDbm"), ReceivedPowerDbm);
                SnapshotLink->SetNumberField(TEXT("noiseFloorDbm"), NoiseFloorDbm);
                SnapshotLink->SetNumberField(TEXT("snrDb"), SnrDb);
                SnapshotLink->SetNumberField(TEXT("weatherRFLossDb"), WeatherLossDb);
                SnapshotLink->SetStringField(
                    TEXT("lineOfSightSemantics"),
                    Propagation.bDedicated
                        ? TEXT("DIAGNOSTIC_VISIBILITY_TRACE_ONLY_NOT_RF_LOSS_OR_DETECTION_GATE")
                        : TEXT("LEGACY_BINARY_RF_OBSTRUCTION_INPUT"));
                AddRFPropagationTelemetryFields(
                    SnapshotLink.ToSharedRef(),
                    Propagation,
                    NodeDefinition.SystemLossDb,
                    WeatherLossDb);
                if (const FTargetApproachStatus* Approach =
                        CurrentTargetApproachStatusByActor.Find(Target->GetName()))
                {
                    SnapshotLink->SetStringField(TEXT("airspaceState"), Approach->AirspaceState);
                    SnapshotLink->SetNumberField(TEXT("distanceToPerimeterMeters"), Approach->DistanceToPerimeterMeters);
                    SnapshotLink->SetNumberField(TEXT("approachRateMetersPerSecond"), Approach->ApproachRateMetersPerSecond);
                    SnapshotLink->SetNumberField(TEXT("headingDegrees"), Approach->HeadingDegrees);
                    SnapshotLink->SetNumberField(TEXT("speedMetersPerSecond"), Approach->SpeedMetersPerSecond);
                    SnapshotLink->SetBoolField(TEXT("outsideSimulationPerimeter"), Approach->bOutsideSimulationPerimeter);
                    SnapshotLink->SetStringField(TEXT("ingressCorridorId"), Approach->IngressCorridorId);
                }
                CurrentDetectedRFSnapshotLinks.Add(MoveTemp(SnapshotLink));
            }
        }

        // File/count limits only suppress RF record construction; the link result above
        // still participates in camera tracking and multi-node early-warning corroboration.
        if (TelemetryRecordsWritten >= FMath::Max(ScenarioConfig.MaxTelemetryRecords, 1) ||
            (!bJsonlActive && !bCsvActive))
        {
            continue;
        }

        TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
        Record->SetStringField(TEXT("timestampUtc"), TimestampUtc);
        Record->SetNumberField(TEXT("simulationSeconds"), SimulationSeconds);
        Record->SetStringField(TEXT("nodeId"), NodeDefinition.NodeId);
        Record->SetStringField(TEXT("contactId"), MakeSensorContactId(Target));
        Record->SetStringField(TEXT("targetActor"), Target->GetName());
        Record->SetStringField(TEXT("emitterId"), Emitter->Definition.EmitterId);
        Record->SetBoolField(TEXT("hostileScenarioTruth"), Emitter->Definition.bHostileScenarioTruth);
        Record->SetNumberField(TEXT("frequencyGHz"), FrequencyGHz);
        Record->SetNumberField(TEXT("nodeLongitudeDegrees"), NodeDefinition.LongitudeDegrees);
        Record->SetNumberField(TEXT("nodeLatitudeDegrees"), NodeDefinition.LatitudeDegrees);
        Record->SetNumberField(TEXT("nodeHeightMeters"), NodeDefinition.HeightMeters);
        Record->SetNumberField(TEXT("targetLongitudeDegrees"), TargetLongitudeLatitudeHeight.X);
        Record->SetNumberField(TEXT("targetLatitudeDegrees"), TargetLongitudeLatitudeHeight.Y);
        Record->SetNumberField(TEXT("targetHeightMeters"), TargetLongitudeLatitudeHeight.Z);
        Record->SetNumberField(TEXT("distanceMeters"), DistanceMeters);
        Record->SetNumberField(TEXT("azimuthDegrees"), AzimuthDegrees);
        Record->SetNumberField(TEXT("elevationDegrees"), ElevationDegrees);
        Record->SetBoolField(TEXT("lineOfSight"), bLineOfSight);
        Record->SetStringField(TEXT("blockingActor"), BlockingActor);
        Record->SetNumberField(TEXT("obstructionLossDb"), ObstructionLossDb);
        Record->SetStringField(
            TEXT("obstructionLossSemantics"),
            Propagation.bDedicated
                ? TEXT("DEDICATED_PARAMETRIC_MATERIAL_INTERACTION_LOSS_COMPATIBILITY_ALIAS")
                : TEXT("LEGACY_BINARY_VISIBILITY_NLOS_SCALAR"));
        Record->SetStringField(
            TEXT("lineOfSightSemantics"),
            Propagation.bDedicated
                ? TEXT("DIAGNOSTIC_VISIBILITY_TRACE_ONLY_NOT_RF_LOSS_OR_DETECTION_GATE")
                : TEXT("LEGACY_BINARY_RF_OBSTRUCTION_INPUT"));
        Record->SetStringField(TEXT("weatherProfile"), ActiveWeatherProfileName);
        Record->SetBoolField(TEXT("airSimVisualWeatherApplied"), bAirSimVisualWeatherApplied);
        Record->SetNumberField(TEXT("weatherRainRateMillimetersPerHour"), ActiveWeatherRainRateMillimetersPerHour);
        Record->SetNumberField(TEXT("weatherVisibilityMeters"), ActiveWeatherVisibilityMeters);
        Record->SetNumberField(TEXT("weatherRFSpecificAttenuationDbPerKm"), WeatherSpecificAttenuationDbPerKm);
        Record->SetNumberField(TEXT("weatherRFLossDb"), WeatherLossDb);
        Record->SetNumberField(TEXT("receivedPowerDbm"), ReceivedPowerDbm);
        Record->SetNumberField(TEXT("noiseFloorDbm"), NoiseFloorDbm);
        Record->SetNumberField(TEXT("snrDb"), SnrDb);
        Record->SetBoolField(TEXT("frequencySupported"), bFrequencySupported);
        Record->SetBoolField(TEXT("inRange"), bInRange);
        Record->SetBoolField(TEXT("aboveSensitivity"), bAboveSensitivity);
        Record->SetBoolField(TEXT("detected"), bDetected);
        Record->SetBoolField(TEXT("preliminaryCueEvidence"), bPreliminaryCueEvidence);
        Record->SetBoolField(TEXT("alertEvidence"), bPreliminaryCueEvidence);
        AddRFPropagationTelemetryFields(
            Record,
            Propagation,
            NodeDefinition.SystemLossDb,
            WeatherLossDb);

        FString CsvRecord = FString::Printf(
            TEXT("%s,%.6f,%s,%s,%s,%s,%.6f,%.9f,%.9f,%.3f,%.9f,%.9f,%.3f,%.3f,%.3f,%.3f,%s,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%s,%s,%s,%s,%s,%s,%s,%.3f,%.3f,%.6f,%.3f"),
            *EscapeCsv(TimestampUtc),
            SimulationSeconds,
            *EscapeCsv(NodeDefinition.NodeId),
            *EscapeCsv(Target->GetName()),
            *EscapeCsv(Emitter->Definition.EmitterId),
            Emitter->Definition.bHostileScenarioTruth ? TEXT("true") : TEXT("false"),
            FrequencyGHz,
            NodeDefinition.LongitudeDegrees,
            NodeDefinition.LatitudeDegrees,
            NodeDefinition.HeightMeters,
            TargetLongitudeLatitudeHeight.X,
            TargetLongitudeLatitudeHeight.Y,
            TargetLongitudeLatitudeHeight.Z,
            DistanceMeters,
            AzimuthDegrees,
            ElevationDegrees,
            bLineOfSight ? TEXT("true") : TEXT("false"),
            *EscapeCsv(BlockingActor),
            FreeSpacePathLossDb,
            ObstructionLossDb,
            ReceivedPowerDbm,
            NoiseFloorDbm,
            SnrDb,
            bFrequencySupported ? TEXT("true") : TEXT("false"),
            bInRange ? TEXT("true") : TEXT("false"),
            bAboveSensitivity ? TEXT("true") : TEXT("false"),
            bDetected ? TEXT("true") : TEXT("false"),
            bPreliminaryCueEvidence ? TEXT("true") : TEXT("false"),
            *EscapeCsv(ActiveWeatherProfileName),
            bAirSimVisualWeatherApplied ? TEXT("true") : TEXT("false"),
            ActiveWeatherRainRateMillimetersPerHour,
            ActiveWeatherVisibilityMeters,
            WeatherSpecificAttenuationDbPerKm,
            WeatherLossDb);
        const FString TotalPropagationLossCsv = Propagation.bPathValid
            ? FString::Printf(TEXT("%.3f"), Propagation.PathEvaluation.TotalPropagationLossDb)
            : FString();
        const FString InteractionLossCsv = Propagation.bPathValid
            ? FString::Printf(TEXT("%.3f"), Propagation.PathEvaluation.InteractionLossDb)
            : FString();
        const FString TotalLinkLossCsv = Propagation.bPathValid
            ? FString::Printf(
                TEXT("%.3f"),
                Propagation.PathEvaluation.TotalPropagationLossDb +
                    NodeDefinition.SystemLossDb + WeatherLossDb)
            : FString();
        FString RFInteractionTraceJson;
        const TArray<TSharedPtr<FJsonValue>>* RFInteractionTraceValues = nullptr;
        if (Record->TryGetArrayField(
                TEXT("rfInteractions"),
                RFInteractionTraceValues) &&
            RFInteractionTraceValues)
        {
            TSharedRef<
                TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>
                InteractionWriter =
                    TJsonWriterFactory<
                        TCHAR,
                        TCondensedJsonPrintPolicy<TCHAR>>::Create(
                            &RFInteractionTraceJson);
            FJsonSerializer::Serialize(
                *RFInteractionTraceValues,
                InteractionWriter);
        }
        CsvRecord += FString::Printf(
            TEXT(",%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%.3f,%s,%s,%s,%s,%s,%s\n"),
            *EscapeCsv(Propagation.PropagationMode),
            *EscapeCsv(Propagation.Readiness),
            Propagation.bDegraded ? TEXT("true") : TEXT("false"),
            Propagation.bPathValid ? TEXT("true") : TEXT("false"),
            *EscapeCsv(DedicatedRFMetadata.GeometryQueryId),
            *EscapeCsv(DedicatedRFGeometrySha256),
            *EscapeCsv(DedicatedRFMetadata.GeometrySchemaVersion),
            *EscapeCsv(DedicatedRFMetadata.GeometryStatus),
            *EscapeCsv(DedicatedRFMetadata.MaterialCatalogId),
            *EscapeCsv(DedicatedRFMaterialCatalogSha256),
            *EscapeCsv(DedicatedRFMetadata.MaterialCatalogSchemaVersion),
            *EscapeCsv(Propagation.PathEvaluation.PathId),
            *EscapeCsv(
                Propagation.bPathValid
                    ? RFPathKindToString(Propagation.PathEvaluation.PathKind)
                    : TEXT("NO_ADMITTED_PATH")),
            *EscapeCsv(RFCalibrationStateToString(
                Propagation.PathEvaluation.MaterialCalibrationState)),
            *TotalPropagationLossCsv,
            *InteractionLossCsv,
            NodeDefinition.SystemLossDb,
            *TotalLinkLossCsv,
            *EscapeCsv(Propagation.FailureReason),
            TEXT("false"),
            *EscapeCsv(DedicatedRFMetadata.ContractSha256),
            *EscapeCsv(DedicatedRFMetadata.CoordinateSemantics),
            *EscapeCsv(RFInteractionTraceJson));
        AppendTelemetryRecord(Record, CsvRecord);
    }

    return bAnyFrequencyDetected;
}

bool ATRIADSensorFusionScenarioManager::SampleNodeTargetSearchRadar(
    ATRIADSensorNodeActor* Node,
    AActor* Target,
    const FVector& TargetVelocityWorldCentimetersPerSecond,
    FString& OutTrackId,
    double& OutMeasuredRangeMeters,
    double& OutMeasuredBearingDegrees,
    double& OutMeasuredElevationDegrees,
    double& OutConfidence)
{
    OutTrackId.Reset();
    OutMeasuredRangeMeters = 0.0;
    OutMeasuredBearingDegrees = 0.0;
    OutMeasuredElevationDegrees = 0.0;
    OutConfidence = 0.0;
    if (!IsValid(Node) || !IsValid(Target) || !Georeference)
    {
        return false;
    }

    const FTRIADGeodeticSensorNode& Definition = Node->GetNodeDefinition();
    if (!Definition.bEnabled || !Definition.bEnableSearchRadar)
    {
        return false;
    }

    const FVector LinkVector = Target->GetActorLocation() - Node->GetActorLocation();
    const double TrueRangeMeters = LinkVector.Length() / 100.0;
    const FVector LocalLink = Node->GetActorTransform().InverseTransformVectorNoScale(LinkVector);
    const double HorizontalCentimeters = FVector2D(LocalLink.X, LocalLink.Y).Length();
    double TrueBearingDegrees = FMath::RadiansToDegrees(FMath::Atan2(LocalLink.X, -LocalLink.Y));
    TrueBearingDegrees = FMath::Fmod(TrueBearingDegrees + 360.0, 360.0);
    const double TrueElevationDegrees = FMath::RadiansToDegrees(FMath::Atan2(LocalLink.Z, HorizontalCentimeters));
    const FVector LinkDirection = LinkVector.GetSafeNormal();
    const double TrueRadialVelocityMetersPerSecond = FVector::DotProduct(
        TargetVelocityWorldCentimetersPerSecond / 100.0,
        LinkDirection);

    FString BlockingActor;
    const bool bLineOfSight = ComputeLineOfSight(Node, Target, BlockingActor);
    double SimulatedRcsSquareMeters = FMath::Max(
        ScenarioConfig.DefaultSimulatedRadarCrossSectionSquareMeters,
        0.0001);
    if (const ATRIADDemoDroneActor* DemoTarget = Cast<ATRIADDemoDroneActor>(Target))
    {
        SimulatedRcsSquareMeters = DemoTarget->GetSimulatedRadarCrossSectionSquareMeters();
    }

    const double SimulationSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const int64 SampleIndex = FMath::FloorToInt64(
        SimulationSeconds / FMath::Max(static_cast<double>(ScenarioConfig.SampleCadenceSeconds), 0.01));
    const FString NoiseSeedText = FString::Printf(
        TEXT("%s|%s|%lld"),
        *Definition.NodeId,
        *Target->GetName(),
        SampleIndex);
    FTRIADSearchRadarModelInput Input;
    Input.TrueRangeMeters = TrueRangeMeters;
    Input.TrueBearingDegrees = TrueBearingDegrees;
    Input.TrueElevationDegrees = TrueElevationDegrees;
    Input.TrueRadialVelocityMetersPerSecond = TrueRadialVelocityMetersPerSecond;
    Input.RadarCrossSectionSquareMeters = SimulatedRcsSquareMeters;
    Input.RangeEnvelopeMeters = FMath::Max(Definition.SearchRadarRangeMeters, 5000.0);
    Input.ElevationFieldOfRegardDegrees = Definition.SearchRadarElevationFieldOfRegardDegrees;
    Input.WeatherVisibilityMeters = ActiveWeatherVisibilityMeters;
    Input.RainRateMillimetersPerHour = ActiveWeatherRainRateMillimetersPerHour;
    Input.DetectionThreshold = Definition.SearchRadarDetectionThreshold;
    Input.RangeNoiseSigmaMeters = Definition.SearchRadarRangeNoiseSigmaMeters;
    Input.BearingNoiseSigmaDegrees = Definition.SearchRadarBearingNoiseSigmaDegrees;
    Input.ElevationNoiseSigmaDegrees = Definition.SearchRadarElevationNoiseSigmaDegrees;
    Input.RadialVelocityNoiseSigmaMetersPerSecond = Definition.SearchRadarRadialVelocityNoiseSigmaMetersPerSecond;
    Input.bLineOfSight = bLineOfSight;
    Input.DeterministicSeed = FCrc::StrCrc32(*NoiseSeedText);
    const FTRIADSearchRadarModelOutput Output = FTRIADLongRangeSensorModel::EvaluateSearchRadar(Input);

    OutTrackId = MakeSearchRadarTrackId(Target);
    OutMeasuredRangeMeters = Output.MeasuredRangeMeters;
    OutMeasuredBearingDegrees = Output.MeasuredBearingDegrees;
    OutMeasuredElevationDegrees = Output.MeasuredElevationDegrees;
    OutConfidence = Output.Confidence;
    if (!Output.bDetected)
    {
        return false;
    }

    const FVector TargetLongitudeLatitudeHeight =
        Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(Target->GetActorLocation());
    const double MeasuredBearingRadians = FMath::DegreesToRadians(Output.MeasuredBearingDegrees);
    const double MeasuredElevationRadians = FMath::DegreesToRadians(Output.MeasuredElevationDegrees);
    const double MeasuredCosElevation = FMath::Cos(MeasuredElevationRadians);
    const FVector LocalMeasuredDirection(
        FMath::Sin(MeasuredBearingRadians) * MeasuredCosElevation,
        -FMath::Cos(MeasuredBearingRadians) * MeasuredCosElevation,
        FMath::Sin(MeasuredElevationRadians));
    const FVector WorldMeasuredDirection =
        Node->GetActorTransform().TransformVectorNoScale(LocalMeasuredDirection).GetSafeNormal();
    const FVector EstimatedWorldLocation = Node->GetActorLocation() +
        WorldMeasuredDirection * Output.MeasuredRangeMeters * 100.0;
    const FVector EstimatedTargetLongitudeLatitudeHeight =
        Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(EstimatedWorldLocation);
    TSharedPtr<FJsonObject> Detection = MakeShared<FJsonObject>();
    Detection->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Detection->SetNumberField(TEXT("simulationSeconds"), SimulationSeconds);
    Detection->SetStringField(TEXT("kind"), TEXT("SIMULATED_SENSOR_DETECTION"));
    Detection->SetStringField(TEXT("source"), TEXT("ANALYTIC_SEARCH_RADAR"));
    Detection->SetBoolField(TEXT("simulated"), true);
    Detection->SetBoolField(TEXT("calibratedDetector"), false);
    Detection->SetBoolField(TEXT("detectionOnly"), true);
    Detection->SetStringField(TEXT("nodeId"), Definition.NodeId);
    Detection->SetStringField(TEXT("contactId"), MakeSensorContactId(Target));
    Detection->SetStringField(TEXT("sensorId"), Definition.NodeId + TEXT(":SEARCH_RADAR"));
    Detection->SetStringField(TEXT("sensorType"), TEXT("SEARCH_RADAR"));
    Detection->SetStringField(TEXT("trackId"), OutTrackId);
    Detection->SetStringField(TEXT("targetActor"), Target->GetName());
    Detection->SetStringField(TEXT("id"), Definition.NodeId + TEXT(":SEARCH_RADAR|") + OutTrackId);
    Detection->SetNumberField(TEXT("nodeLongitudeDegrees"), Definition.LongitudeDegrees);
    Detection->SetNumberField(TEXT("nodeLatitudeDegrees"), Definition.LatitudeDegrees);
    Detection->SetNumberField(TEXT("nodeHeightMeters"), Definition.HeightMeters);
    Detection->SetNumberField(TEXT("targetLongitudeDegrees"), TargetLongitudeLatitudeHeight.X);
    Detection->SetNumberField(TEXT("targetLatitudeDegrees"), TargetLongitudeLatitudeHeight.Y);
    Detection->SetNumberField(TEXT("targetHeightMeters"), TargetLongitudeLatitudeHeight.Z);
    Detection->SetNumberField(TEXT("estimatedTargetLongitudeDegrees"), EstimatedTargetLongitudeLatitudeHeight.X);
    Detection->SetNumberField(TEXT("estimatedTargetLatitudeDegrees"), EstimatedTargetLongitudeLatitudeHeight.Y);
    Detection->SetNumberField(TEXT("estimatedTargetHeightMeters"), EstimatedTargetLongitudeLatitudeHeight.Z);
    Detection->SetStringField(TEXT("positionEstimateSource"), TEXT("SEARCH_RADAR_MEASURED_POLAR"));
    Detection->SetStringField(
        TEXT("targetPositionSemantics"),
        TEXT("targetLongitude/Latitude/Height are evaluator truth; estimatedTarget fields are operator measurement-derived"));
    Detection->SetNumberField(TEXT("rangeMeters"), Output.MeasuredRangeMeters);
    Detection->SetNumberField(TEXT("slantRangeMeters"), Output.MeasuredRangeMeters);
    Detection->SetStringField(TEXT("rangeSemantics"), TEXT("noisy_sensor_to_target_3d_slant_range"));
    Detection->SetNumberField(TEXT("bearingDegrees"), Output.MeasuredBearingDegrees);
    Detection->SetNumberField(TEXT("azimuthDegrees"), Output.MeasuredBearingDegrees);
    Detection->SetNumberField(TEXT("elevationDegrees"), Output.MeasuredElevationDegrees);
    Detection->SetNumberField(TEXT("radialVelocityMetersPerSecond"), Output.MeasuredRadialVelocityMetersPerSecond);
    Detection->SetNumberField(TEXT("confidence"), Output.Confidence);
    Detection->SetStringField(
        TEXT("confidenceSemantics"),
        TEXT("uncalibrated_analytic_score_from_range_LOS_authored_RCS_and_weather"));
    Detection->SetNumberField(TEXT("radarCrossSectionSquareMeters"), SimulatedRcsSquareMeters);
    Detection->SetStringField(
        TEXT("radarCrossSectionSemantics"),
        TEXT("authored_demo_target_or_default_simulation_input_not_measured"));
    Detection->SetBoolField(TEXT("lineOfSight"), bLineOfSight);
    Detection->SetStringField(TEXT("blockingActor"), BlockingActor);
    Detection->SetStringField(TEXT("weatherProfile"), ActiveWeatherProfileName);
    Detection->SetNumberField(TEXT("weatherVisibilityMeters"), ActiveWeatherVisibilityMeters);
    Detection->SetNumberField(TEXT("weatherRainRateMillimetersPerHour"), ActiveWeatherRainRateMillimetersPerHour);
    Detection->SetNumberField(TEXT("rangeEnvelopeMeters"), Input.RangeEnvelopeMeters);
    Detection->SetNumberField(TEXT("azimuthFieldOfRegardDegrees"), 360.0);
    Detection->SetNumberField(
        TEXT("elevationFieldOfRegardDegrees"),
        FMath::Clamp(Definition.SearchRadarElevationFieldOfRegardDegrees, 1.0, 180.0));
    Detection->SetStringField(
        TEXT("measurementNoiseModel"),
        TEXT("deterministic_seeded_gaussian_range_bearing_elevation_radial_velocity"));
    CurrentSearchRadarDetections.Add(MoveTemp(Detection));
    return true;
}

bool ATRIADSensorFusionScenarioManager::ComputeLineOfSight(
    const ATRIADSensorNodeActor* Node,
    const AActor* Target,
    FString& OutBlockingActor) const
{
    const UWorld* World = GetWorld();
    if (!World || !Node || !Target)
    {
        return false;
    }

    FCollisionQueryParams QueryParameters(SCENE_QUERY_STAT(TRIADSensorFusionLOS), ScenarioConfig.bTraceComplex);
    QueryParameters.AddIgnoredActor(this);
    QueryParameters.AddIgnoredActor(Node);

    FHitResult Hit;
    const int32 MaximumChannel =
        static_cast<int32>(ECC_GameTraceChannel18);
    const ECollisionChannel TraceChannel = static_cast<ECollisionChannel>(
        FMath::Clamp(ScenarioConfig.LineOfSightTraceChannel, 0, MaximumChannel));
    const bool bHit = World->LineTraceSingleByChannel(
        Hit,
        Node->GetActorLocation(),
        Target->GetActorLocation(),
        TraceChannel,
        QueryParameters);
    if (!bHit || Hit.GetActor() == Target)
    {
        return true;
    }

    OutBlockingActor = Hit.GetActor() ? Hit.GetActor()->GetName() : Hit.Component.IsValid() ? Hit.Component->GetName() : TEXT("UnknownGeometry");
    return false;
}

void ATRIADSensorFusionScenarioManager::WriteLatestRFSnapshot()
{
    if (!ScenarioConfig.bWriteLatestRFSnapshot || LatestRFSnapshotPath.IsEmpty())
    {
        return;
    }

    struct FNodeSampleStatus
    {
        int32 DetectedLinkCount = 0;
        int32 PreliminaryCueEvidenceLinkCount = 0;
        double StrongestSnrDb = -TNumericLimits<double>::Max();
        TSet<FString> DetectedTargetActors;
    };

    struct FTrackSampleStatus
    {
        FString TargetActor;
        FString EmitterId;
        bool bHostileScenarioTruth = false;
        int32 DetectedLinkCount = 0;
        int32 PreliminaryCueEvidenceLinkCount = 0;
        double StrongestSnrDb = -TNumericLimits<double>::Max();
        double NearestSlantRangeMeters = TNumericLimits<double>::Max();
        double TargetLongitudeDegrees = 0.0;
        double TargetLatitudeDegrees = 0.0;
        double TargetHeightMeters = 0.0;
        TSet<FString> ConfirmingNodeIds;
        TSet<FString> PreliminaryCueEvidenceNodeIds;
        TArray<double> DetectedFrequenciesGHz;
    };

    TMap<FString, FNodeSampleStatus> NodeStatusById;
    TMap<FString, FTrackSampleStatus> TrackStatusByTarget;

    for (const TSharedPtr<FJsonObject>& Link : CurrentDetectedRFSnapshotLinks)
    {
        if (!Link.IsValid())
        {
            continue;
        }

        const FString NodeId = Link->GetStringField(TEXT("nodeId"));
        const FString TargetActor = Link->GetStringField(TEXT("targetActor"));
        const FString EmitterId = Link->GetStringField(TEXT("emitterId"));
        const double SnrDb = Link->GetNumberField(TEXT("snrDb"));
        const double SlantRangeMeters = Link->GetNumberField(TEXT("slantRangeMeters"));
        const double FrequencyGHz = Link->GetNumberField(TEXT("frequencyGHz"));
        const bool bHostileScenarioTruth = Link->GetBoolField(TEXT("hostileScenarioTruth"));
        const bool bPreliminaryCueEvidence = Link->HasTypedField<EJson::Boolean>(TEXT("preliminaryCueEvidence"))
            ? Link->GetBoolField(TEXT("preliminaryCueEvidence"))
            : Link->GetBoolField(TEXT("alertEvidence"));

        FNodeSampleStatus& NodeStatus = NodeStatusById.FindOrAdd(NodeId);
        ++NodeStatus.DetectedLinkCount;
        NodeStatus.PreliminaryCueEvidenceLinkCount += bPreliminaryCueEvidence ? 1 : 0;
        NodeStatus.StrongestSnrDb = FMath::Max(NodeStatus.StrongestSnrDb, SnrDb);
        NodeStatus.DetectedTargetActors.Add(TargetActor);

        FTrackSampleStatus& TrackStatus = TrackStatusByTarget.FindOrAdd(TargetActor);
        if (TrackStatus.TargetActor.IsEmpty())
        {
            TrackStatus.TargetActor = TargetActor;
            TrackStatus.EmitterId = EmitterId;
            TrackStatus.TargetLongitudeDegrees = Link->GetNumberField(TEXT("targetLongitudeDegrees"));
            TrackStatus.TargetLatitudeDegrees = Link->GetNumberField(TEXT("targetLatitudeDegrees"));
            TrackStatus.TargetHeightMeters = Link->GetNumberField(TEXT("targetHeightMeters"));
        }
        TrackStatus.bHostileScenarioTruth = TrackStatus.bHostileScenarioTruth || bHostileScenarioTruth;
        ++TrackStatus.DetectedLinkCount;
        TrackStatus.PreliminaryCueEvidenceLinkCount += bPreliminaryCueEvidence ? 1 : 0;
        TrackStatus.StrongestSnrDb = FMath::Max(TrackStatus.StrongestSnrDb, SnrDb);
        TrackStatus.NearestSlantRangeMeters = FMath::Min(TrackStatus.NearestSlantRangeMeters, SlantRangeMeters);
        TrackStatus.ConfirmingNodeIds.Add(NodeId);
        if (bPreliminaryCueEvidence)
        {
            TrackStatus.PreliminaryCueEvidenceNodeIds.Add(NodeId);
        }
        TrackStatus.DetectedFrequenciesGHz.AddUnique(FrequencyGHz);
    }

    TArray<TSharedPtr<FJsonObject>> StrongestLinks = CurrentDetectedRFSnapshotLinks;
    StrongestLinks.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
    {
        if (!A.IsValid())
        {
            return false;
        }
        if (!B.IsValid())
        {
            return true;
        }
        const double ASnrDb = A->GetNumberField(TEXT("snrDb"));
        const double BSnrDb = B->GetNumberField(TEXT("snrDb"));
        if (!FMath::IsNearlyEqual(ASnrDb, BSnrDb, 0.000001))
        {
            return ASnrDb > BSnrDb;
        }
        return A->GetNumberField(TEXT("slantRangeMeters")) < B->GetNumberField(TEXT("slantRangeMeters"));
    });

    const int32 LinkCap = FMath::Clamp(ScenarioConfig.MaxLiveSnapshotDetectedLinks, 1, 4096);
    const int32 RetainedLinkCount = FMath::Min(StrongestLinks.Num(), LinkCap);
    TArray<TSharedPtr<FJsonValue>> LinkValues;
    LinkValues.Reserve(RetainedLinkCount);
    for (int32 Index = 0; Index < RetainedLinkCount; ++Index)
    {
        LinkValues.Add(MakeShared<FJsonValueObject>(StrongestLinks[Index]));
    }

    TArray<TSharedPtr<FJsonObject>> StrongestRadarDetections = CurrentSearchRadarDetections;
    StrongestRadarDetections.Sort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
    {
        if (!A.IsValid())
        {
            return false;
        }
        if (!B.IsValid())
        {
            return true;
        }
        const double AConfidence = A->GetNumberField(TEXT("confidence"));
        const double BConfidence = B->GetNumberField(TEXT("confidence"));
        if (!FMath::IsNearlyEqual(AConfidence, BConfidence, 0.000001))
        {
            return AConfidence > BConfidence;
        }
        return A->GetNumberField(TEXT("rangeMeters")) < B->GetNumberField(TEXT("rangeMeters"));
    });
    const int32 RadarDetectionCap = FMath::Clamp(
        ScenarioConfig.MaxLiveSnapshotSearchRadarDetections,
        1,
        4096);
    const int32 RetainedRadarDetectionCount = FMath::Min(
        StrongestRadarDetections.Num(),
        RadarDetectionCap);
    TArray<TSharedPtr<FJsonValue>> RadarDetectionValues;
    RadarDetectionValues.Reserve(RetainedRadarDetectionCount);
    for (int32 Index = 0; Index < RetainedRadarDetectionCount; ++Index)
    {
        RadarDetectionValues.Add(MakeShared<FJsonValueObject>(StrongestRadarDetections[Index]));
    }

    TSet<FString> SpawnedNodeIds;
    for (const ATRIADSensorNodeActor* Node : SpawnedSensorNodes)
    {
        if (IsValid(Node))
        {
            SpawnedNodeIds.Add(Node->GetNodeDefinition().NodeId);
        }
    }

    TMap<FString, int32> RadarDetectionCountByNode;
    for (const TSharedPtr<FJsonObject>& Detection : CurrentSearchRadarDetections)
    {
        if (Detection.IsValid())
        {
            ++RadarDetectionCountByNode.FindOrAdd(Detection->GetStringField(TEXT("nodeId")));
        }
    }

    TArray<TSharedPtr<FJsonValue>> NodeValues;
    NodeValues.Reserve(ScenarioConfig.SensorNodes.Num());
    for (const FTRIADGeodeticSensorNode& NodeDefinition : ScenarioConfig.SensorNodes)
    {
        const FNodeSampleStatus* SampleStatus = NodeStatusById.Find(NodeDefinition.NodeId);
        TArray<TSharedPtr<FJsonValue>> FrequencyValues;
        FrequencyValues.Reserve(NodeDefinition.SupportedFrequenciesGHz.Num());
        for (const double FrequencyGHz : NodeDefinition.SupportedFrequenciesGHz)
        {
            FrequencyValues.Add(MakeShared<FJsonValueNumber>(FrequencyGHz));
        }

        TSharedPtr<FJsonObject> NodeJson = MakeShared<FJsonObject>();
        NodeJson->SetStringField(TEXT("nodeId"), NodeDefinition.NodeId);
        NodeJson->SetBoolField(TEXT("enabled"), NodeDefinition.bEnabled);
        NodeJson->SetBoolField(TEXT("spawned"), SpawnedNodeIds.Contains(NodeDefinition.NodeId));
        NodeJson->SetStringField(
            TEXT("runtimeStatus"),
            !NodeDefinition.bEnabled ? TEXT("DISABLED") :
                SpawnedNodeIds.Contains(NodeDefinition.NodeId) ? TEXT("ONLINE") : TEXT("NOT_SPAWNED"));
        NodeJson->SetNumberField(TEXT("longitudeDegrees"), NodeDefinition.LongitudeDegrees);
        NodeJson->SetNumberField(TEXT("latitudeDegrees"), NodeDefinition.LatitudeDegrees);
        NodeJson->SetNumberField(TEXT("heightMeters"), NodeDefinition.HeightMeters);
        NodeJson->SetNumberField(TEXT("detectionRangeMeters"), NodeDefinition.DetectionRangeMeters);
        NodeJson->SetArrayField(TEXT("supportedFrequenciesGHz"), FrequencyValues);
        NodeJson->SetNumberField(TEXT("receiverSensitivityDbm"), NodeDefinition.ReceiverSensitivityDbm);
        NodeJson->SetNumberField(TEXT("receiveAntennaGainDbi"), NodeDefinition.ReceiveAntennaGainDbi);
        NodeJson->SetBoolField(TEXT("cameraCaptureConfigured"), NodeDefinition.bCaptureCameraFrames);
        const bool bNodeSpawned = SpawnedNodeIds.Contains(NodeDefinition.NodeId);
        const FString SearchRadarRuntimeStatus = !NodeDefinition.bEnableSearchRadar ? TEXT("DISABLED") :
            bNodeSpawned ? TEXT("ONLINE") : TEXT("NOT_SPAWNED");
        const FString EOPTZRuntimeStatus = !NodeDefinition.bEnableEOPTZ ? TEXT("DISABLED") :
            bNodeSpawned ? TEXT("ONLINE") : TEXT("NOT_SPAWNED");
        const FString ThermalPTZRuntimeStatus = !NodeDefinition.bEnableThermalPTZ ? TEXT("DISABLED") :
            bNodeSpawned ? TEXT("ONLINE") : TEXT("NOT_SPAWNED");
        NodeJson->SetBoolField(TEXT("searchRadarConfigured"), NodeDefinition.bEnableSearchRadar);
        NodeJson->SetNumberField(
            TEXT("searchRadarRangeMeters"),
            NodeDefinition.bEnableSearchRadar ? FMath::Max(NodeDefinition.SearchRadarRangeMeters, 5000.0) : 0.0);
        NodeJson->SetStringField(TEXT("searchRadarRuntimeStatus"), SearchRadarRuntimeStatus);
        NodeJson->SetNumberField(
            TEXT("currentSearchRadarDetectionCount"),
            RadarDetectionCountByNode.FindRef(NodeDefinition.NodeId));
        NodeJson->SetBoolField(TEXT("eoPtzConfigured"), NodeDefinition.bEnableEOPTZ);
        NodeJson->SetNumberField(TEXT("eoPtzConfirmationRangeMeters"), NodeDefinition.EOPTZConfirmationRangeMeters);
        NodeJson->SetStringField(TEXT("eoPtzRuntimeStatus"), EOPTZRuntimeStatus);
        NodeJson->SetBoolField(TEXT("thermalPtzConfigured"), NodeDefinition.bEnableThermalPTZ);
        NodeJson->SetNumberField(
            TEXT("thermalPtzConfirmationRangeMeters"),
            NodeDefinition.ThermalPTZConfirmationRangeMeters);
        NodeJson->SetStringField(TEXT("thermalPtzRuntimeStatus"), ThermalPTZRuntimeStatus);

        TArray<TSharedPtr<FJsonValue>> LongRangeSensorStack;
        auto AddLongRangeSensor = [&LongRangeSensorStack, &NodeDefinition](
            const FString& Suffix,
            const FString& Type,
            bool bEnabled,
            const FString& RuntimeStatus,
            double RangeMeters,
            double FieldOfViewDegrees)
        {
            TSharedRef<FJsonObject> Sensor = MakeShared<FJsonObject>();
            Sensor->SetStringField(TEXT("sensorId"), NodeDefinition.NodeId + Suffix);
            Sensor->SetStringField(TEXT("sensorType"), Type);
            Sensor->SetBoolField(TEXT("simulated"), true);
            Sensor->SetBoolField(TEXT("enabled"), bEnabled);
            Sensor->SetStringField(TEXT("runtimeStatus"), RuntimeStatus);
            Sensor->SetNumberField(TEXT("rangeMeters"), bEnabled ? RangeMeters : 0.0);
            if (FieldOfViewDegrees > 0.0)
            {
                Sensor->SetNumberField(TEXT("horizontalFovDegrees"), FieldOfViewDegrees);
            }
            LongRangeSensorStack.Add(MakeShared<FJsonValueObject>(MoveTemp(Sensor)));
        };
        AddLongRangeSensor(
            TEXT(":SEARCH_RADAR"),
            TEXT("SEARCH_RADAR"),
            NodeDefinition.bEnableSearchRadar,
            SearchRadarRuntimeStatus,
            FMath::Max(NodeDefinition.SearchRadarRangeMeters, 5000.0),
            360.0);
        AddLongRangeSensor(
            TEXT(":EO_PTZ"),
            TEXT("EO_PTZ"),
            NodeDefinition.bEnableEOPTZ,
            EOPTZRuntimeStatus,
            FMath::Max(NodeDefinition.EOPTZConfirmationRangeMeters, 500.0),
            NodeDefinition.EOPTZFieldOfViewDegrees);
        AddLongRangeSensor(
            TEXT(":THERMAL_PTZ"),
            TEXT("THERMAL_PTZ"),
            NodeDefinition.bEnableThermalPTZ,
            ThermalPTZRuntimeStatus,
            FMath::Max(NodeDefinition.ThermalPTZConfirmationRangeMeters, 500.0),
            NodeDefinition.ThermalPTZFieldOfViewDegrees);
        NodeJson->SetArrayField(TEXT("longRangeSensorStack"), LongRangeSensorStack);
        NodeJson->SetNumberField(TEXT("currentDetectedLinkCount"), SampleStatus ? SampleStatus->DetectedLinkCount : 0);
        NodeJson->SetNumberField(
            TEXT("currentDetectedTargetCount"),
            SampleStatus ? SampleStatus->DetectedTargetActors.Num() : 0);
        NodeJson->SetNumberField(
            TEXT("currentPreliminaryCueEvidenceLinkCount"),
            SampleStatus ? SampleStatus->PreliminaryCueEvidenceLinkCount : 0);
        // Deprecated compatibility alias. This is RF detection evidence and is
        // not gated by hostileScenarioTruth.
        NodeJson->SetNumberField(
            TEXT("currentAlertEvidenceLinkCount"),
            SampleStatus ? SampleStatus->PreliminaryCueEvidenceLinkCount : 0);
        NodeJson->SetBoolField(TEXT("hasCurrentRFDetection"), SampleStatus && SampleStatus->DetectedLinkCount > 0);
        if (SampleStatus && SampleStatus->DetectedLinkCount > 0)
        {
            NodeJson->SetNumberField(TEXT("strongestCurrentSnrDb"), SampleStatus->StrongestSnrDb);
        }
        NodeValues.Add(MakeShared<FJsonValueObject>(MoveTemp(NodeJson)));
    }

    TArray<FTRIADPTZConfirmationResult> PTZConfirmationResults;
    for (const ATRIADSensorNodeActor* Node : SpawnedSensorNodes)
    {
        if (IsValid(Node))
        {
            Node->GetLatestPTZConfirmations(PTZConfirmationResults);
        }
    }
    PTZConfirmationResults.Sort([](
        const FTRIADPTZConfirmationResult& A,
        const FTRIADPTZConfirmationResult& B)
    {
        if (A.bConfirmed != B.bConfirmed)
        {
            return A.bConfirmed;
        }
        const bool bAHasFrame = !A.FrameRelativePath.IsEmpty();
        const bool bBHasFrame = !B.FrameRelativePath.IsEmpty();
        if (bAHasFrame != bBHasFrame)
        {
            return bAHasFrame;
        }
        return A.Confidence > B.Confidence;
    });
    const int32 PTZConfirmationCap = FMath::Clamp(
        ScenarioConfig.MaxLiveSnapshotPTZConfirmations,
        1,
        256);
    const int32 RetainedPTZConfirmationCount = FMath::Min(
        PTZConfirmationResults.Num(),
        PTZConfirmationCap);
    TArray<TSharedPtr<FJsonValue>> PTZConfirmationValues;
    PTZConfirmationValues.Reserve(RetainedPTZConfirmationCount);
    for (int32 Index = 0; Index < RetainedPTZConfirmationCount; ++Index)
    {
        const FTRIADPTZConfirmationResult& Confirmation = PTZConfirmationResults[Index];
        const bool bSafeFrameRelativePath = Confirmation.FrameRelativePath.IsEmpty() ||
            (!Confirmation.FrameRelativePath.Contains(TEXT("..")) &&
                (Confirmation.FrameRelativePath.StartsWith(TEXT("RadarPtzFrames/eo/")) ||
                    Confirmation.FrameRelativePath.StartsWith(TEXT("RadarPtzFrames/thermal/"))));
        const bool bSafeMetadataRelativePath = Confirmation.MetadataRelativePath.IsEmpty() ||
            (!Confirmation.MetadataRelativePath.Contains(TEXT("..")) &&
                (Confirmation.MetadataRelativePath.StartsWith(TEXT("RadarPtzFrames/eo/")) ||
                    Confirmation.MetadataRelativePath.StartsWith(TEXT("RadarPtzFrames/thermal/"))));
        // Defense in depth: only a positive current confirmation may expose
        // the retained latest-confirmed artifacts.  This prevents an
        // unconfirmed status record from making an older file look current,
        // even if a future producer accidentally leaves paths populated.
        const bool bExposeConfirmedFrameArtifacts = Confirmation.bConfirmed &&
            bSafeFrameRelativePath &&
            bSafeMetadataRelativePath &&
            !Confirmation.FrameRelativePath.IsEmpty() &&
            !Confirmation.MetadataRelativePath.IsEmpty();
        const FString FrameRelativePath = bExposeConfirmedFrameArtifacts
            ? Confirmation.FrameRelativePath
            : FString();
        const FString MetadataRelativePath = bExposeConfirmedFrameArtifacts
            ? Confirmation.MetadataRelativePath
            : FString();

        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("timestampUtc"), Confirmation.TimestampUtc);
        Json->SetNumberField(TEXT("simulationSeconds"), Confirmation.SimulationSeconds);
        Json->SetStringField(TEXT("id"), Confirmation.SensorId + TEXT("|") + Confirmation.TrackId);
        Json->SetStringField(TEXT("kind"), TEXT("SIMULATED_SENSOR_CONFIRMATION"));
        Json->SetStringField(TEXT("source"), TEXT("SIMULATION_PROJECTION"));
        Json->SetStringField(TEXT("confirmationMethod"), TEXT("SIMULATION_PROJECTION_TRUTH"));
        Json->SetStringField(TEXT("boxSource"), TEXT("DEBUG_PROJECTION"));
        Json->SetBoolField(TEXT("simulated"), true);
        Json->SetBoolField(TEXT("calibratedDetector"), false);
        Json->SetStringField(
            TEXT("label"),
            Confirmation.bSyntheticThermal
                ? TEXT("SYNTHETIC THERMAL RADAR-CUED PTZ")
                : TEXT("UNREAL VISIBLE PIXELS + SIMULATED RADAR-CUED PTZ"));
        Json->SetStringField(TEXT("nodeId"), Confirmation.NodeId);
        Json->SetStringField(TEXT("sensorId"), Confirmation.SensorId);
        Json->SetStringField(TEXT("sensorType"), Confirmation.SensorType);
        Json->SetStringField(TEXT("modality"), Confirmation.Modality);
        Json->SetStringField(TEXT("contactId"), Confirmation.ContactId);
        Json->SetStringField(TEXT("trackId"), Confirmation.TrackId);
        Json->SetStringField(TEXT("targetActor"), Confirmation.TargetActor);
        Json->SetStringField(TEXT("radarCueSensorId"), Confirmation.RadarCueSensorId);
        Json->SetNumberField(TEXT("cueAgeSeconds"), Confirmation.CueAgeSeconds);
        Json->SetStringField(TEXT("slewState"), Confirmation.SlewState);
        Json->SetBoolField(TEXT("confirmed"), Confirmation.bConfirmed);
        Json->SetBoolField(TEXT("lineOfSight"), Confirmation.bLineOfSight);
        Json->SetStringField(TEXT("blockingActor"), Confirmation.BlockingActor);
        Json->SetStringField(TEXT("occlusionSemantics"), Confirmation.OcclusionSemantics);
        Json->SetBoolField(TEXT("hasFrame"), bExposeConfirmedFrameArtifacts);
        Json->SetNumberField(TEXT("rangeMeters"), Confirmation.RangeMeters);
        Json->SetNumberField(TEXT("confidence"), Confirmation.Confidence);
        Json->SetStringField(TEXT("weatherProfile"), Confirmation.WeatherProfile);
        Json->SetNumberField(TEXT("weatherVisibilityMeters"), Confirmation.WeatherVisibilityMeters);
        Json->SetNumberField(
            TEXT("weatherRainRateMillimetersPerHour"),
            Confirmation.WeatherRainRateMillimetersPerHour);
        Json->SetNumberField(TEXT("weatherConfidenceFactor"), Confirmation.WeatherConfidenceFactor);
        Json->SetStringField(TEXT("weatherConfidenceSemantics"), Confirmation.WeatherConfidenceSemantics);
        Json->SetNumberField(TEXT("azimuthDeg"), Confirmation.AzimuthDegrees);
        Json->SetNumberField(TEXT("bearingDegrees"), Confirmation.AzimuthDegrees);
        Json->SetNumberField(TEXT("elevationDeg"), Confirmation.ElevationDegrees);
        Json->SetNumberField(TEXT("elevationDegrees"), Confirmation.ElevationDegrees);
        Json->SetNumberField(TEXT("horizontalFovDeg"), Confirmation.FieldOfViewDegrees);
        Json->SetNumberField(TEXT("fovDegrees"), Confirmation.FieldOfViewDegrees);
        Json->SetNumberField(TEXT("imageWidthPixels"), Confirmation.ImageWidthPixels);
        Json->SetNumberField(TEXT("imageHeightPixels"), Confirmation.ImageHeightPixels);
        Json->SetNumberField(TEXT("width"), Confirmation.ImageWidthPixels);
        Json->SetNumberField(TEXT("height"), Confirmation.ImageHeightPixels);
        Json->SetNumberField(TEXT("pixelExtentWidth"), Confirmation.PixelExtentWidth);
        Json->SetNumberField(TEXT("pixelExtentHeight"), Confirmation.PixelExtentHeight);
        Json->SetStringField(TEXT("frameRelativePath"), FrameRelativePath);
        Json->SetStringField(TEXT("relativePath"), FrameRelativePath);
        Json->SetStringField(TEXT("metadataRelativePath"), MetadataRelativePath);
        Json->SetBoolField(TEXT("syntheticThermal"), Confirmation.bSyntheticThermal);
        Json->SetStringField(TEXT("thermalSemantics"), Confirmation.ThermalSemantics);
        Json->SetStringField(TEXT("actionsTaken"), TEXT("none"));

        TSharedRef<FJsonObject> Reticle = MakeShared<FJsonObject>();
        Reticle->SetNumberField(TEXT("x"), Confirmation.ImageWidthPixels * 0.5);
        Reticle->SetNumberField(TEXT("y"), Confirmation.ImageHeightPixels * 0.5);
        Reticle->SetStringField(TEXT("coordinateSpace"), TEXT("pixels"));
        Json->SetObjectField(TEXT("reticle"), MoveTemp(Reticle));

        TArray<TSharedPtr<FJsonValue>> BoundingBoxPixels;
        TArray<TSharedPtr<FJsonValue>> Boxes;
        if (Confirmation.BoundingBoxMaximumX > Confirmation.BoundingBoxMinimumX &&
            Confirmation.BoundingBoxMaximumY > Confirmation.BoundingBoxMinimumY)
        {
            BoundingBoxPixels.Add(MakeShared<FJsonValueNumber>(Confirmation.BoundingBoxMinimumX));
            BoundingBoxPixels.Add(MakeShared<FJsonValueNumber>(Confirmation.BoundingBoxMinimumY));
            BoundingBoxPixels.Add(MakeShared<FJsonValueNumber>(Confirmation.BoundingBoxMaximumX));
            BoundingBoxPixels.Add(MakeShared<FJsonValueNumber>(Confirmation.BoundingBoxMaximumY));
            TSharedRef<FJsonObject> Box = MakeShared<FJsonObject>();
            Box->SetStringField(TEXT("kind"), TEXT("SIMULATED_SENSOR_CONFIRMATION"));
            Box->SetStringField(TEXT("source"), TEXT("SIMULATION_PROJECTION"));
            Box->SetStringField(TEXT("label"), TEXT("SIMULATED SENSOR CONFIRMATION"));
            Box->SetBoolField(TEXT("simulated"), true);
            Box->SetArrayField(TEXT("xyxyPixels"), BoundingBoxPixels);
            TArray<TSharedPtr<FJsonValue>> NormalizedCoordinates;
            NormalizedCoordinates.Add(MakeShared<FJsonValueNumber>(
                Confirmation.BoundingBoxMinimumX / FMath::Max(Confirmation.ImageWidthPixels, 1)));
            NormalizedCoordinates.Add(MakeShared<FJsonValueNumber>(
                Confirmation.BoundingBoxMinimumY / FMath::Max(Confirmation.ImageHeightPixels, 1)));
            NormalizedCoordinates.Add(MakeShared<FJsonValueNumber>(
                Confirmation.BoundingBoxMaximumX / FMath::Max(Confirmation.ImageWidthPixels, 1)));
            NormalizedCoordinates.Add(MakeShared<FJsonValueNumber>(
                Confirmation.BoundingBoxMaximumY / FMath::Max(Confirmation.ImageHeightPixels, 1)));
            Box->SetArrayField(TEXT("xyxyNormalized"), NormalizedCoordinates);
            Boxes.Add(MakeShared<FJsonValueObject>(MoveTemp(Box)));
        }
        Json->SetArrayField(TEXT("boundingBoxPixels"), BoundingBoxPixels);
        Json->SetArrayField(TEXT("boxes"), Boxes);
        PTZConfirmationValues.Add(MakeShared<FJsonValueObject>(MoveTemp(Json)));
    }

    TArray<FTrackSampleStatus> StrongestTracks;
    TrackStatusByTarget.GenerateValueArray(StrongestTracks);
    StrongestTracks.Sort([](const FTrackSampleStatus& A, const FTrackSampleStatus& B)
    {
        if (!FMath::IsNearlyEqual(A.StrongestSnrDb, B.StrongestSnrDb, 0.000001))
        {
            return A.StrongestSnrDb > B.StrongestSnrDb;
        }
        return A.NearestSlantRangeMeters < B.NearestSlantRangeMeters;
    });

    const int32 RetainedTrackCount = FMath::Min(StrongestTracks.Num(), LinkCap);
    const int32 MinimumConfirmingNodes = FMath::Max(ScenarioConfig.AlertMinimumConfirmingNodes, 1);
    TArray<TSharedPtr<FJsonValue>> TrackValues;
    TrackValues.Reserve(RetainedTrackCount);
    for (int32 Index = 0; Index < RetainedTrackCount; ++Index)
    {
        FTrackSampleStatus& Track = StrongestTracks[Index];
        TArray<FString> ConfirmingNodeIds = Track.ConfirmingNodeIds.Array();
        TArray<FString> PreliminaryCueEvidenceNodeIds = Track.PreliminaryCueEvidenceNodeIds.Array();
        ConfirmingNodeIds.Sort();
        PreliminaryCueEvidenceNodeIds.Sort();
        Track.DetectedFrequenciesGHz.Sort();

        TArray<TSharedPtr<FJsonValue>> ConfirmingNodeValues;
        for (const FString& NodeId : ConfirmingNodeIds)
        {
            ConfirmingNodeValues.Add(MakeShared<FJsonValueString>(NodeId));
        }
        TArray<TSharedPtr<FJsonValue>> PreliminaryCueEvidenceNodeValues;
        for (const FString& NodeId : PreliminaryCueEvidenceNodeIds)
        {
            PreliminaryCueEvidenceNodeValues.Add(MakeShared<FJsonValueString>(NodeId));
        }
        TArray<TSharedPtr<FJsonValue>> DetectedFrequencyValues;
        for (const double FrequencyGHz : Track.DetectedFrequenciesGHz)
        {
            DetectedFrequencyValues.Add(MakeShared<FJsonValueNumber>(FrequencyGHz));
        }

        TSharedPtr<FJsonObject> TrackJson = MakeShared<FJsonObject>();
        TrackJson->SetStringField(TEXT("contactId"), MakeSensorContactIdFromTargetSeed(Track.TargetActor));
        TrackJson->SetStringField(TEXT("trackId"), Track.TargetActor);
        TrackJson->SetStringField(TEXT("targetActor"), Track.TargetActor);
        TrackJson->SetStringField(TEXT("emitterId"), Track.EmitterId);
        TrackJson->SetBoolField(TEXT("hostileScenarioTruth"), Track.bHostileScenarioTruth);
        TrackJson->SetNumberField(TEXT("targetLongitudeDegrees"), Track.TargetLongitudeDegrees);
        TrackJson->SetNumberField(TEXT("targetLatitudeDegrees"), Track.TargetLatitudeDegrees);
        TrackJson->SetNumberField(TEXT("targetHeightMeters"), Track.TargetHeightMeters);
        TrackJson->SetNumberField(TEXT("strongestSnrDb"), Track.StrongestSnrDb);
        TrackJson->SetNumberField(TEXT("nearestSlantRangeMeters"), Track.NearestSlantRangeMeters);
        TrackJson->SetStringField(TEXT("rangeSemantics"), TEXT("sensor_to_target_3d_slant_range"));
        TrackJson->SetNumberField(TEXT("detectedLinkCount"), Track.DetectedLinkCount);
        TrackJson->SetNumberField(TEXT("confirmingNodeCount"), ConfirmingNodeIds.Num());
        TrackJson->SetArrayField(TEXT("confirmingNodeIds"), ConfirmingNodeValues);
        TrackJson->SetArrayField(TEXT("detectedFrequenciesGHz"), DetectedFrequencyValues);
        TrackJson->SetNumberField(TEXT("preliminaryCueEvidenceLinkCount"), Track.PreliminaryCueEvidenceLinkCount);
        TrackJson->SetNumberField(TEXT("preliminaryCueEvidenceNodeCount"), PreliminaryCueEvidenceNodeIds.Num());
        TrackJson->SetArrayField(TEXT("preliminaryCueEvidenceNodeIds"), PreliminaryCueEvidenceNodeValues);
        // Deprecated compatibility aliases; both carry sensor evidence only.
        TrackJson->SetNumberField(TEXT("alertEvidenceLinkCount"), Track.PreliminaryCueEvidenceLinkCount);
        TrackJson->SetNumberField(TEXT("alertEvidenceNodeCount"), PreliminaryCueEvidenceNodeIds.Num());
        TrackJson->SetArrayField(TEXT("alertEvidenceNodeIds"), PreliminaryCueEvidenceNodeValues);
        TrackJson->SetBoolField(
            TEXT("rfMultinodePreliminaryCueRuleSatisfied"),
            PreliminaryCueEvidenceNodeIds.Num() >= MinimumConfirmingNodes);
        if (const FTargetApproachStatus* Approach =
                CurrentTargetApproachStatusByActor.Find(Track.TargetActor))
        {
            TrackJson->SetStringField(TEXT("airspaceState"), Approach->AirspaceState);
            TrackJson->SetNumberField(TEXT("distanceToPerimeterMeters"), Approach->DistanceToPerimeterMeters);
            TrackJson->SetNumberField(TEXT("approachRateMetersPerSecond"), Approach->ApproachRateMetersPerSecond);
            TrackJson->SetNumberField(TEXT("headingDegrees"), Approach->HeadingDegrees);
            TrackJson->SetNumberField(TEXT("speedMetersPerSecond"), Approach->SpeedMetersPerSecond);
            TrackJson->SetBoolField(TEXT("outsideSimulationPerimeter"), Approach->bOutsideSimulationPerimeter);
            TrackJson->SetStringField(TEXT("ingressCorridorId"), Approach->IngressCorridorId);
            TrackJson->SetBoolField(TEXT("inboundApproachScenario"), Approach->bInboundApproachScenario);
            TrackJson->SetBoolField(TEXT("kinematicsAvailable"), Approach->bKinematicsAvailable);
        }
        TrackValues.Add(MakeShared<FJsonValueObject>(MoveTemp(TrackJson)));
    }

    // Scenario targets are separate from detected RF tracks. This makes the
    // outside-to-detection transition auditable without representing truth as a detection.
    TArray<FString> ScenarioTargetNames;
    CurrentTargetApproachStatusByActor.GenerateKeyArray(ScenarioTargetNames);
    ScenarioTargetNames.Sort();
    TArray<TSharedPtr<FJsonValue>> ScenarioTargetValues;
    ScenarioTargetValues.Reserve(ScenarioTargetNames.Num());
    int32 OutsideScenarioTargetCount = 0;
    int32 ApproachingScenarioTargetCount = 0;
    int32 OutsideDetectedTrackCount = 0;
    for (const FString& TargetName : ScenarioTargetNames)
    {
        const FTargetApproachStatus* Approach = CurrentTargetApproachStatusByActor.Find(TargetName);
        if (!Approach)
        {
            continue;
        }
        const FTrackSampleStatus* DetectedTrack = TrackStatusByTarget.Find(TargetName);
        OutsideScenarioTargetCount += Approach->bOutsideSimulationPerimeter ? 1 : 0;
        ApproachingScenarioTargetCount += Approach->AirspaceState == TEXT("APPROACHING") ? 1 : 0;
        OutsideDetectedTrackCount += Approach->bOutsideSimulationPerimeter && DetectedTrack ? 1 : 0;

        TSharedPtr<FJsonObject> TargetJson = MakeShared<FJsonObject>();
        TargetJson->SetStringField(TEXT("targetActor"), TargetName);
        TargetJson->SetStringField(TEXT("ingressCorridorId"), Approach->IngressCorridorId);
        TargetJson->SetBoolField(TEXT("inboundApproachScenario"), Approach->bInboundApproachScenario);
        TargetJson->SetBoolField(TEXT("hostileScenarioTruth"), Approach->bHostileScenarioTruth);
        TargetJson->SetNumberField(TEXT("targetLongitudeDegrees"), Approach->LongitudeDegrees);
        TargetJson->SetNumberField(TEXT("targetLatitudeDegrees"), Approach->LatitudeDegrees);
        TargetJson->SetNumberField(TEXT("targetHeightMeters"), Approach->HeightMeters);
        TargetJson->SetStringField(TEXT("airspaceState"), Approach->AirspaceState);
        TargetJson->SetNumberField(TEXT("distanceToPerimeterMeters"), Approach->DistanceToPerimeterMeters);
        TargetJson->SetNumberField(TEXT("approachRateMetersPerSecond"), Approach->ApproachRateMetersPerSecond);
        TargetJson->SetNumberField(TEXT("headingDegrees"), Approach->HeadingDegrees);
        TargetJson->SetNumberField(TEXT("speedMetersPerSecond"), Approach->SpeedMetersPerSecond);
        TargetJson->SetBoolField(TEXT("outsideSimulationPerimeter"), Approach->bOutsideSimulationPerimeter);
        TargetJson->SetBoolField(TEXT("kinematicsAvailable"), Approach->bKinematicsAvailable);
        TargetJson->SetBoolField(TEXT("detectedThisSample"), DetectedTrack != nullptr);
        TargetJson->SetNumberField(
            TEXT("detectedLinkCount"),
            DetectedTrack ? DetectedTrack->DetectedLinkCount : 0);
        ScenarioTargetValues.Add(MakeShared<FJsonValueObject>(MoveTemp(TargetJson)));
    }

    const FTRIADSimulationPerimeter& Perimeter = ScenarioConfig.SimulationPerimeter;
    TSharedPtr<FJsonObject> PerimeterJson = MakeShared<FJsonObject>();
    PerimeterJson->SetBoolField(TEXT("enabled"), Perimeter.bEnabled);
    PerimeterJson->SetStringField(TEXT("referenceName"), Perimeter.ReferenceName);
    const bool bCirclePerimeter = TRIAD::Geodesy::IsCircle(Perimeter);
    PerimeterJson->SetStringField(
        TEXT("geometryType"),
        bCirclePerimeter ? TEXT("wgs84_geodesic_circle") : TEXT("axis_aligned_wgs84_rectangle"));
    PerimeterJson->SetStringField(TEXT("shape"), bCirclePerimeter ? TEXT("Circle") : TEXT("Rectangle"));
    if (bCirclePerimeter)
    {
        PerimeterJson->SetNumberField(TEXT("centerLongitudeDegrees"), Perimeter.CenterLongitudeDegrees);
        PerimeterJson->SetNumberField(TEXT("centerLatitudeDegrees"), Perimeter.CenterLatitudeDegrees);
        PerimeterJson->SetNumberField(TEXT("radiusMeters"), Perimeter.RadiusMeters);
    }
    else
    {
        PerimeterJson->SetNumberField(
            TEXT("minimumLongitudeDegrees"),
            FMath::Min(Perimeter.MinimumLongitudeDegrees, Perimeter.MaximumLongitudeDegrees));
        PerimeterJson->SetNumberField(
            TEXT("maximumLongitudeDegrees"),
            FMath::Max(Perimeter.MinimumLongitudeDegrees, Perimeter.MaximumLongitudeDegrees));
        PerimeterJson->SetNumberField(
            TEXT("minimumLatitudeDegrees"),
            FMath::Min(Perimeter.MinimumLatitudeDegrees, Perimeter.MaximumLatitudeDegrees));
        PerimeterJson->SetNumberField(
            TEXT("maximumLatitudeDegrees"),
            FMath::Max(Perimeter.MinimumLatitudeDegrees, Perimeter.MaximumLatitudeDegrees));
    }
    PerimeterJson->SetBoolField(TEXT("boundaryInclusive"), true);
    PerimeterJson->SetBoolField(TEXT("legalOrNationalBoundary"), false);
    PerimeterJson->SetStringField(
        TEXT("purpose"),
        TEXT("Deterministic simulation approach/inside/departure classification only."));
    PerimeterJson->SetStringField(
        TEXT("distanceMethod"),
        bCirclePerimeter
            ? TEXT("signed WGS84 Vincenty geodesic center distance minus radius")
            : TEXT("signed horizontal distance to the rectangle using the backwards-compatible local equirectangular WGS84 approximation"));

    TSharedPtr<FJsonObject> WeatherJson = MakeShared<FJsonObject>();
    WeatherJson->SetStringField(TEXT("profile"), ActiveWeatherProfileName);
    WeatherJson->SetBoolField(TEXT("airSimVisualWeatherRequested"), ScenarioConfig.Weather.bApplyAirSimVisualWeather);
    WeatherJson->SetBoolField(TEXT("airSimVisualWeatherApplied"), bAirSimVisualWeatherApplied);
    WeatherJson->SetBoolField(TEXT("airSimWeatherInitialized"), bAirSimWeatherInitialized);
    WeatherJson->SetBoolField(TEXT("airSimWeatherActorsVerified"), bAirSimWeatherActorsVerified);
    WeatherJson->SetStringField(
        TEXT("visualVerificationStatus"),
        bAirSimVisualWeatherApplied ? TEXT("VERIFIED") :
            ScenarioConfig.Weather.bApplyAirSimVisualWeather ? TEXT("REQUESTED_NOT_VERIFIED") : TEXT("NOT_REQUESTED"));
    WeatherJson->SetNumberField(TEXT("rainAmount"), ActiveWeatherRainAmount);
    WeatherJson->SetNumberField(TEXT("roadWetnessAmount"), ActiveWeatherRoadWetnessAmount);
    WeatherJson->SetNumberField(TEXT("fogAmount"), ActiveWeatherFogAmount);
    WeatherJson->SetNumberField(TEXT("dustAmount"), ActiveWeatherDustAmount);
    WeatherJson->SetNumberField(TEXT("rainRateMillimetersPerHour"), ActiveWeatherRainRateMillimetersPerHour);
    WeatherJson->SetNumberField(TEXT("visibilityMeters"), ActiveWeatherVisibilityMeters);
    WeatherJson->SetNumberField(
        TEXT("rfSpecificAttenuationDbPerKmAt2_4GHz"),
        ActiveWeatherRFSpecificAttenuationDbPerKmAt2_4GHz);
    WeatherJson->SetNumberField(
        TEXT("rfSpecificAttenuationDbPerKmAt5_8GHz"),
        ActiveWeatherRFSpecificAttenuationDbPerKmAt5_8GHz);
    TSharedPtr<FJsonObject> WindJson = MakeShared<FJsonObject>();
    WindJson->SetNumberField(TEXT("x"), ActiveWeatherWind.X);
    WindJson->SetNumberField(TEXT("y"), ActiveWeatherWind.Y);
    WindJson->SetNumberField(TEXT("z"), ActiveWeatherWind.Z);
    WeatherJson->SetObjectField(TEXT("airSimWindScenarioVector"), MoveTemp(WindJson));

    TSharedPtr<FJsonObject> CountsJson = MakeShared<FJsonObject>();
    CountsJson->SetNumberField(TEXT("configuredSensorNodes"), ScenarioConfig.SensorNodes.Num());
    CountsJson->SetNumberField(TEXT("spawnedSensorNodes"), SpawnedNodeIds.Num());
    CountsJson->SetNumberField(TEXT("detectedRFLinksBeforeCap"), CurrentDetectedRFSnapshotLinks.Num());
    CountsJson->SetNumberField(TEXT("retainedDetectedRFLinks"), RetainedLinkCount);
    CountsJson->SetNumberField(TEXT("searchRadarDetectionsBeforeCap"), CurrentSearchRadarDetections.Num());
    CountsJson->SetNumberField(TEXT("retainedSearchRadarDetections"), RetainedRadarDetectionCount);
    CountsJson->SetNumberField(TEXT("ptzConfirmationRecordsBeforeCap"), PTZConfirmationResults.Num());
    CountsJson->SetNumberField(TEXT("retainedPtzConfirmationRecords"), RetainedPTZConfirmationCount);
    CountsJson->SetNumberField(TEXT("detectedTracksBeforeCap"), StrongestTracks.Num());
    CountsJson->SetNumberField(TEXT("retainedDetectedTracks"), RetainedTrackCount);
    CountsJson->SetNumberField(TEXT("scenarioTargets"), ScenarioTargetValues.Num());
    CountsJson->SetNumberField(TEXT("outsideScenarioTargets"), OutsideScenarioTargetCount);
    CountsJson->SetNumberField(TEXT("approachingScenarioTargets"), ApproachingScenarioTargetCount);
    CountsJson->SetNumberField(TEXT("outsideDetectedTracks"), OutsideDetectedTrackCount);

    TSharedPtr<FJsonObject> SemanticsJson = MakeShared<FJsonObject>();
    SemanticsJson->SetStringField(
        TEXT("hostileScenarioTruth"),
        TEXT("Authored synthetic scenario truth for offline scoring only; it is not inferred and never gates an operator cue."));
    SemanticsJson->SetStringField(
        TEXT("preliminaryCueEvidence"),
        TEXT("A model-derived RF detection that can corroborate a preliminary cue; independent of scenario truth and not a calibrated probability."));
    SemanticsJson->SetStringField(
        TEXT("alertEvidence"),
        TEXT("Deprecated compatibility alias for preliminaryCueEvidence; independent of scenario truth."));
    SemanticsJson->SetStringField(
        TEXT("fusionRule"),
        TEXT("Transparent threshold on distinct RF-detecting nodes; produces a preliminary early-warning cue without hostility inference."));
    SemanticsJson->SetStringField(
        TEXT("scenarioTargets"),
        TEXT("Authored/discovered target geometry, including undetected targets; detectedThisSample distinguishes RF observations from scenario truth."));
    SemanticsJson->SetStringField(
        TEXT("airspaceState"),
        TEXT("APPROACHING/DEPARTING/OUTSIDE apply only outside the configured perimeter shape; boundary and interior use INSIDE."));
    SemanticsJson->SetStringField(
        TEXT("distanceToPerimeterMeters"),
        TEXT("Signed horizontal/geodesic distance to the documented simulation perimeter: positive outside, zero on its inclusive boundary, negative inside."));
    SemanticsJson->SetStringField(
        TEXT("approachRateMetersPerSecond"),
        TEXT("Positive when signed distanceToPerimeterMeters is decreasing (inbound); negative when increasing (outbound)."));
    SemanticsJson->SetStringField(
        TEXT("headingDegrees"),
        TEXT("Horizontal course clockwise from geodetic north: 0=N, 90=E, 180=S, 270=W."));
    SemanticsJson->SetStringField(
        TEXT("searchRadarDetections"),
        TEXT("Explicitly simulated geometry-only analytic search-radar observations; independent of RF emissions and not calibrated hardware claims."));
    SemanticsJson->SetStringField(
        TEXT("ptzConfirmations"),
        TEXT("Radar-cued simulated sensor confirmation using Unreal SceneCapture pixels and debug projection truth; boxes are not learned-model output."));
    SemanticsJson->SetStringField(
        TEXT("thermalPtz"),
        TEXT("Synthetic false-colour thermal proxy with an embedded SIM THERMAL SYNTHETIC label; not a physical radiometric camera."));
    SemanticsJson->SetStringField(
        TEXT("rfPropagation"),
        TEXT("Dedicated mode uses only hash-bound direct or straight-transmission candidates from the admitted CPU RF geometry. OneKilometreV2 additionally requires strict WGS84-circle and EPSG:3414 frame admission. Visibility LOS is diagnostic; weather and receiver-system losses remain separate. Neither mode is field validated."));

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schemaVersion"), TEXT("triad.live_rf_snapshot.v3"));
    Root->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetNumberField(TEXT("simulationSeconds"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
    Root->SetNumberField(TEXT("sampleCadenceSeconds"), ScenarioConfig.SampleCadenceSeconds);
    Root->SetBoolField(TEXT("sampleComplete"), true);
    Root->SetBoolField(TEXT("detectionOnly"), true);
    Root->SetStringField(TEXT("actionsTaken"), TEXT("none"));
    Root->SetBoolField(TEXT("calibratedOperationalSystem"), false);
    Root->SetStringField(TEXT("calibrationStatus"), TEXT("uncalibrated_deterministic_simulation_model"));
    Root->SetStringField(TEXT("sortOrder"), TEXT("strongest_snr_descending_then_nearest_range"));
    Root->SetNumberField(TEXT("maxDetectedLinksConfigured"), LinkCap);
    Root->SetBoolField(TEXT("detectedLinksTruncated"), CurrentDetectedRFSnapshotLinks.Num() > RetainedLinkCount);
    Root->SetNumberField(TEXT("alertMinimumConfirmingNodes"), MinimumConfirmingNodes);
    Root->SetNumberField(TEXT("rfCueMinimumConfirmingNodes"), MinimumConfirmingNodes);
    Root->SetStringField(TEXT("approachTelemetryVersion"), TEXT("triad.approach.v1"));
    Root->SetObjectField(TEXT("weather"), MoveTemp(WeatherJson));
    Root->SetObjectField(TEXT("rfPropagation"), MakeRFPropagationStatusJson());
    Root->SetObjectField(TEXT("simulationPerimeter"), MoveTemp(PerimeterJson));
    Root->SetObjectField(TEXT("counts"), MoveTemp(CountsJson));
    Root->SetObjectField(TEXT("fieldSemantics"), MoveTemp(SemanticsJson));
    Root->SetArrayField(TEXT("sensorNodes"), NodeValues);
    Root->SetArrayField(TEXT("detectedRFLinks"), LinkValues);
    Root->SetArrayField(TEXT("searchRadarDetections"), RadarDetectionValues);
    Root->SetArrayField(TEXT("ptzConfirmations"), PTZConfirmationValues);
    Root->SetArrayField(TEXT("tracks"), TrackValues);
    Root->SetArrayField(TEXT("scenarioTargets"), ScenarioTargetValues);

    FString SnapshotJson;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&SnapshotJson);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        if (!bLiveSnapshotWriteFailureLogged)
        {
            UE_LOG(LogTemp, Warning, TEXT("TRIAD latest RF snapshot serialization failed."));
            bLiveSnapshotWriteFailureLogged = true;
        }
        return;
    }

    // Save beside the destination, then replace by rename. Readers see either the
    // previous complete document or the new complete document, never a partial write.
    const FString TemporaryPath = LatestRFSnapshotPath + TEXT(".tmp");
    const bool bTemporarySaved = FFileHelper::SaveStringToFile(
        SnapshotJson,
        *TemporaryPath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    const bool bReplaced = bTemporarySaved && AtomicallyPublishFile(TemporaryPath, LatestRFSnapshotPath);
    if (!bReplaced)
    {
        if (!bLiveSnapshotWriteFailureLogged)
        {
            UE_LOG(LogTemp, Warning, TEXT("TRIAD could not atomically replace latest RF snapshot: %s"), *LatestRFSnapshotPath);
            bLiveSnapshotWriteFailureLogged = true;
        }
        return;
    }
    bLiveSnapshotWriteFailureLogged = false;
}

void ATRIADSensorFusionScenarioManager::EmitRFEarlyWarningCue(
    AActor* Target,
    const TMap<FString, double>& ConfirmingNodeDistancesMeters,
    double MaxSnrDb)
{
    if (!ScenarioConfig.bEnableThreatAlerts || !IsValid(Target))
    {
        return;
    }

    // A detected emitter component is required for provenance, but authored
    // hostile/friendly truth is intentionally not consulted here.
    const UTRIADRFEmitterComponent* Emitter = Target->FindComponentByClass<UTRIADRFEmitterComponent>();
    if (!Emitter)
    {
        return;
    }

    const double SimulationSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const FString TargetName = Target->GetName();
    const FString OperatorTrackId = MakeOperatorRFCueTrackId(Target, Emitter);
    if (const double* LastAlertSeconds = LastAlertSimulationSecondsByTarget.Find(TargetName))
    {
        if (SimulationSeconds - *LastAlertSeconds < FMath::Max(ScenarioConfig.AlertCooldownSeconds, 0.0f))
        {
            return;
        }
    }
    LastAlertSimulationSecondsByTarget.Add(TargetName, SimulationSeconds);

    TArray<FString> SortedNodeIds;
    ConfirmingNodeDistancesMeters.GenerateKeyArray(SortedNodeIds);
    SortedNodeIds.Sort();

    // Iterating sorted IDs also makes an exact distance tie deterministic: the
    // lexicographically first node remains the nearest reporter.
    FString NearestNodeId;
    double NearestDistanceMeters = TNumericLimits<double>::Max();
    for (const FString& NodeId : SortedNodeIds)
    {
        const double* DistanceMeters = ConfirmingNodeDistancesMeters.Find(NodeId);
        if (DistanceMeters && FMath::IsFinite(*DistanceMeters) && *DistanceMeters >= 0.0 &&
            *DistanceMeters < NearestDistanceMeters)
        {
            NearestNodeId = NodeId;
            NearestDistanceMeters = *DistanceMeters;
        }
    }

    const bool bHasValidDistance = !NearestNodeId.IsEmpty() && FMath::IsFinite(NearestDistanceMeters);
    const FString NearestDistanceText = bHasValidDistance
        ? (NearestDistanceMeters >= 1000.0
            ? FString::Printf(TEXT("%.2f km"), NearestDistanceMeters / 1000.0)
            : FString::Printf(TEXT("%.0f m"), NearestDistanceMeters))
        : TEXT("range unavailable");
    const FString NearestNodeText = bHasValidDistance ? NearestNodeId : TEXT("unknown node");
    const FString Message = FString::Printf(
        TEXT("PRELIMINARY RF EARLY WARNING: unclassified contact %s detected by %d nodes; nearest %s at %s (3D slant range; max SNR %.1f dB); position/approach estimate unavailable at raw RF cue stage"),
        *OperatorTrackId,
        SortedNodeIds.Num(),
        *NearestNodeText,
        *NearestDistanceText,
        MaxSnrDb);
    UE_LOG(LogTemp, Warning, TEXT("%s. Detection-only cue; no engagement action."), *Message);
    if (GEngine)
    {
        // A stable per-target key updates its alert instead of stacking duplicate lines.
        const uint64 AlertMessageKey =
            (static_cast<uint64>(GetTypeHash(OperatorTrackId)) << 32) | 0x54524941ULL; // "TRIA"
        GEngine->AddOnScreenDebugMessage(AlertMessageKey, 5.0f, FColor(255, 140, 0), Message);
    }

    TArray<TSharedPtr<FJsonValue>> NodeIdValues;
    NodeIdValues.Reserve(SortedNodeIds.Num());
    TArray<TSharedPtr<FJsonValue>> ConfirmingNodeValues;
    ConfirmingNodeValues.Reserve(SortedNodeIds.Num());
    for (const FString& NodeId : SortedNodeIds)
    {
        NodeIdValues.Add(MakeShared<FJsonValueString>(NodeId));
        const double* DistanceMeters = ConfirmingNodeDistancesMeters.Find(NodeId);
        if (DistanceMeters && FMath::IsFinite(*DistanceMeters) && *DistanceMeters >= 0.0)
        {
            TSharedRef<FJsonObject> ConfirmingNode = MakeShared<FJsonObject>();
            ConfirmingNode->SetStringField(TEXT("nodeId"), NodeId);
            ConfirmingNode->SetNumberField(TEXT("slantDistanceMeters"), *DistanceMeters);
            ConfirmingNodeValues.Add(MakeShared<FJsonValueObject>(ConfirmingNode));
        }
    }

    TSharedRef<FJsonObject> Alert = MakeShared<FJsonObject>();
    Alert->SetStringField(TEXT("timestampUtc"), FDateTime::UtcNow().ToIso8601());
    Alert->SetNumberField(TEXT("simulationSeconds"), SimulationSeconds);
    Alert->SetStringField(TEXT("alertType"), TEXT("rf_multinode_preliminary_cue"));
    Alert->SetStringField(TEXT("cueLevel"), TEXT("preliminary"));
    Alert->SetStringField(TEXT("classification"), TEXT("unclassified_emitter"));
    Alert->SetStringField(TEXT("hostilityAssessment"), TEXT("not_inferred"));
    Alert->SetStringField(TEXT("evidenceBasis"), TEXT("rf_detection_by_distinct_nodes"));
    Alert->SetStringField(TEXT("trackId"), OperatorTrackId);
    Alert->SetStringField(TEXT("identifierSemantics"), TEXT("deterministic_pseudonym_no_raw_actor_or_emitter_id"));
    Alert->SetBoolField(TEXT("positionEstimateAvailable"), false);
    Alert->SetBoolField(TEXT("approachEstimateAvailable"), false);
    Alert->SetBoolField(TEXT("bearingEstimateAvailable"), false);
    Alert->SetStringField(TEXT("positionApproachSemantics"), TEXT("unavailable_at_raw_rf_cue_stage"));
    Alert->SetNumberField(TEXT("confirmingNodeCount"), SortedNodeIds.Num());
    Alert->SetArrayField(TEXT("confirmingNodeIds"), NodeIdValues);
    Alert->SetArrayField(TEXT("confirmingNodes"), ConfirmingNodeValues);
    Alert->SetNumberField(TEXT("minimumConfirmingNodes"), FMath::Max(ScenarioConfig.AlertMinimumConfirmingNodes, 1));
    Alert->SetNumberField(TEXT("maxSnrDb"), MaxSnrDb);
    Alert->SetBoolField(TEXT("distanceValid"), bHasValidDistance);
    Alert->SetStringField(TEXT("distanceSemantics"), TEXT("sensor_to_target_3d_slant_range"));
    if (bHasValidDistance)
    {
        Alert->SetStringField(TEXT("nearestConfirmingNodeId"), NearestNodeId);
        Alert->SetNumberField(TEXT("nearestConfirmingNodeDistanceMeters"), NearestDistanceMeters);
    }
    Alert->SetStringField(TEXT("weatherProfile"), ActiveWeatherProfileName);
    Alert->SetBoolField(TEXT("airSimVisualWeatherApplied"), bAirSimVisualWeatherApplied);
    Alert->SetNumberField(TEXT("weatherRainRateMillimetersPerHour"), ActiveWeatherRainRateMillimetersPerHour);
    Alert->SetNumberField(TEXT("weatherVisibilityMeters"), ActiveWeatherVisibilityMeters);
    Alert->SetBoolField(TEXT("detectionOnly"), true);
    Alert->SetStringField(TEXT("actionsTaken"), TEXT("none"));
    // AppendAlertRecord enforces the bounded file/count policy independently.
    // Reaching that storage cap must never silence later on-screen/log cues.
    AppendAlertRecord(Alert);
}

void ATRIADSensorFusionScenarioManager::AppendAlertRecord(const TSharedRef<FJsonObject>& AlertRecord)
{
    if (AlertRecordsWritten >= FMath::Max(ScenarioConfig.MaxAlertRecords, 1))
    {
        return;
    }

    if (bAlertsActive)
    {
        FString JsonLine;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonLine);
        FJsonSerializer::Serialize(AlertRecord, Writer);
        JsonLine.AppendChar(TEXT('\n'));
        const int64 LineBytes = Utf8Length(JsonLine);
        if (AlertBytesWritten + LineBytes > FMath::Max<int64>(ScenarioConfig.MaxTelemetryFileBytes, 1024))
        {
            bAlertsActive = false;
            UE_LOG(LogTemp, Display, TEXT("TRIAD alert JSONL stopped: byte limit reached."));
        }
        else
        {
            PendingAlertBuffer += JsonLine;
            AlertBytesWritten += LineBytes;
        }
    }

    ++AlertRecordsWritten;
}

void ATRIADSensorFusionScenarioManager::InitializeTelemetry()
{
    FString TelemetryOverride;
    if (FParse::Value(FCommandLine::Get(), TEXT("-TRIADTelemetryDir="), TelemetryOverride) &&
        !TelemetryOverride.IsEmpty())
    {
        TelemetryDirectory = FPaths::ConvertRelativePathToFull(TelemetryOverride);
    }
    else
    {
        TelemetryDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SingaporeSensorFusion"));
    }
    IFileManager::Get().MakeDirectory(*TelemetryDirectory, true);
    UE_LOG(LogTemp, Display, TEXT("TRIAD telemetry directory: %s"), *TelemetryDirectory);

    const FString SessionId = FString::Printf(
        TEXT("%s_%s"),
        *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    JsonlTelemetryPath = FPaths::Combine(TelemetryDirectory, FString::Printf(TEXT("rf_links_%s.jsonl"), *SessionId));
    CsvTelemetryPath = FPaths::Combine(TelemetryDirectory, FString::Printf(TEXT("rf_links_%s.csv"), *SessionId));
    AlertJsonlPath = FPaths::Combine(TelemetryDirectory, FString::Printf(TEXT("alerts_%s.jsonl"), *SessionId));
    LatestRFSnapshotPath = FPaths::Combine(TelemetryDirectory, TEXT("latest_rf_snapshot.json"));

    bJsonlActive = ScenarioConfig.bWriteJsonl &&
        FFileHelper::SaveStringToFile(TEXT(""), *JsonlTelemetryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    const FString CsvHeader =
        TEXT("timestamp_utc,simulation_seconds,node_id,target_actor,emitter_id,hostile_scenario_truth,frequency_ghz,node_longitude_deg,node_latitude_deg,node_height_m,target_longitude_deg,target_latitude_deg,target_height_m,distance_m,azimuth_deg,elevation_deg,line_of_sight,blocking_actor,fspl_db,obstruction_loss_db,received_power_dbm,noise_floor_dbm,snr_db,frequency_supported,in_range,above_sensitivity,detected,alert_evidence,weather_profile,airsim_visual_weather_applied,weather_rain_rate_mm_per_hour,weather_visibility_m,weather_rf_specific_attenuation_db_per_km,weather_rf_loss_db,")
        TEXT("propagation_mode,rf_propagation_readiness,rf_propagation_degraded,rf_propagation_path_valid,rf_geometry_query_id,rf_geometry_sha256,rf_geometry_schema_version,rf_geometry_status,rf_material_catalog_id,rf_material_catalog_sha256,rf_material_catalog_schema_version,rf_path_id,rf_path_kind,rf_material_calibration_state,total_propagation_loss_db,interaction_loss_db,system_loss_db,total_link_loss_db,propagation_failure_reason,ready_for_survey_truth,rf_contract_sha256,rf_coordinate_semantics,rf_interaction_trace_json\n");
    bCsvActive = ScenarioConfig.bWriteCsv &&
        FFileHelper::SaveStringToFile(CsvHeader, *CsvTelemetryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    bAlertsActive = ScenarioConfig.bEnableThreatAlerts &&
        FFileHelper::SaveStringToFile(TEXT(""), *AlertJsonlPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    CsvBytesWritten = bCsvActive ? Utf8Length(CsvHeader) : 0;
    AlertBytesWritten = 0;
    AlertRecordsWritten = 0;
    PendingJsonlBuffer.Empty();
    PendingCsvBuffer.Empty();
    PendingAlertBuffer.Empty();
    CurrentDetectedRFSnapshotLinks.Reset();
    bLiveSnapshotWriteFailureLogged = false;
    LastAlertSimulationSecondsByTarget.Empty();
    CurrentSearchRadarDetections.Reset();
    PreviousRadarTargetSampleByActor.Reset();
}

void ATRIADSensorFusionScenarioManager::AppendTelemetryRecord(
    const TSharedRef<FJsonObject>& JsonRecord,
    const FString& CsvRecord)
{
    if (TelemetryRecordsWritten >= FMath::Max(ScenarioConfig.MaxTelemetryRecords, 1))
    {
        DisableTelemetryFormat(true, TEXT("record-count limit reached"));
        DisableTelemetryFormat(false, TEXT("record-count limit reached"));
        return;
    }

    bool bWroteAnyFormat = false;
    if (bJsonlActive)
    {
        FString JsonLine;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonLine);
        FJsonSerializer::Serialize(JsonRecord, Writer);
        JsonLine.AppendChar(TEXT('\n'));
        const int64 LineBytes = Utf8Length(JsonLine);
        if (JsonlBytesWritten + LineBytes > FMath::Max<int64>(ScenarioConfig.MaxTelemetryFileBytes, 1024))
        {
            DisableTelemetryFormat(true, TEXT("JSONL byte limit reached"));
        }
        else
        {
            PendingJsonlBuffer += JsonLine;
            JsonlBytesWritten += LineBytes;
            bWroteAnyFormat = true;
        }
    }

    if (bCsvActive)
    {
        const int64 LineBytes = Utf8Length(CsvRecord);
        if (CsvBytesWritten + LineBytes > FMath::Max<int64>(ScenarioConfig.MaxTelemetryFileBytes, 1024))
        {
            DisableTelemetryFormat(false, TEXT("CSV byte limit reached"));
        }
        else
        {
            PendingCsvBuffer += CsvRecord;
            CsvBytesWritten += LineBytes;
            bWroteAnyFormat = true;
        }
    }

    if (bWroteAnyFormat)
    {
        ++TelemetryRecordsWritten;
    }
}

void ATRIADSensorFusionScenarioManager::FlushTelemetryBuffers()
{
    if (!PendingJsonlBuffer.IsEmpty())
    {
        const bool bWriteSucceeded = FFileHelper::SaveStringToFile(
            PendingJsonlBuffer,
            *JsonlTelemetryPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
            &IFileManager::Get(),
            FILEWRITE_Append);
        PendingJsonlBuffer.Empty();
        if (!bWriteSucceeded)
        {
            DisableTelemetryFormat(true, TEXT("JSONL batch write failed"));
        }
    }

    if (!PendingCsvBuffer.IsEmpty())
    {
        const bool bWriteSucceeded = FFileHelper::SaveStringToFile(
            PendingCsvBuffer,
            *CsvTelemetryPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
            &IFileManager::Get(),
            FILEWRITE_Append);
        PendingCsvBuffer.Empty();
        if (!bWriteSucceeded)
        {
            DisableTelemetryFormat(false, TEXT("CSV batch write failed"));
        }
    }

    if (!PendingAlertBuffer.IsEmpty())
    {
        const bool bWriteSucceeded = FFileHelper::SaveStringToFile(
            PendingAlertBuffer,
            *AlertJsonlPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
            &IFileManager::Get(),
            FILEWRITE_Append);
        PendingAlertBuffer.Empty();
        if (!bWriteSucceeded)
        {
            bAlertsActive = false;
            UE_LOG(LogTemp, Warning, TEXT("TRIAD alert JSONL stopped: batch write failed."));
        }
    }
}

void ATRIADSensorFusionScenarioManager::DisableTelemetryFormat(bool bJsonl, const FString& Reason)
{
    bool& bFormatActive = bJsonl ? bJsonlActive : bCsvActive;
    if (bFormatActive)
    {
        bFormatActive = false;
        UE_LOG(LogTemp, Display, TEXT("TRIAD %s telemetry stopped: %s."), bJsonl ? TEXT("JSONL") : TEXT("CSV"), *Reason);
    }
}
