#include "Async_ReadJson.h"

UAsync_ReadJson* UAsync_ReadJson::Async_ReadJson(UObject* WorldContextObject, const FString& InJsonStr, const bool IsLargeJson)
{
	UAsync_ReadJson* AsyncTask = NewObject<UAsync_ReadJson>();
	AsyncTask->WorldContext = WorldContextObject;
	AsyncTask->JsonStr = InJsonStr;
	AsyncTask->IsLargeJson = IsLargeJson;
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
        if (Elem.Value->Type == EJson::Object)
        {
            Count += CountJsonNodes(Elem.Value->AsObject());
        }
        ++Count;
    }
    return Count;
}

void UAsync_ReadJson::LoadJson(const FString& JsonString)
{
    if (JsonString.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("[ %hs ] JsonString is Invalid"), __FUNCTION__);
        OnReadJsonFailed.Broadcast({});
        DestroyTask();
        return;
    }
    
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
        UE_LOG(LogReadJson, Error, TEXT("[ %hs ] Deserialize Failed, JsonString is invalid"), __FUNCTION__);
		OnReadJsonFailed.Broadcast({});
		DestroyTask();
        return;
	}

	if (IsLargeJson)
	{
	    const auto Count = CountJsonNodes(JsonObject);
		ParsedDataMap.Empty(Count);
	    // TMap 初始扩容大小为 Count
	    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Begin Parse Large Json, TMap initial size is %d"), __FUNCTION__, Count);
	    
		ParseJsonIterative(JsonObject);
	}
	else
	{
	    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Begin Parse Json"), __FUNCTION__);
		ParseJson(JsonObject, TEXT(""));
	}
	
	OnReadJsonCompleted.Broadcast({ ParsedDataMap });
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] End Parse Json"), __FUNCTION__);
	DestroyTask();
}

void UAsync_ReadJson::ParseJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath)
{
    if (!JsonObject.IsValid() || JsonObject->Values.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("[ %hs ] JsonObject is invalid or empty"), __FUNCTION__);
        
        // 如果 JsonObject 为空或没有键值对，则返回
        return;
    }
    
    for (const auto& Elem : JsonObject->Values)
    {
        FString NewPath = CurrentPath.IsEmpty() ? Elem.Key : CurrentPath + TEXT(".") + Elem.Key;
        const TSharedPtr<FJsonValue>& Value = Elem.Value;

        if (Value->Type == EJson::Object)
        {
            // 解析遇到对象类型，先转成字符串，然后递归解析子对象
            UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Parse Json Object: [ %s ] => Json String; Wait for further parsing..."), __FUNCTION__, *NewPath);
            
            // 将整个子对象序列化为 JSON 字符串并存储在 StringValue 中
            FString ObjectString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ObjectString);
            if (FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer))
            {
                ParsedDataMap.Add(NewPath, { ObjectString, {}, {}, {}, { EValueType::String } });
            }

            // 递归解析子对象
            ParseJson(Value->AsObject(), NewPath);
        }
        else if (Value->Type == EJson::Array)
        {
            // 解析遇到数组类型，不进行迭代解析，转成字符串
            UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Parse Json Array: [ %s ] => Json String"), __FUNCTION__, *NewPath);
            
            // 将整个数组序列化为 JSON 字符串并存储在 StringValue 中, 不对数组内部元素进行迭代解析
            FString ArrayString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArrayString);
            if (FJsonSerializer::Serialize(Value->AsArray(), Writer))
            {
                ParsedDataMap.Add(NewPath, { ArrayString, {}, {}, {}, { EValueType::String } });
            }
        }
        else
        {
            // 处理基本类型（字符串、数值、布尔值等）
            switch (Value->Type)
            {
                case EJson::String:
                    ParsedDataMap.Add(NewPath, { Value->AsString(), {}, {}, {}, { EValueType::String } });
                    break;
                case EJson::Boolean:
                    ParsedDataMap.Add(NewPath, { {}, Value->AsBool(), {}, {}, { EValueType::Bool } });    
                    break;
                case EJson::Number:
                {
                    if (const double Num = Value->AsNumber(); FMath::IsFinite(Num))
                    {
                        if (FMath::RoundToInt32(Num) == Num)
                            ParsedDataMap.Add(NewPath, { {}, {}, FMath::RoundToInt32(Num), {}, { EValueType::Int } });    
                        else
                            ParsedDataMap.Add(NewPath, { {}, {}, {}, static_cast<float>(Num), { EValueType::Float } });
                    }
                    break;
                }
                default:
                    // 对于 Array、None 或其他未处理的类型，尝试将其转换为字符串
                    ParsedDataMap.Add(NewPath, { Value->AsString(), {}, {}, {}, { EValueType::String } });
                    break;
            }
        }
    }
}

