#include "Async_ReadJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// 定义日志类别
DEFINE_LOG_CATEGORY(LogReadJson);

// ============================================================================
// 内部解析实现
// ============================================================================
UAsync_ReadJson* UAsync_ReadJson::Async_ReadJson(UObject* WorldContextObject, const FString& InJsonStr)
{
    UAsync_ReadJson* AsyncTask = NewObject<UAsync_ReadJson>();
    AsyncTask->WorldContext = WorldContextObject;
    AsyncTask->JsonStr = InJsonStr;
    return AsyncTask;
}

void UAsync_ReadJson::Activate()
{
    RegisterWithGameInstance(WorldContext);
    LoadJson(JsonStr);
}

int32 UAsync_ReadJson::CountJsonNodes(const TSharedPtr<FJsonObject>& JsonObject)
{
    if (!JsonObject.IsValid() || JsonObject->Values.IsEmpty())
    {
        return 0;
    }

    int32 Count = 0;
    for (const auto& Elem : JsonObject->Values)
    {
        ++Count;
        if (Elem.Value->Type == EJson::Object)
        {
            Count += CountJsonNodes(Elem.Value->AsObject());
        }
    }
    return Count;
}

void UAsync_ReadJson::LoadJson(const FString& JsonString)
{
    if (JsonString.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("[ %s ] - [ %hs ] JsonString is Invalid"), *GetCallerName(), __FUNCTION__);
        OnReadJsonFailed.Broadcast({});
        DestroyTask();
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogReadJson, Error, TEXT("[ %s ] - [ %hs ] Deserialize Failed, JsonString is invalid"), *GetCallerName(), __FUNCTION__);
        OnReadJsonFailed.Broadcast({});
        DestroyTask();
        return;
    }

    // 自动检测是否是大型Json
    if (ShouldUseIterativeParsing(JsonString))
    {
        const int32 Count = CountJsonNodes(JsonObject);
        ParsedDataMap.Empty(Count);
        UE_LOG(LogReadJson, Log, TEXT("[ %s ] - [ %hs ] Begin Parse Large Json, TMap initial size is %d"), 
            *GetCallerName(), __FUNCTION__, Count);
        ParseJsonIterative(JsonObject);
    }
    else
    {
        UE_LOG(LogReadJson, Log, TEXT("[ %s ] - [ %hs ] Begin Parse Json"), *GetCallerName(), __FUNCTION__);
        ParseJson(JsonObject, TEXT(""));
    }

    OnReadJsonCompleted.Broadcast({ ParsedDataMap });
    UE_LOG(LogReadJson, Log, TEXT("[ %s ] - [ %hs ] End Parse Json"), *GetCallerName(), __FUNCTION__);
    DestroyTask();
}

void UAsync_ReadJson::ParseJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath)
{
    if (!JsonObject.IsValid() || JsonObject->Values.IsEmpty())
    {
        UE_LOG(LogReadJson, Warning, TEXT("[ %s ] - [ %hs ] JsonObject is invalid or empty"), *GetCallerName(), __FUNCTION__);
        return;
    }

    for (const auto& Elem : JsonObject->Values)
    {
        const FString NewPath = JsonDataHelper::BuildNodePath(CurrentPath, Elem.Key);
        const TSharedPtr<FJsonValue>& Value = Elem.Value;

        ParseJsonValue(Value, NewPath, ParsedDataMap);

        // 如果是对象类型，递归解析子对象
        if (Value->Type == EJson::Object)
        {
            UE_LOG(LogReadJson, Log, TEXT("[ %s ] - [ %hs ] Parse Json Object: [ %s ]"), 
                *GetCallerName(), __FUNCTION__, *NewPath);
            ParseJson(Value->AsObject(), NewPath);
        }
    }
}

