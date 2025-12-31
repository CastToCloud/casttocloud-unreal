// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsEditorViewerSettings.h"

#include <Containers/Ticker.h>
#include <Dom/JsonValue.h>
#include <DrawDebugHelpers.h>
#include <Engine/World.h>
#include <Misc/FileHelper.h>
#include <Serialization/JsonReader.h>
#include <Serialization/JsonSerializer.h>

UCtcAnalyticsEditorViewerSettings::UCtcAnalyticsEditorViewerSettings(const FObjectInitializer& ObjectInitializer)
{
	FCoreUObjectDelegates::OnPostObjectPropertyChanged.AddUObject(this, &UCtcAnalyticsEditorViewerSettings::OnPostObjectPropertyChanged);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCtcAnalyticsEditorViewerSettings::OnTick), 0);
}

UCtcAnalyticsEditorViewerDataSource* UCtcAnalyticsEditorViewerSettings::GetDataSourceInstance() const
{
	return DataSourceInstance;
}

bool UCtcAnalyticsEditorViewerSettings::OnTick(float DeltaTime)
{
	DrawPoints();
	return true;
}

void UCtcAnalyticsEditorViewerSettings::DrawPoints()
{
	if (!DataSourceInstance)
	{
		return;
	}

	TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> Result = DataSourceInstance->GetResult();
	if (Result.HasError())
	{
		return;
	}

	const TArray<FCtcAnalyticsEditorHeatmapPoint> Points = Result.GetValue().Points;
	if (Points.IsEmpty())
	{
		return;
	}

	TArray<int32> CountArray;
	Algo::Transform(
		Points,
		CountArray,
		[](const FCtcAnalyticsEditorHeatmapPoint& Point)
		{
			return Point.Count;
		}
	);

	const int32 MinCount = *Algo::MinElement(CountArray);
	const int32 MaxCount = *Algo::MaxElement(CountArray);

	for (const FCtcAnalyticsEditorHeatmapPoint& Point : Points)
	{
		const float Alpha = static_cast<float>(Point.Count - MinCount) / (MaxCount - MinCount);
		const FColor PointColor = FLinearColor::LerpUsingHSV(DrawStartColor, DrawEndColor, Alpha).ToFColor(true);

		if (DrawShape == ECtcAnalyticsEditorViewerDrawShape::Cube && DrawMode == ECtcAnalyticsEditorViewerDrawMode::Solid)
		{
			DrawDebugSolidBox(GWorld, Point.Position, FVector::OneVector * DrawSize, PointColor);
		}
		if (DrawShape == ECtcAnalyticsEditorViewerDrawShape::Cube && DrawMode == ECtcAnalyticsEditorViewerDrawMode::Wired)
		{
			DrawDebugBox(GWorld, Point.Position, FVector::OneVector * DrawSize, PointColor);
		}
		if (DrawShape == ECtcAnalyticsEditorViewerDrawShape::Sphere && DrawMode == ECtcAnalyticsEditorViewerDrawMode::Solid)
		{
			DrawDebugSphere(GWorld, Point.Position, DrawSize, 64, PointColor);
		}
		if (DrawShape == ECtcAnalyticsEditorViewerDrawShape::Sphere && DrawMode == ECtcAnalyticsEditorViewerDrawMode::Wired)
		{
			DrawDebugSphere(GWorld, Point.Position, DrawSize, 8, PointColor);
		}
	}
}

void UCtcAnalyticsEditorViewerSettings::RefreshResult()
{
	if (DataSourceInstance)
	{
		DataSourceInstance->RefreshResult();
	}
}

void UCtcAnalyticsEditorViewerSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCtcAnalyticsEditorViewerSettings, DataSource))
	{
		if (DataSource)
		{
			DataSourceInstance = NewObject<UCtcAnalyticsEditorViewerDataSource>(this, DataSource);
		}
		else
		{
			DataSourceInstance = nullptr;
		}
	}

	RefreshResult();
}

void UCtcAnalyticsEditorViewerSettings::OnPostObjectPropertyChanged(UObject* Object, const FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (Object != DataSourceInstance)
	{
		return;
	}

	RefreshResult();
}

void UCtcAnalyticsEditorViewerDataSource::RefreshResult()
{
	checkNoEntry();
}

TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> UCtcAnalyticsEditorViewerDataSource::GetResult() const
{
	checkNoEntry();
	return MakeError(TEXT("Not implemented"));
}

double UCtcAnalyticsEditorViewerDataSource::GetBucketSize() const
{
	return Cast<UCtcAnalyticsEditorViewerSettings>(GetOuter())->DrawSize * 2;
}

void UCtcAnalyticsEditorFileViewerSource::RefreshResult()
{
	FString RawEventsJSON;
	const bool bReadSuccess = FFileHelper::LoadFileToString(RawEventsJSON, *PathReference.Path);

	if (!bReadSuccess || RawEventsJSON.IsEmpty())
	{
		CachedResult = MakeError(TEXT("Failed to read events from file."));
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Events;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawEventsJSON);
	const bool bDeserializeSuccess = FJsonSerializer::Deserialize(Reader, Events);

	if (!bDeserializeSuccess || Events.IsEmpty())
	{
		CachedResult = MakeError(TEXT("Failed to deserialize events from file."));
		return;
	}

	auto ToBucketCenter = [&](double Value) -> double
	{
		const double BucketSize = GetBucketSize();
		const int32 Index = FMath::FloorToInt(Value / BucketSize);
		return (Index + 0.5) * BucketSize;
	};

	TMap<FVector, int> MapPoints;

	for (const TSharedPtr<FJsonValue>& EventObject : Events)
	{
		double X, Y, Z;
		bool bHasAllFields = true;
		bHasAllFields &= EventObject->AsObject()->TryGetNumberField(TEXT("position_x"), X);
		bHasAllFields &= EventObject->AsObject()->TryGetNumberField(TEXT("position_y"), Y);
		bHasAllFields &= EventObject->AsObject()->TryGetNumberField(TEXT("position_z"), Z);

		if (!bHasAllFields)
		{
			continue;
		}

		FVector Location = FVector(ToBucketCenter(X), ToBucketCenter(Y), ToBucketCenter(Z));
		int& CountAtLocation = MapPoints.FindOrAdd(Location);
		CountAtLocation++;
	}

	FCtcAnalyticsEditorHeatmapPoints Result;
	for (const TPair<FVector, int>& MapPoint : MapPoints)
	{
		Result.Points.Emplace(MapPoint.Key, MapPoint.Value);
	}

	CachedResult = MakeValue(Result);
}

TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> UCtcAnalyticsEditorFileViewerSource::GetResult() const
{
	return CachedResult;
}

void UCtcAnalyticsEditorApiViewerSource::RefreshResult()
{
	CachedResult = MakeError(TEXT("Request in progress..."));

	// TODO: Trigger the async request and wait for it to finish
	// TODO: This will need to handle canceling requests in case rapid changes are done with other requets run.
}

TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> UCtcAnalyticsEditorApiViewerSource::GetResult() const
{
	return CachedResult;
}
