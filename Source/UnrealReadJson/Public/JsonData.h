#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "JsonData.generated.h"

// ============================================================================
// 日志类别声明
// ============================================================================
DECLARE_LOG_CATEGORY_EXTERN(LogReadJson, Log, All);

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

    // ========================================================================
    // 辅助方法
    // ========================================================================

    /** 检查是否有任何元素 */
    bool HasAnyElements() const
    {
        return !StringArray.IsEmpty() || !BoolArray.IsEmpty() || 
               !IntArray.IsEmpty() || !FloatArray.IsEmpty();
    }

    /** 获取所有数组的元素总数 */
    int32 GetTotalCount() const
    {
        return StringArray.Num() + BoolArray.Num() + IntArray.Num() + FloatArray.Num();
    }

    /** 清空所有数组 */
    void Empty()
    {
        StringArray.Empty();
        BoolArray.Empty();
        IntArray.Empty();
        FloatArray.Empty();
    }
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

    /**
     * 判断数值是否为整数
     * 
     * 判断逻辑说明:
     * 1. 首先检查数值是否在int32的有效范围内 [MIN_int32, MAX_int32]
     * 2. 然后使用 FMath::IsNearlyEqual 比较原值与四舍五入后的值
     * 3. 容差使用 KINDA_SMALL_NUMBER (约 0.0001)
     * 
     * 注意事项:
     * - 对于极大或极小的整数，可能因浮点精度问题被误判
     * - 如 1.0000001 可能被判定为整数
     * - 如需严格整数判断，建议在业务层额外验证
     * 
     * @param Value 待检查的双精度浮点数
     * @return 如果数值可安全转换为int32且无小数部分，返回true
     */
    inline bool IsIntegerValue(const double Value)
    {
        // 检查是否在int32范围内
        if (Value < static_cast<double>(MIN_int32) || Value > static_cast<double>(MAX_int32))
        {
            return false;
        }
        // 检查是否没有小数部分
        return FMath::IsNearlyEqual(Value, FMath::RoundToDouble(Value), KINDA_SMALL_NUMBER);
    }

    /**
     * 构建节点路径（优化：减少临时字符串分配）
     * 
     * @param CurrentPath 当前路径
     * @param Key 新增的键名
     * @return 拼接后的完整路径
     */
    inline FString BuildNodePath(const FString& CurrentPath, const FString& Key)
    {
        if (CurrentPath.IsEmpty())
        {
            return Key;
        }
        FString NewPath;
        NewPath.Reserve(CurrentPath.Len() + Key.Len() + 1);
        NewPath = CurrentPath;
        NewPath.AppendChar(TEXT('.'));
        NewPath.Append(Key);
        return NewPath;
    }

    /**
     * 验证节点路径是否有效（替代 CHECK_NODE_PATH_RET 宏）
     * 
     * @param NodePath 待验证的节点路径
     * @param FunctionName 调用函数名（用于日志）
     * @return 路径有效返回true，否则返回false
     */
    inline bool ValidateNodePath(const FString& NodePath, const TCHAR* FunctionName)
    {
        if (NodePath.IsEmpty())
        {
            UE_LOG(LogReadJson, Error, TEXT("[ %s ] NodePath is empty"), FunctionName);
            return false;
        }
        return true;
    }

    /**
     * 验证JSON数组字符串是否有效（替代 CHECK_JSON_ARRAY_EMPTY 宏）
     * 
     * @param JsonArrayStr 待验证的JSON数组字符串
     * @param FunctionName 调用函数名（用于日志）
     * @return 字符串有效返回true，否则返回false
     */
    inline bool ValidateJsonArrayString(const FString& JsonArrayStr, const TCHAR* FunctionName)
    {
        if (JsonArrayStr.IsEmpty())
        {
            UE_LOG(LogReadJson, Error, TEXT("[ %s ] JsonArray is empty"), FunctionName);
            return false;
        }
        return true;
    }

    /**
     * 验证JSON值解析结果（替代 TRY_PARSE_JSON_VALUE 宏）
     * 
     * @param JsonValue 解析后的JSON值
     * @param FunctionName 调用函数名（用于日志）
     * @return 解析成功返回true，否则返回false
     */
    inline bool ValidateJsonValue(const TSharedPtr<FJsonValue>& JsonValue, const TCHAR* FunctionName)
    {
        if (!JsonValue.IsValid())
        {
            UE_LOG(LogReadJson, Warning, TEXT("[ %s ] Failed to parse JsonArray Or JsonValue is Invalid"), FunctionName);
            return false;
        }
        return true;
    }

    /**
     * 验证JSON值是否为数组类型（替代 CHECK_JSON_VALUE_IS_ARRAY 宏）
     * 
     * @param JsonValue 待验证的JSON值
     * @param FunctionName 调用函数名（用于日志）
     * @return 是数组类型返回true，否则返回false
     */
    inline bool ValidateJsonValueIsArray(const TSharedPtr<FJsonValue>& JsonValue, const TCHAR* FunctionName)
    {
        if (JsonValue->Type != EJson::Array)
        {
            UE_LOG(LogReadJson, Warning, TEXT("[ %s ] JsonValue is not an array"), FunctionName);
            return false;
        }
        return true;
    }

    /**
     * 验证JSON数组是否有元素（替代 CHECK_JSON_VALUE_TO_ARRAY_ELEMENT_COUNT 宏）
     * 
     * @param JsonValue 待验证的JSON数组值
     * @param FunctionName 调用函数名（用于日志）
     * @return 数组非空返回true，否则返回false
     */
    inline bool ValidateJsonArrayNotEmpty(const TSharedPtr<FJsonValue>& JsonValue, const TCHAR* FunctionName)
    {
        if (JsonValue->AsArray().Num() == 0)
        {
            UE_LOG(LogReadJson, Warning, TEXT("[ %s ] JsonValue to array is empty"), FunctionName);
            return false;
        }
        return true;
    }
    

    // ========================================================================
    // 模板辅助函数 - 获取节点值
    // ========================================================================

    /**
     * 用于类型安全地从FJsonDataStruct提取值的特征模板
     */
    template<typename T>
    struct TJsonValueTraits;

    template<>
    struct TJsonValueTraits<FString>
    {
        static constexpr EValueType ExpectedType = EValueType::String;
        static const FString& GetValue(const FJsonDataStruct& Data) { return Data.StringValue; }
        static const TCHAR* GetTypeName() { return TEXT("string"); }
    };

    template<>
    struct TJsonValueTraits<int32>
    {
        static constexpr EValueType ExpectedType = EValueType::Int;
        static int32 GetValue(const FJsonDataStruct& Data) { return Data.IntValue; }
        static const TCHAR* GetTypeName() { return TEXT("integer"); }
    };

    template<>
    struct TJsonValueTraits<float>
    {
        static constexpr EValueType ExpectedType = EValueType::Float;
        static float GetValue(const FJsonDataStruct& Data) { return Data.FloatValue; }
        static const TCHAR* GetTypeName() { return TEXT("float"); }
    };

    template<>
    struct TJsonValueTraits<bool>
    {
        static constexpr EValueType ExpectedType = EValueType::Bool;
        static bool GetValue(const FJsonDataStruct& Data) { return Data.BoolValue; }
        static const TCHAR* GetTypeName() { return TEXT("boolean"); }
    };

    /**
     * 统一的节点值获取模板函数
     * 
     * @tparam T 目标值类型 (FString, int32, float, bool)
     * @param NodePath 节点路径
     * @param ParsedDataMap 已解析的数据
     * @param OutValue 输出值
     * @param bOutValid 是否成功
     * @param FunctionName 调用函数名（用于日志）
     */
    template<typename T>
    inline void GetNodeValueImpl(
        const FString& NodePath,
        const TMap<FString, FJsonDataStruct>& ParsedDataMap,
        T& OutValue,
        bool& bOutValid,
        const TCHAR* FunctionName)
    {
        bOutValid = false;
        if (!ValidateNodePath(NodePath, FunctionName))
        {
            return;
        }

        if (const FJsonDataStruct* FoundData = ParsedDataMap.Find(NodePath))
        {
            using Traits = TJsonValueTraits<T>;
            if (FoundData->ValueType == Traits::ExpectedType)
            {
                OutValue = Traits::GetValue(*FoundData);
                bOutValid = true;
                return;
            }
            UE_LOG(LogReadJson, Warning, TEXT("[ %s ] Node [ %s ] is not a %s, Node Type: [ %s ]"), 
                FunctionName, *NodePath, Traits::GetTypeName(), *GetValueTypeName(FoundData->ValueType));
            return;
        }

        UE_LOG(LogReadJson, Warning, TEXT("[ %s ] Node [ %s ] not found"), FunctionName, *NodePath);
    }
}

