// Copyright 2023 Big Cat Energising. All Rights Reserved.


#include "BlueprintThumbnailExporterRenderer.h"

#include "ThumbnailExporterSettings.h"
#include "ThumbnailExporterThumbnailDummy.h"
#include "ThumbnailHelpers.h"
#include "CanvasTypes.h"
#include "ContentStreaming.h"
#include "EngineUtils.h"

class FThumbnailExporterScene : public FBlueprintThumbnailScene
{
public:
	FThumbnailExporterScene(bool bInHideBackgroundMeshes)
		: FBlueprintThumbnailScene(), bHideBackgroundMeshes(bInHideBackgroundMeshes)
	{
		// Iterate over all of the objects inside the scene and hide them

		const UWorld* MyWorld = GetWorld();
		for (TObjectIterator<UActorComponent> ObjectItr; ObjectItr; ++ObjectItr)
		{
			// skip if this object is not associated with our current game world
			if (ObjectItr->GetWorld() != MyWorld)
			{
				continue;
			}

			UActorComponent* Component = *ObjectItr;
			if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
			{
				if (bHideBackgroundMeshes)
				{
					MeshComponent->SetRenderInMainPass(false);
				}
			}
			else
			{
				continue;
			}
		}
	}

	virtual ~FThumbnailExporterScene()
	{

	}

	/** Allocates then adds an FSceneView to the ViewFamily. */
	FSceneView* CreateView(FSceneViewFamily* ViewFamily, int32 X, int32 Y, uint32 SizeX, uint32 SizeY) const
	{
		FSceneView* View = FThumbnailPreviewScene::CreateView(ViewFamily, X, Y, SizeX, SizeY);

		View->bIsSceneCapture = true;

		const float FOVDegrees = 30.f;

		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			IStreamingManager::Get().AddViewInformation(View->ViewMatrices.GetViewOrigin(), SizeX, SizeX / FMath::Tan(FOVDegrees), 10.f, false, 5.f, *It);
		}
		IStreamingManager::Get().StreamAllResources();

		return View;
	}

	bool bHideBackgroundMeshes;
};

UBlueprintThumbnailExporterRenderer::UBlueprintThumbnailExporterRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UBlueprintThumbnailExporterRenderer::~UBlueprintThumbnailExporterRenderer()
{

}

void UBlueprintThumbnailExporterRenderer::DrawThumbnailWithConfig(const FThumbnailCreationConfig& CreationConfig, UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
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
		//ViewFamily.EngineShowFlags.

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
		if (ThumbnailScene->bHideBackgroundMeshes == CreationConfig.bHideThumbnailBackgroundMeshes)
		{
			return *ThumbnailScene;
		}
	}

	return *ThumbnailScenes.Add_GetRef(new FThumbnailExporterScene(CreationConfig.bHideThumbnailBackgroundMeshes));
}