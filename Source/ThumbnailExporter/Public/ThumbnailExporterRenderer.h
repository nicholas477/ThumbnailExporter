// Copyright 2023 Big Cat Energising. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectTools.h"

struct FThumbnailCreationConfig;
class FObjectThumbnail;

class THUMBNAILEXPORTER_API FThumbnailExporterRenderer
{
public:
	static FObjectThumbnail* GenerateThumbnail(const FThumbnailCreationConfig& CreationConfig, UObject* InObject);
	static void RenderThumbnail(const FThumbnailCreationConfig& CreationConfig, UObject* InObject, const uint32 InImageWidth, const uint32 InImageHeight, ThumbnailTools::EThumbnailTextureFlushMode::Type InFlushMode, FTextureRenderTargetResource* InRenderTargetResource = NULL, FObjectThumbnail* OutThumbnail = NULL);
};
