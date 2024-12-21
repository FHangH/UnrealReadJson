#pragma once

#include "CoreMinimal.h"
#include "JsonData.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Async_ReadJson.generated.h"

namespace TempJsonNode
{
	struct FJsonNode
	{
		TSharedPtr<FJsonObject> JsonObject;
		FString CurrentPath;
	};
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReadJsonSingature, FParsedData, ParsedData);

UCLASS(BlueprintType, meta=(ExposedAsyncProxy="AsyncTask"))
class UNREALREADJSON_API UAsync_ReadJson : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable, Category="AsyncTask|Async_ReadJson", DisplayName="Completed")
	FReadJsonSingature OnReadJsonCompleted;

	UPROPERTY(BlueprintAssignable, Category="AsyncTask|Async_ReadJson", DisplayName="Failed")
	FReadJsonSingature OnReadJsonFailed;

	UPROPERTY(BlueprintAssignable, Category="AsyncTask|Async_ReadJson", DisplayName="End")
	FReadJsonSingature OnReadJsonEnd;

	static UObject* WorldContext;
	FString JsonStr {};
	bool IsLargeJson { false };

	TMap<FString, FJsonDataStruct> ParsedDataMap {};

public:
	UFUNCTION(BlueprintCallable, Category="AsyncTask|Async_ReadJson", DisplayName="ReadJson_Async", meta=(BlueprintInternalUseOnly="true", DefaultToSelf="WorldContextObject"))
	static UAsync_ReadJson* Async_ReadJson(UObject* WorldContextObject, const FString& InJsonStr, const bool IsLargeJson = false);

	virtual void Activate() override;
	
	static int32 CountJsonNodes(const TSharedPtr<FJsonObject>& JsonObject);
	
	void LoadJson(const FString& JsonString);
	
	void ParseJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath);
	void ParseJsonIterative(const TSharedPtr<FJsonObject>& RootJson);
	
	UFUNCTION(BlueprintPure, Category="AsyncTask|Async_ReadJson")
	static bool GetNodeData(const FString& NodePath, const FParsedData& ParsedData, FJsonNode& NodeData);

	UFUNCTION(BlueprintPure, Category="AsyncTask|Async_ReadJson")
	static bool ParseJsonArray(const FString& StringArray, FJsonArray& ArrayValue);

	UFUNCTION(BlueprintCallable, Category="AsyncTask|Async_ReadJson")
	void EndTask();

	void DestroyTask();
};
UObject* UAsync_ReadJson::WorldContext = nullptr;