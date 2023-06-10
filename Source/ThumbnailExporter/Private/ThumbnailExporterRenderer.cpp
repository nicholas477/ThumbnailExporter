// Copyright 2023 Big Cat Energising. All Rights Reserved.


#include "ThumbnailExporterRenderer.h"
#include "ThumbnailExporterSettings.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "TextureCompiler.h"
#include "Misc/ScopedSlowTask.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ThumbnailRendering/TextureThumbnailRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasTypes.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include "ThumbnailExporterThumbnailDummy.h"
#include "BlueprintThumbnailExporterRenderer.h"

#if ENGINE_MINOR_VERSION == 0
static void TransitionAndCopyTexture(FRHICommandList& RHICmdList, FRHITexture* Source, FRHITexture* Destination, const FRHICopyTextureInfo& CopyInfo)
{
	FRHITransitionInfo TransitionsBefore[] = {
		FRHITransitionInfo(Source, ERHIAccess::SRVMask, ERHIAccess::CopySrc),
		FRHITransitionInfo(Destination, ERHIAccess::SRVMask, ERHIAccess::CopyDest)
	};

	RHICmdList.Transition(MakeArrayView(TransitionsBefore, UE_ARRAY_COUNT(TransitionsBefore)));

	RHICmdList.CopyTexture(Source, Destination, CopyInfo);

	FRHITransitionInfo TransitionsAfter[] = {
		FRHITransitionInfo(Source, ERHIAccess::CopySrc, ERHIAccess::SRVMask),
		FRHITransitionInfo(Destination, ERHIAccess::CopyDest, ERHIAccess::SRVMask)
	};

	RHICmdList.Transition(MakeArrayView(TransitionsAfter, UE_ARRAY_COUNT(TransitionsAfter)));
}
#endif

FObjectThumbnail* FThumbnailExporterRenderer::GenerateThumbnail(const FThumbnailCreationConfig& CreationConfig, UObject* InObject)
{
	// Does the object support thumbnails?
	FThumbnailRenderingInfo* RenderInfo = GUnrealEd ? GUnrealEd->GetThumbnailManager()->GetRenderingInfo(UThumbnailExporterThumbnailDummy::StaticClass()->ClassDefaultObject) : nullptr;
	if (RenderInfo != NULL && RenderInfo->Renderer != NULL)
	{
		// Set the size of cached thumbnails
		const int32 ImageWidth = CreationConfig.ThumbnailSize;
		const int32 ImageHeight = CreationConfig.ThumbnailSize;

		// For cached thumbnails we want to make sure that textures are fully streamed in so that the thumbnail we're saving won't have artifacts
		// However, this can add 30s - 100s to editor load
		//@todo - come up with a cleaner solution for this, preferably not blocking on texture streaming at all but updating when textures are fully streamed in
		ThumbnailTools::EThumbnailTextureFlushMode::Type TextureFlushMode = ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush;

		// Generate the thumbnail
		FObjectThumbnail NewThumbnail;
		FThumbnailExporterRenderer::RenderThumbnail(
			CreationConfig, InObject, ImageWidth, ImageHeight, TextureFlushMode, NULL,
			&NewThumbnail);		// Out

		UPackage* MyOutermostPackage = InObject->GetOutermost();
		return ThumbnailTools::CacheThumbnail(InObject->GetFullName(), &NewThumbnail, MyOutermostPackage);
	}

	return NULL;
}

