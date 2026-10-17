// Copyright 2023 Big Cat Energising. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ThumbnailExporterRenderer.h"
#include "ThumbnailExporterSettings.h"
#include "Engine/StaticMesh.h"
#include "ObjectTools.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FThumbnailExporterDeterminismTest, "ThumbnailExporter.Rendering.DeterministicStaticMeshThumbnail", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	bool CaptureThumbnailPixels(FThumbnailCreationConfig InConfig, UObject* InObject, TArray<uint8>& OutImageData)
	{
		FObjectThumbnail* Thumbnail = FThumbnailExporterRenderer::GenerateThumbnail(InConfig, InObject);
		if (Thumbnail == nullptr)
		{
			return false;
		}

		OutImageData = Thumbnail->GetUncompressedImageData();
		return OutImageData.Num() > 0;
	}
}

bool FThumbnailExporterDeterminismTest::RunTest(const FString& Parameters)
{
	UStaticMesh* TestMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Expected test mesh /Engine/BasicShapes/Cube.Cube"), TestMesh))
	{
		return false;
	}

	FThumbnailCreationConfig Config;
	Config.ThumbnailSize = 256;
	Config.bHideThumbnailBackgroundMeshes = true;
	Config.bEnablePostProcessing = false;
	Config.bEnableBloom = false;
	Config.ThumbnailBackground = FLinearColor::Transparent;

	TArray<uint8> FirstImage;
	TArray<uint8> SecondImage;
	TArray<uint8> ThirdImage;

	if (!TestTrue(TEXT("First thumbnail capture should succeed"), CaptureThumbnailPixels(Config, TestMesh, FirstImage)))
	{
		return false;
	}
	FlushRenderingCommands();

	if (!TestTrue(TEXT("Second thumbnail capture should succeed"), CaptureThumbnailPixels(Config, TestMesh, SecondImage)))
	{
		return false;
	}
	FlushRenderingCommands();

	if (!TestTrue(TEXT("Third thumbnail capture should succeed"), CaptureThumbnailPixels(Config, TestMesh, ThirdImage)))
	{
		return false;
	}

	if (!TestEqual(TEXT("First and second captures should have identical byte counts"), FirstImage.Num(), SecondImage.Num()))
	{
		return false;
	}

	if (!TestEqual(TEXT("Second and third captures should have identical byte counts"), SecondImage.Num(), ThirdImage.Num()))
	{
		return false;
	}

	const bool bFirstMatchesSecond = FirstImage == SecondImage;
	const bool bSecondMatchesThird = SecondImage == ThirdImage;

	if (!TestTrue(TEXT("First and second captures should match exactly"), bFirstMatchesSecond))
	{
		return false;
	}

	if (!TestTrue(TEXT("Second and third captures should match exactly"), bSecondMatchesThird))
	{
		return false;
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
