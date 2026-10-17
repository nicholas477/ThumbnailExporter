// Copyright 2023 Big Cat Energising. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ThumbnailExporterBlueprintFunctionLibrary.h"
#include "ThumbnailExporterAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FThumbnailExporterCompleted, bool, bSuccess, const FString&, ThumbnailPath);

UCLASS()
class THUMBNAILEXPORTER_API UThumbnailExporterAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FThumbnailExporterCompleted OnCompleted;

	UFUNCTION(BlueprintCallable, Category = "Thumbnail Exporter", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", AutoCreateRefTerm = "CreationDelegate"))
	static UThumbnailExporterAsyncAction* ExportThumbnailAsync(UObject* WorldContextObject, const FThumbnailCreationConfig& CreationConfig, const FAssetData& Asset, const FPreCreateThumbnail& CreationDelegate);

	virtual void Activate() override;

private:
	void HandleExportCompleted(bool bSuccess, const FString& ThumbnailPath);

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;

	FThumbnailCreationConfig CreationConfig;
	FAssetData Asset;
	FPreCreateThumbnail CreationDelegate;
};