void UAsync_ReadJson::ParseJsonIterative(const TSharedPtr<FJsonObject>& RootJson)
{
    if (!RootJson.IsValid() || RootJson->Values.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("[ %hs ] JsonObject is invalid or empty"), __FUNCTION__);
        
        // 如果 JsonObject 为空或没有键值对，则返回
        return;
    }

    TArray<TempJsonNode::FJsonNode> Stack;
    Stack.Push({ RootJson, TEXT("") });

    while (Stack.Num() > 0)
    {
        const auto CurrentNode = Stack.Pop();
        const TSharedPtr<FJsonObject> JsonObject = CurrentNode.JsonObject;
        FString CurrentPath = CurrentNode.CurrentPath;

        for (const auto& Elem : JsonObject->Values)
        {
            FString NewPath = CurrentPath.IsEmpty() ? Elem.Key : CurrentPath + TEXT(".") + Elem.Key;
            const TSharedPtr<FJsonValue>& Value = Elem.Value;

            if (Value->Type == EJson::Object)
            {
                // 解析遇到对象类型，先转成字符串，然后递归解析子对象
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Parse Json Object: [ %s ] => Json String; Wait for further parsing..."), __FUNCTION__, *NewPath);
                
                // 将整个子对象序列化为 JSON 字符串
                FString ObjectString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ObjectString);
                if (FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer))
                {
                    // 创建 DataStruct 并添加到 ParsedDataMap
                    ParsedDataMap.Add(NewPath, { ObjectString, {}, {}, {}, { EValueType::String } });
                }

                // 将子对象压入栈中以继续解析
                Stack.Push({ Value->AsObject(), NewPath });
            }
            else if (Value->Type == EJson::Array)
            {
                // 解析遇到数组类型，不进行迭代解析，转成字符串
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Parse Json Array: [ %s ] => Json String"), __FUNCTION__, *NewPath);
                
                // 将数组序列化为 JSON 字符串并存储在 StringValue 中, 不对数组内部元素进行迭代解析
                FString ArrayString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArrayString);
                if (FJsonSerializer::Serialize(Value->AsArray(), Writer))
                {
                    ParsedDataMap.Add(NewPath, { ArrayString, {}, {}, {}, { EValueType::String } });
                }
            }
            else
            {
                switch (Value->Type)
                {
                case EJson::String:
                    ParsedDataMap.Add(NewPath, { Value->AsString(), {}, {}, {}, { EValueType::String } });
                    break;
                case EJson::Boolean:
                    ParsedDataMap.Add(NewPath, { {}, Value->AsBool(), {}, {}, { EValueType::Bool } });
                    break;
                case EJson::Number:
                    {
	                    if (const double Num = Value->AsNumber(); FMath::IsFinite(Num))
                        {
                            if (FMath::RoundToInt32(Num) == Num)
                            {
                                ParsedDataMap.Add(NewPath, { {}, {}, FMath::RoundToInt32(Num), {}, { EValueType::Int } });
                            }
                            else
                                ParsedDataMap.Add(NewPath, { {}, {}, {}, static_cast<float>(Num), { EValueType::Float } });
                        }
                        break;
                    }
                case EJson::Array:
                case EJson::None:
                default:
                    ParsedDataMap.Add(NewPath, { Value->AsString(), {}, {}, {}, { EValueType::String } });
                    break;
                }
            }
        }
    }
}

bool UAsync_ReadJson::GetNodeData(const FString& NodePath, const FParsedData& ParsedData, FJsonNode& NodeData)
{
    // 如果节点路径为空，则返回 false
    CHECK_NODE_PATH(NodePath);
    
	if (ParsedData.ParsedDataMap.Contains(NodePath))
	{
	    NodeData =  {NodePath, ParsedData.ParsedDataMap[NodePath] };
		return true;
	}
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
	return false;
}

TArray<TSharedPtr<FJsonValue>> UAsync_ReadJson::GetJsonValueArray(const FString& JsonArray)
{
    // 检查输入字符串是否为空
    CHECK_JSON_ARRAY_EMPTY(JsonArray);

    // 创建一个用于存储解析后的 JSON 数组的指针
    TSharedPtr<FJsonValue> JsonValue;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonArray);

    // 尝试解析输入字符串为 JSON 值
    TRY_PARSE_JSON_VALUE(Reader, JsonValue);

    // 确保解析的 JSON 值是一个数组
    CHECK_JSON_VALUE_IS_ARRAY(JsonValue);

    // 确保数组元素数量大于 0
    CHECK_JSON_VALUE_TO_ARRAY_ELEMENT_COUNT(JsonValue);

    // 获取数组元素
    return JsonValue->AsArray();
}

