// Copyright 2023 Big Cat Energising. All Rights Reserved.

#include "ThumbnailExporterAsyncAction.h"

#include "ThumbnailExporter.h"

UThumbnailExporterAsyncAction* UThumbnailExporterAsyncAction::ExportThumbnailAsync(UObject* WorldContextObject, const FThumbnailCreationConfig& CreationConfig, const FAssetData& Asset, const FPreCreateThumbnail& CreationDelegate)
{
	UThumbnailExporterAsyncAction* Action = NewObject<UThumbnailExporterAsyncAction>();
	Action->WorldContextObject = WorldContextObject;
	Action->CreationConfig = CreationConfig;
	Action->Asset = Asset;
	Action->CreationDelegate = CreationDelegate;
	return Action;
}

void UThumbnailExporterAsyncAction::Activate()
{
	if (!IsValid(WorldContextObject))
	{
		HandleExportCompleted(false, FString());
		return;
	}

	RegisterWithGameInstance(WorldContextObject);
	FModuleManager::GetModuleChecked<FThumbnailExporterModule>("ThumbnailExporter").ExportThumbnailAsync(
		CreationConfig,
		Asset,
		FOnThumbnailExportCompleted::CreateUObject(this, &UThumbnailExporterAsyncAction::HandleExportCompleted),
		CreationDelegate);
}

void UThumbnailExporterAsyncAction::HandleExportCompleted(bool bSuccess, const FString& ThumbnailPath)
{
	OnCompleted.Broadcast(bSuccess, ThumbnailPath);
	SetReadyToDestroy();
	WorldContextObject = nullptr;
}
