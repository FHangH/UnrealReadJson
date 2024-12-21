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
	int32 Count = 0;
	for (const auto& Elem : JsonObject->Values)
	{
		if (Elem.Value->Type == EJson::Object)
		{
			Count += CountJsonNodes(Elem.Value->AsObject());
		}
		Count++;
	}
	return Count;
}

void UAsync_ReadJson::LoadJson(const FString& JsonString)
{
    if (JsonString.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("<< ReadJson_Async_LoadJson >> JsonString is Invalid"));
        OnReadJsonFailed.Broadcast({});
        DestroyTask();
        return;
    }
    
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
        UE_LOG(LogReadJson, Error, TEXT("<< ReadJson_Async_LoadJson >> Deserialize Failed, JsonString is invalid"));
		OnReadJsonFailed.Broadcast({});
		DestroyTask();
        return;
	}

	if (IsLargeJson)
	{
	    const auto Count = CountJsonNodes(JsonObject);
		ParsedDataMap.Empty(Count);
	    // TMap 初始扩容大小为 Count
	    UE_LOG(LogReadJson, Warning, TEXT("<< ReadJson_Async_LoadJson >> Begin Parse Large Json, TMap initial size is %d"), Count);
	    
		ParseJsonIterative(JsonObject);
	}
	else
	{
	    UE_LOG(LogReadJson, Warning, TEXT("<< ReadJson_Async_LoadJson >> Begin Parse Json"));
		ParseJson(JsonObject, TEXT(""));
	}
	
	OnReadJsonCompleted.Broadcast({ ParsedDataMap });
    UE_LOG(LogReadJson, Warning, TEXT("<< ReadJson_Async_LoadJson >> End Parse Json"));
	DestroyTask();
}

void UAsync_ReadJson::ParseJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath)
{
    if (!JsonObject.IsValid() || JsonObject->Values.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("<< ReadJson_Async_ParseJson >> JsonObject is invalid or empty"));
        
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
            UE_LOG(LogReadJson, Warning, TEXT("<< ReadJson_Async_ParseJson >> Parse Json Object: [ %s ] => Json String; Wait for further parsing..."), *NewPath);
            
            // 将整个子对象序列化为 JSON 字符串并存储在 StringValue 中
            FString ObjectString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ObjectString);
            if (FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer))
            {
                ParsedDataMap.Add(NewPath, { ObjectString, {}, {}, {} });
            }

            // 递归解析子对象
            ParseJson(Value->AsObject(), NewPath);
        }
        else if (Value->Type == EJson::Array)
        {
            // 解析遇到数组类型，不进行迭代解析，转成字符串
            UE_LOG(LogReadJson, Warning, TEXT("<< ReadJson_Async_ParseJson >> Parse Json Array: [ %s ] => Json String"), *NewPath);
            
            // 将整个数组序列化为 JSON 字符串并存储在 StringValue 中, 不对数组内部元素进行迭代解析
            FString ArrayString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArrayString);
            if (FJsonSerializer::Serialize(Value->AsArray(), Writer))
            {
                ParsedDataMap.Add(NewPath, { ArrayString, {}, {}, {} });
            }
        }
        else
        {
            // 处理基本类型（字符串、数值、布尔值等）
            switch (Value->Type)
            {
                case EJson::String:
                    ParsedDataMap.Add(NewPath, { Value->AsString(), {}, {}, {} });
                    break;
                case EJson::Boolean:
                    ParsedDataMap.Add(NewPath, { {}, Value->AsBool(), {}, {} });    
                    break;
                case EJson::Number:
                {
                    if (const double Num = Value->AsNumber(); FMath::IsFinite(Num))
                    {
                        if (FMath::RoundToInt32(Num) == Num)
                            ParsedDataMap.Add(NewPath, { {}, {}, FMath::RoundToInt32(Num), {} });    
                        else
                            ParsedDataMap.Add(NewPath, { {}, {}, {}, static_cast<float>(Num) });
                    }
                    break;
                }
                default:
                    // 对于 Array、None 或其他未处理的类型，尝试将其转换为字符串
                    ParsedDataMap.Add(NewPath, { Value->AsString(), {}, {}, {} });
                    break;
            }
        }
    }
}