void FThumbnailExporterRenderer::RenderThumbnail(const FThumbnailCreationConfig& CreationConfig, UObject* InObject, const uint32 InImageWidth, const uint32 InImageHeight, ThumbnailTools::EThumbnailTextureFlushMode::Type InFlushMode, FTextureRenderTargetResource* InTextureRenderTargetResource, FObjectThumbnail* OutThumbnail)
{
	if (!FApp::CanEverRender())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FThumbnailExporterRenderer::RenderThumbnail);

	// Renderer must be initialized before generating thumbnails
	check(GIsRHIInitialized);

	// Store dimensions
	if (OutThumbnail)
	{
		OutThumbnail->SetImageSize(InImageWidth, InImageHeight);
	}

	// Grab the actual render target resource from the texture.  Note that we're absolutely NOT ALLOWED to
	// dereference this pointer.  We're just passing it along to other functions that will use it on the render
	// thread.  The only thing we're allowed to do is check to see if it's NULL or not.
	FTextureRenderTargetResource* RenderTargetResource = InTextureRenderTargetResource;
	if (RenderTargetResource == NULL)
	{
		// No render target was supplied, just use a scratch texture render target
		const uint32 MinRenderTargetSize = FMath::Max(InImageWidth, InImageHeight);
		UTextureRenderTarget2D* RenderTargetTexture = GEditor->GetScratchRenderTarget(MinRenderTargetSize);
		check(RenderTargetTexture != NULL);
		RenderTargetTexture->ClearColor = CreationConfig.GetAdjustedBackgroundColor();
		RenderTargetTexture->InitAutoFormat(RenderTargetTexture->GetSurfaceWidth(), RenderTargetTexture->GetSurfaceHeight());
		RenderTargetTexture->UpdateResourceImmediate(true);

		// Make sure the input dimensions are OK.  The requested dimensions must be less than or equal to
		// our scratch render target size.
		check(InImageWidth <= RenderTargetTexture->GetSurfaceWidth());
		check(InImageHeight <= RenderTargetTexture->GetSurfaceHeight());

		RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();
	}
	check(RenderTargetResource != NULL);

	// Create a canvas for the render target and clear it
	FCanvas Canvas(RenderTargetResource, NULL, FGameTime::GetTimeSinceAppStart(), GMaxRHIFeatureLevel);
	Canvas.Clear(CreationConfig.GetAdjustedBackgroundColor());

	// Get the rendering info for this object
	FThumbnailRenderingInfo* RenderInfo = GUnrealEd ? GUnrealEd->GetThumbnailManager()->GetRenderingInfo(UThumbnailExporterThumbnailDummy::StaticClass()->ClassDefaultObject) : nullptr;

	// Wait for all textures to be streamed in before we render the thumbnail
	// @todo CB: This helps but doesn't result in 100%-streamed-in resources every time! :(
	if (InFlushMode == ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush)
	{
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->ProcessAsyncResults(false, true);
		}

		if (UTexture* Texture = Cast<UTexture>(InObject))
		{
			FTextureCompilingManager::Get().FinishCompilation({ Texture });
		}

		FlushAsyncLoading();

		IStreamingManager::Get().StreamAllResources(100.0f);
	}

	if (RenderInfo != NULL && RenderInfo->Renderer != NULL)
	{
		// Make sure we suppress any message dialogs that might result from constructing
		// or initializing any of the renderable objects.
		TGuardValue<bool> Unattended(GIsRunningUnattendedScript, true);

		// Draw the thumbnail
		const int32 XPos = 0;
		const int32 YPos = 0;
		const bool bAdditionalViewFamily = false;

		UBlueprintThumbnailExporterRenderer* OurThumbnailRenderer = Cast<UBlueprintThumbnailExporterRenderer>(RenderInfo->Renderer);
		check(OurThumbnailRenderer != nullptr);

		OurThumbnailRenderer->DrawThumbnailWithConfig(
			CreationConfig,
			InObject,
			XPos,
			YPos,
			InImageWidth,
			InImageHeight,
			RenderTargetResource,
			&Canvas,
			bAdditionalViewFamily
		);
	}

	// Tell the rendering thread to draw any remaining batched elements
	Canvas.Flush_GameThread();

	ENQUEUE_RENDER_COMMAND(UpdateThumbnailRTCommand)(
		[RenderTargetResource](FRHICommandListImmediate& RHICmdList)
		{
			TransitionAndCopyTexture(RHICmdList, RenderTargetResource->GetRenderTargetTexture(), RenderTargetResource->TextureRHI, {});
		}
	);

	if (OutThumbnail)
	{
		const FIntRect InSrcRect(0, 0, OutThumbnail->GetImageWidth(), OutThumbnail->GetImageHeight());

		TArray<uint8>& OutData = OutThumbnail->AccessImageData();

		OutData.Empty();
		OutData.AddUninitialized(OutThumbnail->GetImageWidth() * OutThumbnail->GetImageHeight() * sizeof(FColor));

		// Copy the contents of the remote texture to system memory
		// NOTE: OutRawImageData must be a preallocated buffer!
		RenderTargetResource->ReadPixelsPtr((FColor*)OutData.GetData(), FReadSurfaceDataFlags(), InSrcRect);

		if (CreationConfig.InvertBackgroundAlpha())
		{
			for (FColor* Color = (FColor*)OutData.GetData(); Color < (FColor*)(OutData.GetData() + OutData.Num()); ++Color)
			{
				Color->A = 255 - Color->A;
			}
		}
	}
}