void UAsync_ReadJson::ParseJson_Block(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath, FParsedData& OutParsedData)
{
    if (!JsonObject.IsValid() || JsonObject->Values.IsEmpty())
    {
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] JsonObject is invalid or empty"), __FUNCTION__);
        return;
    }

    for (const auto& Elem : JsonObject->Values)
    {
        const FString NewPath = JsonDataHelper::BuildNodePath(CurrentPath, Elem.Key);
        const TSharedPtr<FJsonValue>& Value = Elem.Value;

        ParseJsonValue(Value, NewPath, OutParsedData.ParsedDataMap);

        // 如果是对象类型，递归解析子对象
        if (Value->Type == EJson::Object)
        {
            UE_LOG(LogReadJson, Log, TEXT("[ %hs ] Parse Json Object: [ %s ]"), __FUNCTION__, *NewPath);
            ParseJson_Block(Value->AsObject(), NewPath, OutParsedData);
        }
    }
}

void UAsync_ReadJson::ParseJsonIterative(const TSharedPtr<FJsonObject>& RootJson)
{
    if (!RootJson.IsValid() || RootJson->Values.IsEmpty())
    {
        UE_LOG(LogReadJson, Warning, TEXT("[ %s ] - [ %hs ] JsonObject is invalid or empty"), *GetCallerName(), __FUNCTION__);
        return;
    }

    // 预分配栈空间，避免频繁扩容
    TArray<FJsonParseStackNode> Stack;
    Stack.Reserve(32);
    Stack.Emplace(RootJson, TEXT(""));

    while (Stack.Num() > 0)
    {
        // 使用Last()+Pop(false)避免拷贝
        FJsonParseStackNode CurrentNode = MoveTemp(Stack.Last());
        
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
        Stack.Pop(EAllowShrinking::No);
#else
        Stack.Pop(false);
#endif
        
        const TSharedPtr<FJsonObject>& JsonObject = CurrentNode.JsonObject;
        const FString& CurrentPath = CurrentNode.CurrentPath;

        for (const auto& Elem : JsonObject->Values)
        {
            const FString NewPath = JsonDataHelper::BuildNodePath(CurrentPath, Elem.Key);
            const TSharedPtr<FJsonValue>& Value = Elem.Value;

            ParseJsonValue(Value, NewPath, ParsedDataMap);

            // 如果是对象类型，将子对象压入栈中继续解析
            if (Value->Type == EJson::Object)
            {
                UE_LOG(LogReadJson, Log, TEXT("[ %s ] - [ %hs ] Parse Json Object: [ %s ]"), 
                    *GetCallerName(), __FUNCTION__, *NewPath);
                Stack.Emplace(Value->AsObject(), NewPath);
            }
        }
    }
}

void UAsync_ReadJson::ParseJsonValue(const TSharedPtr<FJsonValue>& Value, const FString& Path, TMap<FString, FJsonDataStruct>& OutMap)
{
    if (!Value.IsValid())
    {
        return;
    }

    switch (Value->Type)
    {
    case EJson::Object:
        {
            // 对象类型：序列化为字符串存储
            FString ObjectString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ObjectString);
            if (FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer))
            {
                OutMap.Add(Path, FJsonDataStruct::MakeString(ObjectString));
            }
            break;
        }
    case EJson::Array:
        {
            // 数组类型：序列化为字符串存储
            FString ArrayString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArrayString);
            if (FJsonSerializer::Serialize(Value->AsArray(), Writer))
            {
                OutMap.Add(Path, FJsonDataStruct::MakeString(ArrayString));
            }
            break;
        }
    case EJson::String:
        OutMap.Add(Path, FJsonDataStruct::MakeString(Value->AsString()));
        break;
    case EJson::Boolean:
        OutMap.Add(Path, FJsonDataStruct::MakeBool(Value->AsBool()));
        break;
    case EJson::Number:
        {
            if (const double Num = Value->AsNumber(); FMath::IsFinite(Num))
            {
                if (JsonDataHelper::IsIntegerValue(Num))
                {
                    OutMap.Add(Path, FJsonDataStruct::MakeInt(static_cast<int32>(Num)));
                }
                else
                {
                    OutMap.Add(Path, FJsonDataStruct::MakeFloat(static_cast<float>(Num)));
                }
            }
            break;
        }
    case EJson::Null:
        // 显式处理Null值，保留字段但值为空
        OutMap.Add(Path, FJsonDataStruct::MakeString(TEXT("")));
        break;
    case EJson::None:
        // None类型不添加到Map，仅记录日志
        UE_LOG(LogReadJson, Verbose, TEXT("[ %hs ] Skipped None value at path: %s"), __FUNCTION__, *Path);
        break;
    default:
        OutMap.Add(Path, FJsonDataStruct::MakeString(Value->AsString()));
        break;
    }
}

