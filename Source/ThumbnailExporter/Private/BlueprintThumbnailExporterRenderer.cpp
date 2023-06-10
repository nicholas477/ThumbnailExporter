// Copyright 2023 Big Cat Energising. All Rights Reserved.


#include "BlueprintThumbnailExporterRenderer.h"

#include "ThumbnailExporterSettings.h"
#include "ThumbnailExporterThumbnailDummy.h"
#include "ThumbnailExporterScene.h"
#include "CanvasTypes.h"
#include "EngineUtils.h"

UBlueprintThumbnailExporterRenderer::UBlueprintThumbnailExporterRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UBlueprintThumbnailExporterRenderer::~UBlueprintThumbnailExporterRenderer()
{

}

void UBlueprintThumbnailExporterRenderer::DrawThumbnailWithConfig(const FThumbnailCreationConfig& CreationConfig, UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	bool bCanRender = false;
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);

	// Strict validation - it may hopefully fix UE-35705.
	const bool bIsBlueprintValid = IsValid(Blueprint)
		&& IsValid(Blueprint->GeneratedClass)
		&& Blueprint->bHasBeenRegenerated
		//&& Blueprint->IsUpToDate() - This condition makes the thumbnail blank whenever the BP is dirty. It seems too strict.
		&& !Blueprint->bBeingCompiled
		&& !Blueprint->HasAnyFlags(RF_Transient);
	if (bIsBlueprintValid)
	{
		FThumbnailExporterScene& ThumbnailScene = GetThumbnailScene(CreationConfig);

		ThumbnailScene.SetBlueprint(Blueprint);

		bCanRender = true;
	}

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object);
	if (IsValid(StaticMesh))
	{
		FThumbnailExporterScene& ThumbnailScene = GetThumbnailScene(CreationConfig);

		ThumbnailScene.SetStaticMesh(StaticMesh);
		ThumbnailScene.GetScene()->UpdateSpeedTreeWind(0.0);

		bCanRender = true;
	}

	if (bCanRender)
	{
		FThumbnailExporterScene& ThumbnailScene = GetThumbnailScene(CreationConfig);
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene.GetScene(), FEngineShowFlags(ESFIM_Game))
			.SetTime(UThumbnailRenderer::GetTime())
			.SetDeferClear(true)
			.SetAdditionalViewFamily(bAdditionalViewFamily));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.SetScreenPercentage(false);
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.Fog = 0;
		ViewFamily.EngineShowFlags.Atmosphere = 0;
		ViewFamily.EngineShowFlags.LOD = 0;
		ViewFamily.EngineShowFlags.AntiAliasing = 0;
		ViewFamily.EngineShowFlags.Bloom = CreationConfig.bEnableBloom;

		ViewFamily.SceneCaptureSource = CreationConfig.ThumbnailCaptureSource;
		ViewFamily.SceneCaptureCompositeMode = CreationConfig.ThumbnailCompositeMode;

		FSceneView* View = ThumbnailScene.CreateView(&ViewFamily, X, Y, Width, Height);
		View->BackgroundColor = CreationConfig.GetAdjustedBackgroundColor();
		RenderViewFamily(Canvas, &ViewFamily, View);
	}
}

bool UBlueprintThumbnailExporterRenderer::CanVisualizeAsset(UObject* Object)
{
	if (Cast<UThumbnailExporterThumbnailDummy>(Object))
	{
		return true;
	}

	if (Cast<UStaticMesh>(Object))
	{
		return true;
	}

	return Super::CanVisualizeAsset(Object);
}

void UBlueprintThumbnailExporterRenderer::BeginDestroy()
{
	for (FThumbnailExporterScene* ThumbnailScene : ThumbnailScenes)
	{
		delete ThumbnailScene;
	}
	ThumbnailScenes.Empty();

	Super::BeginDestroy();
}

FThumbnailExporterScene& UBlueprintThumbnailExporterRenderer::GetThumbnailScene(const FThumbnailCreationConfig& CreationConfig)
{
	for (FThumbnailExporterScene* ThumbnailScene : ThumbnailScenes)
	{
		if (ThumbnailScene->GetBackgroundMeshesHidden() == CreationConfig.bHideThumbnailBackgroundMeshes)
		{
			return *ThumbnailScene;
		}
	}

	return *ThumbnailScenes.Add_GetRef(new FThumbnailExporterScene(CreationConfig.bHideThumbnailBackgroundMeshes));
}