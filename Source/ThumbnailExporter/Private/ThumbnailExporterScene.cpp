// Copyright 2023 Big Cat Energising. All Rights Reserved.


#include "ThumbnailExporterScene.h"

#include "ContentStreaming.h"
#include "EngineUtils.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

static USkeletalMesh* GetSkeletalMesh(USkeletalMeshComponent* SkelMeshComp)
{
#if ENGINE_MINOR_VERSION == 0
	return SkelMeshComp->SkeletalMesh;
#else
	return SkelMeshComp->GetSkeletalMeshAsset();
#endif
}

FThumbnailExporterScene::FThumbnailExporterScene(bool bInHideBackgroundMeshes)
	: FThumbnailPreviewScene()
	, bHideBackgroundMeshes(bInHideBackgroundMeshes)
	, NumStartingActors(0)
	, PreviewActor(nullptr)
	, CurrentBlueprint(nullptr)
{
	NumStartingActors = GetWorld()->GetCurrentLevel()->Actors.Num();

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

FSceneView* FThumbnailExporterScene::CreateView(FSceneViewFamily* ViewFamily, int32 X, int32 Y, uint32 SizeX, uint32 SizeY) const
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

bool FThumbnailExporterScene::IsValidComponentForVisualization(UActorComponent* Component)
{
	UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
	if (PrimComp && PrimComp->IsVisible() && !PrimComp->bHiddenInGame)
	{
		UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component);
		if (StaticMeshComp && StaticMeshComp->GetStaticMesh())
		{
			return true;
		}

		USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(Component);
		if (SkelMeshComp && GetSkeletalMesh(SkelMeshComp))
		{
			return true;
		}
	}

	return false;
}

void FThumbnailExporterScene::SetBlueprint(UBlueprint* Blueprint)
{
	CurrentBlueprint = Blueprint;
	UClass* BPClass = (Blueprint ? Blueprint->GeneratedClass : nullptr);
	SpawnPreviewActor(BPClass);
}

void FThumbnailExporterScene::BlueprintChanged(UBlueprint* Blueprint)
{
	if (CurrentBlueprint == Blueprint)
	{
		UClass* BPClass = (Blueprint ? Blueprint->GeneratedClass : nullptr);
		SpawnPreviewActor(BPClass);
	}
}

void FThumbnailExporterScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	// Add extra size to view slightly outside of the sphere to compensate for perspective
	const FBoxSphereBounds Bounds = GetPreviewActorBounds();

	const float HalfMeshSize = Bounds.SphereRadius * 1.15;
	const float BoundsZOffset = GetBoundsZOffset(Bounds);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	USceneThumbnailInfo* ThumbnailInfo = GetSceneThumbnailInfo(TargetDistance);
	check(ThumbnailInfo);

	OutOrigin = FVector(0, 0, -BoundsZOffset);
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}

void FThumbnailExporterScene::SpawnPreviewActor(UClass* InClass)
{
	if (PreviewActor.IsStale())
	{
		PreviewActor = nullptr;
		ClearStaleActors();
	}

	if (PreviewActor.IsValid())
	{
		if (PreviewActor->GetClass() == InClass)
		{
			return;
		}

		PreviewActor->Destroy();
		PreviewActor = nullptr;
	}
	if (InClass && !InClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_Abstract))
	{
		// Create preview actor
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.bNoFail = true;
		SpawnInfo.ObjectFlags = RF_Transient;
		PreviewActor = GetWorld()->SpawnActor<AActor>(InClass, SpawnInfo);

		if (PreviewActor.IsValid())
		{
			const FBoxSphereBounds Bounds = GetPreviewActorBounds();
			const float BoundsZOffset = GetBoundsZOffset(Bounds);
			const FTransform Transform(-Bounds.Origin + FVector(0, 0, BoundsZOffset));

			PreviewActor->SetActorTransform(Transform);
		}
	}
}

USceneThumbnailInfo* FThumbnailExporterScene::GetSceneThumbnailInfo(const float TargetDistance) const
{
	UBlueprint* Blueprint = CurrentBlueprint.Get();
	check(Blueprint);

	USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(Blueprint->ThumbnailInfo);
	if (ThumbnailInfo)
	{
		if (TargetDistance + ThumbnailInfo->OrbitZoom < 0)
		{
			ThumbnailInfo->OrbitZoom = -TargetDistance;
		}
	}
	else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	return ThumbnailInfo;
}

FBoxSphereBounds FThumbnailExporterScene::GetPreviewActorBounds() const
{
	FBoxSphereBounds Bounds(ForceInitToZero);
	if (PreviewActor.IsValid() && PreviewActor->GetRootComponent())
	{
		TArray<USceneComponent*> PreviewComponents;
		PreviewActor->GetRootComponent()->GetChildrenComponents(true, PreviewComponents);
		PreviewComponents.Add(PreviewActor->GetRootComponent());

		for (USceneComponent* PreviewComponent : PreviewComponents)
		{
			if (IsValidComponentForVisualization(PreviewComponent))
			{
				Bounds = Bounds + PreviewComponent->Bounds;
			}
		}
	}

	return Bounds;
}

void FThumbnailExporterScene::ClearStaleActors()
{
	ULevel* Level = GetWorld()->GetCurrentLevel();

	for (int32 i = NumStartingActors; i < Level->Actors.Num(); ++i)
	{
		if (Level->Actors[i])
		{
			Level->Actors[i]->Destroy();
		}
	}
}