TArray<TSharedPtr<FJsonValue>> UAsync_ReadJson::GetJsonValueArray(const FString& JsonArray)
{
    // 使用内联函数替代宏
    if (!JsonDataHelper::ValidateJsonArrayString(JsonArray, TEXT("GetJsonValueArray")))
    {
        return {};
    }

    TSharedPtr<FJsonValue> JsonValue;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonArray);

    if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonDataHelper::ValidateJsonValue(JsonValue, TEXT("GetJsonValueArray")))
    {
        return {};
    }
    
    if (!JsonDataHelper::ValidateJsonValueIsArray(JsonValue, TEXT("GetJsonValueArray")))
    {
        return {};
    }
    
    if (!JsonDataHelper::ValidateJsonArrayNotEmpty(JsonValue, TEXT("GetJsonValueArray")))
    {
        return {};
    }

    return JsonValue->AsArray();
}

void UAsync_ReadJson::DestroyTask()
{
    SetReadyToDestroy();
    MarkAsGarbage();
    UE_LOG(LogReadJson, Log, TEXT("[ %s ] - [ %hs ] Async_ReadJson DestroyTask"), *GetCallerName(), __FUNCTION__);
}

void UAsync_ReadJson::EndTask()
{
    OnReadJsonEnd.Broadcast({ ParsedDataMap });
    DestroyTask();
    UE_LOG(LogReadJson, Log, TEXT("[ %s ] - [ %hs ] Async_ReadJson EndTask"), *GetCallerName(), __FUNCTION__);
}

FString UAsync_ReadJson::GetCallerName() const
{
    return WorldContext ? WorldContext->GetName() : TEXT("Unknown");
}

bool UAsync_ReadJson::ShouldUseIterativeParsing(const FString& JsonStr)
{
    // 基于字符串长度判断是否应使用迭代解析
    return JsonStr.Len() >= LargeJsonThreshold;
}

// ============================================================================
// 主要接口实现
// ============================================================================
void UAsync_ReadJson::ReadJson_Block(const UObject* WorldContextObject, const FString& InJsonStr, FParsedData& OutParsedData, bool& bIsValid)
{
    bIsValid = false;
    OutParsedData = {};
    const FString CallerName = WorldContextObject ? WorldContextObject->GetName() : TEXT("Unknown");

    if (InJsonStr.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("[ %s ] - [ %hs ] JsonString is Invalid"), *CallerName, __FUNCTION__);
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJsonStr);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogReadJson, Error, TEXT("[ %s ] - [ %hs ] Deserialize Failed, JsonString is invalid"), *CallerName, __FUNCTION__);
        return;
    }

    UE_LOG(LogReadJson, Log, TEXT("[ %s ] - [ %hs ] Begin Parse Json"), *CallerName, __FUNCTION__);
    ParseJson_Block(JsonObject, TEXT(""), OutParsedData);

    if (OutParsedData.ParsedDataMap.Num() == 0)
    {
        UE_LOG(LogReadJson, Warning, TEXT("[ %s ] - [ %hs ] Parse Json Value Is Empty"), *CallerName, __FUNCTION__);
        return;
    }

    bIsValid = true;
}

