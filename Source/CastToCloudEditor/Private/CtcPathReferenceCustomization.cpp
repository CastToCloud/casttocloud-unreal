// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcPathReferenceCustomization.h"

#include <DetailWidgetRow.h>
#include <HAL/PlatformFileManager.h>
#include <PropertyHandle.h>
#include <Widgets/Input/SFilePathPicker.h>

#include "EditorDirectories.h"

namespace
{
	FString ExpandVariables(const FString& Path)
	{
		FString Result = Path;
		Result.ReplaceInline(TEXT("$(ProjectDir)/"), *FPaths::ProjectDir());
		Result.ReplaceInline(TEXT("$(EngineDir)/"), *FPaths::EngineDir());
		return FPaths::ConvertRelativePathToFull(Result);
	}

	FString AddTrailingSlash(const FString& Path)
	{
		const FString Suffix = !Path.EndsWith(TEXT("/")) ? TEXT("/") : TEXT("");
		return Path + Suffix;
	}
}

TSharedRef<IPropertyTypeCustomization> FCtcPathReferenceCustomization::MakeInstance()
{
	return MakeShared<FCtcPathReferenceCustomization>();
}

void FCtcPathReferenceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PathStringProperty = PropertyHandle->GetChildHandle("Path");
	FallbackBrowseDirectory = PropertyHandle->GetMetaData(TEXT("FallbackBrowseDirectory"));

	const FString FileTypeFilter = PropertyHandle->GetMetaData(TEXT("FileTypeFilter"));

	// clang-format off
	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(0.0f)
		.MinDesiredWidth(125.0f)
		[
			SNew(SFilePathPicker)
			.BrowseButtonImage(FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
			.BrowseButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.BrowseButtonToolTip(INVTEXT("Choose a Events Session file to import"))
			.BrowseDirectory(this, &FCtcPathReferenceCustomization::GetBrowseDirectory)
			.BrowseTitle(INVTEXT("Select session file to import..."))
			.FilePath(this, &FCtcPathReferenceCustomization::GetFilePath)
			.FileTypeFilter(FileTypeFilter)
			.OnPathPicked(this, &FCtcPathReferenceCustomization::OnFilePathPicked)
		];
	// clang-format on
}

void FCtcPathReferenceCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{

}

FString FCtcPathReferenceCustomization::GetBrowseDirectory() const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	const FString CurrentDirectory = FPaths::GetPath(GetFilePath());
	if (!CurrentDirectory.IsEmpty() && PlatformFile.DirectoryExists(*CurrentDirectory))
	{
		return AddTrailingSlash(CurrentDirectory);
	}

	const FString BrowseDirectory = ExpandVariables(FallbackBrowseDirectory.Get(TEXT("")));
	if (!BrowseDirectory.IsEmpty() && PlatformFile.CreateDirectoryTree(*BrowseDirectory))
	{
		return AddTrailingSlash(BrowseDirectory);
	}

	return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
}

FString FCtcPathReferenceCustomization::GetFilePath() const
{
	FString FilePath;
	PathStringProperty->GetValue(FilePath);

	return FilePath;
}

void FCtcPathReferenceCustomization::OnFilePathPicked(const FString& PickedPath)
{
	FString FullPickedPath = FPaths::ConvertRelativePathToFull(PickedPath);
	PathStringProperty->SetValue(FullPickedPath);
}