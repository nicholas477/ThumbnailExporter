// Copyright 2023 Big Cat Energising. All Rights Reserved.


#include "BlueprintThumbnailExporterRenderer.h"

#include "ThumbnailExporterSettings.h"
#include "ThumbnailExporterThumbnailDummy.h"
#include "ThumbnailExporterScene.h"
#include "CanvasTypes.h"

#include "RendererInterface.h"
#include "EngineModule.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "LegacyScreenPercentageDriver.h"
#include "RenderGraphBuilder.h"

#include "CanvasItem.h"
#include "Engine/StaticMesh.h"

UBlueprintThumbnailExporterRenderer::UBlueprintThumbnailExporterRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UBlueprintThumbnailExporterRenderer::~UBlueprintThumbnailExporterRenderer()
{

}

static ESimpleElementBlendMode GetMaterialBlendMode(const UMaterialInterface* Material)
{
	switch (Material->GetBlendMode())
	{
	case EBlendMode::BLEND_Additive:
		return ESimpleElementBlendMode::SE_BLEND_Additive;

	case EBlendMode::BLEND_Translucent:
		return ESimpleElementBlendMode::SE_BLEND_Translucent;

	default:
	case EBlendMode::BLEND_Opaque:
		return ESimpleElementBlendMode::SE_BLEND_Opaque;
	}
}

void UBlueprintThumbnailExporterRenderer::DrawThumbnailWithConfig(FThumbnailCreationParams& CreationParams)
{
	bool bCanRender = false;
	UBlueprint* Blueprint = Cast<UBlueprint>(CreationParams.Object);

	FThumbnailExporterScene* ThisThumbnailScene = &GetThumbnailScene(CreationParams.CreationConfig);

	auto DestroyThumbnailScene = [this, ThisThumbnailScene]()
	{
		for (int32 SceneIndex = 0; SceneIndex < ThumbnailScenes.Num(); ++SceneIndex)
		{
			if (ThumbnailScenes[SceneIndex] == ThisThumbnailScene)
			{
				delete ThisThumbnailScene;
				ThumbnailScenes.RemoveAtSwap(SceneIndex);
				return;
			}
		}
	};

	// Strict validation - it may hopefully fix UE-35705.
	const bool bIsBlueprintValid = IsValid(Blueprint)
		&& IsValid(Blueprint->GeneratedClass)
		&& Blueprint->bHasBeenRegenerated
		//&& Blueprint->IsUpToDate() - This condition makes the thumbnail blank whenever the BP is dirty. It seems too strict.
		&& !Blueprint->bBeingCompiled
		&& !Blueprint->HasAnyFlags(RF_Transient);
	if (bIsBlueprintValid)
	{
		ThisThumbnailScene->SetBlueprint(Blueprint);
		bCanRender = true;
	}

	UStaticMesh* StaticMesh = Cast<UStaticMesh>(CreationParams.Object);
	if (IsValid(StaticMesh))
	{
		ThisThumbnailScene->SetStaticMesh(StaticMesh);
		ThisThumbnailScene->GetScene()->UpdateSpeedTreeWind(0.0);
		bCanRender = true;
	}

	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(CreationParams.Object);
	if (IsValid(SkeletalMesh))
	{
		ThisThumbnailScene->SetSkeletalMesh(SkeletalMesh);
		bCanRender = true;
	}

	if (bCanRender)
	{
		auto RenderSinglePass = [&](const bool bApplyCreationDelegate)
		{
			FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(CreationParams.RenderTarget, ThisThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
				.SetTime(UThumbnailRenderer::GetTime())
				.SetDeferClear(true)
				.SetResolveScene(false)
				.SetAdditionalViewFamily(CreationParams.bAdditionalViewFamily));

			ViewFamily.bThumbnailRendering = true;

			if (CreationParams.bIsAlpha)
			{
				ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
				ViewFamily.EngineShowFlags.SetScreenPercentage(false);
				ViewFamily.EngineShowFlags.MotionBlur = 0;
				ViewFamily.EngineShowFlags.Fog = 0;
				ViewFamily.EngineShowFlags.DepthOfField = 0;
				ViewFamily.EngineShowFlags.LocalExposure = 0;
				ViewFamily.EngineShowFlags.EyeAdaptation = 0;
				ViewFamily.EngineShowFlags.Vignette = 0;
				ViewFamily.EngineShowFlags.Grain = 0;
				ViewFamily.EngineShowFlags.Atmosphere = 0;
				ViewFamily.EngineShowFlags.LOD = 0;
				ViewFamily.EngineShowFlags.NaniteMeshes = 1;
				ViewFamily.EngineShowFlags.NaniteStreamingGeometry = 1;
				ViewFamily.EngineShowFlags.AntiAliasing = 0;
				ViewFamily.EngineShowFlags.PostProcessMaterial = 0;
				ViewFamily.EngineShowFlags.Tonemapper = 1;
				ViewFamily.EngineShowFlags.ColorGrading = 1;
				ViewFamily.EngineShowFlags.IndirectLightingCache = 0;
				ViewFamily.EngineShowFlags.PostProcessing = false;
				ViewFamily.EngineShowFlags.Bloom = false;
				ViewFamily.bIsHDR = false;

				ViewFamily.SceneCaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
				ViewFamily.SceneCaptureCompositeMode = ESceneCaptureCompositeMode::SCCM_Composite;
			}
			else
			{
				ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
				ViewFamily.EngineShowFlags.MotionBlur = 0;

				// Resolve scene is needed for LDR rendering, no idea why
				ViewFamily.bResolveScene = true;

				ViewFamily.EngineShowFlags.SetScreenPercentage(false);
				ViewFamily.EngineShowFlags.Fog = 0;
				ViewFamily.EngineShowFlags.DepthOfField = 0;
				ViewFamily.EngineShowFlags.LocalExposure = 0;
				ViewFamily.EngineShowFlags.EyeAdaptation = 0;
				ViewFamily.EngineShowFlags.Vignette = 0;
				ViewFamily.EngineShowFlags.Grain = 0;
				ViewFamily.EngineShowFlags.Atmosphere = 0;
				ViewFamily.EngineShowFlags.LOD = 0;
				ViewFamily.EngineShowFlags.NaniteMeshes = 1;
				ViewFamily.EngineShowFlags.NaniteStreamingGeometry = 1;
				ViewFamily.EngineShowFlags.AntiAliasing = 0;
				ViewFamily.EngineShowFlags.PostProcessMaterial = 0;
				ViewFamily.EngineShowFlags.PostProcessing = CreationParams.CreationConfig.bEnablePostProcessing;
				ViewFamily.EngineShowFlags.Bloom = CreationParams.CreationConfig.bEnableBloom;

				ViewFamily.SceneCaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
				ViewFamily.SceneCaptureCompositeMode = ESceneCaptureCompositeMode::SCCM_Overwrite;
			}

			FSceneView* View = ThisThumbnailScene->CreateView(&ViewFamily, 0, 0, CreationParams.Width, CreationParams.Height);
			if (!View)
			{
				return;
			}

			View->BackgroundColor = CreationParams.CreationConfig.GetAdjustedBackgroundColor();
			if (bApplyCreationDelegate && CreationParams.CreationDelegate.IsBound())
			{
				CreationParams.CreationConfig = CreationParams.CreationDelegate.Execute(CreationParams.CreationConfig, ThisThumbnailScene->GetPreviewActor().Get());
			}

			RenderViewFamily(CreationParams.Canvas, &ViewFamily, View);
		};

		RenderSinglePass(true);
	}

	DestroyThumbnailScene();
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

	//if (Cast<UMaterialInterface>(Object))
	//{
	//	return true;
	//}

	return Super::CanVisualizeAsset(Object);
}