// ============================================================================
// 获取节点值实现
// ============================================================================
void UAsync_ReadJson::GetNodeData(const FString& NodePath, const FParsedData& ParsedData, FJsonNode& NodeData, bool& bIsValid)
{
    bIsValid = false;
    if (!JsonDataHelper::ValidateNodePath(NodePath, TEXT("GetNodeData")))
    {
        return;
    }

    if (const FJsonDataStruct* FoundData = ParsedData.ParsedDataMap.Find(NodePath))
    {
        NodeData = { NodePath, *FoundData };
        bIsValid = true;
        return;
    }

    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
}

void UAsync_ReadJson::GetNodeValueToString(const FString& NodePath, const FParsedData& ParsedData, FString& NodeValue, bool& bIsValid)
{
    JsonDataHelper::GetNodeValueImpl(NodePath, ParsedData.ParsedDataMap, NodeValue, bIsValid, TEXT("GetNodeValueToString"));
}

void UAsync_ReadJson::GetNodeValueToInt(const FString& NodePath, const FParsedData& ParsedData, int32& NodeValue, bool& bIsValid)
{
    JsonDataHelper::GetNodeValueImpl(NodePath, ParsedData.ParsedDataMap, NodeValue, bIsValid, TEXT("GetNodeValueToInt"));
}

void UAsync_ReadJson::GetNodeValueToFloat(const FString& NodePath, const FParsedData& ParsedData, float& NodeValue, bool& bIsValid)
{
    JsonDataHelper::GetNodeValueImpl(NodePath, ParsedData.ParsedDataMap, NodeValue, bIsValid, TEXT("GetNodeValueToFloat"));
}

void UAsync_ReadJson::GetNodeValueToBool(const FString& NodePath, const FParsedData& ParsedData, bool& NodeValue, bool& bIsValid)
{
    JsonDataHelper::GetNodeValueImpl(NodePath, ParsedData.ParsedDataMap, NodeValue, bIsValid, TEXT("GetNodeValueToBool"));
}

// ============================================================================
// 获取节点数组值实现
// ============================================================================
void UAsync_ReadJson::GetNodeValueToStringArray(const FString& NodePath, const FParsedData& ParsedData, TArray<FString>& NodeArray, bool& bIsValid)
{
    NodeArray.Empty();
    bIsValid = false;
    if (!JsonDataHelper::ValidateNodePath(NodePath, TEXT("GetNodeValueToStringArray")))
    {
        return;
    }

    if (const FJsonDataStruct* FoundData = ParsedData.ParsedDataMap.Find(NodePath))
    {
        if (FoundData->ValueType == EValueType::String)
        {
            if (FoundData->StringValue.IsEmpty())
            {
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node Value: [ %s ] is empty"), __FUNCTION__, *NodePath);
                return;
            }
            ParseJsonArrayToStringArray(FoundData->StringValue, NodeArray, bIsValid);
            return;
        }
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string (array), Node Type: [ %s ]"), 
            __FUNCTION__, *NodePath, *JsonDataHelper::GetValueTypeName(FoundData->ValueType));
        return;
    }

    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
}

void UAsync_ReadJson::GetNodeValueToIntArray(const FString& NodePath, const FParsedData& ParsedData, TArray<int32>& NodeArray, bool& bIsValid)
{
    NodeArray.Empty();
    bIsValid = false;
    if (!JsonDataHelper::ValidateNodePath(NodePath, TEXT("GetNodeValueToIntArray")))
    {
        return;
    }

    if (const FJsonDataStruct* FoundData = ParsedData.ParsedDataMap.Find(NodePath))
    {
        if (FoundData->ValueType == EValueType::String)
        {
            if (FoundData->StringValue.IsEmpty())
            {
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node Value: [ %s ] is empty"), __FUNCTION__, *NodePath);
                return;
            }
            ParseJsonArrayToIntArray(FoundData->StringValue, NodeArray, bIsValid);
            return;
        }
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string (array), Node Type: [ %s ]"), 
            __FUNCTION__, *NodePath, *JsonDataHelper::GetValueTypeName(FoundData->ValueType));
        return;
    }

    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
}

