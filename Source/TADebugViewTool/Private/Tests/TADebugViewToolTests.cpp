#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TADebugViewPresetRegistry.h"
#include "TADebugViewUpdateService.h"
#include "TADebugViewWorkflowRegistry.h"

namespace
{
FString WriteTemporaryWorkflowFile(const FString& Json)
{
	const FString FilePath = FPaths::CreateTempFilename(
		*FPaths::ProjectIntermediateDir(),
		TEXT("TADebugViewToolTest_"),
		TEXT(".json"));
	FFileHelper::SaveStringToFile(Json, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return FilePath;
}

bool HasCommand(const TADebugViewTool::FWorkflowPreset& Workflow, const FString& ExpectedCommand)
{
	return Workflow.ActivateActions.ContainsByPredicate([&ExpectedCommand](const TADebugViewTool::FDebugViewAction& Action)
	{
		return Action.ActionType == TADebugViewTool::EPresetActionType::Command
			&& Action.Commands.Equals(ExpectedCommand, ESearchCase::IgnoreCase);
	});
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTADebugViewAtomicWorkflowParsingTest,
	"TADebugViewTool.Workflow.AtomicParsing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTADebugViewAtomicWorkflowParsingTest::RunTest(const FString&)
{
	const FString Json = TEXT(R"JSON({
		"format":"TADebugViewToolWorkflows",
		"version":2,
		"workflows":[
			{"id":"ValidFirst","label":"Valid","tooltip":"","activateActions":[{"type":"ViewMode","value":"VMI_Lit"}],"deactivateActions":[]},
			{"id":"BrokenSecond","tooltip":"missing label","activateActions":[{"type":"ViewMode","value":"VMI_Lit"}]}
		]
	})JSON");
	const FString FilePath = WriteTemporaryWorkflowFile(Json);
	TArray<TADebugViewTool::FWorkflowPreset> Workflows;
	TArray<FString> Diagnostics;
	const bool bLoaded = TADebugViewTool::LoadWorkflowFileForTesting(FilePath, false, Workflows, Diagnostics);
	IFileManager::Get().Delete(*FilePath);

	TestFalse(TEXT("A document with any invalid entry is rejected"), bLoaded);
	TestEqual(TEXT("No valid prefix leaks from a rejected document"), Workflows.Num(), 0);
	TestTrue(TEXT("The rejected entry produces a diagnostic"), !Diagnostics.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTADebugViewWorkflowSchemaVersionTest,
	"TADebugViewTool.Workflow.ExactSchemaVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTADebugViewWorkflowSchemaVersionTest::RunTest(const FString&)
{
	const FString InvalidPath = WriteTemporaryWorkflowFile(TEXT(R"JSON({"format":"TADebugViewToolWorkflows","version":2.9,"workflows":[]})JSON"));
	TArray<TADebugViewTool::FWorkflowPreset> Workflows;
	TArray<FString> Diagnostics;
	TestFalse(
		TEXT("Fractional schema versions are rejected"),
		TADebugViewTool::LoadWorkflowFileForTesting(InvalidPath, false, Workflows, Diagnostics));
	IFileManager::Get().Delete(*InvalidPath);

	const FString ValidPath = WriteTemporaryWorkflowFile(TEXT(R"JSON({"format":"TADebugViewToolWorkflows","version":2,"workflows":[]})JSON"));
	Workflows.Reset();
	Diagnostics.Reset();
	TestTrue(
		TEXT("Exact schema version 2 is accepted for an empty override file"),
		TADebugViewTool::LoadWorkflowFileForTesting(ValidPath, false, Workflows, Diagnostics));
	Workflows.Reset();
	Diagnostics.Reset();
	TestFalse(
		TEXT("The default workflow document cannot be empty"),
		TADebugViewTool::LoadWorkflowFileForTesting(ValidPath, true, Workflows, Diagnostics));
	IFileManager::Get().Delete(*ValidPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTADebugViewCaseInsensitiveWorkflowIdTest,
	"TADebugViewTool.Workflow.CaseInsensitiveIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTADebugViewCaseInsensitiveWorkflowIdTest::RunTest(const FString&)
{
	const FString Json = TEXT(R"JSON({
		"format":"TADebugViewToolWorkflows",
		"version":2,
		"workflows":[
			{"id":"LightingAudit","label":"First","tooltip":"","activateActions":[{"type":"ViewMode","value":"VMI_Lit"}]},
			{"id":"lightingaudit","label":"Second","tooltip":"","activateActions":[{"type":"ViewMode","value":"VMI_Lit"}]}
		]
	})JSON");
	const FString FilePath = WriteTemporaryWorkflowFile(Json);
	TArray<TADebugViewTool::FWorkflowPreset> Workflows;
	TArray<FString> Diagnostics;
	TestFalse(
		TEXT("Workflow Ids use case-insensitive FName identity"),
		TADebugViewTool::LoadWorkflowFileForTesting(FilePath, false, Workflows, Diagnostics));
	TestEqual(TEXT("Duplicate Id rejection remains atomic"), Workflows.Num(), 0);
	IFileManager::Get().Delete(*FilePath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTADebugViewLegacyCommandMigrationTest,
	"TADebugViewTool.Workflow.LegacyCommandPreservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTADebugViewLegacyCommandMigrationTest::RunTest(const FString&)
{
	FTADebugViewCustomWorkflowPreset Preset;
	Preset.ActivateActions.Add(FTADebugViewCustomAction(ETADebugViewCustomActionType::ViewMode, TEXT("VMI_Lit")));
	Preset.ActivateCommands = TEXT("stat unit");
	Preset.DeactivateActions.Add(FTADebugViewCustomAction(ETADebugViewCustomActionType::Command, TEXT("stat none")));
	Preset.DeactivateCommands = TEXT("showflag.bounds 0");
	Preset.MigrateLegacyCommands();

	TestEqual(TEXT("Legacy activate command is appended after structured actions"), Preset.ActivateActions.Num(), 2);
	TestEqual(TEXT("Legacy restore command is appended after structured actions"), Preset.DeactivateActions.Num(), 2);
	TestEqual(TEXT("Activate command value is preserved"), Preset.ActivateActions[1].Value, FString(TEXT("stat unit")));
	TestEqual(TEXT("Restore command value is preserved"), Preset.DeactivateActions[1].Value, FString(TEXT("showflag.bounds 0")));
	TestTrue(TEXT("Legacy activate field is cleared on the migration copy"), Preset.ActivateCommands.IsEmpty());
	TestTrue(TEXT("Legacy restore field is cleared on the migration copy"), Preset.DeactivateCommands.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTADebugViewSemanticVersionTest,
	"TADebugViewTool.Update.SemanticVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTADebugViewSemanticVersionTest::RunTest(const FString&)
{
	using TADebugViewTool::FTADebugViewUpdateService;
	auto ExpectComparison = [this](const TCHAR* Left, const TCHAR* Right, int32 Expected)
	{
		const TOptional<int32> Result = FTADebugViewUpdateService::CompareSemanticVersions(Left, Right);
		TestTrue(FString::Printf(TEXT("%s and %s parse"), Left, Right), Result.IsSet());
		if (Result.IsSet())
		{
			TestEqual(FString::Printf(TEXT("%s compares to %s"), Left, Right), Result.GetValue(), Expected);
		}
	};

	ExpectComparison(TEXT("1.3.0-beta.2"), TEXT("1.3.0-beta.11"), -1);
	ExpectComparison(TEXT("1.3.0-beta.11"), TEXT("1.3.0-rc.1"), -1);
	ExpectComparison(TEXT("1.3.0-rc.1"), TEXT("1.3.0"), -1);
	ExpectComparison(TEXT("1.3.0+build.1"), TEXT("1.3.0+build.2"), 0);
	ExpectComparison(TEXT("v1.4.0"), TEXT("1.3.9"), 1);
	TestFalse(TEXT("Core numeric identifiers reject leading zeroes"), FTADebugViewUpdateService::CompareSemanticVersions(TEXT("1.03.0"), TEXT("1.3.0")).IsSet());
	TestFalse(TEXT("Prerelease numeric identifiers reject leading zeroes"), FTADebugViewUpdateService::CompareSemanticVersions(TEXT("1.3.0-beta.01"), TEXT("1.3.0-beta.1")).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTADebugViewReleaseUrlTest,
	"TADebugViewTool.Update.TrustedReleaseUrl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTADebugViewReleaseUrlTest::RunTest(const FString&)
{
	FString Url;
	TestTrue(TEXT("A valid release tag builds a trusted URL"), TADebugViewTool::FTADebugViewUpdateService::TryBuildTrustedReleaseUrl(TEXT("v1.3.0"), Url));
	TestEqual(TEXT("Release URL stays on the configured repository"), Url, FString(TEXT("https://github.com/eogist/TADebugViewTool/releases/tag/v1.3.0")));
	TestFalse(TEXT("An arbitrary URL cannot be used as a release tag"), TADebugViewTool::FTADebugViewUpdateService::TryBuildTrustedReleaseUrl(TEXT("https://example.com/file"), Url));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTADebugViewResetDefinitionTest,
	"TADebugViewTool.Workflow.ResetCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTADebugViewResetDefinitionTest::RunTest(const FString&)
{
	const TADebugViewTool::FWorkflowPreset* ResetWorkflow = TADebugViewTool::GetWorkflowPresets().FindByPredicate([](const TADebugViewTool::FWorkflowPreset& Workflow)
	{
		return Workflow.Id == FName(TEXT("TADebugWorkflow_ResetDebug"));
	});
	TestNotNull(TEXT("Compiled defaults include Reset Debug"), ResetWorkflow);
	if (ResetWorkflow)
	{
		TestTrue(TEXT("Reset disables bounds"), HasCommand(*ResetWorkflow, TEXT("showflag.bounds 0")));
		TestTrue(TEXT("Reset disables navigation"), HasCommand(*ResetWorkflow, TEXT("showflag.navigation 0")));
		TestTrue(TEXT("Reset disables cached-page filtering"), HasCommand(*ResetWorkflow, TEXT("r.Shadow.Virtual.Visualize.ShowCachedPagesOnly 0")));
	}
	return true;
}

#endif
