#include "STADebugViewPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/WidgetPath.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "TADebugViewCustomPresetSettings.h"
#include "TADebugViewExecutor.h"
#include "TADebugViewPresetDiagnostics.h"
#include "TADebugViewPresetRegistry.h"
#include "TADebugViewQuickActionRuntime.h"
#include "TADebugViewToolConstants.h"
#include "TADebugViewWorkflowRegistry.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"

#define LOCTEXT_NAMESPACE "STADebugViewPanel"

namespace
{
DECLARE_DELEGATE_RetVal_OneParam(bool, FOnQuickAccessPageDelta, int32);

class SQuickAccessPager final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SQuickAccessPager)
	{
	}
		SLATE_EVENT(FOnQuickAccessPageDelta, OnPageDelta)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnPageDelta = InArgs._OnPageDelta;
		ChildSlot
		[
			InArgs._Content.Widget
		];
	}

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		const float WheelDelta = MouseEvent.GetWheelDelta();
		if (!OnPageDelta.IsBound() || FMath::IsNearlyZero(WheelDelta))
		{
			return FReply::Unhandled();
		}

		const int32 PageDelta = WheelDelta < 0.0f ? 1 : -1;
		return OnPageDelta.Execute(PageDelta)
			? FReply::Handled()
			: FReply::Unhandled();
	}

private:
	FOnQuickAccessPageDelta OnPageDelta;
};

class SWorkflowDoubleClickButton final : public SButton
{
public:
	void SetOnDoubleClicked(FPointerEventHandler InOnDoubleClicked)
	{
		OnDoubleClicked = MoveTemp(InOnDoubleClicked);
	}

	virtual FReply OnMouseButtonDoubleClick(
		const FGeometry& MyGeometry,
		const FPointerEvent& MouseEvent) override
	{
		if (OnDoubleClicked.IsBound())
		{
			const FReply Reply = OnDoubleClicked.Execute(MyGeometry, MouseEvent);
			if (Reply.IsEventHandled())
			{
				return Reply;
			}
		}

		return SButton::OnMouseButtonDoubleClick(MyGeometry, MouseEvent);
	}

private:
	FPointerEventHandler OnDoubleClicked;
};

struct FActionOption
{
	FActionOption(ETADebugViewCustomActionType InActionType, const TCHAR* InLabel)
		: ActionType(InActionType)
		, Label(InLabel)
	{
	}

	ETADebugViewCustomActionType ActionType;
	FString Label;
};

struct FActionValueOption
{
	FActionValueOption(const TCHAR* InLabel, const TCHAR* InValue)
		: Label(InLabel)
		, Value(InValue)
	{
	}

	FString Label;
	FString Value;
};

const TArray<TSharedPtr<FActionOption>>& GetActionTypeOptions()
{
	static const TArray<TSharedPtr<FActionOption>> Options =
	{
		MakeShared<FActionOption>(ETADebugViewCustomActionType::ViewMode, TEXT("ViewMode")),
		MakeShared<FActionOption>(ETADebugViewCustomActionType::NaniteVisualization, TEXT("Nanite")),
		MakeShared<FActionOption>(ETADebugViewCustomActionType::LumenVisualization, TEXT("Lumen")),
		MakeShared<FActionOption>(ETADebugViewCustomActionType::VirtualShadowMapVisualization, TEXT("VSM")),
		MakeShared<FActionOption>(ETADebugViewCustomActionType::Command, TEXT("Command (Advanced)"))
	};

	return Options;
}

TSharedPtr<FActionOption> FindActionTypeOption(ETADebugViewCustomActionType ActionType)
{
	for (const TSharedPtr<FActionOption>& Option : GetActionTypeOptions())
	{
		if (Option->ActionType == ActionType)
		{
			return Option;
		}
	}

	return GetActionTypeOptions()[0];
}

const TArray<TSharedPtr<FActionValueOption>>& GetValueOptions(ETADebugViewCustomActionType ActionType)
{
	static const TArray<TSharedPtr<FActionValueOption>> ViewModeOptions =
	{
		MakeShared<FActionValueOption>(TEXT("Lit"), TEXT("VMI_Lit")),
		MakeShared<FActionValueOption>(TEXT("Unlit"), TEXT("VMI_Unlit")),
		MakeShared<FActionValueOption>(TEXT("Wireframe"), TEXT("VMI_Wireframe")),
		MakeShared<FActionValueOption>(TEXT("Detail Lighting"), TEXT("VMI_Lit_DetailLighting")),
		MakeShared<FActionValueOption>(TEXT("Lighting Only"), TEXT("VMI_LightingOnly")),
		MakeShared<FActionValueOption>(TEXT("Shader Complexity"), TEXT("VMI_ShaderComplexity")),
		MakeShared<FActionValueOption>(TEXT("Quad Overdraw"), TEXT("VMI_QuadOverdraw")),
		MakeShared<FActionValueOption>(TEXT("Shader Complexity + Quad"), TEXT("VMI_ShaderComplexityWithQuadOverdraw")),
		MakeShared<FActionValueOption>(TEXT("Texture Density"), TEXT("VMI_MaterialTextureScaleAccuracy")),
		MakeShared<FActionValueOption>(TEXT("Light Complexity"), TEXT("VMI_LightComplexity")),
		MakeShared<FActionValueOption>(TEXT("Lightmap Density"), TEXT("VMI_LightmapDensity")),
		MakeShared<FActionValueOption>(TEXT("Stationary Light Overlap"), TEXT("VMI_StationaryLightOverlap")),
		MakeShared<FActionValueOption>(TEXT("Reflection Override"), TEXT("VMI_ReflectionOverride")),
		MakeShared<FActionValueOption>(TEXT("Collision Pawn"), TEXT("VMI_CollisionPawn")),
		MakeShared<FActionValueOption>(TEXT("Collision Visibility"), TEXT("VMI_CollisionVisibility")),
		MakeShared<FActionValueOption>(TEXT("LOD Coloration"), TEXT("VMI_LODColoration")),
		MakeShared<FActionValueOption>(TEXT("Virtual Texture"), TEXT("VMI_VisualizeVirtualTexture"))
	};

	static const TArray<TSharedPtr<FActionValueOption>> NaniteOptions =
	{
		MakeShared<FActionValueOption>(TEXT("Overview"), TEXT("Overview")),
		MakeShared<FActionValueOption>(TEXT("Mask"), TEXT("Mask")),
		MakeShared<FActionValueOption>(TEXT("Triangles"), TEXT("Triangles")),
		MakeShared<FActionValueOption>(TEXT("Clusters"), TEXT("Clusters")),
		MakeShared<FActionValueOption>(TEXT("Primitives"), TEXT("Primitives")),
		MakeShared<FActionValueOption>(TEXT("Instances"), TEXT("Instances")),
		MakeShared<FActionValueOption>(TEXT("Overdraw"), TEXT("Overdraw")),
		MakeShared<FActionValueOption>(TEXT("Evaluate WPO"), TEXT("EvaluateWPO")),
		MakeShared<FActionValueOption>(TEXT("Raster Bins"), TEXT("RasterBins")),
		MakeShared<FActionValueOption>(TEXT("Shading Bins"), TEXT("ShadingBins"))
	};

	static const TArray<TSharedPtr<FActionValueOption>> LumenOptions =
	{
		MakeShared<FActionValueOption>(TEXT("Overview"), TEXT("Overview")),
		MakeShared<FActionValueOption>(TEXT("Performance Overview"), TEXT("PerformanceOverview")),
		MakeShared<FActionValueOption>(TEXT("Lumen Scene"), TEXT("LumenScene")),
		MakeShared<FActionValueOption>(TEXT("Geometry Normals"), TEXT("GeometryNormals")),
		MakeShared<FActionValueOption>(TEXT("Reflection View"), TEXT("ReflectionView")),
		MakeShared<FActionValueOption>(TEXT("Surface Cache"), TEXT("SurfaceCache")),
		MakeShared<FActionValueOption>(TEXT("Dedicated Reflection Rays"), TEXT("DedicatedReflectionRays"))
	};

	static const TArray<TSharedPtr<FActionValueOption>> VSMOptions =
	{
		MakeShared<FActionValueOption>(TEXT("Shadow Mask"), TEXT("mask")),
		MakeShared<FActionValueOption>(TEXT("Clipmap / Mip Level"), TEXT("mip")),
		MakeShared<FActionValueOption>(TEXT("Virtual Page"), TEXT("vpage")),
		MakeShared<FActionValueOption>(TEXT("Cached Page"), TEXT("cache")),
		MakeShared<FActionValueOption>(TEXT("Nanite Overdraw"), TEXT("naniteoverdraw")),
		MakeShared<FActionValueOption>(TEXT("Shadow Casters"), TEXT("casters")),
		MakeShared<FActionValueOption>(TEXT("SMRT Ray Count"), TEXT("raycount")),
		MakeShared<FActionValueOption>(TEXT("Dirty Page"), TEXT("dirty")),
		MakeShared<FActionValueOption>(TEXT("GPU Invalidated Page"), TEXT("invalid")),
		MakeShared<FActionValueOption>(TEXT("Merged Page"), TEXT("merged")),
		MakeShared<FActionValueOption>(TEXT("General Debug"), TEXT("debug")),
		MakeShared<FActionValueOption>(TEXT("Clipmap Virtual Address Space"), TEXT("clipmapvirtual"))
	};

	if (ActionType == ETADebugViewCustomActionType::NaniteVisualization)
	{
		return NaniteOptions;
	}

	if (ActionType == ETADebugViewCustomActionType::LumenVisualization)
	{
		return LumenOptions;
	}

	if (ActionType == ETADebugViewCustomActionType::VirtualShadowMapVisualization)
	{
		return VSMOptions;
	}

	return ViewModeOptions;
}

TSharedPtr<FActionValueOption> FindActionValueOption(ETADebugViewCustomActionType ActionType, const FString& Value)
{
	const TArray<TSharedPtr<FActionValueOption>>& Options = GetValueOptions(ActionType);
	for (const TSharedPtr<FActionValueOption>& Option : Options)
	{
		if (Option->Value == Value)
		{
			return Option;
		}
	}

	return Options[0];
}

FString GetDefaultValueForActionType(ETADebugViewCustomActionType ActionType)
{
	if (ActionType == ETADebugViewCustomActionType::Command)
	{
		return TEXT("stat unit");
	}

	return GetValueOptions(ActionType)[0]->Value;
}


bool IsValidViewportTargetValue(uint8 ViewportTargetValue)
{
	return ViewportTargetValue <= static_cast<uint8>(TADebugViewTool::EDebugViewportTarget::AllLevel);
}

// Presets are split across two pages by action type rather than by group. Raw
// console commands are utilities that belong on Advanced, while view mode and
// visualization presets are the per-system Debug Views lists. Splitting by group
// instead would list the VSM and Geometry groups on both pages.
bool IsAdvancedPagePreset(const TADebugViewTool::FDebugViewPreset& Preset)
{
	return Preset.ActionType == TADebugViewTool::EPresetActionType::Command;
}

// True when a group still has at least one preset left for the Debug Views page.
bool GroupHasDebugViewPresets(const TADebugViewTool::FDebugViewGroup& Group)
{
	return Group.Presets.ContainsByPredicate([](const TADebugViewTool::FDebugViewPreset& Preset)
	{
		return !IsAdvancedPagePreset(Preset);
	});
}

FText GetWorkflowActionDisplayText(const TADebugViewTool::FDebugViewAction& Action)
{
	switch (Action.ActionType)
	{
	case TADebugViewTool::EPresetActionType::ViewMode:
	{
		FTADebugViewCustomAction SavedAction;
		if (TADebugViewTool::ConvertRuntimeActionToSavedAction(Action, SavedAction))
		{
			return FText::FromString(SavedAction.Value);
		}
		return LOCTEXT("UnknownViewModeAction", "Unknown ViewMode");
	}
	case TADebugViewTool::EPresetActionType::NaniteVisualization:
		return FText::FromString(FString::Printf(TEXT("Nanite: %s"), *Action.VisualizationMode.ToString()));
	case TADebugViewTool::EPresetActionType::LumenVisualization:
		return FText::FromString(FString::Printf(TEXT("Lumen: %s"), *Action.VisualizationMode.ToString()));
	case TADebugViewTool::EPresetActionType::VirtualShadowMapVisualization:
		return FText::FromString(FString::Printf(TEXT("VSM: %s"), *Action.VisualizationMode.ToString()));
	default:
		return FText::FromString(Action.Commands);
	}
}