void UAsync_ReadJson::GetNodeValueToFloatArray(const FString& NodePath, const FParsedData& ParsedData, TArray<float>& NodeArray, bool& bIsValid)
{
    NodeArray.Empty();
    bIsValid = false;
    if (!JsonDataHelper::ValidateNodePath(NodePath, TEXT("GetNodeValueToFloatArray")))
    {
        return;
    }

    if (const FJsonDataStruct* FoundData = ParsedData.ParsedDataMap.Find(NodePath))
    {
        if (FoundData->ValueType == EValueType::String)
        {
            if (FoundData->StringValue.IsEmpty())
            {
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node Value: [ %s ] is empty"), __FUNCTION__, *NodePath);
                return;
            }
            ParseJsonArrayToFloatArray(FoundData->StringValue, NodeArray, bIsValid);
            return;
        }
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string (array), Node Type: [ %s ]"), 
            __FUNCTION__, *NodePath, *JsonDataHelper::GetValueTypeName(FoundData->ValueType));
        return;
    }

    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
}

void UAsync_ReadJson::GetNodeValueToBoolArray(const FString& NodePath, const FParsedData& ParsedData, TArray<bool>& NodeArray, bool& bIsValid)
{
    NodeArray.Empty();
    bIsValid = false;
    if (!JsonDataHelper::ValidateNodePath(NodePath, TEXT("GetNodeValueToBoolArray")))
    {
        return;
    }

    if (const FJsonDataStruct* FoundData = ParsedData.ParsedDataMap.Find(NodePath))
    {
        if (FoundData->ValueType == EValueType::String)
        {
            if (FoundData->StringValue.IsEmpty())
            {
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node Value: [ %s ] is empty"), __FUNCTION__, *NodePath);
                return;
            }
            ParseJsonArrayToBoolArray(FoundData->StringValue, NodeArray, bIsValid);
            return;
        }
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string (array), Node Type: [ %s ]"), 
            __FUNCTION__, *NodePath, *JsonDataHelper::GetValueTypeName(FoundData->ValueType));
        return;
    }

    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
}

// ============================================================================
// 数组解析实现
// ============================================================================
void UAsync_ReadJson::ParseJsonArray(const FString& JsonArray, FJsonArray& ArrayValue, bool& bIsValid)
{
    ArrayValue = {};
    bIsValid = false;

    const TArray<TSharedPtr<FJsonValue>> JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty())
    {
        return;
    }

    for (const TSharedPtr<FJsonValue>& Element : JsonValueArray)
    {
        switch (Element->Type)
        {
        case EJson::String:
            ArrayValue.StringArray.Add(Element->AsString());
            break;
        case EJson::Boolean:
            ArrayValue.BoolArray.Add(Element->AsBool());
            break;
        case EJson::Number:
            {
                const double Num = Element->AsNumber();
                if (FMath::IsFinite(Num))
                {
                    if (JsonDataHelper::IsIntegerValue(Num))
                    {
                        ArrayValue.IntArray.Add(static_cast<int32>(Num));
                    }
                    else
                    {
                        ArrayValue.FloatArray.Add(static_cast<float>(Num));
                    }
                }
                break;
            }
        case EJson::Object:
            {
                FString ObjectString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ObjectString);
                if (FJsonSerializer::Serialize(Element->AsObject().ToSharedRef(), Writer))
                {
                    ArrayValue.StringArray.Add(ObjectString);
                }
                break;
            }
        case EJson::Array:
            {
                FString NestedArrayString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NestedArrayString);
                if (FJsonSerializer::Serialize(Element->AsArray(), Writer))
                {
                    ArrayValue.StringArray.Add(NestedArrayString);
                }
                break;
            }
        default:
            ArrayValue.StringArray.Add(Element->AsString());
            break;
        }
    }
    bIsValid = true;
}