bool UAsync_ReadJson::ParseJsonArray(const FString& JsonArray, FJsonArray& ArrayValue)
{
    // 在开始解析前，清空所有目标数组，避免之前的数据影响当前解析
    ArrayValue = {};
    
    auto JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty()) return false;

    for (int32 Index = 0; Index < JsonValueArray.Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& Element = JsonValueArray[Index];

        switch (Element->Type)
        {
            case EJson::String:
            {
                // 字符串类型
                ArrayValue.StringArray.Add(Element->AsString());
                break;
            }
            case EJson::Boolean:
            {
                // 布尔类型
                ArrayValue.BoolArray.Add(Element->AsBool());
                break;
            }
            case EJson::Number:
            {
                // 数值类型，区分整数和浮点数
                if (const double Num = Element->AsNumber(); FMath::IsFinite(Num))
                {
                    if (FMath::RoundToInt32(Num) == Num)
                    {
                        // 整数
                        ArrayValue.IntArray.Add(FMath::RoundToInt32(Num));
                    }
                    else
                    {
                        // 浮点数
                        ArrayValue.FloatArray.Add(static_cast<float>(Num));
                    }
                }
                break;
            }
            case EJson::Object:
            {
                // 对象类型，序列化为字符串并添加到 StringArray
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
                // 数组类型，序列化为字符串并添加到 StringArray
                FString NestedArrayString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NestedArrayString);
                if (FJsonSerializer::Serialize(Element->AsArray(), Writer))
                {
                    ArrayValue.StringArray.Add(NestedArrayString);
                }
                break;
            }
            default:
            {
                // 其他类型（如 Null），尝试转换为字符串
                ArrayValue.StringArray.Add(Element->AsString());
                break;
            }
        }
    }
    return true;
}

void UAsync_ReadJson::EndTask()
{
	OnReadJsonEnd.Broadcast({ ParsedDataMap });
	DestroyTask();
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Async_ReadJson EndTask"), __FUNCTION__);
}

void UAsync_ReadJson::DestroyTask()
{
	SetReadyToDestroy();
	MarkAsGarbage();
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Async_ReadJson DestroyTask"), __FUNCTION__);
}

FString UAsync_ReadJson::GetValueTypeName(const EValueType& ValueType)
{
    switch (ValueType)
    {
    case EValueType::String:
        return FString{ "String" };
    case EValueType::Float:
        return FString{ "Float" };
    case EValueType::Int:
        return FString{ "Int" };
    case EValueType::Bool:
        return FString{ "Bool" };
    default:
        return FString{ "String" };
    }
}

bool UAsync_ReadJson::GetNodeValueToString(const FString& NodePath, const FParsedData& ParsedData, FString& NodeValue)
{
    // 如果节点路径为空，则返回 false
    CHECK_NODE_PATH(NodePath);
    
    if (ParsedData.ParsedDataMap.Contains(NodePath))
    {
        const auto ValueType = ParsedData.ParsedDataMap[NodePath].ValueType;
        if (ValueType == EValueType::String)
        {
            NodeValue = ParsedData.ParsedDataMap[NodePath].StringValue;
            return true;
        }
        // 节点类型不匹配
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string, Node Type: [ %s ]"), __FUNCTION__, *NodePath, *GetValueTypeName(ValueType));
        return false;
    }
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
    return false;
}

bool UAsync_ReadJson::GetNodeValueToInt(const FString& NodePath, const FParsedData& ParsedData, int32& NodeValue)
{
    // 如果节点路径为空，则返回 false
    CHECK_NODE_PATH(NodePath);
    
    if (ParsedData.ParsedDataMap.Contains(NodePath))
    {
        const auto ValueType = ParsedData.ParsedDataMap[NodePath].ValueType;
        if (ValueType == EValueType::Int)
        {
            NodeValue = ParsedData.ParsedDataMap[NodePath].IntValue;
            return true;
        }
        // 节点类型不匹配
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string, Node Type: [ %s ]"), __FUNCTION__, *NodePath, *GetValueTypeName(ValueType));
        return false;
    }
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
    return false;
}