const FSlateBrush* GetAppFrameBrush()
{
	static const FSlateRoundedBoxBrush Brush(
		FLinearColor::FromSRGBColor(FColor(16, 18, 19)),
		12.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	return &Brush;
}

const FSlateBrush* GetSectionPanelBrush()
{
	static const FSlateRoundedBoxBrush Brush(
		FLinearColor::FromSRGBColor(FColor(29, 30, 33)),
		12.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	return &Brush;
}

const FSlateBrush* GetContentPanelBrush()
{
	static const FSlateRoundedBoxBrush Brush(
		FLinearColor::FromSRGBColor(FColor(26, 26, 26)),
		12.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	return &Brush;
}

const FSlateBrush* GetFlatSectionBrush()
{
	static const FSlateRoundedBoxBrush Brush(
		FLinearColor::FromSRGBColor(FColor(16, 18, 19)),
		0.0f);
	return &Brush;
}

const FSlateBrush* GetInsetPanelBrush()
{
	static const FSlateRoundedBoxBrush Brush(
		FLinearColor::FromSRGBColor(FColor(34, 34, 34)),
		8.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	return &Brush;
}

const FSlateBrush* GetQuickAccessPageDotBrush(bool bActive)
{
	static const FSlateRoundedBoxBrush ActiveBrush(
		FLinearColor::FromSRGBColor(FColor(0, 145, 190)),
		4.0f);
	static const FSlateRoundedBoxBrush InactiveBrush(
		FLinearColor::FromSRGBColor(FColor(78, 82, 87)),
		4.0f);
	return bActive ? &ActiveBrush : &InactiveBrush;
}

const FSlateBrush* GetCommandSearchBrush(bool bFocused)
{
	static const FSlateRoundedBoxBrush NormalBrush(
		FLinearColor::FromSRGBColor(FColor(34, 34, 34)),
		9.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	static const FSlateRoundedBoxBrush FocusedBrush(
		FLinearColor::FromSRGBColor(FColor(34, 34, 34)),
		9.0f,
		FLinearColor::FromSRGBColor(FColor(0, 112, 224)),
		1.0f);
	return bFocused ? &FocusedBrush : &NormalBrush;
}

const FEditableTextBoxStyle* GetCommandSearchTextStyle()
{
	static const FEditableTextBoxStyle Style = []()
	{
		FEditableTextBoxStyle Result = FAppStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
		const FSlateNoResource NoResource;
		Result
			.SetBackgroundImageNormal(NoResource)
			.SetBackgroundImageHovered(NoResource)
			.SetBackgroundImageFocused(NoResource)
			.SetBackgroundImageReadOnly(NoResource)
			.SetBackgroundColor(FLinearColor::Transparent)
			.SetPadding(FMargin(38.0f, 0.0f, 92.0f, 0.0f))
			.SetFont(FAppStyle::GetFontStyle(TEXT("Editor.SearchBoxFont")));
		return Result;
	}();
	return &Style;
}

const FSlateBrush* GetBrandMarkBrush()
{
	static const FSlateRoundedBoxBrush Brush(
		FLinearColor::FromSRGBColor(FColor(38, 41, 45)),
		8.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	return &Brush;
}

const FSlateBrush* GetTargetButtonBrush(bool bSelected)
{
	static const FSlateRoundedBoxBrush NormalBrush(
		FLinearColor::FromSRGBColor(FColor(34, 34, 34)),
		6.0f);
	static const FSlateRoundedBoxBrush SelectedBrush(
		FLinearColor::FromSRGBColor(FColor(0, 112, 224)),
		6.0f);
	return bSelected ? &SelectedBrush : &NormalBrush;
}

const FSlateBrush* GetListRowBrush(bool bActive)
{
	static const FSlateRoundedBoxBrush NormalBrush(
		FLinearColor::FromSRGBColor(FColor(34, 34, 34)),
		9.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	static const FSlateRoundedBoxBrush ActiveBrush(
		FLinearColor::FromSRGBColor(FColor(35, 52, 74)),
		9.0f,
		FLinearColor::FromSRGBColor(FColor(52, 72, 96)),
		1.0f);
	return bActive ? &ActiveBrush : &NormalBrush;
}

const FSlateBrush* GetDebugGroupButtonBrush(bool bSelected)
{
	static const FSlateRoundedBoxBrush NormalBrush(
		FLinearColor::FromSRGBColor(FColor(34, 34, 34)),
		7.0f);
	static const FSlateRoundedBoxBrush SelectedBrush(
		FLinearColor::FromSRGBColor(FColor(35, 52, 74)),
		7.0f);
	return bSelected ? &SelectedBrush : &NormalBrush;
}

const FSlateBrush* GetSeparatorBrush()
{
	static const FSlateRoundedBoxBrush Brush(
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		0.0f);
	return &Brush;
}

TSharedRef<SWidget> MakeHorizontalSeparator()
{
	return SNew(SBox)
		.HeightOverride(1.0f)
		[
			SNew(SImage)
			.Image(GetSeparatorBrush())
		];
}

const FSlateBrush* GetQuickActionBrush(bool bActive)
{
	static const FSlateRoundedBoxBrush NormalBrush(
		FLinearColor::FromSRGBColor(FColor(34, 34, 34)),
		8.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	static const FSlateRoundedBoxBrush ActiveBrush(
		FLinearColor::FromSRGBColor(FColor(35, 52, 74)),
		8.0f,
		FLinearColor::FromSRGBColor(FColor(24, 84, 145)),
		1.0f);
	return bActive ? &ActiveBrush : &NormalBrush;
}

const FSlateBrush* GetNavigationBrush(bool bSelected)
{
	static const FSlateRoundedBoxBrush NormalBrush(
		FLinearColor::FromSRGBColor(FColor(29, 30, 33)),
		8.0f);
	static const FSlateRoundedBoxBrush SelectedBrush(
		FLinearColor::FromSRGBColor(FColor(35, 52, 74)),
		8.0f,
		FLinearColor::FromSRGBColor(FColor(52, 72, 96)),
		1.0f);
	return bSelected ? &SelectedBrush : &NormalBrush;
}

const FSlateBrush* GetWorkflowCardBrush(bool bActive, bool bSelected)
{
	static const FSlateRoundedBoxBrush NormalBrush(
		FLinearColor::FromSRGBColor(FColor(34, 34, 34)),
		9.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	static const FSlateRoundedBoxBrush SelectedBrush(
		FLinearColor::FromSRGBColor(FColor(34, 42, 52)),
		9.0f,
		FLinearColor::FromSRGBColor(FColor(25, 84, 145)),
		1.0f);
	static const FSlateRoundedBoxBrush ActiveBrush(
		FLinearColor::FromSRGBColor(FColor(28, 49, 39)),
		9.0f,
		FLinearColor::FromSRGBColor(FColor(20, 105, 70)),
		1.0f);
	return bActive ? &ActiveBrush : bSelected ? &SelectedBrush : &NormalBrush;
}

FLinearColor GetWorkflowAccentColor(FName WorkflowId)
{
	const FString Id = WorkflowId.ToString();
	if (Id.Contains(TEXT("Material"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Shader"), ESearchCase::IgnoreCase))
	{
		return FLinearColor::FromSRGBColor(FColor(64, 169, 255));
	}
	if (Id.Contains(TEXT("Nanite"), ESearchCase::IgnoreCase))
	{
		return FLinearColor::FromSRGBColor(FColor(0, 205, 189));
	}
	if (Id.Contains(TEXT("Lumen"), ESearchCase::IgnoreCase))
	{
		return FLinearColor::FromSRGBColor(FColor(0, 207, 232));
	}
	if (Id.Contains(TEXT("VSM"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Shadow"), ESearchCase::IgnoreCase))
	{
		return FLinearColor::FromSRGBColor(FColor(242, 171, 25));
	}
	if (Id.Contains(TEXT("Collision"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Geometry"), ESearchCase::IgnoreCase))
	{
		return FLinearColor::FromSRGBColor(FColor(190, 125, 255));
	}
	if (Id.Contains(TEXT("Performance"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Stat"), ESearchCase::IgnoreCase))
	{
		return FLinearColor::FromSRGBColor(FColor(227, 100, 219));
	}
	return FLinearColor::FromSRGBColor(FColor(127, 135, 145));
}

const FSlateBrush* GetWorkflowIconBrush(FName WorkflowId)
{
	const FString Id = WorkflowId.ToString();
	static const FSlateRoundedBoxBrush MaterialBrush(FLinearColor::FromSRGBColor(FColor(24, 35, 44)), 8.0f, FLinearColor::FromSRGBColor(FColor(39, 74, 101)), 1.0f);
	static const FSlateRoundedBoxBrush NaniteBrush(FLinearColor::FromSRGBColor(FColor(20, 39, 37)), 8.0f, FLinearColor::FromSRGBColor(FColor(31, 81, 77)), 1.0f);
	static const FSlateRoundedBoxBrush LumenBrush(FLinearColor::FromSRGBColor(FColor(20, 39, 42)), 8.0f, FLinearColor::FromSRGBColor(FColor(31, 82, 96)), 1.0f);
	static const FSlateRoundedBoxBrush VsmBrush(FLinearColor::FromSRGBColor(FColor(43, 37, 23)), 8.0f, FLinearColor::FromSRGBColor(FColor(91, 72, 33)), 1.0f);
	static const FSlateRoundedBoxBrush GeometryBrush(FLinearColor::FromSRGBColor(FColor(40, 29, 47)), 8.0f, FLinearColor::FromSRGBColor(FColor(89, 64, 107)), 1.0f);
	static const FSlateRoundedBoxBrush PerformanceBrush(FLinearColor::FromSRGBColor(FColor(43, 28, 42)), 8.0f, FLinearColor::FromSRGBColor(FColor(99, 57, 95)), 1.0f);
	static const FSlateRoundedBoxBrush DefaultBrush(FLinearColor::FromSRGBColor(FColor(34, 34, 34)), 8.0f, FLinearColor::FromSRGBColor(FColor(46, 46, 46)), 1.0f);
	if (Id.Contains(TEXT("Material"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Shader"), ESearchCase::IgnoreCase)) return &MaterialBrush;
	if (Id.Contains(TEXT("Nanite"), ESearchCase::IgnoreCase)) return &NaniteBrush;
	if (Id.Contains(TEXT("Lumen"), ESearchCase::IgnoreCase)) return &LumenBrush;
	if (Id.Contains(TEXT("VSM"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Shadow"), ESearchCase::IgnoreCase)) return &VsmBrush;
	if (Id.Contains(TEXT("Collision"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Geometry"), ESearchCase::IgnoreCase)) return &GeometryBrush;
	if (Id.Contains(TEXT("Performance"), ESearchCase::IgnoreCase) || Id.Contains(TEXT("Stat"), ESearchCase::IgnoreCase)) return &PerformanceBrush;
	return &DefaultBrush;
}

const FSlateBrush* GetWorkflowSourceBadgeBrush(TADebugViewTool::EWorkflowPresetSource Source)
{
	static const FSlateRoundedBoxBrush DefaultBrush(
		FLinearColor::FromSRGBColor(FColor(34, 34, 34)),
		5.0f,
		FLinearColor::FromSRGBColor(FColor(46, 46, 46)),
		1.0f);
	static const FSlateRoundedBoxBrush ModifiedBrush(
		FLinearColor::FromSRGBColor(FColor(43, 37, 23)),
		5.0f,
		FLinearColor::FromSRGBColor(FColor(91, 72, 33)),
		1.0f);
	static const FSlateRoundedBoxBrush UserCreatedBrush(
		FLinearColor::FromSRGBColor(FColor(24, 40, 31)),
		5.0f,
		FLinearColor::FromSRGBColor(FColor(35, 87, 58)),
		1.0f);
	switch (Source)
	{
	case TADebugViewTool::EWorkflowPresetSource::Modified:
		return &ModifiedBrush;
	case TADebugViewTool::EWorkflowPresetSource::UserCreated:
		return &UserCreatedBrush;
	default:
		return &DefaultBrush;
	}
}
}

void STADebugViewPanel::Construct(const FArguments& InArgs)
{
	Executor = InArgs._Executor;
	TADebugViewTool::GetEffectiveWorkflowPresets();
	TADebugViewTool::InitializeQuickAccessDefaults();
	LoadUserPreferences();
	if (Executor)
	{
		Executor->SynchronizeFromCurrentViewportState();
	}
	if (TADebugViewTool::GetEffectiveWorkflowPresets().Num() > 0)
	{
		SelectedWorkflowId = TADebugViewTool::GetEffectiveWorkflowPresets()[0].Id;
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(GetAppFrameBrush())
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(58.0f)
				[
					SNew(SBorder)
					.BorderImage(GetFlatSectionBrush())
					.Padding(FMargin(16.0f, 9.0f))
					[
						MakeHeaderBar()
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeHorizontalSeparator()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(64.0f)
				[
					SNew(SBorder)
					.BorderImage(GetFlatSectionBrush())
					.Padding(FMargin(16.0f, 10.0f))
					[
						MakeSearchArea()
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeHorizontalSeparator()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(80.0f)
				.Visibility_Lambda([]()
				{
					const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
					return Settings && !Settings->FavoriteActions.IsEmpty()
						? EVisibility::Visible
						: EVisibility::Collapsed;
				})
				[
					SNew(SBorder)
					.BorderImage(GetFlatSectionBrush())
					.Padding(FMargin(16.0f, 8.0f))
					[
						MakeQuickAccessArea()
					]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(FMargin(10.0f, 0.0f, 10.0f, 10.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(222.0f)
					[
						SNew(SBorder)
						.BorderImage(GetSectionPanelBrush())
						.Padding(FMargin(10.0f))
						[
							MakeNavigationBar()
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SBorder)
					.BorderImage(GetContentPanelBrush())
					.Padding(0.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(SBox)
							.Padding(FMargin(18.0f, 16.0f))
							[
								// Slot order below must match EPanelPage, since ActivePage is
								// used directly as the switcher index.
								SNew(SWidgetSwitcher)
								.WidgetIndex_Lambda([this]()
								{
									return static_cast<int32>(ActivePage);
								})
								+ SWidgetSwitcher::Slot()
								[
									MakeWorkflowsPage()
								]
								+ SWidgetSwitcher::Slot()
								[
									MakeDebugViewsPage()
								]
								+ SWidgetSwitcher::Slot()
								[
									MakeAdvancedPage()
								]
								+ SWidgetSwitcher::Slot()
								[
									MakeHelpPage()
								]
								+ SWidgetSwitcher::Slot()
								[
									MakeDiagnosticsPage()
								]
							]
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(10.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(360.0f)
					.Visibility_Lambda([this]()
					{
						return ActivePage == EPanelPage::Workflows ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						MakeContextInspector()
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(10.0f, 0.0f, 10.0f, 6.0f))
			[
				SNew(SBorder)
				.BorderImage(GetFlatSectionBrush())
				.Padding(FMargin(8.0f, 4.0f))
				[
					MakeLiveStatusBar()
				]
			]
		]
	];

	RebuildQuickAccess();
	RebuildContextInspector();
	RefreshStatusCache();
	RefreshDiagnosticsCache();
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda(
		[this](double, float)
		{
			if (Executor)
			{
				// Viewport layout restoration can finish after this panel is
				// constructed during editor startup, so repeat the initial sync
				// once on the following Slate tick.
				Executor->SynchronizeFromCurrentViewportState();
				RefreshStatusCache();
			}
			return EActiveTimerReturnType::Stop;
		}));
	RegisterActiveTimer(0.25f, FWidgetActiveTimerDelegate::CreateSP(this, &STADebugViewPanel::UpdateStatusCache));
}

void STADebugViewPanel::RefreshQuickAccess()
{
	RebuildQuickAccess();
}

TSharedRef<SWidget> STADebugViewPanel::MakeHeaderBar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 9.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(28.0f)
				.HeightOverride(28.0f)
				[
					SNew(SBorder)
					.BorderImage(GetBrandMarkBrush())
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BrandMark", "TA"))
						.Font(FAppStyle::GetFontStyle(TEXT("MonoFontBold")))
						.RenderTransform(FSlateRenderTransform(FVector2D(0.0f, 1.0f)))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PanelHeading", "TA Debug Views"))
				.Font(FAppStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
				.RenderTransform(FSlateRenderTransform(FVector2D(0.0f, 1.0f)))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(16.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			// Status dot: green while a workflow is applied, subdued when idle.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Icons.FilledCircle")))
					.DesiredSizeOverride(FVector2D(8.0f, 8.0f))
					.ColorAndOpacity_Lambda([this]()
					{
						return Executor && !Executor->GetActiveWorkflowPresetId().IsNone()
							? FSlateColor(FLinearColor::FromSRGBColor(FColor(53, 175, 109)))
							: FSlateColor::UseSubduedForeground();
					})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(this, &STADebugViewPanel::GetHeaderWorkflowText)
					.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
					.RenderTransform(FSlateRenderTransform(FVector2D(0.0f, 1.0f)))
					.ColorAndOpacity_Lambda([this]()
					{
						return Executor && !Executor->GetActiveWorkflowPresetId().IsNone()
							? FSlateColor(FLinearColor::FromSRGBColor(FColor(200, 200, 200)))
							: FSlateColor::UseSubduedForeground();
					})
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 16.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(GetInsetPanelBrush())
			.Padding(3.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					MakeViewportTargetButton(TADebugViewTool::EDebugViewportTarget::ActivePreferred, LOCTEXT("TargetActivePreferred", "Active"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					MakeViewportTargetButton(TADebugViewTool::EDebugViewportTarget::AllPerspective, LOCTEXT("TargetAllPerspective", "Perspective"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(3.0f, 0.0f, 0.0f, 0.0f)
				[
					MakeViewportTargetButton(TADebugViewTool::EDebugViewportTarget::AllLevel, LOCTEXT("TargetAllLevel", "All"))
				]
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
				.ContentPadding(FMargin(12.0f, 7.0f))
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("HeaderResetTooltip", "Return viewports to Lit and clear common debug overlays."))
				.OnClicked_Lambda([this]()
				{
				for (const TADebugViewTool::FWorkflowPreset& WorkflowPreset : TADebugViewTool::GetEffectiveWorkflowPresets())
					{
						if (WorkflowPreset.Id == TEXT("TADebugWorkflow_ResetDebug"))
						{
							ExecuteWorkflowPresetFromPanel(WorkflowPreset);
							break;
						}
					}

					return FReply::Handled();
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("HeaderResetButton", "Reset"))
							.RenderTransform(FSlateRenderTransform(FVector2D(0.0f, 1.0f)))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("HeaderResetChord", "Alt+Shift+0"))
							.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
							.RenderTransform(FSlateRenderTransform(FVector2D(0.0f, 1.0f)))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeViewportTargetButton(TADebugViewTool::EDebugViewportTarget Target, const FText& Label)
{
	return SNew(SBorder)
		.BorderImage_Lambda([this, Target]()
		{
			return GetTargetButtonBrush(Executor && Executor->GetViewportTarget() == Target);
		})
		.Padding(0.0f)
		[
			SNew(SBox)
			.MinDesiredWidth(62.0f)
			.HeightOverride(30.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(12.0f, 0.0f))
				.ToolTipText_Lambda([this, Target]()
				{
					if (Target == TADebugViewTool::EDebugViewportTarget::AllPerspective)
					{
						return LOCTEXT("TargetAllPerspectiveTooltip", "Apply viewport debug views to every perspective level viewport.");
					}

					if (Target == TADebugViewTool::EDebugViewportTarget::AllLevel)
					{
						return LOCTEXT("TargetAllLevelTooltip", "Apply viewport debug views to every level viewport.");
					}

					return LOCTEXT("TargetActivePreferredTooltip", "Apply viewport debug views to the active level viewport, falling back to the preferred perspective viewport.");
				})
				.ForegroundColor_Lambda([this, Target]()
				{
					return Executor && Executor->GetViewportTarget() == Target
						? FSlateColor(FLinearColor::White)
						: FSlateColor::UseSubduedForeground();
				})
				.OnClicked_Lambda([this, Target]()
				{
					if (Executor)
					{
						Executor->SetViewportTarget(Target);
						SaveViewportTargetPreference();
					}

					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(Label)
					.RenderTransform(FSlateRenderTransform(FVector2D(0.0f, 1.0f)))
					.ColorAndOpacity_Lambda([this, Target]()
					{
						return Executor && Executor->GetViewportTarget() == Target
							? FSlateColor(FLinearColor::White)
							: FSlateColor::UseSubduedForeground();
					})
				]
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeLiveStatusBar()
{
	return SNew(SWrapBox)
		.UseAllottedSize(true)
		+ SWrapBox::Slot()
		.Padding(0.0f, 0.0f, 4.0f, 3.0f)
		[
			MakeStatusChip(
				LOCTEXT("StatusChipTarget", "Target"),
				TAttribute<FText>::CreateLambda([this]() { return CachedTargetStatus; }))
		]
		+ SWrapBox::Slot()
		.Padding(0.0f, 0.0f, 4.0f, 3.0f)
		[
			MakeStatusChip(
				LOCTEXT("StatusChipViewMode", "ViewMode"),
				TAttribute<FText>::CreateLambda([this]() { return CachedViewModeStatus; }))
		]
		+ SWrapBox::Slot()
		.Padding(0.0f, 0.0f, 4.0f, 3.0f)
		[
			MakeStatusChip(
				LOCTEXT("StatusChipVisualization", "Visualization"),
				TAttribute<FText>::CreateLambda([this]() { return CachedVisualizationStatus; }))
		]
		+ SWrapBox::Slot()
		.Padding(0.0f, 0.0f, 4.0f, 3.0f)
		[
			MakeStatusChip(
				LOCTEXT("StatusChipWorkflow", "Workflow"),
				TAttribute<FText>::CreateLambda([this]() { return CachedWorkflowStatus; }))
		]
		+ SWrapBox::Slot()
		.Padding(0.0f, 0.0f, 4.0f, 3.0f)
		[
			MakeStatusChip(
				LOCTEXT("StatusChipDiagnostics", "Diagnostics"),
				TAttribute<FText>::CreateLambda([this]() { return CachedDiagnosticsStatus; }))
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeSearchArea()
{
	return SAssignNew(SearchResultsAnchor, SMenuAnchor)
		.Placement(MenuPlacement_BelowAnchor)
		.FitInWindow(true)
		.Method(EPopupMethod::UseCurrentWindow)
		.ShouldDeferPaintingAfterWindowContent(true)
		.UseApplicationMenuStack(true)
		.ShowMenuBackground(false)
		.MenuContent(
			SNew(SBorder)
			.BorderImage(GetInsetPanelBrush())
			.Padding(6.0f)
			[
				SNew(SBox)
				.MinDesiredWidth_Lambda([this]()
				{
					return SearchResultsAnchor.IsValid()
						? SearchResultsAnchor->GetCachedGeometry().GetLocalSize().X
						: 320.0f;
				})
				.MaxDesiredHeight(420.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(SearchResultsBox, SVerticalBox)
					]
				]
			])
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SBox)
				.HeightOverride(38.0f)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage_Lambda([this]()
						{
							return GetCommandSearchBrush(SearchBox.IsValid() && SearchBox->HasKeyboardFocus());
						})
						.Padding(0.0f)
						[
							SAssignNew(SearchBox, SEditableTextBox)
							.Style(GetCommandSearchTextStyle())
							.HintText(LOCTEXT("ActionSearchHint", "Search Commands..."))
							.ForegroundColor(FSlateColor::UseForeground())
							.FocusedForegroundColor(FSlateColor::UseForeground())
							.OnTextChanged(this, &STADebugViewPanel::OnSearchTextChanged)
							.OnTextCommitted(this, &STADebugViewPanel::OnSearchTextCommitted)
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(12.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SBox)
						.WidthOverride(14.0f)
						.HeightOverride(14.0f)
						.Visibility(EVisibility::HitTestInvisible)
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush(TEXT("Symbols.SearchGlass")))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SBorder)
						.BorderImage(GetWorkflowSourceBadgeBrush(TADebugViewTool::EWorkflowPresetSource::Default))
						.Padding(FMargin(7.0f, 2.0f))
						.Visibility(EVisibility::HitTestInvisible)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SearchChord", "Alt+Shift+D"))
							.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.ToolTipText(LOCTEXT("SearchChordTooltip", "Open or focus the TA Debug Views panel."))
						]
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.HeightOverride(38.0f)
				.MinDesiredWidth(112.0f)
				[
					SNew(SComboButton)
					.ContentPadding(FMargin(12.0f, 0.0f))
					.OnGetMenuContent(this, &STADebugViewPanel::MakeSearchCategoryMenu)
					.ToolTipText(LOCTEXT("SearchCategoryTooltip", "Limit command results to one category."))
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(this, &STADebugViewPanel::GetSearchCategoryLabel)
						.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
					]
				]
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeSearchCategoryMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	auto AddCategory = [this, &MenuBuilder](ESearchCategory Category, const FText& Label, const FText& Tooltip)
	{
		MenuBuilder.AddMenuEntry(
			Label,
			Tooltip,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, Category]()
				{
					SetSearchCategory(Category);
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, Category]()
				{
					return SearchCategory == Category;
				})),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	};

	AddCategory(
		ESearchCategory::All,
		LOCTEXT("SearchCategoryAll", "All"),
		LOCTEXT("SearchCategoryAllTooltip", "Search Workflows, Debug Views, and Advanced commands."));
	AddCategory(
		ESearchCategory::Workflows,
		LOCTEXT("SearchCategoryWorkflows", "Workflows"),
		LOCTEXT("SearchCategoryWorkflowsTooltip", "Search repeatable multi-action Workflows."));
	AddCategory(
		ESearchCategory::DebugViews,
		LOCTEXT("SearchCategoryDebugViews", "Debug Views"),
		LOCTEXT("SearchCategoryDebugViewsTooltip", "Search viewport visualization actions."));
	AddCategory(
		ESearchCategory::Advanced,
		LOCTEXT("SearchCategoryAdvanced", "Advanced"),
		LOCTEXT("SearchCategoryAdvancedTooltip", "Search utility and console-command actions."));

	return MenuBuilder.MakeWidget();
}

FText STADebugViewPanel::GetSearchCategoryLabel() const
{
	switch (SearchCategory)
	{
	case ESearchCategory::Workflows:
		return LOCTEXT("SearchCategoryLabelWorkflows", "Workflows");
	case ESearchCategory::DebugViews:
		return LOCTEXT("SearchCategoryLabelDebugViews", "Debug Views");
	case ESearchCategory::Advanced:
		return LOCTEXT("SearchCategoryLabelAdvanced", "Advanced");
	default:
		return LOCTEXT("SearchCategoryLabelAll", "All");
	}
}

void STADebugViewPanel::SetSearchCategory(ESearchCategory Category)
{
	if (SearchCategory == Category)
	{
		return;
	}

	SearchCategory = Category;
	RebuildSearchResults();
	if (!SearchText.IsEmpty() && SearchResultsAnchor.IsValid())
	{
		// The category command runs inside its own menu. Reopen the result popup
		// on the next Slate tick, after that menu has finished dismissing.
		RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda(
			[this](double, float)
			{
				if (SearchResultsAnchor.IsValid() && !SearchText.IsEmpty())
				{
					SearchResultsAnchor->SetIsOpen(true, false);
				}
				return EActiveTimerReturnType::Stop;
			}));
	}
}

void STADebugViewPanel::OnWorkflowFilterTextChanged(const FText& Text)
{
	WorkflowFilterText = Text.ToString().TrimStartAndEnd();
	RebuildCustomPresetButtons();
}

bool STADebugViewPanel::DoesWorkflowMatchFilter(const TADebugViewTool::FWorkflowPreset& WorkflowPreset) const
{
	if (WorkflowFilterText.IsEmpty())
	{
		return true;
	}

	FString SearchableText =
		WorkflowPreset.Label.ToString()
		+ TEXT(" ") + WorkflowPreset.Tooltip.ToString()
		+ TEXT(" ") + WorkflowPreset.Id.ToString()
		+ TEXT(" ") + TADebugViewTool::GetWorkflowSourceLabel(WorkflowPreset.Source).ToString();
	auto AppendActions = [&SearchableText](const TArray<TADebugViewTool::FDebugViewAction>& Actions)
	{
		for (const TADebugViewTool::FDebugViewAction& Action : Actions)
		{
			SearchableText += TEXT(" ");
			SearchableText += GetWorkflowActionDisplayText(Action).ToString();
		}
	};
	AppendActions(WorkflowPreset.ActivateActions);
	AppendActions(WorkflowPreset.DeactivateActions);

	return SearchableText.Contains(WorkflowFilterText, ESearchCase::IgnoreCase);
}

TSharedRef<SWidget> STADebugViewPanel::MakeStatusChip(const FText& Label, TAttribute<FText> Value)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(STextBlock)
				.Text(Label)
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
		// Values use the mono font so mode names and counts stay column-aligned.
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
				.Text(Value)
				.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeQuickAccessArea()
{
	return SNew(SQuickAccessPager)
		.OnPageDelta(FOnQuickAccessPageDelta::CreateSP(this, &STADebugViewPanel::ChangeQuickAccessPage))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SAssignNew(QuickAccessBox, SHorizontalBox)
				.Visibility_Lambda([]()
				{
					const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
					return Settings && !Settings->FavoriteActions.IsEmpty()
						? EVisibility::Visible
						: EVisibility::Collapsed;
				})
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			[
				SAssignNew(QuickAccessPageDotsBox, SHorizontalBox)
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeQuickActionButton(
	const FTADebugViewQuickAction& QuickAction,
	int32 FavoriteIndex,
	bool bCloseFavoritesMenu)
{
	TOptional<TADebugViewTool::FDebugViewPreset> DebugPreset;
	TOptional<TADebugViewTool::FWorkflowPreset> WorkflowPreset;
	if (!TADebugViewTool::ResolveQuickAction(QuickAction, DebugPreset, WorkflowPreset))
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("MissingQuickAction", "Missing preset"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground());
	}

	const FText Label = DebugPreset.IsSet() ? DebugPreset->Label : WorkflowPreset->Label;
	const FText Tooltip = DebugPreset.IsSet() ? DebugPreset->Tooltip : WorkflowPreset->Tooltip;
	const FSlateIcon Icon = DebugPreset.IsSet() ? DebugPreset->Icon : WorkflowPreset->Icon;
	const FName AccentId = WorkflowPreset.IsSet() ? WorkflowPreset->Id : DebugPreset->Id;
	const FLinearColor AccentColor = GetWorkflowAccentColor(AccentId);

	// Only the first FavoriteShortcutCount entries have a bound chord.
	const bool bHasShortcut = FavoriteIndex >= 0 && FavoriteIndex < TADebugViewTool::FavoriteShortcutCount;

	TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 5.0f, 0.0f)
		[
			SNew(SImage)
			.Image(Icon.GetIcon())
			.ColorAndOpacity(AccentColor)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			.ColorAndOpacity(FSlateColor::UseForeground())
		];

	if (bHasShortcut)
	{
		Content->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("FavoriteChord", "Alt+Shift+{0}"), FText::AsNumber(FavoriteIndex + 1)))
					.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}

	return SNew(SBorder)
		.BorderImage_Lambda([this, QuickAction]()
		{
			return GetQuickActionBrush(IsQuickActionActiveCached(QuickAction));
		})
		.Padding(0.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
			.ContentPadding(FMargin(10.0f, 0.0f))
			.HAlign(HAlign_Fill)
			.ToolTipText(Tooltip)
			.OnClicked_Lambda([this, QuickAction, bCloseFavoritesMenu]()
			{
				if (bCloseFavoritesMenu && QuickAccessFavoritesButton.IsValid())
				{
					QuickAccessFavoritesButton->SetIsOpen(false);
				}
				ExecuteQuickAction(QuickAction);
				return FReply::Handled();
			})
			[
				Content
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeQuickAccessFavoritesMenu()
{
	TSharedRef<SVerticalBox> FavoriteList = SNew(SVerticalBox);
	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
	if (!Settings)
	{
		return SNullWidget::NullWidget;
	}

	int32 ValidFavoriteCount = 0;
	for (int32 ActionIndex = 0; ActionIndex < Settings->FavoriteActions.Num(); ++ActionIndex)
	{
		const FTADebugViewQuickAction& QuickAction = Settings->FavoriteActions[ActionIndex];
		TOptional<TADebugViewTool::FDebugViewPreset> DebugPreset;
		TOptional<TADebugViewTool::FWorkflowPreset> WorkflowPreset;
		if (!TADebugViewTool::ResolveQuickAction(QuickAction, DebugPreset, WorkflowPreset))
		{
			continue;
		}

		++ValidFavoriteCount;
		FavoriteList->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SBox)
				.HeightOverride(42.0f)
				[
					MakeQuickActionButton(QuickAction, ActionIndex, true)
				]
			];
	}

	return SNew(SBorder)
		.BorderImage(GetInsetPanelBrush())
		.Padding(8.0f)
		[
			SNew(SBox)
			.WidthOverride(360.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(4.0f, 2.0f, 4.0f, 7.0f))
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 7.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AllFavoritesHeadingStar", "★"))
							.ColorAndOpacity(FLinearColor(0.95f, 0.73f, 0.28f))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AllFavoritesHeading", "All Favorites"))
							.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::AsNumber(ValidFavoriteCount))
							.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					MakeHorizontalSeparator()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.MaxDesiredHeight(360.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							FavoriteList
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeFavoriteToggleButton(const FTADebugViewQuickAction& QuickAction)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
		.ContentPadding(FMargin(7.0f, 4.0f))
		.Text_Lambda([this, QuickAction]()
		{
			return IsFavoriteAction(QuickAction)
				? LOCTEXT("RemoveFavoriteButton", "★")
				: LOCTEXT("AddFavoriteButton", "☆");
		})
		.ForegroundColor_Lambda([this, QuickAction]()
		{
			return IsFavoriteAction(QuickAction)
				? FSlateColor(FLinearColor::FromSRGBColor(FColor(222, 161, 50)))
				: FSlateColor::UseSubduedForeground();
		})
		.ToolTipText_Lambda([this, QuickAction]()
		{
			return IsFavoriteAction(QuickAction)
				? LOCTEXT("RemoveFavoriteTooltip", "Remove this item from Favorites.")
				: LOCTEXT("AddFavoriteTooltip", "Add this item to Favorites.");
		})
		.OnClicked_Lambda([this, QuickAction]()
		{
			ToggleFavoriteAction(QuickAction);
			return FReply::Handled();
		});
}

TSharedRef<SWidget> STADebugViewPanel::MakeNavigationBar()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeNavigationButton(LOCTEXT("NavWorkflows", "Workflows"), EPanelPage::Workflows)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeNavigationButton(LOCTEXT("NavDebugViews", "Debug Views"), EPanelPage::DebugViews)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNullWidget::NullWidget
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			MakeHorizontalSeparator()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeNavigationButton(LOCTEXT("NavAdvanced", "Advanced"), EPanelPage::Advanced)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeNavigationButton(LOCTEXT("NavDiagnostics", "Diagnostics"), EPanelPage::Diagnostics)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeNavigationButton(LOCTEXT("NavHelp", "Help"), EPanelPage::Help)
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeNavigationButton(const FText& Label, EPanelPage Page)
{
	FText Tooltip;
	switch (Page)
	{
	case EPanelPage::Workflows:
		Tooltip = LOCTEXT("NavWorkflowsTooltip", "Common TA checks combined into one repeatable action.");
		break;
	case EPanelPage::DebugViews:
		Tooltip = LOCTEXT("NavDebugViewsTooltip", "Viewport visualizations grouped by rendering system.");
		break;
	case EPanelPage::Advanced:
		Tooltip = LOCTEXT("NavAdvancedTooltip", "Lower-frequency VSM, geometry, and performance commands.");
		break;
	case EPanelPage::Diagnostics:
		Tooltip = LOCTEXT("NavDiagnosticsTooltip", "Validate workflow Ids, action values, and stale references.");
		break;
	default:
		Tooltip = LOCTEXT("NavHelpTooltip", "Shortcuts, viewport targets, and data layout.");
		break;
	}

	FName IconName = TEXT("Icons.Help");
	switch (Page)
	{
	case EPanelPage::Workflows:
		IconName = TEXT("Icons.Check");
		break;
	case EPanelPage::DebugViews:
		IconName = TEXT("LevelEditor.Tabs.Viewports");
		break;
	case EPanelPage::Advanced:
		IconName = TEXT("Icons.Settings");
		break;
	case EPanelPage::Diagnostics:
		IconName = TEXT("Icons.Warning");
		break;
	default:
		break;
	}

	return SNew(SBorder)
		.BorderImage_Lambda([this, Page]()
		{
			return GetNavigationBrush(ActivePage == Page);
		})
		.Padding(0.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
			.ToolTipText(Tooltip)
			.ContentPadding(FMargin(10.0f, 8.0f))
			.HAlign(HAlign_Fill)
			.OnClicked_Lambda([this, Page]()
			{
				ActivePage = Page;
				if (ActivePage == EPanelPage::DebugViews && SelectedDebugGroupId.IsNone() && TADebugViewTool::GetPresetGroups().Num() > 0)
				{
					SelectedDebugGroupId = TADebugViewTool::GetPresetGroups()[0].Id;
				}

				SavePanelPagePreference();
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(IconName))
					.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
					.ColorAndOpacity_Lambda([this, Page]()
					{
						return ActivePage == Page
							? FSlateColor(FLinearColor::FromSRGBColor(FColor(200, 200, 200)))
							: FSlateColor(FLinearColor::FromSRGBColor(FColor(127, 135, 145)));
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(Label)
						.Font(FAppStyle::GetFontStyle(TEXT("NormalFont")))
						.ColorAndOpacity_Lambda([this, Page]()
						{
							return ActivePage == Page
								? FSlateColor(FLinearColor::FromSRGBColor(FColor(200, 200, 200)))
								: FSlateColor(FLinearColor::FromSRGBColor(FColor(127, 135, 145)));
						})
				]
				// Zero-padded count badge, mono so the digits stay aligned down the rail.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text_Lambda([this, Page]() { return GetPageCountText(Page); })
						.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
						.ColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(127, 135, 145))))
				]
			]
		];
}

FText STADebugViewPanel::GetPageCountText(EPanelPage Page) const
{
	int32 Count = 0;
	switch (Page)
	{
	case EPanelPage::Workflows:
		// Reset Debug is driven from the header button, not listed as a card.
		for (const TADebugViewTool::FWorkflowPreset& WorkflowPreset : TADebugViewTool::GetEffectiveWorkflowPresets())
		{
			if (WorkflowPreset.Id != TEXT("TADebugWorkflow_ResetDebug"))
			{
				++Count;
			}
		}
		break;
	case EPanelPage::DebugViews:
		// Counts groups, not presets: the page is navigated one group at a time.
		for (const TADebugViewTool::FDebugViewGroup& Group : TADebugViewTool::GetPresetGroups())
		{
			if (GroupHasDebugViewPresets(Group))
			{
				++Count;
			}
		}
		break;
	case EPanelPage::Advanced:
		for (const TADebugViewTool::FDebugViewGroup& Group : TADebugViewTool::GetPresetGroups())
		{
			for (const TADebugViewTool::FDebugViewPreset& Preset : Group.Presets)
			{
				if (IsAdvancedPagePreset(Preset))
				{
					++Count;
				}
			}
		}
		break;
	case EPanelPage::Diagnostics:
		// Read the cached count. This runs from a per-frame Text_Lambda, so the
		// checks themselves are only re-run when RebuildDiagnostics is called.
		Count = CachedDiagnosticsFailureCount;
		break;
	default:
		// Help has no meaningful count.
		return FText::GetEmpty();
	}

	// Zero-padded to two digits, matching the design's 08 / 12 / 00 badges.
	return FText::FromString(FString::Printf(TEXT("%02d"), Count));
}

TSharedRef<SWidget> STADebugViewPanel::MakeSectionHeading(const FText& Label)
{
	return SNew(STextBlock)
		.Text(Label)
		.Font(FAppStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")));
}

TSharedRef<SWidget> STADebugViewPanel::MakeWorkflowsPage()
{
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeSectionHeading(LOCTEXT("WorkflowHeading", "Workflows"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("WorkflowDescription", "Combine viewport modes and performance overlays into one repeatable check."))
						.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(FMargin(12.0f, 6.0f))
				.Text(LOCTEXT("NewWorkflowButton", "+  New Workflow"))
				.OnClicked_Lambda([this]()
				{
					AddNewCustomPreset();
					return FReply::Handled();
				})
			]
		];

	Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SBox)
				.HeightOverride(34.0f)
				[
					SAssignNew(WorkflowFilterBox, SSearchBox)
					.HintText(LOCTEXT("WorkflowFilterHint", "Filter workflows..."))
					.OnTextChanged(this, &STADebugViewPanel::OnWorkflowFilterTextChanged)
				]
		];

	Content->AddSlot()
		.AutoHeight()
		[
			SAssignNew(CustomPresetListBox, SVerticalBox)
		];

	RebuildCustomPresetButtons();

	return Content;
}

TSharedRef<SWidget> STADebugViewPanel::MakeContextInspector()
{
	return SNew(SBorder)
		.BorderImage(GetSectionPanelBrush())
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(GetFlatSectionBrush())
				.Padding(FMargin(14.0f, 11.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("ContextDetailsHeading", "Context Details"))
							.Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
							.Text(LOCTEXT("ContextDetailsShortcut", "F1"))
							.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeHorizontalSeparator()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SBox)
						.Padding(14.0f)
						[
							SAssignNew(ContextDetailsBox, SVerticalBox)
						]
				]
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeWorkflowDetails(const TADebugViewTool::FWorkflowPreset& WorkflowPreset)
{
	auto MakeDetailLine = [](const FText& Label, const FText& Value, bool bMonoValue = false)
	{
		return SNew(SBox)
			.Padding(FMargin(10.0f, 7.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				[
					SNew(STextBlock)
						.Text(Label)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.65f)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
						.Text(Value)
						.Font(bMonoValue
							? FAppStyle::GetFontStyle(TEXT("MonoFont"))
							: FAppStyle::GetFontStyle(TEXT("NormalFont")))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
			];
	};

	TSharedRef<SVerticalBox> DetailList = SNew(SVerticalBox);
	bool bHasDetailLine = false;
	auto AddDetailLine = [&DetailList, &MakeDetailLine, &bHasDetailLine](const FText& Label, const FText& Value, bool bMonoValue = false)
	{
		if (bHasDetailLine)
		{
			DetailList->AddSlot()
				.AutoHeight()
				[
					MakeHorizontalSeparator()
				];
		}
		DetailList->AddSlot()
			.AutoHeight()
			[
				MakeDetailLine(Label, Value, bMonoValue)
			];
		bHasDetailLine = true;
	};

	AddDetailLine(LOCTEXT("WorkflowDetailSource", "Source"), TADebugViewTool::GetWorkflowSourceLabel(WorkflowPreset.Source));
	AddDetailLine(LOCTEXT("WorkflowDetailType", "Type"), LOCTEXT("WorkflowDetailTypeValue", "Workflow"));
	AddDetailLine(LOCTEXT("WorkflowDetailTarget", "Target"), Executor ? Executor->GetViewportTargetLabel() : FText::GetEmpty());
	AddDetailLine(
		LOCTEXT("WorkflowDetailActions", "Actions"),
		FText::Format(
			LOCTEXT("WorkflowDetailActionsValue", "{0} activate · {1} restore"),
			FText::AsNumber(WorkflowPreset.ActivateActions.Num()),
			FText::AsNumber(WorkflowPreset.DeactivateActions.Num())));
	AddDetailLine(
		LOCTEXT("WorkflowDetailShortcut", "Shortcut"),
		GetQuickActionShortcutText(
			FTADebugViewQuickAction(ETADebugViewQuickActionType::WorkflowPreset, WorkflowPreset.Id.ToString())),
		/*bMonoValue=*/true);

	auto MakeSequence = [](const FText& Title, const TArray<TADebugViewTool::FDebugViewAction>& Actions)
	{
		TSharedRef<SVerticalBox> Sequence = SNew(SVerticalBox);
		Sequence->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 5.0f)
			[
				SNew(STextBlock)
					.Text(Title)
					.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		for (int32 ActionIndex = 0; ActionIndex < Actions.Num(); ++ActionIndex)
		{
			if (ActionIndex > 0)
			{
				Sequence->AddSlot()
					.AutoHeight()
					[
						MakeHorizontalSeparator()
					];
			}
			Sequence->AddSlot()
				.AutoHeight()
				[
					SNew(SBox)
						.Padding(FMargin(8.0f, 6.0f))
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 8.0f, 0.0f)
								[
									SNew(STextBlock)
										.Text(FText::AsNumber(ActionIndex + 1))
										.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
										.ColorAndOpacity(FSlateColor::UseSubduedForeground())
								]
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								[
									SNew(STextBlock)
										.Text(GetWorkflowActionDisplayText(Actions[ActionIndex]))
										.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
										.AutoWrapText(true)
								]
						]
				];
		}
		if (Actions.IsEmpty())
		{
			Sequence->AddSlot()
				.AutoHeight()
				[
					SNew(STextBlock)
						.Text(LOCTEXT("NoWorkflowActions", "No actions"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				];
		}
		return SNew(SBorder)
			.BorderImage(GetInsetPanelBrush())
			.Padding(10.0f)
			[
				Sequence
			];
	};

	const bool bIsActive = Executor && Executor->GetActiveWorkflowPresetId() == WorkflowPreset.Id;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBox)
						.WidthOverride(40.0f)
						.HeightOverride(40.0f)
						[
							SNew(SBorder)
								.BorderImage(GetWorkflowIconBrush(WorkflowPreset.Id))
								.Padding(9.0f)
								[
									SNew(SImage)
										.Image(WorkflowPreset.Icon.GetIcon())
										.ColorAndOpacity(GetWorkflowAccentColor(WorkflowPreset.Id))
								]
						]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
								.Text(WorkflowPreset.Label)
								.Font(FAppStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 5.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(WorkflowPreset.Tooltip)
								.AutoWrapText(true)
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
				]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(GetInsetPanelBrush())
			.Padding(0.0f)
			[
				DetailList
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 10.0f, 0.0f, 0.0f)
		[
			MakeSequence(LOCTEXT("ActivateSequenceHeading", "ACTIVATE SEQUENCE"), WorkflowPreset.ActivateActions)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			MakeSequence(LOCTEXT("RestoreSequenceHeading", "RESTORE SEQUENCE"), WorkflowPreset.DeactivateActions)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 12.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 0.0f, 5.0f, 0.0f)
				[
					SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(LOCTEXT("EditWorkflowButton", "Edit Workflow"))
						.OnClicked_Lambda([this, WorkflowPreset]()
						{
							LoadCustomPresetForEditing(WorkflowPreset.Id.ToString());
							return FReply::Handled();
						})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonColorAndOpacity(FLinearColor::FromSRGBColor(FColor(0, 112, 224)))
						.Text(bIsActive ? LOCTEXT("StopActiveWorkflowButton", "Stop Workflow") : LOCTEXT("RunSelectedWorkflowButton", "Run Workflow"))
						.OnClicked_Lambda([this, WorkflowPreset]()
						{
							ExecuteWorkflowPresetFromPanel(WorkflowPreset);
							RebuildContextInspector();
							RebuildCustomPresetButtons();
							return FReply::Handled();
						})
				]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeDebugViewsPage()
{
	TSharedRef<SHorizontalBox> Layout = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 18.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(180.0f)
			[
				SNew(SBorder)
				.BorderImage(GetInsetPanelBrush())
				.Padding(8.0f)
				[
					SAssignNew(DebugGroupListBox, SVerticalBox)
				]
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(DebugPresetListBox, SVerticalBox)
		];

	RebuildDebugGroupList();
	RebuildDebugPresetButtons();

	return Layout;
}

void STADebugViewPanel::RebuildDebugGroupList()
{
	if (!DebugGroupListBox.IsValid())
	{
		return;
	}

	DebugGroupListBox->ClearChildren();

	// Ensure the selected group is one this page actually lists.
	const TADebugViewTool::FDebugViewGroup* SelectedGroup = TADebugViewTool::GetPresetGroups().FindByPredicate(
		[this](const TADebugViewTool::FDebugViewGroup& Group)
		{
			return Group.Id == SelectedDebugGroupId && GroupHasDebugViewPresets(Group);
		});
	if (!SelectedGroup)
	{
		for (const TADebugViewTool::FDebugViewGroup& Group : TADebugViewTool::GetPresetGroups())
		{
			if (GroupHasDebugViewPresets(Group))
			{
				SelectedDebugGroupId = Group.Id;
				break;
			}
		}
	}

	for (const TADebugViewTool::FDebugViewGroup& Group : TADebugViewTool::GetPresetGroups())
	{
		if (!GroupHasDebugViewPresets(Group))
		{
			continue;
		}

		DebugGroupListBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeDebugGroupButton(Group)
			];
	}
}

TSharedRef<SWidget> STADebugViewPanel::MakeDebugViewRow(const TADebugViewTool::FDebugViewPreset& Preset)
{
	const FName PresetId = Preset.Id;
	const FTADebugViewQuickAction FavoriteAction(ETADebugViewQuickActionType::DebugPreset, PresetId.ToString());

	// Short kind label, matching the design's trailing type column.
	FText KindLabel;
	switch (Preset.ActionType)
	{
	case TADebugViewTool::EPresetActionType::NaniteVisualization:
		KindLabel = LOCTEXT("KindNanite", "Nanite");
		break;
	case TADebugViewTool::EPresetActionType::LumenVisualization:
		KindLabel = LOCTEXT("KindLumen", "Lumen");
		break;
	case TADebugViewTool::EPresetActionType::VirtualShadowMapVisualization:
		KindLabel = LOCTEXT("KindVSM", "VSM");
		break;
	case TADebugViewTool::EPresetActionType::Command:
		KindLabel = LOCTEXT("KindCommand", "Command");
		break;
	default:
		KindLabel = LOCTEXT("KindViewMode", "ViewMode");
		break;
	}

	return SNew(SBorder)
		.BorderImage_Lambda([this, PresetId]()
		{
			return GetListRowBrush(IsDebugPresetActiveCached(PresetId));
		})
		.Padding(FMargin(2.0f, 1.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				MakeFavoriteToggleButton(FavoriteAction)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
					.ToolTipText(Preset.Tooltip)
					.ContentPadding(FMargin(8.0f, 8.0f))
					.HAlign(HAlign_Fill)
					.OnClicked_Lambda([this, Preset]()
					{
						ExecuteDebugPresetFromPanel(Preset);
						return FReply::Handled();
					})
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SImage)
								.Image(Preset.Icon.GetIcon())
								.ColorAndOpacity_Lambda([this, PresetId]()
								{
									return IsDebugPresetActiveCached(PresetId)
										? FSlateColor(FLinearColor::White)
										: FSlateColor::UseForeground();
								})
						]
						// Name column.
						+ SHorizontalBox::Slot()
						.FillWidth(0.32f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(Preset.Label)
								.Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.ColorAndOpacity_Lambda([this, PresetId]()
								{
									return IsDebugPresetActiveCached(PresetId)
										? FSlateColor(FLinearColor::White)
										: FSlateColor::UseForeground();
								})
						]
						// Description column.
						+ SHorizontalBox::Slot()
						.FillWidth(0.52f)
						.VAlign(VAlign_Center)
						.Padding(10.0f, 0.0f, 10.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text(Preset.Tooltip)
								.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
								.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								.ColorAndOpacity_Lambda([this, PresetId]()
								{
									return IsDebugPresetActiveCached(PresetId)
										? FSlateColor(FLinearColor::FromSRGBColor(FColor(200, 200, 200)))
										: FSlateColor::UseSubduedForeground();
								})
						]
						// Kind column.
						+ SHorizontalBox::Slot()
						.FillWidth(0.16f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						[
							SNew(STextBlock)
								.Text(KindLabel)
								.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
								.ColorAndOpacity_Lambda([this, PresetId]()
								{
									return IsDebugPresetActiveCached(PresetId)
										? FSlateColor(FLinearColor::FromSRGBColor(FColor(200, 200, 200)))
										: FSlateColor::UseSubduedForeground();
								})
						]
					]
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeAdvancedPage()
{
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeSectionHeading(LOCTEXT("AdvancedHeading", "Advanced"))
		];

	Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			SNew(STextBlock)
				.Text(LOCTEXT("AdvancedDescription", "VSM, geometry, and performance commands, kept out of the day-to-day workflow lists."))
				.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
		];

	// One flat list of console-command presets, regardless of source group.
	for (const TADebugViewTool::FDebugViewGroup& Group : TADebugViewTool::GetPresetGroups())
	{
		for (const TADebugViewTool::FDebugViewPreset& Preset : Group.Presets)
		{
			if (!IsAdvancedPagePreset(Preset))
			{
				continue;
			}

			Content->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					MakeUtilityRow(Preset)
				];
		}
	}

	return Content;
}

TSharedRef<SWidget> STADebugViewPanel::MakeUtilityRow(const TADebugViewTool::FDebugViewPreset& Preset)
{
	const FName PresetId = Preset.Id;
	const FTADebugViewQuickAction FavoriteAction(ETADebugViewQuickActionType::DebugPreset, PresetId.ToString());

	return SNew(SBorder)
		.BorderImage(GetInsetPanelBrush())
		.Padding(FMargin(12.0f, 9.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SImage)
					.Image(Preset.Icon.GetIcon())
					.ColorAndOpacity(GetWorkflowAccentColor(PresetId))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.34f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(Preset.Label)
					.Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
			// The literal console command, so the row is self-documenting.
			+ SHorizontalBox::Slot()
			.FillWidth(0.46f)
			.VAlign(VAlign_Center)
			.Padding(10.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(FText::FromString(Preset.Commands))
					.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.ToolTipText(Preset.Tooltip)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				MakeFavoriteToggleButton(FavoriteAction)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
					.Text(LOCTEXT("ExecuteUtilityButton", "Execute"))
					.ToolTipText(Preset.Tooltip)
					.ContentPadding(FMargin(10.0f, 3.0f))
					.OnClicked_Lambda([this, Preset]()
					{
						ExecuteDebugPresetFromPanel(Preset);
						return FReply::Handled();
					})
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeHelpBlock(const FText& Heading, const TArray<TSharedRef<SWidget>>& Rows)
{
	TSharedRef<SVerticalBox> Block = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			MakeSectionHeading(Heading)
		];

	for (const TSharedRef<SWidget>& Row : Rows)
	{
		Block->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				Row
			];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 14.0f, 0.0f, 10.0f))
		[
			Block
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeHorizontalSeparator()
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeHelpPage()
{
	const auto MakeBodyText = [](const FText& Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground());
	};

	// Description on the left, mono chord on the right, matching the design.
	const auto MakeShortcutRow = [](const FText& Description, const FText& Chord)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(Description)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
					.Text(Chord)
					.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
			];
	};

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);

	Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeSectionHeading(LOCTEXT("HelpHeading", "Help"))
		];

	Content->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			MakeBodyText(LOCTEXT("HelpIntro", "The panel remembers the last page, Debug View group, viewport target, and Favorites."))
		];

	Content->AddSlot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeHorizontalSeparator()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(0.0f, 0.0f, 28.0f, 0.0f))
				+ SUniformGridPanel::Slot(0, 0)
				[
					MakeHelpBlock(
						LOCTEXT("HelpShortcutsHeading", "Keyboard Shortcuts"),
						{
							MakeShortcutRow(
								LOCTEXT("HelpShortcutOpenPanelText", "Open or focus the panel"),
								LOCTEXT("HelpShortcutOpenPanel", "Alt+Shift+D")),
							MakeShortcutRow(
								LOCTEXT("HelpShortcutResetText", "Reset the debug state"),
								LOCTEXT("HelpShortcutReset", "Alt+Shift+0")),
							MakeShortcutRow(
								LOCTEXT("HelpShortcutFavoritesText", "Run Favorites 1 through 5"),
								LOCTEXT("HelpShortcutFavorites", "Alt+Shift+1...5"))
						})
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					MakeHelpBlock(
						LOCTEXT("HelpTargetHeading", "Viewport Target"),
						{
							MakeBodyText(LOCTEXT("HelpTargetActive", "Active applies to the active level viewport, falling back to the preferred perspective viewport.")),
							MakeBodyText(LOCTEXT("HelpTargetPerspective", "Perspective applies to every perspective level viewport.")),
							MakeBodyText(LOCTEXT("HelpTargetAll", "All applies to every level viewport."))
						})
				]
				+ SUniformGridPanel::Slot(0, 1)
				[
					MakeHelpBlock(
						LOCTEXT("HelpQuickAccessHeading", "Quick Access"),
						{
							MakeBodyText(LOCTEXT("HelpQuickAccessFavorites", "Favorites are user-managed. The first five are bound to global shortcuts and update as soon as an item is unpinned.")),
							MakeBodyText(LOCTEXT("HelpQuickAccessStale", "References to deleted presets are skipped in the strip and can be cleared from the Diagnostics page."))
						})
				]
				+ SUniformGridPanel::Slot(1, 1)
				[
					MakeHelpBlock(
						LOCTEXT("HelpWorkflowDataHeading", "Workflow Data"),
						{
							MakeBodyText(LOCTEXT("HelpWorkflowDataMerge", "Plugin defaults and project overrides are merged by stable Id, so a plugin update can add or fix defaults without discarding project edits.")),
							MakeBodyText(LOCTEXT("HelpWorkflowDataEdit", "Editing in Context Details saves a project override. Reset to Default removes only that override, keeping Favorites and shortcuts bound to the same Id."))
						})
				]
			]
		];

	return Content;
}

TSharedRef<SWidget> STADebugViewPanel::MakeDiagnosticsPage()
{
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeSectionHeading(LOCTEXT("DiagnosticsHeading", "Diagnostics"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("DiagnosticsDescription", "Validate workflow Ids, action values, override relationships, and stale Favorites references."))
						.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CleanStaleActionsButton", "Clean Stale References"))
				.OnClicked_Lambda([this]()
				{
					RemoveStaleQuickActions();
					RebuildDiagnostics();
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshDiagnosticsButton", "Refresh"))
				.OnClicked_Lambda([this]()
				{
					RebuildDiagnostics();
					return FReply::Handled();
				})
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(DiagnosticsBox, SVerticalBox)
		];

	RebuildDiagnostics();
	return Content;
}

TSharedRef<SWidget> STADebugViewPanel::MakeWorkflowPresetButton(TADebugViewTool::FWorkflowPreset WorkflowPreset)
{
	const FName WorkflowId = WorkflowPreset.Id;
	const FTADebugViewQuickAction FavoriteAction(ETADebugViewQuickActionType::WorkflowPreset, WorkflowId.ToString());
	const FLinearColor AccentColor = GetWorkflowAccentColor(WorkflowId);
	TSharedPtr<SWorkflowDoubleClickButton> WorkflowButton;

	TSharedRef<SWidget> Card = SNew(SBorder)
		.BorderImage_Lambda([this, WorkflowId]()
		{
			return GetWorkflowCardBrush(IsWorkflowActive(WorkflowId), SelectedWorkflowId == WorkflowId);
		})
		.Padding(0.0f)
		[
			SAssignNew(WorkflowButton, SWorkflowDoubleClickButton)
			.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
			.ToolTipText(WorkflowPreset.Tooltip)
			.ContentPadding(FMargin(14.0f))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.OnClicked_Lambda([this, WorkflowId]()
			{
				SelectWorkflow(WorkflowId);
				return FReply::Handled();
			})
			[
				SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(32.0f)
					.HeightOverride(32.0f)
					[
						SNew(SBorder)
						.BorderImage(GetWorkflowIconBrush(WorkflowId))
						.Padding(5.0f)
						[
							SNew(SImage)
								.Image(WorkflowPreset.Icon.GetIcon())
								.ColorAndOpacity(AccentColor)
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Top)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
							.Text(WorkflowPreset.Label)
							.Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							.ColorAndOpacity_Lambda([this, WorkflowId]()
							{
								return IsWorkflowActive(WorkflowId)
									? FSlateColor(FLinearColor::White)
									: FSlateColor::UseForeground();
							})
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(WorkflowPreset.Tooltip)
							.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]
				// Favorite toggle sits inside the card, per the design's card-top row.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					MakeFavoriteToggleButton(FavoriteAction)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBorder)
					.BorderImage(GetWorkflowSourceBadgeBrush(WorkflowPreset.Source))
					.Padding(FMargin(6.0f, 2.0f))
					[
						SNew(STextBlock)
							.Text(TADebugViewTool::GetWorkflowSourceLabel(WorkflowPreset.Source))
							.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
							.ColorAndOpacity(
								WorkflowPreset.Source == TADebugViewTool::EWorkflowPresetSource::Modified
									? FSlateColor(FLinearColor::FromSRGBColor(FColor(222, 161, 50)))
									: WorkflowPreset.Source == TADebugViewTool::EWorkflowPresetSource::UserCreated
										? FSlateColor(FLinearColor::FromSRGBColor(FColor(53, 175, 109)))
										: FSlateColor::UseSubduedForeground())
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNullWidget::NullWidget
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(FText::Format(
							LOCTEXT("WorkflowCardActionCounts", "{0} / {1} actions"),
							FText::AsNumber(WorkflowPreset.ActivateActions.Num()),
							FText::AsNumber(WorkflowPreset.DeactivateActions.Num())))
						.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
						.ColorAndOpacity_Lambda([this, WorkflowId]()
						{
							return IsWorkflowActive(WorkflowId)
								? FSlateColor(FLinearColor::FromSRGBColor(FColor(53, 175, 109)))
								: FSlateColor::UseSubduedForeground();
						})
				]
				// Explicit ACTIVE badge so running state does not rely on colour alone.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
						.Text(LOCTEXT("WorkflowCardActiveBadge", "ACTIVE"))
						.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
						.ColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(53, 175, 109))))
						.Visibility_Lambda([this, WorkflowId]()
						{
							return IsWorkflowActive(WorkflowId) ? EVisibility::Visible : EVisibility::Collapsed;
						})
				]
			]
			]
		];

	WorkflowButton->SetOnDoubleClicked(FPointerEventHandler::CreateLambda(
		[this, WorkflowId](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
			{
				return FReply::Unhandled();
			}

			if (SelectedWorkflowId != WorkflowId)
			{
				SelectWorkflow(WorkflowId);
			}

			if (const TADebugViewTool::FWorkflowPreset* SelectedWorkflow =
				TADebugViewTool::FindEffectiveWorkflowPreset(SelectedWorkflowId))
			{
				ExecuteWorkflowPresetFromPanel(*SelectedWorkflow);
			}
			return FReply::Handled();
		}));

	return Card;
}

TSharedRef<SWidget> STADebugViewPanel::MakeDebugGroupButton(const TADebugViewTool::FDebugViewGroup& Group)
{
	const FName GroupId = Group.Id;

	int32 GroupPresetCount = 0;
	for (const TADebugViewTool::FDebugViewPreset& Preset : Group.Presets)
	{
		if (!IsAdvancedPagePreset(Preset))
		{
			++GroupPresetCount;
		}
	}

	return SNew(SBox)
		.HeightOverride(32.0f)
		[
			SNew(SBorder)
			.BorderImage_Lambda([this, GroupId]()
			{
				return GetDebugGroupButtonBrush(SelectedDebugGroupId == GroupId);
			})
			.Padding(0.0f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
				.ContentPadding(FMargin(8.0f, 0.0f))
				.HAlign(HAlign_Fill)
				.OnClicked_Lambda([this, GroupId]()
				{
					SelectedDebugGroupId = GroupId;
					SaveDebugGroupPreference();
					RebuildDebugGroupList();
					RebuildDebugPresetButtons();
					return FReply::Handled();
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(Group.Label)
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
							.ColorAndOpacity_Lambda([this, GroupId]()
							{
								return SelectedDebugGroupId == GroupId
									? FSlateColor(FLinearColor::FromSRGBColor(FColor(200, 200, 200)))
									: FSlateColor::UseSubduedForeground();
							})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%02d"), GroupPresetCount)))
							.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
							.ColorAndOpacity_Lambda([this, GroupId]()
							{
								return SelectedDebugGroupId == GroupId
									? FSlateColor(FLinearColor::FromSRGBColor(FColor(167, 199, 232)))
									: FSlateColor::UseSubduedForeground();
							})
					]
				]
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeCustomPresetEditor()
{
	return SNew(SBorder)
		.BorderImage(GetFlatSectionBrush())
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return GetCustomPresetEditorTitle();
				})
				.Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return TADebugViewTool::GetWorkflowSourceLabel(EditingWorkflowSource);
					})
					.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
					.ColorAndOpacity_Lambda([this]()
					{
						return EditingWorkflowSource == TADebugViewTool::EWorkflowPresetSource::Modified
							? FSlateColor(FLinearColor::FromSRGBColor(FColor(222, 161, 50)))
							: EditingWorkflowSource == TADebugViewTool::EWorkflowPresetSource::UserCreated
								? FSlateColor(FLinearColor::FromSRGBColor(FColor(53, 175, 109)))
								: FSlateColor::UseSubduedForeground();
					})
			]
			// Explains what saving will do to the override file for this source.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
					.Text(this, &STADebugViewPanel::GetEditorSourceNoteText)
					.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeEditorTextField(
					LOCTEXT("CustomPresetNameLabel", "Workflow Name"),
					TAttribute<FText>::CreateLambda([this]()
					{
						return FText::FromString(EditingLabel);
					}),
					FOnTextChanged::CreateLambda([this](const FText& Text)
					{
						EditingLabel = Text.ToString();
						// Clear the inline error as soon as the field becomes non-empty.
						if (!EditedNameError.IsEmpty() && !EditingLabel.TrimStartAndEnd().IsEmpty())
						{
							EditedNameError = FText::GetEmpty();
						}
					}))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock)
					.Text(this, &STADebugViewPanel::GetEditedNameError)
					.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
					.ColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(229, 76, 74))))
					.AutoWrapText(true)
					.Visibility_Lambda([this]()
					{
						return EditedNameError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
					})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				MakeEditorTextField(
					LOCTEXT("CustomPresetDescriptionLabel", "Description"),
					TAttribute<FText>::CreateLambda([this]()
					{
						return FText::FromString(EditingTooltip);
					}),
					FOnTextChanged::CreateLambda([this](const FText& Text)
					{
						EditingTooltip = Text.ToString();
					}),
					/*bMultiLine=*/true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				MakeActionListEditor(LOCTEXT("CustomPresetActivateLabel", "Activate Actions"), EditingActivateActions, ActivateActionListBox, ECustomActionListKind::Activate)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				MakeActionListEditor(LOCTEXT("CustomPresetRestoreLabel", "Restore Actions"), EditingDeactivateActions, DeactivateActionListBox, ECustomActionListKind::Deactivate)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text_Lambda([this]()
					{
						return EditingWorkflowSource == TADebugViewTool::EWorkflowPresetSource::Modified
							? LOCTEXT("ResetWorkflowToDefaultButton", "Reset to Default")
							: LOCTEXT("DeleteWorkflowButton", "Delete Workflow");
					})
					.Visibility_Lambda([this]()
					{
						return !bCreatingWorkflow && EditingWorkflowSource != TADebugViewTool::EWorkflowPresetSource::Default
							? EVisibility::Visible
							: EVisibility::Collapsed;
					})
					.OnClicked_Lambda([this]()
					{
						if (EditingWorkflowSource == TADebugViewTool::EWorkflowPresetSource::Modified)
						{
							ResetEditedWorkflowToDefault();
						}
						else
						{
							DeleteEditedCustomPreset();
						}
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNullWidget::NullWidget
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CancelWorkflowEditButton", "Cancel"))
					.OnClicked_Lambda([this]()
					{
						CancelWorkflowEditing();
						return FReply::Handled();
					})
				]
				// Deliberately always enabled: SaveEditedCustomPreset reports why a save
				// was rejected inline, which a disabled button cannot do.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("SaveWorkflowButton", "Save Workflow"))
					.ButtonColorAndOpacity(FLinearColor::FromSRGBColor(FColor(0, 112, 224)))
					.OnClicked_Lambda([this]()
					{
						SaveEditedCustomPreset();
						return FReply::Handled();
					})
				]
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeEditorTextField(
	const FText& Label,
	TAttribute<FText> Text,
	const FOnTextChanged& OnTextChanged,
	bool bMultiLine)
{
	TSharedRef<SVerticalBox> Field = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
		];

	if (bMultiLine)
	{
		Field->AddSlot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(54.0f)
				[
					SNew(SMultiLineEditableTextBox)
					.Text(Text)
					.OnTextChanged(OnTextChanged)
					.AutoWrapText(true)
				]
			];
	}
	else
	{
		Field->AddSlot()
			.AutoHeight()
			[
				SNew(SEditableTextBox)
				.Text(Text)
				.OnTextChanged(OnTextChanged)
			];
	}

	return Field;
}

TSharedRef<SWidget> STADebugViewPanel::MakeActionListEditor(
	const FText& Label,
	TArray<FTADebugViewCustomAction>& Actions,
	TSharedPtr<SVerticalBox>& ActionListBox,
	ECustomActionListKind ActionListKind)
{
	TSharedRef<SVerticalBox> ActionList = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeHorizontalSeparator()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 10.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
		];

	ActionList->AddSlot()
		.AutoHeight()
		[
			SAssignNew(ActionListBox, SVerticalBox)
		];

	ActionList->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("AddActionButton", "Add Action"))
			.OnClicked_Lambda([this, &Actions, ActionListBox, ActionListKind]()
			{
				Actions.Add(FTADebugViewCustomAction(ETADebugViewCustomActionType::ViewMode, TEXT("VMI_Lit")));
				RebuildActionListEditor(Actions, ActionListBox, ActionListKind);
				return FReply::Handled();
			})
		];

	RebuildActionListEditor(Actions, ActionListBox, ActionListKind);

	return ActionList;
}

TSharedRef<SWidget> STADebugViewPanel::MakeActionRow(TArray<FTADebugViewCustomAction>& Actions, int32 ActionIndex, ECustomActionListKind ActionListKind)
{
	return SNew(SBorder)
		.BorderImage(GetInsetPanelBrush())
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.36f)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				MakeActionTypeCombo(Actions, ActionIndex, ActionListKind)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.54f)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				MakeActionValueWidget(Actions, ActionIndex)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveActionButton", "-"))
				.OnClicked_Lambda([this, &Actions, ActionIndex]()
				{
					if (Actions.IsValidIndex(ActionIndex))
					{
						Actions.RemoveAt(ActionIndex);
						RebuildEditedActionLists();
					}

					return FReply::Handled();
				})
			]
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeActionTypeCombo(TArray<FTADebugViewCustomAction>& Actions, int32 ActionIndex, ECustomActionListKind ActionListKind)
{
	return SNew(SComboBox<TSharedPtr<FActionOption>>)
		.OptionsSource(&GetActionTypeOptions())
		.InitiallySelectedItem(FindActionTypeOption(Actions.IsValidIndex(ActionIndex) ? Actions[ActionIndex].ActionType : ETADebugViewCustomActionType::ViewMode))
		.OnGenerateWidget_Lambda([](TSharedPtr<FActionOption> Option)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(Option.IsValid() ? Option->Label : FString()));
		})
		.OnSelectionChanged_Lambda([this, &Actions, ActionIndex, ActionListKind](TSharedPtr<FActionOption> SelectedOption, ESelectInfo::Type)
		{
			if (SelectedOption.IsValid() && Actions.IsValidIndex(ActionIndex))
			{
				Actions[ActionIndex].ActionType = SelectedOption->ActionType;
				Actions[ActionIndex].Value = GetDefaultValueForActionType(SelectedOption->ActionType);
				// Rebuild so the value cell switches between a preset combo and the advanced command field.
				// This is cheap because preset action lists are intentionally small.
				RebuildActionListEditor(
					Actions,
					ActionListKind == ECustomActionListKind::Activate ? ActivateActionListBox : DeactivateActionListBox,
					ActionListKind);
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([&Actions, ActionIndex]()
			{
				if (!Actions.IsValidIndex(ActionIndex))
				{
					return FText::GetEmpty();
				}

				return FText::FromString(FindActionTypeOption(Actions[ActionIndex].ActionType)->Label);
			})
		];
}

TSharedRef<SWidget> STADebugViewPanel::MakeActionValueWidget(TArray<FTADebugViewCustomAction>& Actions, int32 ActionIndex)
{
	if (!Actions.IsValidIndex(ActionIndex))
	{
		return SNew(STextBlock)
			.Text(FText::GetEmpty());
	}

	if (Actions[ActionIndex].ActionType == ETADebugViewCustomActionType::Command)
	{
		return SNew(SEditableTextBox)
			.Text_Lambda([&Actions, ActionIndex]()
			{
				return Actions.IsValidIndex(ActionIndex) ? FText::FromString(Actions[ActionIndex].Value) : FText::GetEmpty();
			})
			.OnTextChanged_Lambda([&Actions, ActionIndex](const FText& NewText)
			{
				if (Actions.IsValidIndex(ActionIndex))
				{
					Actions[ActionIndex].Value = NewText.ToString();
				}
			})
			.OnTextCommitted_Lambda([&Actions, ActionIndex](const FText& NewText, ETextCommit::Type)
			{
				if (Actions.IsValidIndex(ActionIndex))
				{
					Actions[ActionIndex].Value = NewText.ToString();
				}
			});
	}

	return SNew(SComboBox<TSharedPtr<FActionValueOption>>)
		.OptionsSource(&GetValueOptions(Actions[ActionIndex].ActionType))
		.InitiallySelectedItem(FindActionValueOption(Actions[ActionIndex].ActionType, Actions[ActionIndex].Value))
		.OnGenerateWidget_Lambda([](TSharedPtr<FActionValueOption> Option)
		{
			return SNew(STextBlock)
				.Text(FText::FromString(Option.IsValid() ? Option->Label : FString()));
		})
		.OnSelectionChanged_Lambda([&Actions, ActionIndex](TSharedPtr<FActionValueOption> SelectedOption, ESelectInfo::Type)
		{
			if (SelectedOption.IsValid() && Actions.IsValidIndex(ActionIndex))
			{
				Actions[ActionIndex].Value = SelectedOption->Value;
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([&Actions, ActionIndex]()
			{
				if (!Actions.IsValidIndex(ActionIndex))
				{
					return FText::GetEmpty();
				}

				return FText::FromString(FindActionValueOption(Actions[ActionIndex].ActionType, Actions[ActionIndex].Value)->Label);
			})
		];
}

void STADebugViewPanel::RebuildActionListEditor(
	TArray<FTADebugViewCustomAction>& Actions,
	const TSharedPtr<SVerticalBox>& ActionListBox,
	ECustomActionListKind ActionListKind)
{
	if (!ActionListBox.IsValid())
	{
		return;
	}

	ActionListBox->ClearChildren();

	if (Actions.IsEmpty())
	{
		ActionListBox->AddSlot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoActions", "No actions."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		return;
	}

	for (int32 ActionIndex = 0; ActionIndex < Actions.Num(); ++ActionIndex)
	{
		ActionListBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 1.0f, 0.0f, 1.0f)
			[
				MakeActionRow(Actions, ActionIndex, ActionListKind)
			];
	}
}

void STADebugViewPanel::RebuildEditedActionLists()
{
	RebuildActionListEditor(EditingActivateActions, ActivateActionListBox, ECustomActionListKind::Activate);
	RebuildActionListEditor(EditingDeactivateActions, DeactivateActionListBox, ECustomActionListKind::Deactivate);
}

STADebugViewPanel::EPanelPage STADebugViewPanel::MigrateSavedPanelPage(uint8 SavedPanelPageValue, bool bAlreadyMigrated)
{
	constexpr uint8 PanelPageCount = static_cast<uint8>(EPanelPage::Count);

	if (bAlreadyMigrated)
	{
		return SavedPanelPageValue < PanelPageCount
			? static_cast<EPanelPage>(SavedPanelPageValue)
			: EPanelPage::Workflows;
	}

	// Pre-unification order was: Workflows, DebugViews, PresetEditor, Advanced, Help, Diagnostics.
	switch (SavedPanelPageValue)
	{
	case 0:
		return EPanelPage::Workflows;
	case 1:
		return EPanelPage::DebugViews;
	// PresetEditor is gone. Workflow editing now happens in the Workflows page inspector.
	case 2:
		return EPanelPage::Workflows;
	case 3:
		return EPanelPage::Advanced;
	case 4:
		return EPanelPage::Help;
	case 5:
		return EPanelPage::Diagnostics;
	default:
		return EPanelPage::Workflows;
	}
}

void STADebugViewPanel::LoadUserPreferences()
{
	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
	if (!Settings)
	{
		return;
	}

	ActivePage = MigrateSavedPanelPage(Settings->LastPanelPage, Settings->bHasMigratedPanelPageV2);
	if (!Settings->bHasMigratedPanelPageV2)
	{
		// Persist the remapped value so the migration only runs once.
		if (UTADebugViewCustomPresetSettings* MutableSettings = UTADebugViewCustomPresetSettings::GetMutable())
		{
			MutableSettings->LastPanelPage = static_cast<uint8>(ActivePage);
			MutableSettings->bHasMigratedPanelPageV2 = true;
			MutableSettings->SaveUserSettings();
		}
	}

	if (!Settings->LastDebugGroupId.IsEmpty())
	{
		SelectedDebugGroupId = FName(*Settings->LastDebugGroupId);
	}

	const bool bDebugGroupIsValid = TADebugViewTool::GetPresetGroups().ContainsByPredicate([this](const TADebugViewTool::FDebugViewGroup& Group)
	{
		return Group.Id == SelectedDebugGroupId;
	});

	if (!bDebugGroupIsValid && TADebugViewTool::GetPresetGroups().Num() > 0)
	{
		SelectedDebugGroupId = TADebugViewTool::GetPresetGroups()[0].Id;
	}

	if (Executor && IsValidViewportTargetValue(Settings->LastViewportTarget))
	{
		Executor->SetViewportTarget(static_cast<TADebugViewTool::EDebugViewportTarget>(Settings->LastViewportTarget));
	}
}

void STADebugViewPanel::SaveUserPreferences() const
{
	if (UTADebugViewCustomPresetSettings* Settings = UTADebugViewCustomPresetSettings::GetMutable())
	{
		Settings->LastPanelPage = static_cast<uint8>(ActivePage);
		Settings->LastDebugGroupId = SelectedDebugGroupId.ToString();
		Settings->LastViewportTarget = Executor ? static_cast<uint8>(Executor->GetViewportTarget()) : Settings->LastViewportTarget;
		Settings->SaveUserSettings();
	}
}

void STADebugViewPanel::SavePanelPagePreference() const
{
	if (UTADebugViewCustomPresetSettings* Settings = UTADebugViewCustomPresetSettings::GetMutable())
	{
		Settings->LastPanelPage = static_cast<uint8>(ActivePage);
		Settings->SaveUserSettings();
	}
}

void STADebugViewPanel::SaveDebugGroupPreference() const
{
	if (UTADebugViewCustomPresetSettings* Settings = UTADebugViewCustomPresetSettings::GetMutable())
	{
		Settings->LastDebugGroupId = SelectedDebugGroupId.ToString();
		Settings->SaveUserSettings();
	}
}

void STADebugViewPanel::SaveViewportTargetPreference() const
{
	if (UTADebugViewCustomPresetSettings* Settings = UTADebugViewCustomPresetSettings::GetMutable())
	{
		Settings->LastViewportTarget = Executor ? static_cast<uint8>(Executor->GetViewportTarget()) : Settings->LastViewportTarget;
		Settings->SaveUserSettings();
	}
}

void STADebugViewPanel::ExecuteWorkflowPresetFromPanel(TADebugViewTool::FWorkflowPreset WorkflowPreset)
{
	if (Executor)
	{
		TADebugViewTool::ExecuteWorkflowPresetAction(WorkflowPreset, *Executor);
		RebuildQuickAccess();
	}
}

void STADebugViewPanel::ExecuteDebugPresetFromPanel(TADebugViewTool::FDebugViewPreset Preset)
{
	if (Executor)
	{
		TADebugViewTool::ExecuteDebugPresetAction(Preset, *Executor);
		RebuildQuickAccess();
	}
}

void STADebugViewPanel::ExecuteQuickAction(const FTADebugViewQuickAction& QuickAction)
{
	if (!Executor)
	{
		return;
	}

	if (!TADebugViewTool::ExecuteQuickAction(QuickAction, *Executor))
	{
		RemoveStaleQuickActions();
		return;
	}

	RebuildQuickAccess();
}

void STADebugViewPanel::ToggleFavoriteAction(const FTADebugViewQuickAction& QuickAction)
{
	if (!QuickAction.IsValid())
	{
		return;
	}

	UTADebugViewCustomPresetSettings* Settings = UTADebugViewCustomPresetSettings::GetMutable();
	if (!Settings)
	{
		return;
	}

	const int32 RemovedCount = Settings->FavoriteActions.RemoveAll([&QuickAction](const FTADebugViewQuickAction& ExistingAction)
	{
		return ExistingAction.Matches(QuickAction);
	});

	if (RemovedCount == 0)
	{
		Settings->FavoriteActions.Add(QuickAction);
	}

	Settings->SaveUserSettings();
	RebuildQuickAccess();
	RebuildSearchResults();
	// The inspector shows the bound chord for the selected workflow, and pinning or
	// unpinning an earlier favorite shifts which chord every later entry gets.
	RebuildContextInspector();
	// Favorites feed the stale-reference check.
	RefreshDiagnosticsCache();
}

bool STADebugViewPanel::IsFavoriteAction(const FTADebugViewQuickAction& QuickAction) const
{
	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
	if (!Settings)
	{
		return false;
	}

	return Settings->FavoriteActions.ContainsByPredicate([&QuickAction](const FTADebugViewQuickAction& ExistingAction)
	{
		return ExistingAction.Matches(QuickAction);
	});
}

bool STADebugViewPanel::IsQuickActionActiveCached(const FTADebugViewQuickAction& QuickAction) const
{
	if (!Executor)
	{
		return false;
	}
	if (QuickAction.ActionType == ETADebugViewQuickActionType::DebugPreset)
	{
		return CachedActiveDebugPresetIds.Contains(FName(*QuickAction.Id));
	}
	return Executor->GetActiveWorkflowPresetId() == FName(*QuickAction.Id);
}

bool STADebugViewPanel::IsDebugPresetActiveCached(FName PresetId) const
{
	return CachedActiveDebugPresetIds.Contains(PresetId);
}

void STADebugViewPanel::RemoveStaleQuickActions()
{
	TADebugViewTool::RemoveStaleQuickActions();
	RebuildQuickAccess();
}

void STADebugViewPanel::OnSearchTextChanged(const FText& Text)
{
	SearchText = Text.ToString().TrimStartAndEnd();
	RebuildSearchResults();
	if (SearchResultsAnchor.IsValid())
	{
		const bool bShouldOpen = !SearchText.IsEmpty();
		if (SearchResultsAnchor->IsOpen() != bShouldOpen)
		{
			// Keep typing focus in the search field while the result popup opens.
			SearchResultsAnchor->SetIsOpen(bShouldOpen, false);
		}
	}
}

void STADebugViewPanel::OnFocusChanging(
	const FWeakWidgetPath& PreviousFocusPath,
	const FWidgetPath& NewWidgetPath,
	const FFocusEvent& InFocusEvent)
{
	SCompoundWidget::OnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);

	if (!SearchBox.IsValid())
	{
		return;
	}

	const bool bSearchPreviouslyFocused = PreviousFocusPath.ContainsWidget(SearchBox.Get());
	const bool bSearchNowFocused = NewWidgetPath.ContainsWidget(SearchBox.Get());
	const bool bResultsNowFocused = SearchResultsBox.IsValid()
		&& NewWidgetPath.ContainsWidget(SearchResultsBox.Get());
	if (bSearchPreviouslyFocused && !bSearchNowFocused && !bResultsNowFocused)
	{
		if (SearchResultsAnchor.IsValid() && SearchResultsAnchor->IsOpen())
		{
			SearchResultsAnchor->SetIsOpen(false, false);
		}
		return;
	}

	if (!bSearchPreviouslyFocused && bSearchNowFocused)
	{
		// A preserved query must never have invisible, stale results. Refresh and
		// reopen the command palette whenever focus returns to the search field.
		RebuildSearchResults();
		if (!SearchText.IsEmpty()
			&& SearchResultsAnchor.IsValid()
			&& !SearchResultsAnchor->IsOpen())
		{
			SearchResultsAnchor->SetIsOpen(true, false);
		}
	}
}

FReply STADebugViewPanel::OnPreviewMouseButtonDown(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (SearchBox.IsValid() && SearchResultsAnchor.IsValid() && !SearchText.IsEmpty())
	{
		const FVector2D ScreenPosition = MouseEvent.GetScreenSpacePosition();
		const bool bClickedSearchBox =
			SearchBox->GetCachedGeometry().IsUnderLocation(ScreenPosition);
		const bool bClickedSearchResults = SearchResultsBox.IsValid()
			&& SearchResultsBox->GetCachedGeometry().IsUnderLocation(ScreenPosition);
		if (bClickedSearchBox)
		{
			// A mouse click does not produce another focus-change event when the
			// retained search field is already focused. Refresh and reopen here
			// so one click is always sufficient.
			RebuildSearchResults();
			if (!SearchResultsAnchor->IsOpen())
			{
				SearchResultsAnchor->SetIsOpen(true, false);
			}
		}
		else if (!bClickedSearchResults && SearchResultsAnchor->IsOpen())
		{
			// Non-focusable panel backgrounds never trigger OnFocusChanging, so
			// close the palette from the preview phase before child widgets handle
			// the click. Result clicks are deliberately excluded so their button
			// receives the original mouse event and executes on the first click.
			SearchResultsAnchor->SetIsOpen(false, false);
		}
	}

	return SCompoundWidget::OnPreviewMouseButtonDown(MyGeometry, MouseEvent);
}

void STADebugViewPanel::OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	SearchText = Text.ToString().TrimStartAndEnd();
	if (CommitType != ETextCommit::OnEnter)
	{
		// Clicking a result moves focus away from the editable text box and
		// commits with OnUserMovedFocus. Rebuilding here would destroy the button
		// that received mouse-down before it can receive mouse-up, forcing a
		// second click. OnTextChanged already keeps these results current.
		return;
	}

	RebuildSearchResults();
	if (SearchResultsAnchor.IsValid()
		&& SearchResultsAnchor->IsOpen()
		&& !FilteredSearchActions.IsEmpty())
	{
		ExecuteQuickAction(FilteredSearchActions[0]);
	}
}

void STADebugViewPanel::RebuildSearchResults()
{
	FilteredSearchActions.Reset();
	if (!SearchResultsBox.IsValid())
	{
		return;
	}
	SearchResultsBox->ClearChildren();
	if (SearchText.IsEmpty())
	{
		return;
	}

	const FString Query = SearchText.ToLower();
	struct FSearchEntry
	{
		FTADebugViewQuickAction Action;
		FText Label;
		FText Tooltip;
		FText TypeLabel;
		FString SearchableText;
		int32 MatchScore = MAX_int32;
		ESearchCategory Category = ESearchCategory::All;
	};
	TArray<FSearchEntry> Entries;

	for (const TADebugViewTool::FDebugViewGroup& Group : TADebugViewTool::GetPresetGroups())
	{
		for (const TADebugViewTool::FDebugViewPreset& Preset : Group.Presets)
		{
			FSearchEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Action = FTADebugViewQuickAction(ETADebugViewQuickActionType::DebugPreset, Preset.Id.ToString());
			Entry.Label = Preset.Label;
			Entry.Tooltip = Preset.Tooltip;
			const bool bAdvancedPreset = IsAdvancedPagePreset(Preset);
			Entry.Category = bAdvancedPreset ? ESearchCategory::Advanced : ESearchCategory::DebugViews;
			Entry.TypeLabel = Group.Label;
			const FString PageSearchTerms = bAdvancedPreset
				? TEXT("advanced utility command")
				: TEXT("debug view debug views visualization");
			Entry.SearchableText = (
				Preset.Label.ToString()
				+ TEXT(" ") + Preset.Tooltip.ToString()
				+ TEXT(" ") + Entry.TypeLabel.ToString()
				+ TEXT(" ") + PageSearchTerms
				+ TEXT(" ") + Preset.Id.ToString()
				+ TEXT(" ") + Preset.VisualizationMode.ToString()
				+ TEXT(" ") + Preset.Commands).ToLower();
		}
	}

	auto AddWorkflowEntries = [&Entries](const TArray<TADebugViewTool::FWorkflowPreset>& Presets)
	{
		for (const TADebugViewTool::FWorkflowPreset& Preset : Presets)
		{
			FSearchEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Action = FTADebugViewQuickAction(ETADebugViewQuickActionType::WorkflowPreset, Preset.Id.ToString());
			Entry.Label = Preset.Label;
			Entry.Tooltip = Preset.Tooltip;
			Entry.TypeLabel = TADebugViewTool::GetWorkflowSourceLabel(Preset.Source);
			Entry.Category = ESearchCategory::Workflows;

			FString ActionSearchTerms;
			auto AppendActions = [&ActionSearchTerms](const TArray<TADebugViewTool::FDebugViewAction>& Actions)
			{
				for (const TADebugViewTool::FDebugViewAction& Action : Actions)
				{
					ActionSearchTerms += TEXT(" ");
					ActionSearchTerms += GetWorkflowActionDisplayText(Action).ToString();
				}
			};
			AppendActions(Preset.ActivateActions);
			AppendActions(Preset.DeactivateActions);

			Entry.SearchableText = (
				Preset.Label.ToString()
				+ TEXT(" ") + Preset.Tooltip.ToString()
				+ TEXT(" ") + Entry.TypeLabel.ToString()
				+ TEXT(" workflow workflows preset")
				+ TEXT(" ") + Preset.Id.ToString()
				+ ActionSearchTerms).ToLower();
		}
	};
	AddWorkflowEntries(TADebugViewTool::GetEffectiveWorkflowPresets());

	Entries.RemoveAll([this, &Query](const FSearchEntry& Entry)
	{
		const bool bMatchesCategory = SearchCategory == ESearchCategory::All || SearchCategory == Entry.Category;
		return !bMatchesCategory || !Entry.SearchableText.Contains(Query);
	});

	for (FSearchEntry& Entry : Entries)
	{
		const FString Label = Entry.Label.ToString().ToLower();
		const FString TypeLabel = Entry.TypeLabel.ToString().ToLower();
		if (Label.Equals(Query))
		{
			Entry.MatchScore = 0;
		}
		else if (Label.StartsWith(Query))
		{
			Entry.MatchScore = 10;
		}
		else if (Label.Contains(Query))
		{
			Entry.MatchScore = 20;
		}
		else if (TypeLabel.Contains(Query))
		{
			Entry.MatchScore = 30;
		}
		else
		{
			Entry.MatchScore = 40;
		}
	}
	Entries.StableSort([](const FSearchEntry& Left, const FSearchEntry& Right)
	{
		return Left.MatchScore < Right.MatchScore;
	});

	const int32 MaxResultsPerCategory = SearchCategory == ESearchCategory::All ? 8 : 24;
	auto AddCategorySection = [this, &Entries, MaxResultsPerCategory](
		ESearchCategory Category,
		const FText& Heading)
	{
		int32 MatchingCount = 0;
		for (const FSearchEntry& Entry : Entries)
		{
			if (Entry.Category == Category)
			{
				++MatchingCount;
			}
		}
		if (MatchingCount == 0)
		{
			return;
		}

		if (!FilteredSearchActions.IsEmpty())
		{
			SearchResultsBox->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 5.0f, 0.0f, 3.0f)
				[
					MakeHorizontalSeparator()
				];
		}

		SearchResultsBox->AddSlot()
			.AutoHeight()
			.Padding(8.0f, 4.0f, 8.0f, 3.0f)
			[
				SNew(STextBlock)
					.Text(FText::Format(
						LOCTEXT("SearchSectionHeadingFormat", "{0} ({1})"),
						Heading,
						FText::AsNumber(MatchingCount)))
					.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];

		int32 DisplayedCount = 0;
		for (const FSearchEntry& Entry : Entries)
		{
			if (Entry.Category != Category || DisplayedCount >= MaxResultsPerCategory)
			{
				continue;
			}

			FilteredSearchActions.Add(Entry.Action);
			SearchResultsBox->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SButton)
						.ContentPadding(FMargin(10.0f, 7.0f))
						.ToolTipText(Entry.Tooltip)
						.OnClicked_Lambda([this, Action = Entry.Action]()
						{
							ExecuteQuickAction(Action);
							return FReply::Handled();
						})
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
										.Text(Entry.Label)
										.Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
										.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(0.0f, 2.0f, 0.0f, 0.0f)
								[
									SNew(STextBlock)
										.Text(Entry.Tooltip)
										.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
										.ColorAndOpacity(FSlateColor::UseSubduedForeground())
										.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
								]
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(10.0f, 0.0f)
							[
								SNew(STextBlock)
									.Text(Entry.TypeLabel)
									.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
									.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							]
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(3.0f, 0.0f, 0.0f, 0.0f)
					[
						MakeFavoriteToggleButton(Entry.Action)
					]
				];
			++DisplayedCount;
		}

		if (DisplayedCount < MatchingCount)
		{
			SearchResultsBox->AddSlot()
				.AutoHeight()
				.Padding(8.0f, 4.0f, 8.0f, 2.0f)
				[
					SNew(STextBlock)
						.Text(FText::Format(
							LOCTEXT("SearchMoreResults", "{0} more result(s). Use the category filter to narrow the list."),
							FText::AsNumber(MatchingCount - DisplayedCount)))
						.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				];
		}
	};

	AddCategorySection(ESearchCategory::Workflows, LOCTEXT("SearchSectionWorkflows", "Workflows"));
	AddCategorySection(ESearchCategory::DebugViews, LOCTEXT("SearchSectionDebugViews", "Debug Views"));
	AddCategorySection(ESearchCategory::Advanced, LOCTEXT("SearchSectionAdvanced", "Advanced"));

	if (FilteredSearchActions.IsEmpty())
	{
		SearchResultsBox->AddSlot().AutoHeight().Padding(2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoSearchResults", "No matching actions."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
	}
}

void STADebugViewPanel::RefreshAllPresetUI()
{
	RebuildCustomPresetButtons();
	RebuildContextInspector();
	RebuildQuickAccess();
	RebuildSearchResults();
	RebuildDebugGroupList();
	RebuildDebugPresetButtons();
	RebuildDiagnostics();
}

void STADebugViewPanel::RebuildDiagnostics()
{
	// The status chip and nav badge read the cached result rather than re-running the
	// checks from their per-frame attribute lambdas.
	RefreshDiagnosticsCache();

	if (!DiagnosticsBox.IsValid())
	{
		return;
	}
	DiagnosticsBox->ClearChildren();

	// The named checks always render, so a healthy project still shows the full
	// checklist instead of a single "nothing found" line.
	for (const TADebugViewTool::FPresetDiagnosticCheck& Check : TADebugViewTool::RunPresetDiagnosticChecks())
	{
		DiagnosticsBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SBorder)
			.BorderImage(GetInsetPanelBrush())
			.Padding(FMargin(12.0f, 10.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SImage)
						.Image(FAppStyle::GetBrush(Check.bPassed ? TEXT("Icons.SuccessWithColor") : TEXT("Icons.WarningWithColor")))
						.ColorAndOpacity(TADebugViewTool::GetDiagnosticCheckStatusColor(Check))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.28f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(Check.Label)
						.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.58f)
				.VAlign(VAlign_Center)
				.Padding(10.0f, 0.0f)
				[
					SNew(STextBlock)
						.Text(Check.Detail)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.14f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
						.Text(TADebugViewTool::GetDiagnosticCheckStatusLabel(Check))
						.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
						.ColorAndOpacity(TADebugViewTool::GetDiagnosticCheckStatusColor(Check))
				]
			]
		];
	}

	// Detailed per-preset issues are appended below the checklist so the specific
	// offending preset and message are still reachable.
	const TArray<TADebugViewTool::FPresetDiagnosticIssue> Issues = TADebugViewTool::RunPresetDiagnostics();
	if (Issues.IsEmpty())
	{
		return;
	}

	DiagnosticsBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
	[
		SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("DiagnosticsIssueCount", "Details · {0} issue(s)"), FText::AsNumber(Issues.Num())))
		.Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
	];
	for (const TADebugViewTool::FPresetDiagnosticIssue& Issue : Issues)
	{
		DiagnosticsBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SBorder)
			.BorderImage(GetInsetPanelBrush())
			.Padding(6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(TADebugViewTool::GetDiagnosticSeverityLabel(Issue.Severity))
					.ColorAndOpacity(TADebugViewTool::GetDiagnosticSeverityColor(Issue.Severity))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Issue.PresetLabel)).Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(STextBlock).Text(FText::FromString(Issue.Message)).AutoWrapText(true)
				]
			]
		];
	}
}

void STADebugViewPanel::RefreshStatusCache()
{
	const FText UnavailableStatus = LOCTEXT("StatusUnavailable", "Unavailable");
	CachedTargetStatus = Executor ? Executor->GetViewportTargetStatusText() : UnavailableStatus;
	CachedViewModeStatus = Executor ? Executor->GetViewModeStatusText() : UnavailableStatus;
	CachedVisualizationStatus = Executor ? Executor->GetVisualizationStatusText() : UnavailableStatus;
	CachedWorkflowStatus = GetActiveWorkflowStatusText();
	CachedActiveDebugPresetIds = Executor ? Executor->GetActiveDebugPresetIds() : TSet<FName>();
	// CachedDiagnosticsStatus is deliberately not refreshed here. This runs on a
	// 0.25s timer, and the diagnostic checks only change when presets, favorites,
	// or overrides change, so it is refreshed from RebuildDiagnostics instead.
}

EActiveTimerReturnType STADebugViewPanel::UpdateStatusCache(double CurrentTime, float DeltaTime)
{
	RefreshStatusCache();
	return EActiveTimerReturnType::Continue;
}

bool STADebugViewPanel::ChangeQuickAccessPage(int32 PageDelta)
{
	if (PageDelta == 0)
	{
		return false;
	}

	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
	if (!Settings)
	{
		return false;
	}

	int32 ValidFavoriteCount = 0;
	for (const FTADebugViewQuickAction& QuickAction : Settings->FavoriteActions)
	{
		TOptional<TADebugViewTool::FDebugViewPreset> DebugPreset;
		TOptional<TADebugViewTool::FWorkflowPreset> WorkflowPreset;
		if (TADebugViewTool::ResolveQuickAction(QuickAction, DebugPreset, WorkflowPreset))
		{
			++ValidFavoriteCount;
		}
	}

	const int32 FavoritesPerPage = TADebugViewTool::FavoriteShortcutCount;
	const int32 PageCount = FMath::Max(
		1,
		FMath::DivideAndRoundUp(ValidFavoriteCount, FavoritesPerPage));
	const int32 NewPageIndex = FMath::Clamp(
		QuickAccessPageIndex + PageDelta,
		0,
		PageCount - 1);
	if (NewPageIndex == QuickAccessPageIndex)
	{
		return false;
	}

	QuickAccessPageIndex = NewPageIndex;
	RebuildQuickAccess();
	return true;
}

void STADebugViewPanel::RebuildQuickAccess()
{
	if (!QuickAccessBox.IsValid())
	{
		return;
	}

	if (QuickAccessFavoritesButton.IsValid())
	{
		QuickAccessFavoritesButton->SetIsOpen(false);
		QuickAccessFavoritesButton.Reset();
	}
	QuickAccessBox->ClearChildren();
	if (QuickAccessPageDotsBox.IsValid())
	{
		QuickAccessPageDotsBox->ClearChildren();
	}

	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
	if (!Settings)
	{
		return;
	}

	TArray<TPair<int32, FTADebugViewQuickAction>> ValidFavorites;
	for (int32 ActionIndex = 0; ActionIndex < Settings->FavoriteActions.Num(); ++ActionIndex)
	{
		const FTADebugViewQuickAction& QuickAction = Settings->FavoriteActions[ActionIndex];
		TOptional<TADebugViewTool::FDebugViewPreset> DebugPreset;
		TOptional<TADebugViewTool::FWorkflowPreset> WorkflowPreset;
		if (TADebugViewTool::ResolveQuickAction(QuickAction, DebugPreset, WorkflowPreset))
		{
			ValidFavorites.Emplace(ActionIndex, QuickAction);
		}
	}

	if (ValidFavorites.IsEmpty())
	{
		return;
	}

	const int32 FavoritesPerPage = TADebugViewTool::FavoriteShortcutCount;
	const int32 PageCount = FMath::DivideAndRoundUp(ValidFavorites.Num(), FavoritesPerPage);
	QuickAccessPageIndex = FMath::Clamp(QuickAccessPageIndex, 0, PageCount - 1);
	const int32 PageStartIndex = QuickAccessPageIndex * FavoritesPerPage;
	const int32 PageEndIndex = FMath::Min(PageStartIndex + FavoritesPerPage, ValidFavorites.Num());
	const int32 FavoritesOnCurrentPage = PageEndIndex - PageStartIndex;

	if (QuickAccessPageDotsBox.IsValid())
	{
		for (int32 PageIndex = 0; PageIndex < PageCount; ++PageIndex)
		{
			const bool bIsCurrentPage = PageIndex == QuickAccessPageIndex;
			QuickAccessPageDotsBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), TEXT("NoBorder"))
					.ContentPadding(FMargin(3.0f, 2.0f))
					.ToolTipText(FText::Format(
						LOCTEXT("FavoritesPageDotTooltip", "Show favorites page {0} of {1}."),
						FText::AsNumber(PageIndex + 1),
						FText::AsNumber(PageCount)))
					.OnClicked_Lambda([this, PageIndex]()
					{
						if (QuickAccessPageIndex != PageIndex)
						{
							QuickAccessPageIndex = PageIndex;
							RebuildQuickAccess();
						}
						return FReply::Handled();
					})
					[
						SNew(SBox)
						.WidthOverride(bIsCurrentPage ? 14.0f : 6.0f)
						.HeightOverride(6.0f)
						[
							SNew(SBorder)
							.BorderImage(GetQuickAccessPageDotBrush(bIsCurrentPage))
							.Padding(0.0f)
						]
					]
				];
		}
	}

	QuickAccessBox->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(160.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FavoritesHeading", "Favorites"))
					.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 1.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::Format(
						LOCTEXT("FavoritesPageStatus", "Page {0}/{1} · {2} per page"),
						FText::AsNumber(QuickAccessPageIndex + 1),
						FText::AsNumber(PageCount),
						FText::AsNumber(FavoritesPerPage)))
					.Font(FAppStyle::GetFontStyle(TEXT("MonoFont")))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];

	for (int32 ValidFavoriteIndex = PageStartIndex; ValidFavoriteIndex < PageEndIndex; ++ValidFavoriteIndex)
	{
		const int32 ActionIndex = ValidFavorites[ValidFavoriteIndex].Key;
		const FTADebugViewQuickAction& QuickAction = ValidFavorites[ValidFavoriteIndex].Value;
		// The displayed chord remains tied to the saved index, matching command execution.
		QuickAccessBox->AddSlot()
			.FillWidth(1.0f)
			.Padding(3.0f, 0.0f)
			[
				SNew(SBox)
				.HeightOverride(42.0f)
				[
					MakeQuickActionButton(QuickAction, ActionIndex)
				]
			];
	}

	for (int32 EmptySlotIndex = FavoritesOnCurrentPage; EmptySlotIndex < FavoritesPerPage; ++EmptySlotIndex)
	{
		QuickAccessBox->AddSlot()
			.FillWidth(1.0f)
			.Padding(3.0f, 0.0f)
			[
				SNullWidget::NullWidget
			];
	}

	QuickAccessBox->AddSlot()
		.AutoWidth()
		.Padding(6.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(124.0f)
			.HeightOverride(42.0f)
			[
				SAssignNew(QuickAccessFavoritesButton, SComboButton)
				.ContentPadding(FMargin(10.0f, 0.0f))
				.OnGetMenuContent(this, &STADebugViewPanel::MakeQuickAccessFavoritesMenu)
				.ToolTipText(FText::Format(
					LOCTEXT("AllFavoritesTooltip", "Open all {0} favorites."),
					FText::AsNumber(ValidFavorites.Num())))
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AllFavoritesStar", "★"))
						.ColorAndOpacity(FLinearColor(0.95f, 0.73f, 0.28f))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::Format(
							LOCTEXT("AllFavoritesLabel", "All ({0})"),
							FText::AsNumber(ValidFavorites.Num())))
						.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]
				]
			]
		];
}

