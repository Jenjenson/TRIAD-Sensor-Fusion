#include "TRIADRLAssetBootstrapCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "TRIADAdversarialTrainingManager.h"
#include "TRIADRLTrainingModel.h"
#include "TRIADRLTrainingTypes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
const TCHAR* RLAssetPackageName = TEXT("/Game/TRIAD/RL/DA_TRIADRLDefault");
const TCHAR* RLAssetObjectPath = TEXT("/Game/TRIAD/RL/DA_TRIADRLDefault.DA_TRIADRLDefault");
}

UTRIADRLAssetBootstrapCommandlet::UTRIADRLAssetBootstrapCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UTRIADRLAssetBootstrapCommandlet::Main(const FString& Params)
{
    if (const UTRIADRLTrainingDefinition* Existing = LoadObject<UTRIADRLTrainingDefinition>(
            nullptr, RLAssetObjectPath, nullptr, LOAD_NoWarn))
    {
        FString ValidationError;
        if (!FTRIADRLTrainingModel::ValidateConfig(Existing->Config, ValidationError))
        {
            UE_LOG(LogTemp, Error, TEXT("Existing TRIAD RL asset is invalid and was not overwritten: %s"), *ValidationError);
            return 2;
        }
        UE_LOG(LogTemp, Display, TEXT("TRIAD RL asset already exists and is valid: %s"), RLAssetObjectPath);
        return 0;
    }

    FTRIADRLTrainingConfig Config;
    FString LoadError;
    if (!ATRIADAdversarialTrainingManager::LoadTrainingConfig(Config, LoadError))
    {
        UE_LOG(LogTemp, Error, TEXT("Could not load the JSON-backed TRIAD RL definition: %s"), *LoadError);
        return 3;
    }

    UPackage* Package = CreatePackage(RLAssetPackageName);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not create TRIAD RL asset package."));
        return 4;
    }
    UTRIADRLTrainingDefinition* Asset = NewObject<UTRIADRLTrainingDefinition>(
        Package,
        UTRIADRLTrainingDefinition::StaticClass(),
        TEXT("DA_TRIADRLDefault"),
        RF_Public | RF_Standalone);
    Asset->Config = Config;
    FAssetRegistryModule::AssetCreated(Asset);
    Package->MarkPackageDirty();

    const FString Filename = FPackageName::LongPackageNameToFilename(
        RLAssetPackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    SaveArgs.Error = GError;
    if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("Could not save TRIAD RL asset '%s'."), *Filename);
        return 5;
    }
    UE_LOG(LogTemp, Display, TEXT("Created TRIAD RL asset: %s"), RLAssetObjectPath);
    return 0;
}
