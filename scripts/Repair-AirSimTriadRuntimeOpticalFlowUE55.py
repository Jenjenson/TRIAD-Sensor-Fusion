"""Repair the two legacy AirSim optical-flow materials for Unreal Engine 5.5.

Run this file through Unreal's PythonScript commandlet.  It is intentionally
idempotent and fails closed unless each material has exactly one CameraMotionFlow
custom expression containing either the legacy line or the approved UE 5.5 line.
"""

from __future__ import annotations

import unreal


ASSET_PATHS = (
    "/AirSimTriadRuntime/HUDAssets/OpticalFlowMaterial",
    "/AirSimTriadRuntime/HUDAssets/OpticalFlowRGBMaterial",
)

LEGACY_LINE = "float4 previousPos = mul(worldPos, View.PrevViewProj);"
UE55_LINE = "float4 previousPos = mul(H, ResolvedView.ClipToPrevClip);"


def _custom_expressions(material: unreal.Material) -> list[unreal.MaterialExpressionCustom]:
    # UE 5.5's Python module does not expose get_objects_with_outer().  Walk the
    # live custom-expression objects and retain only nodes nested under this
    # loaded material (legacy assets may insert an editor-only-data outer).
    expressions = []
    for obj in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
        outer = obj.get_outer()
        while outer is not None:
            if outer == material:
                expressions.append(obj)
                break
            outer = outer.get_outer()
    return expressions


def _repair_material(asset_path: str) -> str:
    material = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"required material did not load: {asset_path}")

    custom_nodes = _custom_expressions(material)
    motion_nodes = []
    for node in custom_nodes:
        code = str(node.get_editor_property("code"))
        description = str(node.get_editor_property("description"))
        if (
            description == "CameraMotionFlow"
            or LEGACY_LINE in code
            or UE55_LINE in code
        ):
            motion_nodes.append(node)

    if len(motion_nodes) != 1:
        raise RuntimeError(
            f"{asset_path} must contain exactly one CameraMotionFlow custom node; "
            f"found={len(motion_nodes)} custom_nodes={len(custom_nodes)}"
        )

    node = motion_nodes[0]
    code_before = str(node.get_editor_property("code"))
    legacy_count = code_before.count(LEGACY_LINE)
    ue55_count = code_before.count(UE55_LINE)
    if legacy_count == 1 and ue55_count == 0:
        material.modify()
        node.modify()
        node.set_editor_property("code", code_before.replace(LEGACY_LINE, UE55_LINE))
        unreal.MaterialEditingLibrary.recompile_material(material)
        if not unreal.EditorAssetLibrary.save_loaded_asset(
            material, only_if_is_dirty=False
        ):
            raise RuntimeError(f"failed to save repaired material: {asset_path}")
        status = "REPAIRED"
    elif legacy_count == 0 and ue55_count == 1:
        status = "ALREADY_REPAIRED"
    else:
        raise RuntimeError(
            f"{asset_path} has an unapproved CameraMotionFlow code state: "
            f"legacy_count={legacy_count} ue55_count={ue55_count}"
        )

    code_after = str(node.get_editor_property("code"))
    if code_after.count(LEGACY_LINE) != 0 or code_after.count(UE55_LINE) != 1:
        raise RuntimeError(f"post-repair code validation failed: {asset_path}")

    unreal.log(
        "TRIAD_AIRSIM_OPTICAL_FLOW_UE55_MATERIAL_VALID "
        f"asset={asset_path} status={status} exactReplacement=true"
    )
    return status


def main() -> None:
    statuses = [_repair_material(path) for path in ASSET_PATHS]
    unreal.log(
        "TRIAD_AIRSIM_OPTICAL_FLOW_UE55_REPAIR_PASS "
        f"exactAssets={len(statuses)} statuses={','.join(statuses)} "
        "legacyViewPrevViewProjAbsent=true "
        "resolvedViewClipToPrevClipExact=true"
    )


if __name__ == "__main__":
    main()
