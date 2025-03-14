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
	/*
	 * ReadJson-Async 异步任务（初次解析Json或较大的Json，推荐使用）
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @IsLargeJson: IsLargeJson 是否是大文件Json
	 * @OutParsedData: FParsedData 解析后的Json数据
	 * @AsyncTask: FAsyncTask<UAsync_ReadJson> 异步任务，可创建对象，方便手动调用EndTask手动销毁
	 */
	UFUNCTION(BlueprintCallable, Category="FH|ReadJson|Read|AsyncTask", DisplayName="ReadJson_Async", meta=(BlueprintInternalUseOnly="true", DefaultToSelf="WorldContextObject"))
	static UAsync_ReadJson* Async_ReadJson(UObject* WorldContextObject, const FString& InJsonStr, const bool IsLargeJson = false);

	/*
	 * ReadJson-Block 同步任务（推荐解析较小的Json）
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @OutParsedData: FParsedData 解析后的Json数据
	 * @IsValid: bool 是否解析成功
	 */
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|Read", DisplayName="ReadJson", meta=(DefaultToSelf="WorldContextObject"))
	static void ReadJson_Block(UObject* WorldContextObject, const FString& InJsonStr, FParsedData& OutParsedData, bool& IsValid);
	
	virtual void Activate() override;
	
	static int32 CountJsonNodes(const TSharedPtr<FJsonObject>& JsonObject);
	
	void LoadJson(const FString& JsonString);

	// ParseJson
	void ParseJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath);
	// ParseJson-Block
	static void ParseJson_Block(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath, FParsedData& OutParsedData);

	void ParseJsonIterative(const TSharedPtr<FJsonObject>& RootJson);
	
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|GetNodeToValue")
	static bool GetNodeData(const FString& NodePath, const FParsedData& ParsedData, FJsonNode& NodeData);

	static TArray<TSharedPtr<FJsonValue>> GetJsonValueArray(const FString& JsonArray);

	UFUNCTION(BlueprintPure, Category="FH|ReadJson|ParseToArray")
	static bool ParseJsonArray(const FString& JsonArray, FJsonArray& ArrayValue);

	/*
	 * 用于手动销毁异步任务-ReadJson_Async
	 */
	UFUNCTION(BlueprintCallable, Category="FH|ReadJson")
	void EndTask();

	void DestroyTask();

	/* Convenient Functions */	 
	static FString GetValueTypeName(const EValueType& ValueType);

	static FString CallerName();
	
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

	/*便捷读取解析Json*/
	/*
	 * 读取Json，并依据NodePath解析String, Array, Object类型 字段值
	 * 集合[读取，解析]一体，面向非大型Json，一次性使用 (不推荐重复性调用，更不推荐在循环中使用)
	 * 推荐使用场景：只读取Json中一个字段使用，如果实际需求中需要读取多个Json字段，推荐使用 ReadJson + GetNodeValue 两种组合
	 * 理想情况下，一个Json只因读取一次，后续就是通过GetNodeValue获取字段值
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @NodePath: NodePath Json字段路径
	 * @NodeValue: FString Json字段值
	 * @IsValid: bool 是否解析成功
	 */
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|Read|ToValue", DisplayName="ReadJsonByNode_ToString", meta=(DefaultToSelf="WorldContextObject"))
	static void ReadJson_Block_ByNodePathToString(UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, FString& NodeValue, bool& IsValid);

	/*
	 * 读取Json，并安装NodePath解析Int类型 字段值
	 * 集合[读取，解析]一体，面向非大型Json，一次性使用 (不推荐重复性调用，更不推荐在循环中使用)
	 * 推荐使用场景：只读取Json中一个字段使用，如果实际需求中需要读取多个Json字段，推荐使用 ReadJson + GetNodeValue 两种组合
	 * 理想情况下，一个Json只因读取一次，后续就是通过GetNodeValue获取字段值
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @NodePath: NodePath Json字段路径
	 * @NodeValue: FString Json字段值
	 * @IsValid: bool 是否解析成功
	 */
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|Read|ToValue", DisplayName="ReadJsonByNode_ToInt", meta=(DefaultToSelf="WorldContextObject"))
	static void ReadJson_Block_ByNodePathToInt(UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, int32& NodeValue, bool& IsValid);

	/*
	 * 读取Json，并安装NodePath解析Float类型 字段值
	 * 集合[读取，解析]一体，面向非大型Json，一次性使用 (不推荐重复性调用，更不推荐在循环中使用)
	 * 推荐使用场景：只读取Json中一个字段使用，如果实际需求中需要读取多个Json字段，推荐使用 ReadJson + GetNodeValue 两种组合
	 * 理想情况下，一个Json只因读取一次，后续就是通过GetNodeValue获取字段值
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @NodePath: NodePath Json字段路径
	 * @NodeValue: FString Json字段值
	 * @IsValid: bool 是否解析成功
	 */
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|Read|ToValue", DisplayName="ReadJsonByNode_ToFloat", meta=(DefaultToSelf="WorldContextObject"))
	static void ReadJson_Block_ByNodePathToFloat(UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, float& NodeValue, bool& IsValid);

	/*
	 * 读取Json，并安装NodePath解析Bool类型 字段值
	 * 集合[读取，解析]一体，面向非大型Json，一次性使用 (不推荐重复性调用，更不推荐在循环中使用)
	 * 推荐使用场景：只读取Json中一个字段使用，如果实际需求中需要读取多个Json字段，推荐使用 ReadJson + GetNodeValue 两种组合
	 * 理想情况下，一个Json只因读取一次，后续就是通过GetNodeValue获取字段值
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @NodePath: NodePath Json字段路径
	 * @NodeValue: FString Json字段值
	 * @IsValid: bool 是否解析成功
	 */
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|Read|ToValue", DisplayName="ReadJsonByNode_ToBool", meta=(DefaultToSelf="WorldContextObject"))
	static void ReadJson_Block_ByNodePathToBool(UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, bool& NodeValue, bool& IsValid);

	/*
	 * 读取Json，并依据NodePath解析String, Array, Object类型 数组字段值
	 * 集合[读取，解析]一体，面向非大型Json，一次性使用 (不推荐重复性调用，更不推荐在循环中使用)
	 * 推荐使用场景：只读取Json中一个字段使用，如果实际需求中需要读取多个Json字段，推荐使用 ReadJson + GetNodeValue 两种组合
	 * 理想情况下，一个Json只因读取一次，后续就是通过GetNodeValue获取字段值
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @NodePath: NodePath Json字段路径
	 * @NodeArray: FString Json字段值
	 * @IsValid: bool 是否解析成功
	 */
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|Read|ToArray", DisplayName="ReadJsonByNode_ToStringArray", meta=(DefaultToSelf="WorldContextObject"))
	static void ReadJson_Block_ByNodePathToStringArray(UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<FString>& NodeArray, bool& IsValid);

	/*
	 * 读取Json，并依据NodePath解析Int类型 数组字段值
	 * 集合[读取，解析]一体，面向非大型Json，一次性使用 (不推荐重复性调用，更不推荐在循环中使用)
	 * 推荐使用场景：只读取Json中一个字段使用，如果实际需求中需要读取多个Json字段，推荐使用 ReadJson + GetNodeValue 两种组合
	 * 理想情况下，一个Json只因读取一次，后续就是通过GetNodeValue获取字段值
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @NodePath: NodePath Json字段路径
	 * @NodeArray: FString Json字段值
	 * @IsValid: bool 是否解析成功
	 */
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|Read|ToArray", DisplayName="ReadJsonByNode_ToIntArray", meta=(DefaultToSelf="WorldContextObject"))
	static void ReadJson_Block_ByNodePathToIntArray(UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<int32>& NodeArray, bool& IsValid);

	/*
	 * 读取Json，并依据NodePath解析Float类型 数组字段值
	 * 集合[读取，解析]一体，面向非大型Json，一次性使用 (不推荐重复性调用，更不推荐在循环中使用)
	 * 推荐使用场景：只读取Json中一个字段使用，如果实际需求中需要读取多个Json字段，推荐使用 ReadJson + GetNodeValue 两种组合
	 * 理想情况下，一个Json只因读取一次，后续就是通过GetNodeValue获取字段值
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @NodePath: NodePath Json字段路径
	 * @NodeArray: FString Json字段值
	 * @IsValid: bool 是否解析成功
	 */
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|Read|ToArray", DisplayName="ReadJsonByNode_ToFloatArray", meta=(DefaultToSelf="WorldContextObject"))
	static void ReadJson_Block_ByNodePathToFloatArray(UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<float>& NodeArray, bool& IsValid);

	/*
	 * 读取Json，并依据NodePath解析Bool类型 数组字段值
	 * 集合[读取，解析]一体，面向非大型Json，一次性使用 (不推荐重复性调用，更不推荐在循环中使用)
	 * 推荐使用场景：只读取Json中一个字段使用，如果实际需求中需要读取多个Json字段，推荐使用 ReadJson + GetNodeValue 两种组合
	 * 理想情况下，一个Json只因读取一次，后续就是通过GetNodeValue获取字段值
	 * @WorldContextObject: UObject* 上下文对象(方便在打印中查看调用的蓝图)
	 * @InJsonStr: JsonString 待解析的Json
	 * @NodePath: NodePath Json字段路径
	 * @NodeArray: FString Json字段值
	 * @IsValid: bool 是否解析成功
	 */
	UFUNCTION(BlueprintPure, Category="FH|ReadJson|Read|ToArray", DisplayName="ReadJsonByNode_ToBoolArray", meta=(DefaultToSelf="WorldContextObject"))
	static void ReadJson_Block_ByNodePathToBoolArray(UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<bool>& NodeArray, bool& IsValid);
		
};
UObject* UAsync_ReadJson::WorldContext = nullptr;