void STADebugViewPanel::RebuildCustomPresetButtons()
{
	if (!CustomPresetListBox.IsValid())
	{
		return;
	}

	CustomPresetListBox->ClearChildren();

	TSharedRef<SUniformGridPanel> WorkflowGrid = SNew(SUniformGridPanel)
		.SlotPadding(FMargin(5.0f));

	const TArray<TADebugViewTool::FWorkflowPreset>& Workflows = TADebugViewTool::GetEffectiveWorkflowPresets();
	if (!SelectedWorkflowId.IsNone() && !TADebugViewTool::FindEffectiveWorkflowPreset(SelectedWorkflowId))
	{
		SelectedWorkflowId = Workflows.IsEmpty() ? NAME_None : Workflows[0].Id;
	}

	int32 VisibleWorkflowIndex = 0;
	for (const TADebugViewTool::FWorkflowPreset& WorkflowPreset : Workflows)
	{
		if (WorkflowPreset.Id == TEXT("TADebugWorkflow_ResetDebug") || !DoesWorkflowMatchFilter(WorkflowPreset))
		{
			continue;
		}

		const int32 Column = VisibleWorkflowIndex % 2;
		const int32 Row = VisibleWorkflowIndex / 2;
		WorkflowGrid->AddSlot(Column, Row)
			[
				SNew(SBox)
				.HeightOverride(128.0f)
				[
					MakeWorkflowPresetButton(WorkflowPreset)
				]
			];
		++VisibleWorkflowIndex;
	}

	if (VisibleWorkflowIndex == 0)
	{
		CustomPresetListBox->AddSlot()
			.AutoHeight()
			[
				SNew(SBorder)
					.BorderImage(GetInsetPanelBrush())
					.Padding(FMargin(14.0f, 18.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NoFilteredWorkflows", "No Workflows match this filter."))
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
			];
		return;
	}

	CustomPresetListBox->AddSlot()
		.AutoHeight()
		[
			WorkflowGrid
		];
}

void STADebugViewPanel::RebuildContextInspector()
{
	if (!ContextDetailsBox.IsValid())
	{
		return;
	}
	ContextDetailsBox->ClearChildren();

	if (bWorkflowEditorOpen)
	{
		ContextDetailsBox->AddSlot()
			.AutoHeight()
			[
				MakeCustomPresetEditor()
			];
		return;
	}

	const TADebugViewTool::FWorkflowPreset* WorkflowPreset = TADebugViewTool::FindEffectiveWorkflowPreset(SelectedWorkflowId);
	if (!WorkflowPreset)
	{
		for (const TADebugViewTool::FWorkflowPreset& Candidate : TADebugViewTool::GetEffectiveWorkflowPresets())
		{
			if (Candidate.Id != TEXT("TADebugWorkflow_ResetDebug"))
			{
				SelectedWorkflowId = Candidate.Id;
				WorkflowPreset = &Candidate;
				break;
			}
		}
	}

	if (WorkflowPreset)
	{
		ContextDetailsBox->AddSlot()
			.AutoHeight()
			[
				MakeWorkflowDetails(*WorkflowPreset)
			];
	}
	else
	{
		ContextDetailsBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("NoWorkflowSelected", "Select a Workflow to inspect its actions."))
					.AutoWrapText(true)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
	}
}

void STADebugViewPanel::RebuildDebugPresetButtons()
{
	if (!DebugPresetListBox.IsValid())
	{
		return;
	}

	DebugPresetListBox->ClearChildren();

	const TADebugViewTool::FDebugViewGroup* SelectedGroup = TADebugViewTool::GetPresetGroups().FindByPredicate([this](const TADebugViewTool::FDebugViewGroup& Group)
	{
		return Group.Id == SelectedDebugGroupId;
	});

	if (!SelectedGroup)
	{
		DebugPresetListBox->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoDebugGroupSelected", "Select a category."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
		return;
	}

	// Command presets live on the Advanced page, so count only what is listed here.
	int32 ListedPresetCount = 0;
	for (const TADebugViewTool::FDebugViewPreset& Preset : SelectedGroup->Presets)
	{
		if (!IsAdvancedPagePreset(Preset))
		{
			++ListedPresetCount;
		}
	}

	DebugPresetListBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				MakeSectionHeading(SelectedGroup->Label)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("GroupItemCount", "{0} items"), FText::AsNumber(ListedPresetCount)))
					.Font(FAppStyle::GetFontStyle(TEXT("SmallFont")))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];

	for (const TADebugViewTool::FDebugViewPreset& Preset : SelectedGroup->Presets)
	{
		if (IsAdvancedPagePreset(Preset))
		{
			continue;
		}

		DebugPresetListBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				MakeDebugViewRow(Preset)
			];
	}
}

