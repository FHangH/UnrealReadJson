#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "JsonData.generated.h"

// ============================================================================
// 日志类别声明
// ============================================================================
DECLARE_LOG_CATEGORY_EXTERN(LogReadJson, Log, All);

// ============================================================================
// 检查宏定义
// ============================================================================

/** 检查节点路径是否为空，为空则返回false */
#define CHECK_NODE_PATH_RET(NodePath) \
    if (NodePath.IsEmpty()) \
    { \
        UE_LOG(LogReadJson, Error, TEXT("[ %hs ] NodePath is empty"), __FUNCTION__); \
        return false; \
    }

/** 检查节点路径是否为空，设置IsValid标志 */
#define CHECK_NODE_PATH(NodePath, IsValid) \
    do { \
        IsValid = false; \
        if (NodePath.IsEmpty()) \
        { \
            UE_LOG(LogReadJson, Error, TEXT("[ %hs ] NodePath is empty"), __FUNCTION__); \
            return; \
        } \
    } while(0)

/** 检查JSON数组字符串是否为空 */
#define CHECK_JSON_ARRAY_EMPTY(JsonArray) \
    if (JsonArray.IsEmpty()) \
    { \
        UE_LOG(LogReadJson, Error, TEXT("[ %hs ] JsonArray is empty"), __FUNCTION__); \
        return {}; \
    }

/** 尝试解析JSON值 */
#define TRY_PARSE_JSON_VALUE(Reader, JsonValue) \
    if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid()) \
    { \
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Failed to parse JsonArray Or JsonValue is Invalid"), __FUNCTION__); \
        return {}; \
    }

/** 检查JSON值是否为数组类型 */
#define CHECK_JSON_VALUE_IS_ARRAY(JsonValue) \
    if (JsonValue->Type != EJson::Array) \
    { \
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] JsonValue is not an array"), __FUNCTION__); \
        return {}; \
    }

/** 检查JSON数组元素数量 */
#define CHECK_JSON_VALUE_TO_ARRAY_ELEMENT_COUNT(JsonValue) \
    if (JsonValue->AsArray().Num() == 0) \
    { \
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] JsonValue to array is empty"), __FUNCTION__); \
        return {}; \
    }

// ============================================================================
// 枚举定义
// ============================================================================

/**
 * JSON值类型枚举
 */
UENUM(BlueprintType)
enum class EValueType : uint8
{
    String  UMETA(DisplayName = "String"),
    Bool    UMETA(DisplayName = "Boolean"),
    Int     UMETA(DisplayName = "Integer"),
    Float   UMETA(DisplayName = "Float")
};

// ============================================================================
// 结构体定义
// ============================================================================

/**
 * JSON数据值结构体
 * 根据ValueType决定哪个字段有效
 */
USTRUCT(BlueprintType)
struct FJsonDataStruct
{
    GENERATED_BODY()

    /** 字符串值（当ValueType为String时有效，也用于存储序列化的Object/Array） */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    FString StringValue {};

    /** 布尔值（当ValueType为Bool时有效） */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    bool BoolValue { false };

    /** 整数值（当ValueType为Int时有效） */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    int32 IntValue { 0 };

    /** 浮点值（当ValueType为Float时有效） */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    float FloatValue { 0.f };

    /** 值类型标识 */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    EValueType ValueType { EValueType::String };

    /** 默认构造函数 */
    FJsonDataStruct() = default;

    /** 构造函数 - 字符串类型 */
    static FJsonDataStruct MakeString(const FString& InValue)
    {
        FJsonDataStruct Result;
        Result.StringValue = InValue;
        Result.ValueType = EValueType::String;
        return Result;
    }

    /** 构造函数 - 布尔类型 */
    static FJsonDataStruct MakeBool(const bool InValue)
    {
        FJsonDataStruct Result;
        Result.BoolValue = InValue;
        Result.ValueType = EValueType::Bool;
        return Result;
    }

    /** 构造函数 - 整数类型 */
    static FJsonDataStruct MakeInt(const int32 InValue)
    {
        FJsonDataStruct Result;
        Result.IntValue = InValue;
        Result.ValueType = EValueType::Int;
        return Result;
    }

    /** 构造函数 - 浮点类型 */
    static FJsonDataStruct MakeFloat(const float InValue)
    {
        FJsonDataStruct Result;
        Result.FloatValue = InValue;
        Result.ValueType = EValueType::Float;
        return Result;
    }
};

/**
 * JSON节点结构体 - 蓝图可见的键值对
 */
USTRUCT(BlueprintType)
struct FJsonNode
{
    GENERATED_BODY()

    /** 节点路径/键名 */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    FString Key {};

    /** 节点值 */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    FJsonDataStruct Value {};
};

/**
 * 解析后的JSON数据容器
 */
USTRUCT(BlueprintType)
struct FParsedData
{
    GENERATED_BODY()

    /** 路径到值的映射表 */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    TMap<FString, FJsonDataStruct> ParsedDataMap {};
};

/**
 * JSON数组解析结果
 * 根据数组元素类型，值会被添加到对应的数组中
 */
USTRUCT(BlueprintType)
struct FJsonArray
{
    GENERATED_BODY()

    /** 字符串数组（包含String、Object、Array类型的序列化结果） */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    TArray<FString> StringArray {};

    /** 布尔数组 */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    TArray<bool> BoolArray {};

    /** 整数数组 */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    TArray<int32> IntArray {};

    /** 浮点数组 */
    UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
    TArray<float> FloatArray {};
};

/**
 * 迭代解析时使用的栈节点结构体
 * 用于避免深层递归导致的栈溢出
 */
USTRUCT()
struct FJsonParseStackNode
{
    GENERATED_BODY()

    /** 当前解析的JSON对象 */
    TSharedPtr<FJsonObject> JsonObject;

    /** 当前路径 */
    FString CurrentPath;

    FJsonParseStackNode() = default;

    FJsonParseStackNode(const TSharedPtr<FJsonObject>& InJsonObject, const FString& InPath)
        : JsonObject(InJsonObject)
        , CurrentPath(InPath)
    {
    }
};

// ============================================================================
// 辅助函数声明
// ============================================================================

namespace JsonDataHelper
{
    /** 获取值类型的显示名称 */
    inline FString GetValueTypeName(const EValueType ValueType)
    {
        switch (ValueType)
        {
        case EValueType::String: return TEXT("String");
        case EValueType::Float:  return TEXT("Float");
        case EValueType::Int:    return TEXT("Int");
        case EValueType::Bool:   return TEXT("Bool");
        default:                 return TEXT("Unknown");
        }
    }

    /** 判断数值是否为整数（使用更精确的判断方式） */
    inline bool IsIntegerValue(const double Value)
    {
        // 检查是否在int32范围内，且没有小数部分
        if (Value < static_cast<double>(MIN_int32) || Value > static_cast<double>(MAX_int32))
        {
            return false;
        }
        return FMath::IsNearlyEqual(Value, FMath::RoundToDouble(Value), KINDA_SMALL_NUMBER);
    }
}