void UBlueprintThumbnailExporterRenderer::BeginDestroy()
{
	for (FThumbnailExporterScene* ThisThumbnailScene : ThumbnailScenes)
	{
		delete ThisThumbnailScene;
	}
	ThumbnailScenes.Empty();

	Super::BeginDestroy();
}

void UBlueprintThumbnailExporterRenderer::RenderViewFamily(FCanvas* Canvas, FSceneViewFamily* ViewFamily, FSceneView* View)
{
	if ((ViewFamily == nullptr) || (View == nullptr))
	{
		return;
	}

	check(ViewFamily->Views.Num() == 1 && (ViewFamily->Views[0] == View));

	ViewFamily->EngineShowFlags.ScreenPercentage = false;
	ViewFamily->bThumbnailRendering = true;
	if (ViewFamily->GetScreenPercentageInterface() == nullptr)
	{
		ViewFamily->SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
			*ViewFamily, /* GlobalResolutionFraction = */ 1.0f));
	}

	GetRendererModule().BeginRenderingViewFamily(Canvas, ViewFamily);
}

FThumbnailExporterScene& UBlueprintThumbnailExporterRenderer::GetThumbnailScene(const FThumbnailCreationConfig& CreationConfig)
{
	return *ThumbnailScenes.Add_GetRef(new FThumbnailExporterScene(CreationConfig.bHideThumbnailBackgroundMeshes));
}
