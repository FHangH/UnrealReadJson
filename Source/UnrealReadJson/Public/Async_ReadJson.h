#pragma once

#include "CoreMinimal.h"
#include "JsonData.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Async_ReadJson.generated.h"

/** 异步读取JSON完成时的委托 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FReadJsonSignature, FParsedData, ParsedData);

/**
 * 异步JSON读取类
 * 提供异步和同步两种JSON解析方式，支持蓝图调用
 */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = "AsyncTask"))
class UNREALREADJSON_API UAsync_ReadJson : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

    /* Property */
public:
    // ========================================================================
    // 委托
    // ========================================================================

    /** 解析完成委托 */
    UPROPERTY(BlueprintAssignable, Category = "FH|ReadJson|ToValue", DisplayName = "Completed")
    FReadJsonSignature OnReadJsonCompleted;

    /** 解析失败委托 */
    UPROPERTY(BlueprintAssignable, Category = "FH|ReadJson|ToValue", DisplayName = "Failed")
    FReadJsonSignature OnReadJsonFailed;

    /** 任务结束委托 */
    UPROPERTY(BlueprintAssignable, Category = "FH|ReadJson|ToValue", DisplayName = "End")
    FReadJsonSignature OnReadJsonEnd;
    
private:
    // ========================================================================
    // 常量定义
    // ========================================================================

    /** JSON大小自动检测阈值（字符数，默认100KB） */
    static constexpr int32 LargeJsonThreshold = 100000;

    // ========================================================================
    // 成员变量
    // ========================================================================

    /** 上下文对象（用于日志） */
    UPROPERTY()
    TObjectPtr<UObject> WorldContext;

    /** 待解析的JSON字符串 */
    FString JsonStr;
    
    /** 解析结果 */
    TMap<FString, FJsonDataStruct> ParsedDataMap;

    
    /* Function */
public:
    // ========================================================================
    // 内部实现
    // ========================================================================
    
    /**
     * 异步读取JSON（推荐用于初次解析或大型JSON）, 依据 LargeJsonThreshold 自动判断是否是大型Json（使用迭代解析避免栈溢出）
     * @param WorldContextObject 上下文对象（用于日志显示调用来源）
     * @param InJsonStr 待解析的JSON字符串
     * @return 异步任务对象
     */
    UFUNCTION(BlueprintCallable, Category = "FH|ReadJson|Read|AsyncTask", DisplayName = "ReadJson_Async",
        meta = (BlueprintInternalUseOnly = "true", DefaultToSelf = "WorldContextObject"))
    static UAsync_ReadJson* Async_ReadJson(UObject* WorldContextObject, const FString& InJsonStr);

protected:
    /** 激活异步任务 */
    virtual void Activate() override;
    
public:
    /** 统计JSON节点数量（用于预分配内存） */
    static int32 CountJsonNodes(const TSharedPtr<FJsonObject>& JsonObject);

    /** 加载并解析JSON */
    void LoadJson(const FString& JsonString);

    /** 递归解析JSON（异步版本） */
    void ParseJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath);

    /** 递归解析JSON（同步版本） */
    static void ParseJson_Block(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath, FParsedData& OutParsedData);

    /** 迭代解析JSON（适用于大型JSON） */
    void ParseJsonIterative(const TSharedPtr<FJsonObject>& RootJson);

    /** 解析单个JSON值并存储到Map中 */
    static void ParseJsonValue(const TSharedPtr<FJsonValue>& Value, const FString& Path, TMap<FString, FJsonDataStruct>& OutMap);

    /** 获取JSON值数组 */
    static TArray<TSharedPtr<FJsonValue>> GetJsonValueArray(const FString& JsonArray);

    /** 销毁任务 */
    void DestroyTask();
    
    /** 手动结束异步任务 */
    UFUNCTION(BlueprintCallable, Category = "FH|ReadJson")
    void EndTask();
    
    /** 获取调用者名称（用于日志） */
    FString GetCallerName() const;

    /**
     * 判断是否应使用迭代解析（基于JSON字符串长度自动检测）
     * @param JsonStr JSON字符串
     * @return 如果长度超过LargeJsonThreshold返回true
     */
    static bool ShouldUseIterativeParsing(const FString& JsonStr);
    
    
