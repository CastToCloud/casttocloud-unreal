// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedSettingsDetailsCustomization.h"

#include <DetailLayoutBuilder.h>
#include <PropertyCustomizationHelpers.h>
#include <ScopedTransaction.h>

#include "CtcSharedSettings.h"

namespace
{
	TArray<UFunction*> GetCallInSettingsFunctions(const UClass* InClass)
	{
		TArray<UFunction*> Result;
		for (TFieldIterator<UFunction> FunctionIter(InClass, EFieldIterationFlags::None); FunctionIter; ++FunctionIter)
		{
			if (FunctionIter->HasMetaData(TEXT("CallInSettings")) && FunctionIter->ParmsSize == 0)
			{
				Result.Add(*FunctionIter);
			}
		}

		return Result;
	}

	FReply ExecuteCallInSettingsFunction(TWeakObjectPtr<UFunction> InWeakFunction)
	{
		if (UFunction* Function = InWeakFunction.Get())
		{
			const FString FunctionName = Function->GetName();
			const FString OperationName = FString::Printf(TEXT("Called Settings Function: %s"), *FunctionName);
			FScopedTransaction Transaction(FText::FromString(OperationName));

			TStrongObjectPtr CDO(Function->GetOwnerClass()->GetDefaultObject(false));
			TStrongObjectPtr CallingFunction(Function);

			FEditorScriptExecutionGuard ScriptGuard;
			CDO->ProcessEvent(Function, nullptr);
		}

		return FReply::Handled();
	}

	bool CanExecuteCallInSettingsFunction(TWeakObjectPtr<UFunction> InWeakFunction)
	{
		bool bCanExecute = false;

		if (UFunction* Function = InWeakFunction.Get())
		{
			// TODO: It would be super cool if CanCallInSettings could either report a tooltip message like: "already set" or collapse the button if unpressable.
			const FString CanCallInSettingsFunctionName = Function->GetMetaData(TEXT("CanCallInSettings"));

			TStrongObjectPtr CDO(Function->GetOwnerClass()->GetDefaultObject(false));
			if (UFunction* CanCallInSettingsFunction = FindUField<UFunction>(CDO->GetClass(), *CanCallInSettingsFunctionName))
			{
				FEditorScriptExecutionGuard ScriptGuard;
				CDO->ProcessEvent(CanCallInSettingsFunction, &bCanExecute);
			}
		}

		return bCanExecute;
	}
} // namespace

TSharedRef<IDetailCustomization> FCtcSharedSettingsDetailsCustomization::MakeInstance()
{
	return MakeShared<FCtcSharedSettingsDetailsCustomization>();
}

void FCtcSharedSettingsDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// NOTE: for convince we want to ensure the Shared category is always first.
	DetailBuilder.EditCategory(TEXT("Shared")).SetSortOrder(1);

	TArray<UFunction*> Functions = GetCallInSettingsFunctions(UCtcSharedSettings::StaticClass());

	FPropertyFunctionCallDelegates::FOnExecute ExecuteFunctionCallback = FPropertyFunctionCallDelegates::FOnExecute::CreateStatic(&ExecuteCallInSettingsFunction);
	FPropertyFunctionCallDelegates::FOnCanExecute CanExecuteFunctionCallback = FPropertyFunctionCallDelegates::FOnCanExecute::CreateStatic(&CanExecuteCallInSettingsFunction);
	FPropertyFunctionCallDelegates FunctionDelegates(ExecuteFunctionCallback, CanExecuteFunctionCallback);

	PropertyCustomizationHelpers::AddFunctionCallWidgets(DetailBuilder, Functions, FunctionDelegates);
}