void UAsync_ReadJson::ParseJsonIterative(const TSharedPtr<FJsonObject>& RootJson)
{
    if (!RootJson.IsValid() || RootJson->Values.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("<< ReadJson_Async_ParseJsonIterative >> JsonObject is invalid or empty"));
        
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
                UE_LOG(LogReadJson, Warning, TEXT("<< ReadJson_Async_ParseJsonIterative >> Parse Json Object: [ %s ] => Json String; Wait for further parsing..."), *NewPath);
                
                // 将整个子对象序列化为 JSON 字符串
                FString ObjectString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ObjectString);
                if (FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer))
                {
                    // 创建 DataStruct 并添加到 ParsedDataMap
                    ParsedDataMap.Add(NewPath, { ObjectString, {}, {}, {} });
                }

                // 将子对象压入栈中以继续解析
                Stack.Push({ Value->AsObject(), NewPath });
            }
            else if (Value->Type == EJson::Array)
            {
                // 解析遇到数组类型，不进行迭代解析，转成字符串
                UE_LOG(LogReadJson, Warning, TEXT("<< ReadJson_Async_ParseJsonIterative >> Parse Json Array: [ %s ] => Json String"), *NewPath);
                
                // 将数组序列化为 JSON 字符串并存储在 StringValue 中, 不对数组内部元素进行迭代解析
                FString ArrayString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArrayString);
                if (FJsonSerializer::Serialize(Value->AsArray(), Writer))
                {
                    ParsedDataMap.Add(NewPath, { ArrayString, {}, {}, {} });
                }
            }
            else
            {
                switch (Value->Type)
                {
                case EJson::String:
                    ParsedDataMap.Add(NewPath, { Value->AsString(), {}, {}, {} });
                    break;
                case EJson::Boolean:
                    ParsedDataMap.Add(NewPath, { {}, Value->AsBool(), {}, {} });
                    break;
                case EJson::Number:
                    {
	                    if (const double Num = Value->AsNumber(); FMath::IsFinite(Num))
                        {
                            if (FMath::RoundToInt32(Num) == Num)
                            {
                                ParsedDataMap.Add(NewPath, { {}, {}, FMath::RoundToInt32(Num), {} });
                            }
                            else
                                ParsedDataMap.Add(NewPath, { {}, {}, {}, static_cast<float>(Num) });
                        }
                        break;
                    }
                case EJson::Array:
                case EJson::None:
                default:
                    ParsedDataMap.Add(NewPath, { Value->AsString(), {}, {}, {} });
                    break;
                }
            }
        }
    }
}

bool UAsync_ReadJson::GetNodeData(const FString& NodePath, const FParsedData& ParsedData, FJsonNode& NodeData)
{
    // 如果节点路径为空，则返回 false
    if (NodePath.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("<< GetNodeData >> NodePath is empty"));
        return false;
    }
    
	if (ParsedData.ParsedDataMap.Contains(NodePath))
	{
	    NodeData =  {NodePath, ParsedData.ParsedDataMap[NodePath] };
		return true;
	}
    // 如果找不到节点，则返回 false
    UE_LOG(LogReadJson, Error, TEXT("<< GetNodeData >> Node [ %s ] not found"), *NodePath);
	return false;
}

bool UAsync_ReadJson::ParseJsonArray(const FString& StringArray, FJsonArray& ArrayValue)
{
    if (StringArray.IsEmpty())
    {
        UE_LOG(LogReadJson, Error, TEXT("<< ParseJsonArray >> StringArray is empty"));
    }
    
    // 在开始解析前，清空所有目标数组，避免之前的数据影响当前解析
    ArrayValue = {};

    // 创建一个用于存储解析后的 JSON 数组的指针
    TSharedPtr<FJsonValue> JsonValue;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StringArray);

    // 尝试解析输入字符串为 JSON 值
    if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
    {
        UE_LOG(LogReadJson, Warning, TEXT("<< ParseJsonArray >> Failed to parse JsonArray Or JsonValue is Invalid"));
        return false;
    }

    // 确保解析的 JSON 值是一个数组
    if (JsonValue->Type != EJson::Array)
    {
        UE_LOG(LogReadJson, Warning, TEXT("<< ParseJsonArray >> JsonValue is not an array"));
        return false;
    }

    // 确保数组元素数量大于 0
    if (JsonValue->AsArray().Num()== 0)
    {
        UE_LOG(LogReadJson, Warning, TEXT("<< ParseJsonArray >> JsonValue to array is empty"));
        return false;
    }

    // 获取数组元素
    TArray<TSharedPtr<FJsonValue>> JsonArray = JsonValue->AsArray();

    for (int32 Index = 0; Index < JsonArray.Num(); ++Index)
    {
        const TSharedPtr<FJsonValue>& Element = JsonArray[Index];

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
    UE_LOG(LogReadJson, Warning, TEXT("<< EndTask >> Async_ReadJson EndTask"));
}

void UAsync_ReadJson::DestroyTask()
{
	SetReadyToDestroy();
	MarkAsGarbage();
    UE_LOG(LogReadJson, Warning, TEXT("<< DestroyTask >> Async_ReadJson DestroyTask"));
}
