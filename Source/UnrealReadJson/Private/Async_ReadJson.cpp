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
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OnReadJsonFailed.Broadcast({});
		DestroyTask();
	}

	if (IsLargeJson)
	{
		ParsedDataMap.Empty(CountJsonNodes(JsonObject));
		ParseJsonIterative(JsonObject);
	}
	else
	{
		ParseJson(JsonObject, TEXT(""));
	}
	
	OnReadJsonCompleted.Broadcast({ ParsedDataMap });
	DestroyTask();
}

void UAsync_ReadJson::ParseJson(const TSharedPtr<FJsonObject>& JsonObject, const FString& CurrentPath)
{
    for (const auto& Elem : JsonObject->Values)
    {
        FString NewPath = CurrentPath.IsEmpty() ? Elem.Key : CurrentPath + TEXT(".") + Elem.Key;
        const TSharedPtr<FJsonValue>& Value = Elem.Value;

        if (Value->Type == EJson::Object)
        {
            // 将整个子对象序列化为 JSON 字符串并存储在 StringValue 中
            FString ObjectString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ObjectString);
            if (FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer))
            {
                FJsonDataStruct Data;
                Data.StringValue = ObjectString;
                ParsedDataMap.Add(NewPath, Data);
            }

            // 递归解析子对象
            ParseJson(Value->AsObject(), NewPath);
        }
        else if (Value->Type == EJson::Array)
        {
            // 将整个数组序列化为 JSON 字符串并存储在 StringValue 中, 不对数组内部元素进行迭代解析
            FString ArrayString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArrayString);
            if (FJsonSerializer::Serialize(Value->AsArray(), Writer))
            {
                FJsonDataStruct Data;
                Data.StringValue = ArrayString;
                ParsedDataMap.Add(NewPath, Data);
            }
        }
        else
        {
            // 处理基本类型（字符串、数值、布尔值等）
            FJsonDataStruct Data;
            switch (Value->Type)
            {
                case EJson::String:
                    Data.StringValue = Value->AsString();
                    break;
                case EJson::Boolean:
                    Data.BoolValue = Value->AsBool();
                    break;
                case EJson::Number:
                {
                    if (const double Num = Value->AsNumber(); FMath::IsFinite(Num))
                    {
                        if (FMath::RoundToInt32(Num) == Num)
                            Data.IntValue = FMath::RoundToInt32(Num);
                        else
                            Data.FloatValue = static_cast<float>(Num);
                    }
                    break;
                }
                default:
                    // 对于 Array、None 或其他未处理的类型，尝试将其转换为字符串
                    Data.StringValue = Value->AsString();
                    break;
            }
            ParsedDataMap.Add(NewPath, Data);
        }
    }
}

void UAsync_ReadJson::ParseJsonIterative(const TSharedPtr<FJsonObject>& RootJson)
{
    struct FJsonNode
    {
        TSharedPtr<FJsonObject> JsonObject;
        FString CurrentPath;
    };

    TArray<FJsonNode> Stack;
    Stack.Push({ RootJson, TEXT("") });

    while (Stack.Num() > 0)
    {
        const FJsonNode CurrentNode = Stack.Pop();
        const TSharedPtr<FJsonObject> JsonObject = CurrentNode.JsonObject;
        FString CurrentPath = CurrentNode.CurrentPath;

        for (const auto& Elem : JsonObject->Values)
        {
            FString NewPath = CurrentPath.IsEmpty() ? Elem.Key : CurrentPath + TEXT(".") + Elem.Key;
            const TSharedPtr<FJsonValue>& Value = Elem.Value;

            if (Value->Type == EJson::Object)
            {
                // 将整个子对象序列化为 JSON 字符串
                FString ObjectString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ObjectString);
                if (FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer))
                {
                    // 创建 DataStruct 并添加到 ParsedDataMap
                    FJsonDataStruct ParentData;
                    ParentData.StringValue = ObjectString;
                    ParsedDataMap.Add(NewPath, ParentData);
                }

                // 将子对象压入栈中以继续解析
                Stack.Push({ Value->AsObject(), NewPath });
            }
            else if (Value->Type == EJson::Array)
            {
                // 将数组序列化为 JSON 字符串并存储在 StringValue 中, 不对数组内部元素进行迭代解析
                FString ArrayString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArrayString);
                if (FJsonSerializer::Serialize(Value->AsArray(), Writer))
                {
                    FJsonDataStruct ArrayData;
                    ArrayData.StringValue = ArrayString;
                    ParsedDataMap.Add(NewPath, ArrayData);
                }
            }
            else
            {
                FJsonDataStruct Data;
                switch (Value->Type)
                {
                case EJson::String:
                    Data.StringValue = Value->AsString();
                    break;
                case EJson::Boolean:
                    Data.BoolValue = Value->AsBool();
                    break;
                case EJson::Number:
                    {
	                    if (const double Num = Value->AsNumber(); FMath::IsFinite(Num))
                        {
                            if (FMath::RoundToInt32(Num) == Num)
                                Data.IntValue = FMath::RoundToInt32(Num);
                            else
                                Data.FloatValue = static_cast<float>(Num);
                        }
                        break;
                    }
                case EJson::Array:
                case EJson::None:
                default:
                    Data.StringValue = Value->AsString();
                    break;
                }
                ParsedDataMap.Add(NewPath, Data);
            }
        }
    }
}

bool UAsync_ReadJson::GetNodeData(const FString& NodePath, const FParsedData& ParsedData, FJsonNode& NodeData)
{
	if (ParsedData.ParsedDataMap.Contains(NodePath))
	{
		NodeData.Key = NodePath;
		NodeData.Value = ParsedData.ParsedDataMap[NodePath];
		return true;
	}
	return false;
}

bool UAsync_ReadJson::ParseJsonArray(const FString& StringArray, FJsonArray& ArrayValue)
{
    // 在开始解析前，清空所有目标数组，避免之前的数据影响当前解析
    ArrayValue = {};

    // 创建一个用于存储解析后的 JSON 数组的指针
    TSharedPtr<FJsonValue> JsonValue;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(StringArray);

    // 尝试解析输入字符串为 JSON 值
    if (!FJsonSerializer::Deserialize(Reader, JsonValue) || !JsonValue.IsValid())
    {
        return false;
    }

    // 确保解析的 JSON 值是一个数组
    if (JsonValue->Type != EJson::Array)
    {
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
}

void UAsync_ReadJson::DestroyTask()
{
	SetReadyToDestroy();
	MarkAsGarbage();
}
