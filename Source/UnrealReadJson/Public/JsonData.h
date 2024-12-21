#pragma once

#include "JsonData.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogReadJson, Log, All);
inline DEFINE_LOG_CATEGORY(LogReadJson);

// Define DataStruct
USTRUCT(BlueprintType)
struct FJsonDataStruct
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	FString StringValue {};

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	bool BoolValue { false };

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	int32 IntValue { 0 };

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	float FloatValue { 0.f };
};

// Define Node Struct
USTRUCT(BlueprintType)
struct FJsonNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	FString Key {};

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	FJsonDataStruct Value {};
};

USTRUCT(BlueprintType)
struct FParsedData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	TMap<FString, FJsonDataStruct> ParsedDataMap {};
};

// ArrayString To ArrayValue
USTRUCT(BlueprintType)
struct FJsonArray
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	TArray<FString> StringArray {};

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	TArray<bool> BoolArray {};

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	TArray<int32> IntArray {};

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "ReadJson")
	TArray<float> FloatArray {};
};