void STADebugViewPanel::LoadCustomPresetForEditing(const FString& PresetId)
{
	if (PresetId.IsEmpty())
	{
		return;
	}

	const TADebugViewTool::FWorkflowPreset* Preset = TADebugViewTool::FindEffectiveWorkflowPreset(FName(*PresetId));
	if (!Preset)
	{
		return;
	}

	EditingPresetId = Preset->Id.ToString();
	EditingLabel = Preset->Label.ToString();
	EditingTooltip = Preset->Tooltip.ToString();
	EditingIconName = Preset->IconName;
	EditingWorkflowSource = Preset->Source;
	EditingActivateActions.Reset();
	EditingDeactivateActions.Reset();
	for (const TADebugViewTool::FDebugViewAction& RuntimeAction : Preset->ActivateActions)
	{
		FTADebugViewCustomAction SavedAction;
		if (TADebugViewTool::ConvertRuntimeActionToSavedAction(RuntimeAction, SavedAction))
		{
			EditingActivateActions.Add(MoveTemp(SavedAction));
		}
	}
	for (const TADebugViewTool::FDebugViewAction& RuntimeAction : Preset->DeactivateActions)
	{
		FTADebugViewCustomAction SavedAction;
		if (TADebugViewTool::ConvertRuntimeActionToSavedAction(RuntimeAction, SavedAction))
		{
			EditingDeactivateActions.Add(MoveTemp(SavedAction));
		}
	}
	bCreatingWorkflow = false;
	bWorkflowEditorOpen = true;
	EditedNameError = FText::GetEmpty();
	RebuildContextInspector();
}

