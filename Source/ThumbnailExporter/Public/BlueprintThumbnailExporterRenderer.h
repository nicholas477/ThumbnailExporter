// Copyright 2023 Big Cat Energising. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailRendering/BlueprintThumbnailRenderer.h"
#include "BlueprintThumbnailExporterRenderer.generated.h"

struct FThumbnailCreationConfig;
class FThumbnailExporterScene;

UCLASS()
class THUMBNAILEXPORTER_API UBlueprintThumbnailExporterRenderer : public UBlueprintThumbnailRenderer
{
	GENERATED_BODY()
	
public:
	UBlueprintThumbnailExporterRenderer(const FObjectInitializer& ObjectInitializer);
	virtual ~UBlueprintThumbnailExporterRenderer();

	virtual void DrawThumbnailWithConfig(const FThumbnailCreationConfig& CreationConfig, UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily);
	virtual bool CanVisualizeAsset(UObject* Object) override;

	virtual void BeginDestroy() override;

protected:
	FThumbnailExporterScene& GetThumbnailScene(const FThumbnailCreationConfig& CreationConfig);
	TArray<FThumbnailExporterScene*> ThumbnailScenes;
};