bool UAsync_ReadJson::GetNodeValueToFloat(const FString& NodePath, const FParsedData& ParsedData, float& NodeValue)
{
    // 如果节点路径为空，则返回 false
    CHECK_NODE_PATH(NodePath);
    
    if (ParsedData.ParsedDataMap.Contains(NodePath))
    {
        const auto ValueType = ParsedData.ParsedDataMap[NodePath].ValueType;
        if (ValueType == EValueType::Float)
        {
            NodeValue = ParsedData.ParsedDataMap[NodePath].FloatValue;
            return true;
        }
        // 节点类型不匹配
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string, Node Type: [ %s ]"), __FUNCTION__, *NodePath, *GetValueTypeName(ValueType));
        return false;
    }
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
    return false;
}

bool UAsync_ReadJson::GetNodeValueToBool(const FString& NodePath, const FParsedData& ParsedData, bool& NodeValue)
{
    // 如果节点路径为空，则返回 false
    CHECK_NODE_PATH(NodePath);
    
    if (ParsedData.ParsedDataMap.Contains(NodePath))
    {
        const auto ValueType = ParsedData.ParsedDataMap[NodePath].ValueType;
        if (ValueType == EValueType::Bool)
        {
            NodeValue = ParsedData.ParsedDataMap[NodePath].BoolValue;
            return true;
        }
        // 节点类型不匹配
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string, Node Type: [ %s ]"), __FUNCTION__, *NodePath, *GetValueTypeName(ValueType));
        return false;
    }
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
    return false;
}

bool UAsync_ReadJson::ParseJsonArrayToStringArray(const FString& JsonArray, TArray<FString>& ArrayValue)
{
    // 在开始解析前，清空所有目标数组，避免之前的数据影响当前解析
    ArrayValue = {};
    
    auto JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty()) return false;

    for (int32 Index = 0; Index < JsonValueArray.Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& Element = JsonValueArray[Index];

        switch (Element->Type)
        {
        case EJson::String:
            {
                // 字符串类型
                ArrayValue.Add(Element->AsString());
                break;
            }
        case EJson::Object:
            {
                // 对象类型，序列化为字符串并添加到 StringArray
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
                // 数组类型，序列化为字符串并添加到 StringArray
                FString NestedArrayString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&NestedArrayString);
                if (FJsonSerializer::Serialize(Element->AsArray(), Writer))
                {
                    ArrayValue.Add(NestedArrayString);
                }
                break;
            }
        default:
            {
                // 其他类型（如 Null），尝试转换为字符串
                ArrayValue.Add(Element->AsString());
                break;
            }
        }
    }
    return true;
}

bool UAsync_ReadJson::ParseJsonArrayToIntArray(const FString& JsonArray, TArray<int32>& ArrayValue)
{
    // 在开始解析前，清空所有目标数组，避免之前的数据影响当前解析
    ArrayValue = {};
    
    auto JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty()) return false;

    for (int32 Index = 0; Index < JsonValueArray.Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& Element = JsonValueArray[Index];

        switch (Element->Type)
        {
        case EJson::Number:
            {
                // 数值类型，区分整数和浮点数
                if (const double Num = Element->AsNumber(); FMath::IsFinite(Num))
                {
                    if (FMath::RoundToInt32(Num) == Num)
                    {
                        // 整数
                        ArrayValue.Add(FMath::RoundToInt32(Num));
                    }
                }
                break;
            }
        default: ;
        }
    }
    return true;
}

bool UAsync_ReadJson::ParseJsonArrayToFloatArray(const FString& JsonArray, TArray<float>& ArrayValue)
{
    // 在开始解析前，清空所有目标数组，避免之前的数据影响当前解析
    ArrayValue = {};
    
    auto JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty()) return false;

    for (int32 Index = 0; Index < JsonValueArray.Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& Element = JsonValueArray[Index];

        switch (Element->Type)
        {
        case EJson::Number:
            {
                // 数值类型，区分整数和浮点数
                if (const double Num = Element->AsNumber(); FMath::IsFinite(Num))
                {
                    // 浮点数
                    ArrayValue.Add(static_cast<float>(Num));
                }
                break;
            }
        default: ;
        }
    }
    return true;
}

bool UAsync_ReadJson::ParseJsonArrayToBoolArray(const FString& JsonArray, TArray<bool>& ArrayValue)
{
    // 在开始解析前，清空所有目标数组，避免之前的数据影响当前解析
    ArrayValue = {};
    
    auto JsonValueArray = GetJsonValueArray(JsonArray);
    if (JsonValueArray.IsEmpty()) return false;

    for (int32 Index = 0; Index < JsonValueArray.Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& Element = JsonValueArray[Index];

        switch (Element->Type)
        {
        case EJson::Boolean:
            {
                // 布尔类型
                ArrayValue.Add(Element->AsBool());
                break;
            }
        default: ;
        }
    }
    return true;
}