void UAsync_ReadJson::ParseJsonArrayToStringArray(const FString& JsonArray, TArray<FString>& ArrayValue, bool& bIsValid)
{
    ArrayValue.Empty();
    bIsValid = false;

    const TArray<TSharedPtr<FJsonValue>> JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty())
    {
        return;
    }

    for (const TSharedPtr<FJsonValue>& Element : JsonValueArray)
    {
        switch (Element->Type)
        {
        case EJson::String:
            ArrayValue.Add(Element->AsString());
            break;
        case EJson::Object:
            {
                FString ObjectString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ObjectString);
                if (FJsonSerializer::Serialize(Element->AsObject().ToSharedRef(), Writer))
                {
                    ArrayValue.Add(ObjectString);
                }
                break;
            }
        case EJson::Array:
            {
                FString NestedArrayString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NestedArrayString);
                if (FJsonSerializer::Serialize(Element->AsArray(), Writer))
                {
                    ArrayValue.Add(NestedArrayString);
                }
                break;
            }
        default:
            ArrayValue.Add(Element->AsString());
            break;
        }
    }
    bIsValid = true;
}

void UAsync_ReadJson::ParseJsonArrayToIntArray(const FString& JsonArray, TArray<int32>& ArrayValue, bool& bIsValid)
{
    ArrayValue.Empty();
    bIsValid = false;

    const TArray<TSharedPtr<FJsonValue>> JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty())
    {
        return;
    }

    for (const TSharedPtr<FJsonValue>& Element : JsonValueArray)
    {
        if (Element->Type == EJson::Number)
        {
            const double Num = Element->AsNumber();
            if (FMath::IsFinite(Num) && JsonDataHelper::IsIntegerValue(Num))
            {
                ArrayValue.Add(static_cast<int32>(Num));
            }
            else
            {
                // 警告跳过的非整数元素
                UE_LOG(LogReadJson, Verbose, TEXT("[ %hs ] Skipped non-integer number: %f"), __FUNCTION__, Num);
            }
        }
        else
        {
            // 警告跳过的非数字类型元素
            UE_LOG(LogReadJson, Verbose, TEXT("[ %hs ] Skipped non-number element of type: %d"), __FUNCTION__, static_cast<int32>(Element->Type));
        }
    }
    bIsValid = true;
}

void UAsync_ReadJson::ParseJsonArrayToFloatArray(const FString& JsonArray, TArray<float>& ArrayValue, bool& bIsValid)
{
    ArrayValue.Empty();
    bIsValid = false;

    const TArray<TSharedPtr<FJsonValue>> JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty())
    {
        return;
    }

    for (const TSharedPtr<FJsonValue>& Element : JsonValueArray)
    {
        if (Element->Type == EJson::Number)
        {
            const double Num = Element->AsNumber();
            if (FMath::IsFinite(Num))
            {
                ArrayValue.Add(static_cast<float>(Num));
            }
            else
            {
                // 警告跳过的非有限数值
                UE_LOG(LogReadJson, Verbose, TEXT("[ %hs ] Skipped non-finite number"), __FUNCTION__);
            }
        }
        else
        {
            // 警告跳过的非数字类型元素
            UE_LOG(LogReadJson, Verbose, TEXT("[ %hs ] Skipped non-number element of type: %d"), __FUNCTION__, static_cast<int32>(Element->Type));
        }
    }
    bIsValid = true;
}

