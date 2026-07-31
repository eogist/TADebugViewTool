#pragma once

#include "CoreMinimal.h"

namespace TADebugViewTool
{
enum class EPresetDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error
};

struct FPresetDiagnosticIssue
{
	EPresetDiagnosticSeverity Severity = EPresetDiagnosticSeverity::Info;
	FString PresetLabel;
	FString Message;
};

TArray<FPresetDiagnosticIssue> RunPresetDiagnostics();
FText GetDiagnosticSeverityLabel(EPresetDiagnosticSeverity Severity);
FLinearColor GetDiagnosticSeverityColor(EPresetDiagnosticSeverity Severity);

// A single named check shown as a persistent row on the Diagnostics page. Unlike
// FPresetDiagnosticIssue, a check is always reported so a healthy project still
// renders the full checklist instead of an empty page.
struct FPresetDiagnosticCheck
{
	FText Label;
	FText Detail;
	EPresetDiagnosticSeverity Severity = EPresetDiagnosticSeverity::Info;
	bool bPassed = true;
};

TArray<FPresetDiagnosticCheck> RunPresetDiagnosticChecks();
FText GetDiagnosticCheckStatusLabel(const FPresetDiagnosticCheck& Check);
FLinearColor GetDiagnosticCheckStatusColor(const FPresetDiagnosticCheck& Check);
}
