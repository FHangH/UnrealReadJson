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

#define CHECK_NODE_PATH(NodePath) \
	if (NodePath.IsEmpty()) \
	{ \
		UE_LOG(LogReadJson, Error, TEXT("[ %hs ] NodePath is empty"), __FUNCTION__); \
		return false; \
	}

#define CHECK_JSON_ARRAY_EMPTY(JsonArray) \
	if (JsonArray.IsEmpty()) \
	{ \
		UE_LOG(LogReadJson, Error, TEXT("[ %hs ] JsonArray is empty"), __FUNCTION__); \
		return {}; \
	}

#define TRY_PARSE_JSON_VALUE(Reader, JsonValue) \
	if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid()) \
	{ \
		UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Failed to parse JsonArray Or JsonValue is Invalid"), __FUNCTION__); \
		return {}; \
	}

#define CHECK_JSON_VALUE_IS_ARRAY(JsonValue) \
	if (JsonValue->Type != EJson::Array) \
	{ \
		UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] JsonValue is not an array"), __FUNCTION__); \
		return {}; \
	}

#define CHECK_JSON_VALUE_TO_ARRAY_ELEMENT_COUNT(JsonValue) \
	if (JsonValue->AsArray().Num()== 0) \
	{ \
		UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] JsonValue to array is empty"), __FUNCTION__); \
		return {}; \
	}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReadJsonSingature, FParsedData, ParsedData);

UCLASS(BlueprintType, meta=(ExposedAsyncProxy="AsyncTask"))
class UNREALREADJSON_API UAsync_ReadJson : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable, Category="FH|ReadJson|ToValue", DisplayName="Completed")
	FReadJsonSingature OnReadJsonCompleted;

	UPROPERTY(BlueprintAssignable, Category="FH|ReadJson|ToValue", DisplayName="Failed")
	FReadJsonSingature OnReadJsonFailed;

	UPROPERTY(BlueprintAssignable, Category="FH|ReadJson|ToValue", DisplayName="End")
	FReadJsonSingature OnReadJsonEnd;

	static UObject* WorldContext;
	FString JsonStr {};
	bool IsLargeJson { false };

	TMap<FString, FJsonDataStruct> ParsedDataMap {};

public:
	UFUNCTION(BlueprintCallable, Category="FH|ReadJson|AsyncTask", DisplayName="ReadJson_Async", meta=(BlueprintInternalUseOnly="true", DefaultToSelf="WorldContextObject"))
	static UAsync_ReadJson* Async_ReadJson(UObject* WorldContextObject, const FString& InJsonStr, const bool IsLargeJson = false);

	virtual void Activate() override;
	
	static int32 CountJsonNodes(const TSharedPtr<FJsonObject>& JsonObject);
	
	void LoadJson(const FString& JsonString);
	
	void ParseJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath);
	void ParseJsonIterative(const TSharedPtr<FJsonObject>& RootJson);
	
	UFUNCTION(BlueprintPure, Category="FH|ReadJson")
	static bool GetNodeData(const FString& NodePath, const FParsedData& ParsedData, FJsonNode& NodeData);

	static TArray<TSharedPtr<FJsonValue>> GetJsonValueArray(const FString& JsonArray);

	UFUNCTION(BlueprintPure, Category="FH|ReadJson")
	static bool ParseJsonArray(const FString& JsonArray, FJsonArray& ArrayValue);

	UFUNCTION(BlueprintCallable, Category="FH|ReadJson")
	void EndTask();

	void DestroyTask();

	/* Convenient Functions */	 
	static FString GetValueTypeName(const EValueType& ValueType);
	
	// To Value
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|GetNodeToValue", DisplayName="GetNodeValue_ToString")
	static bool GetNodeValueToString(const FString& NodePath, const FParsedData& ParsedData, FString& NodeValue);

	UFUNCTION(BlueprintPure, Category="FH|ReadJson|GetNodeToValue", DisplayName="GetNodeValue_ToInt")
    static bool GetNodeValueToInt(const FString& NodePath, const FParsedData& ParsedData, int32& NodeValue);

	UFUNCTION(BlueprintPure, Category="FH|ReadJson|GetNodeToValue", DisplayName="GetNodeValue_ToFloat")
    static bool GetNodeValueToFloat(const FString& NodePath, const FParsedData& ParsedData, float& NodeValue);
	
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|GetNodeToValue", DisplayName="GetNodeValue_ToBool")
    static bool GetNodeValueToBool(const FString& NodePath, const FParsedData& ParsedData, bool& NodeValue);

	// Parse Array Value
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|ParseToArray", DisplayName="ParseJsonArray_ToStringArray")
	static bool ParseJsonArrayToStringArray(const FString& JsonArray, TArray<FString>& ArrayValue);

	UFUNCTION(BlueprintPure, Category="FH|ReadJson|ParseToArray", DisplayName="ParseJsonArray_ToIntArray")
	static bool ParseJsonArrayToIntArray(const FString& JsonArray, TArray<int32>& ArrayValue);

	UFUNCTION(BlueprintPure, Category="FH|ReadJson|ParseToArray", DisplayName="ParseJsonArray_ToFloatArray")
	static bool ParseJsonArrayToFloatArray(const FString& JsonArray, TArray<float>& ArrayValue);

	UFUNCTION(BlueprintPure, Category="FH|ReadJson|ParseToArray", DisplayName="ParseJsonArray_ToBoolArray")
	static bool ParseJsonArrayToBoolArray(const FString& JsonArray, TArray<bool>& ArrayValue);

	// To Array
    UFUNCTION(BlueprintPure, Category="FH|ReadJson|GetNodeToArray", DisplayName="GetNodeValue_ToStringArray")
    static bool GetNodeValueToStringArray(const FString& NodePath, const FParsedData& ParsedData, TArray<FString>& NodeArray);

    UFUNCTION(BlueprintPure, Category="FH|ReadJson|GetNodeToArray", DisplayName="GetNodeValue_ToIntArray")
    static bool GetNodeValueToIntArray(const FString& NodePath, const FParsedData& ParsedData, TArray<int32>& NodeArray);

    UFUNCTION(BlueprintPure, Category="FH|ReadJson|GetNodeToArray", DisplayName="GetNodeValue_ToFloatArray")
    static bool GetNodeValueToFloatArray(const FString& NodePath, const FParsedData& ParsedData, TArray<float>& NodeArray);

    UFUNCTION(BlueprintPure, Category="FH|ReadJson|GetNodeToArray", DisplayName="GetNodeValue_ToBoolArray")
    static bool GetNodeValueToBoolArray(const FString& NodePath, const FParsedData& ParsedData, TArray<bool>& NodeArray);
};
UObject* UAsync_ReadJson::WorldContext = nullptr;