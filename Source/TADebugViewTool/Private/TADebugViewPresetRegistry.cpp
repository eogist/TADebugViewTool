#include "TADebugViewPresetRegistry.h"

#define LOCTEXT_NAMESPACE "TADebugViewPresetRegistry"

namespace TADebugViewTool
{
const TArray<FDebugViewGroup>& GetPresetGroups()
{
	static const TArray<FDebugViewGroup> Groups = []()
	{
		TArray<FDebugViewGroup> Result;

		FDebugViewGroup ViewModes(TEXT("TADebugViewTool_ViewModes"), LOCTEXT("Group_ViewModes", "View Modes"));
		ViewModes.Presets.Emplace(TEXT("TADebugViewTool_Lit"), LOCTEXT("Preset_Lit", "Lit"), LOCTEXT("Preset_Lit_Tooltip", "Return the active editor viewport to Lit mode."), VMI_Lit, TEXT("LevelEditor.Tabs.Viewports"));
		ViewModes.Presets.Emplace(TEXT("TADebugViewTool_Unlit"), LOCTEXT("Preset_Unlit", "Unlit"), LOCTEXT("Preset_Unlit_Tooltip", "Inspect base color and emissive without lighting."), VMI_Unlit, TEXT("LevelEditor.Tabs.Viewports"));
		ViewModes.Presets.Emplace(TEXT("TADebugViewTool_Wireframe"), LOCTEXT("Preset_Wireframe", "Wireframe"), LOCTEXT("Preset_Wireframe_Tooltip", "Inspect mesh topology in wireframe view."), VMI_Wireframe, TEXT("LevelEditor.Tabs.Viewports"));
		ViewModes.Presets.Emplace(TEXT("TADebugViewTool_DetailLighting"), LOCTEXT("Preset_DetailLighting", "Detail Lighting"), LOCTEXT("Preset_DetailLighting_Tooltip", "Inspect normal maps and lighting without material color."), VMI_Lit_DetailLighting, TEXT("LevelEditor.Tabs.Viewports"));
		ViewModes.Presets.Emplace(TEXT("TADebugViewTool_LightingOnly"), LOCTEXT("Preset_LightingOnly", "Lighting Only"), LOCTEXT("Preset_LightingOnly_Tooltip", "Inspect lighting contribution without material color."), VMI_LightingOnly, TEXT("LevelEditor.Tabs.Viewports"));
		Result.Add(MoveTemp(ViewModes));

		FDebugViewGroup Materials(TEXT("TADebugViewTool_Materials"), LOCTEXT("Group_Materials", "Materials"));
		Materials.Presets.Emplace(TEXT("TADebugViewTool_ShaderComplexity"), LOCTEXT("Preset_ShaderComplexity", "Shader Complexity"), LOCTEXT("Preset_ShaderComplexity_Tooltip", "Show relative material instruction cost."), VMI_ShaderComplexity, TEXT("ClassIcon.Material"));
		Materials.Presets.Emplace(TEXT("TADebugViewTool_QuadOverdraw"), LOCTEXT("Preset_QuadOverdraw", "Quad Overdraw"), LOCTEXT("Preset_QuadOverdraw_Tooltip", "Show overdraw cost from small triangles and masked materials."), VMI_QuadOverdraw, TEXT("ClassIcon.Material"));
		Materials.Presets.Emplace(TEXT("TADebugViewTool_ShaderComplexityQuad"), LOCTEXT("Preset_ShaderComplexityQuad", "Shader Complexity + Quad"), LOCTEXT("Preset_ShaderComplexityQuad_Tooltip", "Show shader complexity combined with quad overdraw."), VMI_ShaderComplexityWithQuadOverdraw, TEXT("ClassIcon.Material"));
		Materials.Presets.Emplace(TEXT("TADebugViewTool_TextureDensity"), LOCTEXT("Preset_TextureDensity", "Texture Density"), LOCTEXT("Preset_TextureDensity_Tooltip", "Inspect texel density and texture scale accuracy."), VMI_MaterialTextureScaleAccuracy, TEXT("ClassIcon.Material"));
		Result.Add(MoveTemp(Materials));

		FDebugViewGroup Lighting(TEXT("TADebugViewTool_Lighting"), LOCTEXT("Group_Lighting", "Lighting"));
		Lighting.Presets.Emplace(TEXT("TADebugViewTool_LightComplexity"), LOCTEXT("Preset_LightComplexity", "Light Complexity"), LOCTEXT("Preset_LightComplexity_Tooltip", "Show how many lights affect each pixel."), VMI_LightComplexity, TEXT("Icons.Light"));
		Lighting.Presets.Emplace(TEXT("TADebugViewTool_LightmapDensity"), LOCTEXT("Preset_LightmapDensity", "Lightmap Density"), LOCTEXT("Preset_LightmapDensity_Tooltip", "Inspect baked lightmap texel density."), VMI_LightmapDensity, TEXT("Icons.Light"));
		Lighting.Presets.Emplace(TEXT("TADebugViewTool_StationaryOverlap"), LOCTEXT("Preset_StationaryOverlap", "Stationary Light Overlap"), LOCTEXT("Preset_StationaryOverlap_Tooltip", "Find overlapping stationary lights."), VMI_StationaryLightOverlap, TEXT("Icons.Light"));
		Lighting.Presets.Emplace(TEXT("TADebugViewTool_ReflectionOverride"), LOCTEXT("Preset_ReflectionOverride", "Reflection Override"), LOCTEXT("Preset_ReflectionOverride_Tooltip", "Inspect reflection captures and specular response."), VMI_ReflectionOverride, TEXT("Icons.Light"));
		Result.Add(MoveTemp(Lighting));

		FDebugViewGroup Nanite(TEXT("TADebugViewTool_Nanite"), LOCTEXT("Group_Nanite", "Nanite"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NaniteOverview"), LOCTEXT("Preset_NaniteOverview", "Overview"), LOCTEXT("Preset_NaniteOverview_Tooltip", "Show the Nanite visualization overview tiles."), EPresetActionType::NaniteVisualization, TEXT("Overview"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NaniteMask"), LOCTEXT("Preset_NaniteMask", "Mask"), LOCTEXT("Preset_NaniteMask_Tooltip", "Show which pixels are rendered by Nanite."), EPresetActionType::NaniteVisualization, TEXT("Mask"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NaniteTriangles"), LOCTEXT("Preset_NaniteTriangles", "Triangles"), LOCTEXT("Preset_NaniteTriangles_Tooltip", "Visualize Nanite triangle density."), EPresetActionType::NaniteVisualization, TEXT("Triangles"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NaniteClusters"), LOCTEXT("Preset_NaniteClusters", "Clusters"), LOCTEXT("Preset_NaniteClusters_Tooltip", "Visualize Nanite cluster distribution."), EPresetActionType::NaniteVisualization, TEXT("Clusters"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NanitePrimitives"), LOCTEXT("Preset_NanitePrimitives", "Primitives"), LOCTEXT("Preset_NanitePrimitives_Tooltip", "Visualize Nanite primitive IDs."), EPresetActionType::NaniteVisualization, TEXT("Primitives"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NaniteInstances"), LOCTEXT("Preset_NaniteInstances", "Instances"), LOCTEXT("Preset_NaniteInstances_Tooltip", "Visualize Nanite instance IDs."), EPresetActionType::NaniteVisualization, TEXT("Instances"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NaniteOverdraw"), LOCTEXT("Preset_NaniteOverdraw", "Overdraw"), LOCTEXT("Preset_NaniteOverdraw_Tooltip", "Visualize Nanite overdraw."), EPresetActionType::NaniteVisualization, TEXT("Overdraw"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NaniteEvaluateWPO"), LOCTEXT("Preset_NaniteEvaluateWPO", "Evaluate WPO"), LOCTEXT("Preset_NaniteEvaluateWPO_Tooltip", "Visualize Nanite world position offset evaluation."), EPresetActionType::NaniteVisualization, TEXT("EvaluateWPO"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NaniteRasterBins"), LOCTEXT("Preset_NaniteRasterBins", "Raster Bins"), LOCTEXT("Preset_NaniteRasterBins_Tooltip", "Visualize Nanite raster bins."), EPresetActionType::NaniteVisualization, TEXT("RasterBins"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Nanite.Presets.Emplace(TEXT("TADebugViewTool_NaniteShadingBins"), LOCTEXT("Preset_NaniteShadingBins", "Shading Bins"), LOCTEXT("Preset_NaniteShadingBins_Tooltip", "Visualize Nanite shading bins."), EPresetActionType::NaniteVisualization, TEXT("ShadingBins"), TEXT("EditorViewport.VisualizeNaniteMode"));
		Result.Add(MoveTemp(Nanite));

		FDebugViewGroup Lumen(TEXT("TADebugViewTool_Lumen"), LOCTEXT("Group_Lumen", "Lumen"));
		Lumen.Presets.Emplace(TEXT("TADebugViewTool_LumenOverview"), LOCTEXT("Preset_LumenOverview", "Overview"), LOCTEXT("Preset_LumenOverview_Tooltip", "Show the Lumen visualization overview tiles."), EPresetActionType::LumenVisualization, TEXT("Overview"), TEXT("EditorViewport.VisualizeLumenMode"));
		Lumen.Presets.Emplace(TEXT("TADebugViewTool_LumenPerformanceOverview"), LOCTEXT("Preset_LumenPerformanceOverview", "Performance Overview"), LOCTEXT("Preset_LumenPerformanceOverview_Tooltip", "Show the Lumen performance visualization overview."), EPresetActionType::LumenVisualization, TEXT("PerformanceOverview"), TEXT("EditorViewport.VisualizeLumenMode"));
		Lumen.Presets.Emplace(TEXT("TADebugViewTool_LumenScene"), LOCTEXT("Preset_LumenScene", "Lumen Scene"), LOCTEXT("Preset_LumenScene_Tooltip", "Visualize the Lumen scene representation."), EPresetActionType::LumenVisualization, TEXT("LumenScene"), TEXT("EditorViewport.VisualizeLumenMode"));
		Lumen.Presets.Emplace(TEXT("TADebugViewTool_LumenGeometryNormals"), LOCTEXT("Preset_LumenGeometryNormals", "Geometry Normals"), LOCTEXT("Preset_LumenGeometryNormals_Tooltip", "Visualize geometry normals used by Lumen."), EPresetActionType::LumenVisualization, TEXT("GeometryNormals"), TEXT("EditorViewport.VisualizeLumenMode"));
		Lumen.Presets.Emplace(TEXT("TADebugViewTool_LumenReflectionView"), LOCTEXT("Preset_LumenReflectionView", "Reflection View"), LOCTEXT("Preset_LumenReflectionView_Tooltip", "Visualize the Lumen scene with reflection settings."), EPresetActionType::LumenVisualization, TEXT("ReflectionView"), TEXT("EditorViewport.VisualizeLumenMode"));
		Lumen.Presets.Emplace(TEXT("TADebugViewTool_LumenSurfaceCache"), LOCTEXT("Preset_LumenSurfaceCache", "Surface Cache"), LOCTEXT("Preset_LumenSurfaceCache_Tooltip", "Visualize Lumen surface cache coverage."), EPresetActionType::LumenVisualization, TEXT("SurfaceCache"), TEXT("EditorViewport.VisualizeLumenMode"));
		Lumen.Presets.Emplace(TEXT("TADebugViewTool_LumenDedicatedReflectionRays"), LOCTEXT("Preset_LumenDedicatedReflectionRays", "Dedicated Reflection Rays"), LOCTEXT("Preset_LumenDedicatedReflectionRays_Tooltip", "Visualize pixels that require dedicated Lumen reflection rays."), EPresetActionType::LumenVisualization, TEXT("DedicatedReflectionRays"), TEXT("EditorViewport.VisualizeLumenMode"));
		Result.Add(MoveTemp(Lumen));

		FDebugViewGroup VirtualShadowMap(TEXT("TADebugViewTool_VirtualShadowMap"), LOCTEXT("Group_VirtualShadowMap", "Virtual Shadow Map"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMShadowMask"), LOCTEXT("Preset_VSMShadowMask", "Shadow Mask"), LOCTEXT("Preset_VSMShadowMask_Tooltip", "Visualize the final virtual shadow map shadow mask used by shading."), EPresetActionType::VirtualShadowMapVisualization, TEXT("mask"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMClipmapMip"), LOCTEXT("Preset_VSMClipmapMip", "Clipmap / Mip Level"), LOCTEXT("Preset_VSMClipmapMip_Tooltip", "Visualize the chosen directional-light clipmap or local-light mip level."), EPresetActionType::VirtualShadowMapVisualization, TEXT("mip"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMVirtualPage"), LOCTEXT("Preset_VSMVirtualPage", "Virtual Page"), LOCTEXT("Preset_VSMVirtualPage_Tooltip", "Visualize virtual page addresses."), EPresetActionType::VirtualShadowMapVisualization, TEXT("vpage"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMCachedPage"), LOCTEXT("Preset_VSMCachedPage", "Cached Page"), LOCTEXT("Preset_VSMCachedPage_Tooltip", "Visualize cached and uncached virtual shadow map pages."), EPresetActionType::VirtualShadowMapVisualization, TEXT("cache"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMNaniteOverdraw"), LOCTEXT("Preset_VSMNaniteOverdraw", "Nanite Overdraw"), LOCTEXT("Preset_VSMNaniteOverdraw_Tooltip", "Visualize Nanite overdraw into mapped VSM pages."), EPresetActionType::VirtualShadowMapVisualization, TEXT("naniteoverdraw"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMShadowCasters"), LOCTEXT("Preset_VSMShadowCasters", "Shadow Casters"), LOCTEXT("Preset_VSMShadowCasters_Tooltip", "Visualize shadow-casting objects and their invalidation type."), EPresetActionType::VirtualShadowMapVisualization, TEXT("casters"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMRayCount"), LOCTEXT("Preset_VSMRayCount", "SMRT Ray Count"), LOCTEXT("Preset_VSMRayCount_Tooltip", "Visualize shadow map ray count cost per pixel."), EPresetActionType::VirtualShadowMapVisualization, TEXT("raycount"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMDirtyPage"), LOCTEXT("Preset_VSMDirtyPage", "Dirty Page"), LOCTEXT("Preset_VSMDirtyPage_Tooltip", "Show pages marked as dirty."), EPresetActionType::VirtualShadowMapVisualization, TEXT("dirty"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMInvalidPage"), LOCTEXT("Preset_VSMInvalidPage", "GPU Invalidated Page"), LOCTEXT("Preset_VSMInvalidPage_Tooltip", "Show pages marked for GPU-driven invalidation."), EPresetActionType::VirtualShadowMapVisualization, TEXT("invalid"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMMergedPage"), LOCTEXT("Preset_VSMMergedPage", "Merged Page"), LOCTEXT("Preset_VSMMergedPage_Tooltip", "Show pages that were merged."), EPresetActionType::VirtualShadowMapVisualization, TEXT("merged"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMGeneralDebug"), LOCTEXT("Preset_VSMGeneralDebug", "General Debug"), LOCTEXT("Preset_VSMGeneralDebug_Tooltip", "General-purpose VSM shader debug visualization."), EPresetActionType::VirtualShadowMapVisualization, TEXT("debug"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMClipmapVirtual"), LOCTEXT("Preset_VSMClipmapVirtual", "Clipmap Virtual Address Space"), LOCTEXT("Preset_VSMClipmapVirtual_Tooltip", "Visualize clipmap virtual address space and mapped pages."), EPresetActionType::VirtualShadowMapVisualization, TEXT("clipmapvirtual"), TEXT("EditorViewport.VisualizeVirtualShadowMapMode"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMNextLight"), LOCTEXT("Preset_VSMNextLight", "Next Light"), LOCTEXT("Preset_VSMNextLight_Tooltip", "Select the next light for VSM visualization."), TEXT("r.Shadow.Virtual.Visualize.NextLight"), TEXT("Icons.ArrowRight"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMPreviousLight"), LOCTEXT("Preset_VSMPreviousLight", "Previous Light"), LOCTEXT("Preset_VSMPreviousLight_Tooltip", "Select the previous light for VSM visualization."), TEXT("r.Shadow.Virtual.Visualize.PrevLight"), TEXT("Icons.ArrowLeft"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMDumpLightNames"), LOCTEXT("Preset_VSMDumpLightNames", "Dump Light Names"), LOCTEXT("Preset_VSMDumpLightNames_Tooltip", "Print VSM light names to the output log."), TEXT("r.Shadow.Virtual.Visualize.DumpLightNames"), TEXT("Icons.Search"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMCachedPagesOnlyOn"), LOCTEXT("Preset_VSMCachedPagesOnlyOn", "Cached Pages Only On"), LOCTEXT("Preset_VSMCachedPagesOnlyOn_Tooltip", "Show cached VSM pages for all lights and hide uncached pages."), TEXT("r.Shadow.Virtual.Visualize.ShowCachedPagesOnly 1"), TEXT("Icons.Visibility"));
		VirtualShadowMap.Presets.Emplace(TEXT("TADebugViewTool_VSMCachedPagesOnlyOff"), LOCTEXT("Preset_VSMCachedPagesOnlyOff", "Cached Pages Only Off"), LOCTEXT("Preset_VSMCachedPagesOnlyOff_Tooltip", "Disable cached-pages-only VSM filtering."), TEXT("r.Shadow.Virtual.Visualize.ShowCachedPagesOnly 0"), TEXT("Icons.Hidden"));
		Result.Add(MoveTemp(VirtualShadowMap));

		FDebugViewGroup UE5(TEXT("TADebugViewTool_UE5"), LOCTEXT("Group_UE5", "UE5 Rendering"));
		UE5.Presets.Emplace(TEXT("TADebugViewTool_VirtualTexture"), LOCTEXT("Preset_VirtualTexture", "Virtual Texture Visualization"), LOCTEXT("Preset_VirtualTexture_Tooltip", "Open the virtual texture visualization view mode."), VMI_VisualizeVirtualTexture, TEXT("Icons.Visibility"));
		Result.Add(MoveTemp(UE5));

		FDebugViewGroup Geometry(TEXT("TADebugViewTool_Geometry"), LOCTEXT("Group_Geometry", "Geometry"));
		Geometry.Presets.Emplace(TEXT("TADebugViewTool_CollisionPawn"), LOCTEXT("Preset_CollisionPawn", "Collision Pawn"), LOCTEXT("Preset_CollisionPawn_Tooltip", "Show pawn collision view mode."), VMI_CollisionPawn, TEXT("LevelEditor.Tabs.Details"));
		Geometry.Presets.Emplace(TEXT("TADebugViewTool_CollisionVisibility"), LOCTEXT("Preset_CollisionVisibility", "Collision Visibility"), LOCTEXT("Preset_CollisionVisibility_Tooltip", "Show visibility collision view mode."), VMI_CollisionVisibility, TEXT("LevelEditor.Tabs.Details"));
		Geometry.Presets.Emplace(TEXT("TADebugViewTool_LODColoration"), LOCTEXT("Preset_LODColoration", "LOD Coloration"), LOCTEXT("Preset_LODColoration_Tooltip", "Color meshes by their active LOD."), VMI_LODColoration, TEXT("LevelEditor.Tabs.Details"));
		Geometry.Presets.Emplace(TEXT("TADebugViewTool_Bounds"), LOCTEXT("Preset_Bounds", "Toggle Bounds"), LOCTEXT("Preset_Bounds_Tooltip", "Toggle bounds drawing in the active viewport."), TEXT("show bounds"), TEXT("LevelEditor.Tabs.Details"));
		Geometry.Presets.Emplace(TEXT("TADebugViewTool_Navigation"), LOCTEXT("Preset_Navigation", "Toggle Navigation"), LOCTEXT("Preset_Navigation_Tooltip", "Toggle navigation mesh drawing."), TEXT("show navigation"), TEXT("LevelEditor.Tabs.Details"));
		Result.Add(MoveTemp(Geometry));

		FDebugViewGroup Performance(TEXT("TADebugViewTool_Performance"), LOCTEXT("Group_Performance", "Performance"));
		Performance.Presets.Emplace(TEXT("TADebugViewTool_StatUnit"), LOCTEXT("Preset_StatUnit", "Toggle Stat Unit"), LOCTEXT("Preset_StatUnit_Tooltip", "Toggle frame, game, draw, and GPU timing stats."), TEXT("stat unit"), TEXT("Profiler.Tab"));
		Performance.Presets.Emplace(TEXT("TADebugViewTool_StatGPU"), LOCTEXT("Preset_StatGPU", "Toggle Stat GPU"), LOCTEXT("Preset_StatGPU_Tooltip", "Toggle GPU pass timing stats."), TEXT("stat gpu"), TEXT("Profiler.Tab"));
		Performance.Presets.Emplace(TEXT("TADebugViewTool_StatFPS"), LOCTEXT("Preset_StatFPS", "Toggle Stat FPS"), LOCTEXT("Preset_StatFPS_Tooltip", "Toggle FPS display."), TEXT("stat fps"), TEXT("Profiler.Tab"));
		Performance.Presets.Emplace(TEXT("TADebugViewTool_ProfileGPU"), LOCTEXT("Preset_ProfileGPU", "Profile GPU"), LOCTEXT("Preset_ProfileGPU_Tooltip", "Capture a single GPU profile."), TEXT("profilegpu"), TEXT("Profiler.Tab"));
		Performance.Presets.Emplace(TEXT("TADebugViewTool_StatNone"), LOCTEXT("Preset_StatNone", "Clear Stats"), LOCTEXT("Preset_StatNone_Tooltip", "Disable active stat overlays."), TEXT("stat none"), TEXT("Icons.Delete"));
		Result.Add(MoveTemp(Performance));

		return Result;
	}();

	return Groups;
}

const TArray<FWorkflowPreset>& GetWorkflowPresets()
{
	static const TArray<FWorkflowPreset> Presets = []()
	{
		TArray<FWorkflowPreset> Result;

		Result.Emplace(
			TEXT("TADebugWorkflow_MaterialCost"),
			LOCTEXT("Workflow_MaterialCost", "Material Cost"),
			LOCTEXT("Workflow_MaterialCost_Tooltip", "Shader complexity with GPU and frame timing overlays."),
			TEXT("ClassIcon.Material"),
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_ShaderComplexityWithQuadOverdraw),
				FDebugViewAction::Command(TEXT("stat none")),
				FDebugViewAction::Command(TEXT("stat unit")),
				FDebugViewAction::Command(TEXT("stat gpu"))
			},
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_Lit),
				FDebugViewAction::Command(TEXT("stat none"))
			});