void UAsync_ReadJson::ParseJsonArrayToBoolArray(const FString& JsonArray, TArray<bool>& ArrayValue, bool& bIsValid)
{
    ArrayValue.Empty();
    bIsValid = false;

    const TArray<TSharedPtr<FJsonValue>> JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty())
    {
        return;
    }

    for (const TSharedPtr<FJsonValue>& Element : JsonValueArray)
    {
        if (Element->Type == EJson::Boolean)
        {
            ArrayValue.Add(Element->AsBool());
        }
        else
        {
            // 警告跳过的非布尔类型元素
            UE_LOG(LogReadJson, Verbose, TEXT("[ %hs ] Skipped non-boolean element of type: %d"), __FUNCTION__, static_cast<int32>(Element->Type));
        }
    }
    bIsValid = true;
}

// ============================================================================
// 便捷函数实现
// ============================================================================
void UAsync_ReadJson::ReadJson_Block_ByNodePathToString(
    const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, FString& NodeValue, bool& bIsValid)
{
    FParsedData ParsedData;
    ReadJson_Block(WorldContextObject, InJsonStr, ParsedData, bIsValid);
    if (bIsValid)
    {
        GetNodeValueToString(NodePath, ParsedData, NodeValue, bIsValid);
    }
}

void UAsync_ReadJson::ReadJson_Block_ByNodePathToInt(
    const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, int32& NodeValue, bool& bIsValid)
{
    FParsedData ParsedData;
    ReadJson_Block(WorldContextObject, InJsonStr, ParsedData, bIsValid);
    if (bIsValid)
    {
        GetNodeValueToInt(NodePath, ParsedData, NodeValue, bIsValid);
    }
}

void UAsync_ReadJson::ReadJson_Block_ByNodePathToFloat(
    const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, float& NodeValue, bool& bIsValid)
{
    FParsedData ParsedData;
    ReadJson_Block(WorldContextObject, InJsonStr, ParsedData, bIsValid);
    if (bIsValid)
    {
        GetNodeValueToFloat(NodePath, ParsedData, NodeValue, bIsValid);
    }
}

void UAsync_ReadJson::ReadJson_Block_ByNodePathToBool(
    const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, bool& NodeValue, bool& bIsValid)
{
    FParsedData ParsedData;
    ReadJson_Block(WorldContextObject, InJsonStr, ParsedData, bIsValid);
    if (bIsValid)
    {
        GetNodeValueToBool(NodePath, ParsedData, NodeValue, bIsValid);
    }
}

void UAsync_ReadJson::ReadJson_Block_ByNodePathToStringArray(
    const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<FString>& NodeArray, bool& bIsValid)
{
    FParsedData ParsedData;
    ReadJson_Block(WorldContextObject, InJsonStr, ParsedData, bIsValid);
    if (bIsValid)
    {
        GetNodeValueToStringArray(NodePath, ParsedData, NodeArray, bIsValid);
    }
}

void UAsync_ReadJson::ReadJson_Block_ByNodePathToIntArray(
    const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<int32>& NodeArray, bool& bIsValid)
{
    FParsedData ParsedData;
    ReadJson_Block(WorldContextObject, InJsonStr, ParsedData, bIsValid);
    if (bIsValid)
    {
        GetNodeValueToIntArray(NodePath, ParsedData, NodeArray, bIsValid);
    }
}

void UAsync_ReadJson::ReadJson_Block_ByNodePathToFloatArray(
    const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<float>& NodeArray, bool& bIsValid)
{
    FParsedData ParsedData;
    ReadJson_Block(WorldContextObject, InJsonStr, ParsedData, bIsValid);
    if (bIsValid)
    {
        GetNodeValueToFloatArray(NodePath, ParsedData, NodeArray, bIsValid);
    }
}

void UAsync_ReadJson::ReadJson_Block_ByNodePathToBoolArray(
    const UObject* WorldContextObject, const FString& InJsonStr, const FString& NodePath, TArray<bool>& NodeArray, bool& bIsValid)
{
    FParsedData ParsedData;
    ReadJson_Block(WorldContextObject, InJsonStr, ParsedData, bIsValid);
    if (bIsValid)
    {
        GetNodeValueToBoolArray(NodePath, ParsedData, NodeArray, bIsValid);
    }
}