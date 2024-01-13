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

void UBlueprintThumbnailExporterRenderer::DrawThumbnailWithConfig(FThumbnailCreationParams& CreationParams)
{
	bool bCanRender = false;
	UBlueprint* Blueprint = Cast<UBlueprint>(CreationParams.Object);

	FThumbnailExporterScene* ThumbnailScene = &GetThumbnailScene(CreationParams.CreationConfig);

	// Strict validation - it may hopefully fix UE-35705.
	const bool bIsBlueprintValid = IsValid(Blueprint)
		&& IsValid(Blueprint->GeneratedClass)
		&& Blueprint->bHasBeenRegenerated
		//&& Blueprint->IsUpToDate() - This condition makes the thumbnail blank whenever the BP is dirty. It seems too strict.
		&& !Blueprint->bBeingCompiled
		&& !Blueprint->HasAnyFlags(RF_Transient);
	if (bIsBlueprintValid)
	{
		ThumbnailScene->SetBlueprint(Blueprint);

		bCanRender = true;
	}

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(CreationParams.Object);
	if (IsValid(StaticMesh))
	{
		ThumbnailScene->SetStaticMesh(StaticMesh);
		ThumbnailScene->GetScene()->UpdateSpeedTreeWind(0.0);

		bCanRender = true;
	}

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(CreationParams.Object);
	if (IsValid(SkeletalMesh))
	{
		ThumbnailScene->SetSkeletalMesh(SkeletalMesh);
		bCanRender = true;
	}

	if (bCanRender)
	{
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(CreationParams.RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
			.SetTime(UThumbnailRenderer::GetTime())
			.SetDeferClear(true)
			.SetAdditionalViewFamily(CreationParams.bAdditionalViewFamily));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.SetScreenPercentage(false);
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.Fog = 0;
		ViewFamily.EngineShowFlags.Atmosphere = 0;
		ViewFamily.EngineShowFlags.LOD = 0;
		ViewFamily.EngineShowFlags.AntiAliasing = 0;
		ViewFamily.EngineShowFlags.PostProcessing = CreationParams.CreationConfig.bEnablePostProcessing;
		ViewFamily.EngineShowFlags.Bloom = CreationParams.CreationConfig.bEnableBloom;

		ViewFamily.SceneCaptureSource = CreationParams.CreationConfig.ThumbnailCaptureSource;
		ViewFamily.SceneCaptureCompositeMode = CreationParams.CreationConfig.ThumbnailCompositeMode;

		FSceneView* View = ThumbnailScene->CreateView(&ViewFamily, 0, 0, CreationParams.Width, CreationParams.Height);
		View->BackgroundColor = CreationParams.CreationConfig.GetAdjustedBackgroundColor();

		if (CreationParams.CreationDelegate.IsBound())
		{
			CreationParams.CreationConfig = CreationParams.CreationDelegate.Execute(CreationParams.CreationConfig, ThumbnailScene->GetPreviewActor().Get());
		}

		RenderViewFamily(CreationParams.Canvas, &ViewFamily, View);

		// If we used a creation delegate, then delete the scene.
		// The scene can be messed up by the creation delegate, so its better to just recreate it
		if (CreationParams.CreationDelegate.IsBound())
		{
			for (FThumbnailExporterScene* DestroyThumbnailScene : ThumbnailScenes)
			{
				if (DestroyThumbnailScene == ThumbnailScene)
				{
					delete DestroyThumbnailScene;
					ThumbnailScenes.Remove(ThumbnailScene);
					break;
				}
			}
		}
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

	if (Cast<USkeletalMesh>(Object))
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