		Result.Emplace(
			TEXT("TADebugWorkflow_NaniteAudit"),
			LOCTEXT("Workflow_NaniteAudit", "Nanite Audit"),
			LOCTEXT("Workflow_NaniteAudit_Tooltip", "Nanite overview with bounds and frame timing."),
			TEXT("EditorViewport.VisualizeNaniteMode"),
			TArray<FDebugViewAction>
			{
				FDebugViewAction::Nanite(TEXT("Overview")),
				FDebugViewAction::Command(TEXT("showflag.bounds 1")),
				FDebugViewAction::Command(TEXT("stat none")),
				FDebugViewAction::Command(TEXT("stat unit"))
			},
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_Lit),
				FDebugViewAction::Command(TEXT("showflag.bounds 0")),
				FDebugViewAction::Command(TEXT("stat none"))
			});

		Result.Emplace(
			TEXT("TADebugWorkflow_LumenCheck"),
			LOCTEXT("Workflow_LumenCheck", "Lumen Check"),
			LOCTEXT("Workflow_LumenCheck_Tooltip", "Lumen surface cache with frame timing."),
			TEXT("EditorViewport.VisualizeLumenMode"),
			TArray<FDebugViewAction>
			{
				FDebugViewAction::Lumen(TEXT("SurfaceCache")),
				FDebugViewAction::Command(TEXT("stat none")),
				FDebugViewAction::Command(TEXT("stat unit"))
			},
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_Lit),
				FDebugViewAction::Command(TEXT("stat none"))
			});

		Result.Emplace(
			TEXT("TADebugWorkflow_VSMCache"),
			LOCTEXT("Workflow_VSMCache", "VSM Cache"),
			LOCTEXT("Workflow_VSMCache_Tooltip", "Virtual Shadow Map cached page view with GPU timing."),
			TEXT("EditorViewport.VisualizeVirtualShadowMapMode"),
			TArray<FDebugViewAction>
			{
				FDebugViewAction::VirtualShadowMap(TEXT("cache")),
				FDebugViewAction::Command(TEXT("r.Shadow.Virtual.Visualize.ShowCachedPagesOnly 1")),
				FDebugViewAction::Command(TEXT("stat none")),
				FDebugViewAction::Command(TEXT("stat gpu"))
			},
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_Lit),
				FDebugViewAction::Command(TEXT("stat none")),
				FDebugViewAction::Command(TEXT("r.Shadow.Virtual.Visualize.ShowCachedPagesOnly 0"))
			});

		Result.Emplace(
			TEXT("TADebugWorkflow_CollisionQA"),
			LOCTEXT("Workflow_CollisionQA", "Collision QA"),
			LOCTEXT("Workflow_CollisionQA_Tooltip", "Pawn collision view with navigation and bounds overlays."),
			TEXT("LevelEditor.Tabs.Details"),
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_CollisionPawn),
				FDebugViewAction::Command(TEXT("showflag.navigation 1")),
				FDebugViewAction::Command(TEXT("showflag.bounds 1"))
			},
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_Lit),
				FDebugViewAction::Command(TEXT("showflag.navigation 0")),
				FDebugViewAction::Command(TEXT("showflag.bounds 0"))
			});

		Result.Emplace(
			TEXT("TADebugWorkflow_PerformanceHUD"),
			LOCTEXT("Workflow_PerformanceHUD", "Performance HUD"),
			LOCTEXT("Workflow_PerformanceHUD_Tooltip", "Lit view with FPS, frame timing, and GPU timing overlays."),
			TEXT("Profiler.Tab"),
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_Lit),
				FDebugViewAction::Command(TEXT("stat none")),
				FDebugViewAction::Command(TEXT("stat fps")),
				FDebugViewAction::Command(TEXT("stat unit")),
				FDebugViewAction::Command(TEXT("stat gpu"))
			},
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_Lit),
				FDebugViewAction::Command(TEXT("stat none"))
			});

		Result.Emplace(
			TEXT("TADebugWorkflow_ResetDebug"),
			LOCTEXT("Workflow_ResetDebug", "Reset Debug"),
			LOCTEXT("Workflow_ResetDebug_Tooltip", "Return to Lit and clear common stat overlays."),
			TEXT("Icons.Refresh"),
			TArray<FDebugViewAction>
			{
				FDebugViewAction::ViewMode(VMI_Lit),
				FDebugViewAction::Command(TEXT("stat none")),
				FDebugViewAction::Command(TEXT("r.Shadow.Virtual.Visualize.ShowCachedPagesOnly 0"))
			},
			TArray<FDebugViewAction>());

		return Result;
	}();

	return Presets;
}
}

#undef LOCTEXT_NAMESPACE