void STADebugViewPanel::SelectWorkflow(FName WorkflowId)
{
	if (!TADebugViewTool::FindEffectiveWorkflowPreset(WorkflowId))
	{
		return;
	}
	SelectedWorkflowId = WorkflowId;
	bWorkflowEditorOpen = false;
	bCreatingWorkflow = false;
	RebuildCustomPresetButtons();
	RebuildContextInspector();
}

void STADebugViewPanel::AddNewCustomPreset()
{
	EditingPresetId.Reset();
	EditingLabel.Reset();
	EditingTooltip = TEXT("Project debug workflow.");
	EditingIconName = TEXT("Icons.Settings");
	EditingWorkflowSource = TADebugViewTool::EWorkflowPresetSource::UserCreated;
	EditingActivateActions = { FTADebugViewCustomAction(ETADebugViewCustomActionType::ViewMode, TEXT("VMI_Lit")) };
	EditingDeactivateActions = { FTADebugViewCustomAction(ETADebugViewCustomActionType::ViewMode, TEXT("VMI_Lit")) };
	bCreatingWorkflow = true;
	bWorkflowEditorOpen = true;
	EditedNameError = FText::GetEmpty();
	RebuildContextInspector();
}

void STADebugViewPanel::SaveEditedCustomPreset()
{
	EditedNameError = FText::GetEmpty();

	const FString TrimmedLabel = EditingLabel.TrimStartAndEnd();
	if (TrimmedLabel.IsEmpty())
	{
		EditedNameError = LOCTEXT("NameErrorEmpty", "Workflow Name cannot be empty.");
		return;
	}

	// The registry keys on stable Id, not name, so duplicate names are caught here
	// to keep the list, search, and palette unambiguous for the user.
	const FName EditingId = EditingPresetId.IsEmpty() ? NAME_None : FName(*EditingPresetId);
	for (const TADebugViewTool::FWorkflowPreset& Existing : TADebugViewTool::GetEffectiveWorkflowPresets())
	{
		if (Existing.Id == EditingId)
		{
			continue;
		}
		if (Existing.Label.ToString().Equals(TrimmedLabel, ESearchCase::IgnoreCase))
		{
			EditedNameError = LOCTEXT("NameErrorDuplicate", "A workflow with this name already exists.");
			return;
		}
	}

	if (EditingActivateActions.IsEmpty())
	{
		EditedNameError = LOCTEXT("NameErrorNoActivate", "Add at least one Activate Action.");
		return;
	}

	if (!CanSaveEditedCustomPreset())
	{
		EditedNameError = LOCTEXT("NameErrorInvalidAction", "One or more actions have an invalid value.");
		return;
	}

	FName SavedWorkflowId;
	FString Error;
	if (!TADebugViewTool::SaveWorkflowOverride(
		EditingPresetId.IsEmpty() ? NAME_None : FName(*EditingPresetId),
		EditingLabel,
		EditingTooltip,
		EditingIconName,
		EditingActivateActions,
		EditingDeactivateActions,
		SavedWorkflowId,
		Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
		return;
	}

	EditingPresetId = SavedWorkflowId.ToString();
	SelectedWorkflowId = SavedWorkflowId;
	bWorkflowEditorOpen = false;
	bCreatingWorkflow = false;
	EditedNameError = FText::GetEmpty();
	RefreshAllPresetUI();
}

void STADebugViewPanel::DeleteEditedCustomPreset()
{
	if (EditingPresetId.IsEmpty() || EditingWorkflowSource != TADebugViewTool::EWorkflowPresetSource::UserCreated)
	{
		return;
	}
	FString Error;
	if (!TADebugViewTool::DeleteUserWorkflow(FName(*EditingPresetId), Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
		return;
	}
	SelectedWorkflowId = NAME_None;
	CancelWorkflowEditing();
	RemoveStaleQuickActions();
	RefreshAllPresetUI();
}

void STADebugViewPanel::ResetEditedWorkflowToDefault()
{
	if (EditingPresetId.IsEmpty() || EditingWorkflowSource != TADebugViewTool::EWorkflowPresetSource::Modified)
	{
		return;
	}
	const FName WorkflowId(*EditingPresetId);
	FString Error;
	if (!TADebugViewTool::ResetWorkflowToDefault(WorkflowId, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Error));
		return;
	}
	SelectedWorkflowId = WorkflowId;
	CancelWorkflowEditing();
	RefreshAllPresetUI();
}

void STADebugViewPanel::CancelWorkflowEditing()
{
	bWorkflowEditorOpen = false;
	bCreatingWorkflow = false;
	EditingPresetId.Reset();
	EditingLabel.Reset();
	EditingTooltip.Reset();
	EditingActivateActions.Reset();
	EditingDeactivateActions.Reset();
	EditedNameError = FText::GetEmpty();
	RebuildContextInspector();
}

bool STADebugViewPanel::CanSaveEditedCustomPreset() const
{
	if (EditingLabel.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	if (EditingActivateActions.IsEmpty())
	{
		return false;
	}

	for (const FTADebugViewCustomAction& Action : EditingActivateActions)
	{
		if (!TADebugViewTool::IsWorkflowActionRuntimeValid(Action))
		{
			return false;
		}
	}
	for (const FTADebugViewCustomAction& Action : EditingDeactivateActions)
	{
		if (!TADebugViewTool::IsWorkflowActionRuntimeValid(Action))
		{
			return false;
		}
	}
	return true;
}

FText STADebugViewPanel::GetCustomPresetEditorTitle() const
{
	return bCreatingWorkflow
		? LOCTEXT("WorkflowNewTitle", "New Workflow")
		: LOCTEXT("WorkflowEditTitle", "Edit Workflow");
}

FText STADebugViewPanel::GetEditorSourceNoteText() const
{
	if (bCreatingWorkflow)
	{
		return LOCTEXT("EditNoteNew", "This workflow is saved to the project override JSON and appears in the unified list immediately.");
	}

	switch (EditingWorkflowSource)
	{
	case TADebugViewTool::EWorkflowPresetSource::Default:
		return LOCTEXT("EditNoteDefault", "Saving creates a project override with the same stable Id, so Favorites and shortcuts stay bound.");
	case TADebugViewTool::EWorkflowPresetSource::Modified:
		return LOCTEXT("EditNoteModified", "This is a project override of a plugin default. Reset to Default removes the override and restores the shipped actions.");
	default:
		return LOCTEXT("EditNoteUserCreated", "This is a project-level workflow. It can be edited or deleted outright.");
	}
}

FText STADebugViewPanel::GetEditedNameError() const
{
	return EditedNameError;
}

FText STADebugViewPanel::GetActiveWorkflowStatusText() const
{
	if (!Executor || Executor->GetActiveWorkflowPresetId().IsNone())
	{
		return LOCTEXT("NoActiveWorkflowStatus", "None");
	}

	const FName ActiveWorkflowId = Executor->GetActiveWorkflowPresetId();
	for (const TADebugViewTool::FWorkflowPreset& WorkflowPreset : TADebugViewTool::GetEffectiveWorkflowPresets())
	{
		if (WorkflowPreset.Id == ActiveWorkflowId)
		{
			return WorkflowPreset.Label;
		}
	}

	// The active workflow was deleted or renamed out from under the executor.
	return FText::FromName(ActiveWorkflowId);
}

FText STADebugViewPanel::GetHeaderWorkflowText() const
{
	if (!Executor || Executor->GetActiveWorkflowPresetId().IsNone())
	{
		return LOCTEXT("HeaderWorkflowIdle", "Idle · no workflow applied");
	}

	return FText::Format(LOCTEXT("HeaderWorkflowActive", "Active · {0}"), GetActiveWorkflowStatusText());
}

void STADebugViewPanel::RefreshDiagnosticsCache()
{
	CachedDiagnosticsFailureCount = 0;
	for (const TADebugViewTool::FPresetDiagnosticCheck& Check : TADebugViewTool::RunPresetDiagnosticChecks())
	{
		if (!Check.bPassed)
		{
			++CachedDiagnosticsFailureCount;
		}
	}

	CachedDiagnosticsStatus = CachedDiagnosticsFailureCount == 0
		? LOCTEXT("DiagnosticsStatusClean", "Clean")
		: FText::Format(LOCTEXT("DiagnosticsStatusIssues", "{0} issue(s)"), FText::AsNumber(CachedDiagnosticsFailureCount));
}

bool STADebugViewPanel::IsWorkflowActive(FName WorkflowId) const
{
	return Executor && !WorkflowId.IsNone() && Executor->GetActiveWorkflowPresetId() == WorkflowId;
}

FText STADebugViewPanel::GetQuickActionShortcutText(const FTADebugViewQuickAction& QuickAction) const
{
	const UTADebugViewCustomPresetSettings* Settings = GetDefault<UTADebugViewCustomPresetSettings>();
	if (!Settings)
	{
		return LOCTEXT("ShortcutUnbound", "Unbound");
	}

	// Chords are bound to the first FavoriteShortcutCount slots of the saved array.
	for (int32 FavoriteIndex = 0; FavoriteIndex < Settings->FavoriteActions.Num(); ++FavoriteIndex)
	{
		if (!Settings->FavoriteActions[FavoriteIndex].Matches(QuickAction))
		{
			continue;
		}

		return FavoriteIndex < TADebugViewTool::FavoriteShortcutCount
			? FText::Format(LOCTEXT("ShortcutBound", "Alt+Shift+{0}"), FText::AsNumber(FavoriteIndex + 1))
			: LOCTEXT("ShortcutUnbound", "Unbound");
	}

	return LOCTEXT("ShortcutUnbound", "Unbound");
}

#undef LOCTEXT_NAMESPACE