public:
    // ========================================================================
    // 主要接口 - 读取JSON
    // ========================================================================
    
    /**
     * 同步读取JSON（推荐用于小型JSON）
     * @param WorldContextObject 上下文对象
     * @param InJsonStr 待解析的JSON字符串
     * @param OutParsedData 解析结果
     * @param bIsValid 是否解析成功
     */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|Read", DisplayName = "ReadJson", meta = (DefaultToSelf = "WorldContextObject"))
    static void ReadJson_Block(const UObject* WorldContextObject, const FString& InJsonStr, FParsedData& OutParsedData, bool& bIsValid);

    // ========================================================================
    // 获取节点值 - 单值
    // ========================================================================

    /** 获取节点完整数据 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|GetNodeToValue")
    static void GetNodeData(const FString& NodePath, const FParsedData& ParsedData, FJsonNode& NodeData, bool& bIsValid);

    /** 获取字符串值 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|GetNodeToValue", DisplayName = "GetNodeValue_ToString")
    static void GetNodeValueToString(const FString& NodePath, const FParsedData& ParsedData, FString& NodeValue, bool& bIsValid);

    /** 获取整数值 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|GetNodeToValue", DisplayName = "GetNodeValue_ToInt")
    static void GetNodeValueToInt(const FString& NodePath, const FParsedData& ParsedData, int32& NodeValue, bool& bIsValid);

    /** 获取浮点值 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|GetNodeToValue", DisplayName = "GetNodeValue_ToFloat")
    static void GetNodeValueToFloat(const FString& NodePath, const FParsedData& ParsedData, float& NodeValue, bool& bIsValid);

    /** 获取布尔值 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|GetNodeToValue", DisplayName = "GetNodeValue_ToBool")
    static void GetNodeValueToBool(const FString& NodePath, const FParsedData& ParsedData, bool& NodeValue, bool& bIsValid);

    // ========================================================================
    // 获取节点值 - 数组
    // ========================================================================

    /** 获取字符串数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|GetNodeToArray", DisplayName = "GetNodeValue_ToStringArray")
    static void GetNodeValueToStringArray(const FString& NodePath, const FParsedData& ParsedData, TArray<FString>& NodeArray, bool& bIsValid);

    /** 获取整数数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|GetNodeToArray", DisplayName = "GetNodeValue_ToIntArray")
    static void GetNodeValueToIntArray(const FString& NodePath, const FParsedData& ParsedData, TArray<int32>& NodeArray, bool& bIsValid);

    /** 获取浮点数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|GetNodeToArray", DisplayName = "GetNodeValue_ToFloatArray")
    static void GetNodeValueToFloatArray(const FString& NodePath, const FParsedData& ParsedData, TArray<float>& NodeArray, bool& bIsValid);

    /** 获取布尔数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|GetNodeToArray", DisplayName = "GetNodeValue_ToBoolArray")
    static void GetNodeValueToBoolArray(const FString& NodePath, const FParsedData& ParsedData, TArray<bool>& NodeArray, bool& bIsValid);

    // ========================================================================
    // 解析数组字符串
    // ========================================================================

    /** 解析JSON数组字符串为FJsonArray */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|ParseToArray")
    static void ParseJsonArray(const FString& JsonArray, FJsonArray& ArrayValue, bool& bIsValid);

    /** 解析JSON数组字符串为字符串数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|ParseToArray", DisplayName = "ParseJsonArray_ToStringArray")
    static void ParseJsonArrayToStringArray(const FString& JsonArray, TArray<FString>& ArrayValue, bool& bIsValid);

    /** 解析JSON数组字符串为整数数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|ParseToArray", DisplayName = "ParseJsonArray_ToIntArray")
    static void ParseJsonArrayToIntArray(const FString& JsonArray, TArray<int32>& ArrayValue, bool& bIsValid);

    /** 解析JSON数组字符串为浮点数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|ParseToArray", DisplayName = "ParseJsonArray_ToFloatArray")
    static void ParseJsonArrayToFloatArray(const FString& JsonArray, TArray<float>& ArrayValue, bool& bIsValid);

    /** 解析JSON数组字符串为布尔数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|ParseToArray", DisplayName = "ParseJsonArray_ToBoolArray")
    static void ParseJsonArrayToBoolArray(const FString& JsonArray, TArray<bool>& ArrayValue, bool& bIsValid);

    // ========================================================================
    // 便捷函数 - 读取并直接获取值（适用于只读取单个字段的场景）
    // ========================================================================

    /**
     * 读取JSON并获取指定路径的字符串值
     * @note 不推荐在循环中使用，如需读取多个字段请使用 ReadJson + GetNodeValue 组合
     */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|Read|ToValue", DisplayName = "ReadJsonByNode_ToString", meta = (DefaultToSelf = "WorldContextObject"))
    static void ReadJson_Block_ByNodePathToString(const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, FString& NodeValue, bool& bIsValid);

    /** 读取JSON并获取指定路径的整数值 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|Read|ToValue", DisplayName = "ReadJsonByNode_ToInt", meta = (DefaultToSelf = "WorldContextObject"))
    static void ReadJson_Block_ByNodePathToInt(const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, int32& NodeValue, bool& bIsValid);

    /** 读取JSON并获取指定路径的浮点值 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|Read|ToValue", DisplayName = "ReadJsonByNode_ToFloat", meta = (DefaultToSelf = "WorldContextObject"))
    static void ReadJson_Block_ByNodePathToFloat(const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, float& NodeValue, bool& bIsValid);

    /** 读取JSON并获取指定路径的布尔值 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|Read|ToValue", DisplayName = "ReadJsonByNode_ToBool", meta = (DefaultToSelf = "WorldContextObject"))
    static void ReadJson_Block_ByNodePathToBool(const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, bool& NodeValue, bool& bIsValid);

    /** 读取JSON并获取指定路径的字符串数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|Read|ToArray", DisplayName = "ReadJsonByNode_ToStringArray", meta = (DefaultToSelf = "WorldContextObject"))
    static void ReadJson_Block_ByNodePathToStringArray(const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<FString>& NodeArray, bool& bIsValid);

    /** 读取JSON并获取指定路径的整数数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|Read|ToArray", DisplayName = "ReadJsonByNode_ToIntArray", meta = (DefaultToSelf = "WorldContextObject"))
    static void ReadJson_Block_ByNodePathToIntArray(const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<int32>& NodeArray, bool& bIsValid);

    /** 读取JSON并获取指定路径的浮点数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|Read|ToArray", DisplayName = "ReadJsonByNode_ToFloatArray", meta = (DefaultToSelf = "WorldContextObject"))
    static void ReadJson_Block_ByNodePathToFloatArray(const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<float>& NodeArray, bool& bIsValid);

    /** 读取JSON并获取指定路径的布尔数组 */
    UFUNCTION(BlueprintPure, Category = "FH|ReadJson|Read|ToArray", DisplayName = "ReadJsonByNode_ToBoolArray", meta = (DefaultToSelf = "WorldContextObject"))
    static void ReadJson_Block_ByNodePathToBoolArray(const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<bool>& NodeArray, bool& bIsValid);
};