bool UAsync_ReadJson::GetNodeValueToStringArray(const FString& NodePath, const FParsedData& ParsedData, TArray<FString>& NodeArray)
{
    NodeArray = {};
    
    // 如果节点路径为空，则返回 false
    CHECK_NODE_PATH(NodePath);
    
    if (ParsedData.ParsedDataMap.Contains(NodePath))
    {
        const auto JsonData = ParsedData.ParsedDataMap[NodePath];
        const auto ValueType = JsonData.ValueType;
        if (ValueType == EValueType::String)
        {
            // 检查是否是空字符串
            if (JsonData.StringValue.IsEmpty())
            {
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node Value: [ %s ] is empty"), __FUNCTION__, *NodePath);
                return false;
            }
            
            // 解析字符串到数组
            return ParseJsonArrayToStringArray(JsonData.StringValue, NodeArray);
        }
        // 节点类型不匹配
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string, Node Type: [ %s ]"), __FUNCTION__, *NodePath, *GetValueTypeName(ValueType));
        return false;
    }
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
    return false;
}

bool UAsync_ReadJson::GetNodeValueToIntArray(const FString& NodePath, const FParsedData& ParsedData, TArray<int32>& NodeArray)
{
    NodeArray = {};
    
    // 如果节点路径为空，则返回 false
    CHECK_NODE_PATH(NodePath);
    
    if (ParsedData.ParsedDataMap.Contains(NodePath))
    {
        const auto JsonData = ParsedData.ParsedDataMap[NodePath];
        const auto ValueType = JsonData.ValueType;
        if (ValueType == EValueType::String)
        {
            // 检查是否是空字符串
            if (JsonData.StringValue.IsEmpty())
            {
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node Value: [ %s ] is empty"), __FUNCTION__, *NodePath);
                return false;
            }
            
            // 解析字符串到数组
            return ParseJsonArrayToIntArray(JsonData.StringValue, NodeArray);
        }
        // 节点类型不匹配
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string, Node Type: [ %s ]"), __FUNCTION__, *NodePath, *GetValueTypeName(ValueType));
        return false;
    }
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
    return false;
}

bool UAsync_ReadJson::GetNodeValueToFloatArray(const FString& NodePath, const FParsedData& ParsedData, TArray<float>& NodeArray)
{
    NodeArray = {};
    
    // 如果节点路径为空，则返回 false
    CHECK_NODE_PATH(NodePath);
    
    if (ParsedData.ParsedDataMap.Contains(NodePath))
    {
        const auto JsonData = ParsedData.ParsedDataMap[NodePath];
        const auto ValueType = JsonData.ValueType;
        if (ValueType == EValueType::String)
        {
            // 检查是否是空字符串
            if (JsonData.StringValue.IsEmpty())
            {
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node Value: [ %s ] is empty"), __FUNCTION__, *NodePath);
                return false;
            }
            
            // 解析字符串到数组
            return ParseJsonArrayToFloatArray(JsonData.StringValue, NodeArray);
        }
        // 节点类型不匹配
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string, Node Type: [ %s ]"), __FUNCTION__, *NodePath, *GetValueTypeName(ValueType));
        return false;
    }
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
    return false;
}

bool UAsync_ReadJson::GetNodeValueToBoolArray(const FString& NodePath, const FParsedData& ParsedData, TArray<bool>& NodeArray)
{
    NodeArray = {};
    
    // 如果节点路径为空，则返回 false
    CHECK_NODE_PATH(NodePath);
    
    if (ParsedData.ParsedDataMap.Contains(NodePath))
    {
        const auto JsonData = ParsedData.ParsedDataMap[NodePath];
        const auto ValueType = JsonData.ValueType;
        if (ValueType == EValueType::String)
        {
            // 检查是否是空字符串
            if (JsonData.StringValue.IsEmpty())
            {
                UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node Value: [ %s ] is empty"), __FUNCTION__, *NodePath);
                return false;
            }
            
            // 解析字符串到数组
            return ParseJsonArrayToBoolArray(JsonData.StringValue, NodeArray);
        }
        // 节点类型不匹配
        UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] is not a string, Node Type: [ %s ]"), __FUNCTION__, *NodePath, *GetValueTypeName(ValueType));
        return false;
    }
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Warning, TEXT("[ %hs ] Node [ %s ] not found"), __FUNCTION__, *NodePath);
    return false;
}
