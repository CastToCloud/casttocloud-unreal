// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Templates/SubclassOf.h>
#include <Templates/ValueOrError.h>
#include <UObject/Object.h>
#include <UObject/SoftObjectPath.h>

#include "CtcAnalyticsEditorViewerData.h"
#include "CtcPathReference.h"

#include "CtcAnalyticsEditorViewerSettings.generated.h"

class UCtcAnalyticsEditorViewerDataSource;

UENUM()
enum class ECtcAnalyticsEditorViewerDrawShape : uint8
{
	Cube,
	Sphere,
};

UENUM()
enum class ECtcAnalyticsEditorViewerDrawMode : uint8
{
	Solid,
	Wired,
};

UCLASS()
class UCtcAnalyticsEditorViewerSettings : public UObject
{
	GENERATED_BODY()

public:
	UCtcAnalyticsEditorViewerSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Returns the currently spawned source instance
	 */
	UCtcAnalyticsEditorViewerDataSource* GetDataSourceInstance() const;

protected:
	UPROPERTY(EditAnywhere, Category = "CastToCloud", meta = (ClampMin = 1.0, Units = cm))
	double DrawSize = 100.0;

	UPROPERTY(EditAnywhere, Category = "CastToCloud")
	ECtcAnalyticsEditorViewerDrawShape DrawShape = ECtcAnalyticsEditorViewerDrawShape::Cube;

	UPROPERTY(EditAnywhere, Category = "CastToCloud")
	ECtcAnalyticsEditorViewerDrawMode DrawMode = ECtcAnalyticsEditorViewerDrawMode::Solid;

	UPROPERTY(EditAnywhere, Category = "CastToCloud")
	FColor DrawStartColor = FColor::Green;

	UPROPERTY(EditAnywhere, Category = "CastToCloud")
	FColor DrawEndColor = FColor::Red;

	UPROPERTY(EditAnywhere, Category = "CastToCloud", meta = (ShowDisplayNames))
	TSubclassOf<UCtcAnalyticsEditorViewerDataSource> DataSource;

	UPROPERTY(Transient)
	TObjectPtr<UCtcAnalyticsEditorViewerDataSource> DataSourceInstance;

private:
	/**
	 * Callback executed every frame
	 */
	bool OnTick(float DeltaTime);
	/**
	 * Callback executed when a property of any object is changed
	 */
	void OnPostObjectPropertyChanged(UObject* Object, const FPropertyChangedChainEvent& PropertyChangedEvent);
	/**
	 * Performs the drawing based on the current source instance's results and own settings
	 */
	void DrawPoints();
	/**
	 * Requests an async refresh of the data of the source instance
	 */
	void RefreshResult();

	//~Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~End UObject interface

	friend class UCtcAnalyticsEditorViewerDataSource;
};

UCLASS(Abstract, EditInlineNew)
class UCtcAnalyticsEditorViewerDataSource : public UObject
{
	GENERATED_BODY()

public:
	/*
	 * Triggers an async recompute of the data displayed.
	 */
	virtual void RefreshResult();
	/*
	 * Returns the current state of the result.
	 */
	virtual TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> GetResult() const;

protected:
	double GetBucketSize() const;
};

UCLASS(DisplayName = "From File")
class UCtcAnalyticsEditorFileViewerSource : public UCtcAnalyticsEditorViewerDataSource
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "CastToCloud", meta = (FileTypeFilter = "Events Session (*.json)|*.json", FallbackBrowseDirectory = "$(ProjectDir)/Saved/CastToCloud/Analytics"))
	FCtcPathReference PathReference;

private:
	//~Begin UCtcAnalyticsEditorViewerSource interface
	virtual void RefreshResult() override;
	virtual TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> GetResult() const override;
	//~End UCtcAnalyticsEditorViewerSource interface

	TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> CachedResult = MakeError("Not initialized");
};

UCLASS(DisplayName = "From API")
class UCtcAnalyticsEditorApiViewerSource : public UCtcAnalyticsEditorViewerDataSource
{
	GENERATED_BODY()

	//~Begin UCtcAnalyticsEditorViewerSource interface
	virtual void RefreshResult() override;
	virtual TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> GetResult() const override;
	//~End UCtcAnalyticsEditorViewerSource interface

protected:
	UPROPERTY(EditAnywhere, Category = "CastToCloud")
	FString Parameters;

	TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> CachedResult = MakeError("Not initialized");
};

// TODO: In the future enable this to aggreate all the valid json's from a whole folder.
// UCLASS(DisplayName = "From Folder")
// UCtcAnalyticsEditorFolderViewerSource
// FDirectoryPath DirectoryPath;
