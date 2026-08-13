"""Create the first saved Black Meridian map shells in the UE editor.

This is intentionally an editor-only asset authoring tool.  The runtime C++
directors still own mission state and interaction spawning; this script owns
the replaceable level shell (collision, routes, landmarks, starts, lighting,
and navigation bounds) so those systems no longer depend on the engine Entry
map's transient world.
"""

import unreal


ROOT = "/Game/Levels"
BLOCK_PATH = "/Game/Environment/Meshes/SM_BM_Env_Block.SM_BM_Env_Block"
PIPE_PATH = "/Game/Environment/Meshes/SM_BM_Env_Pipe.SM_BM_Env_Pipe"
ORB_PATH = "/Game/Environment/Meshes/SM_BM_Env_Orb.SM_BM_Env_Orb"
DECK_PATH = "/Game/Environment/Meshes/SM_BM_Env_Deck.SM_BM_Env_Deck"
TRUSS_PATH = "/Game/Environment/Meshes/SM_BM_Env_Truss.SM_BM_Env_Truss"
GRATE_PATH = "/Game/Environment/Meshes/SM_BM_Env_Grate.SM_BM_Env_Grate"
ENGINE_CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
ENGINE_CYLINDER_PATH = "/Engine/BasicShapes/Cylinder.Cylinder"
ENGINE_SPHERE_PATH = "/Engine/BasicShapes/Sphere.Sphere"

# UE5.8 removed the legacy EditorAssetLibrary/EditorLevelLibrary Python
# wrappers from the command-line editor context. Keep the authoring tool
# usable on both the older UE5.1-style API and the current subsystem API.
ASSET_SUBSYSTEM = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
ACTOR_SUBSYSTEM = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EDITOR_SUBSYSTEM = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def log(message):
    unreal.log("Nightglass authored-map tool: %s" % message)


def vec(values):
    return unreal.Vector(values[0], values[1], values[2])


def rot(yaw=0.0):
    return unreal.Rotator(0.0, yaw, 0.0)


def spawn_actor(actor_class, location=(0.0, 0.0, 0.0), rotation=None, label=""):
    actor = ACTOR_SUBSYSTEM.spawn_actor_from_class(
        actor_class,
        vec(location),
        rotation or unreal.Rotator(0.0, 0.0, 0.0),
    )
    if actor is None:
        raise RuntimeError("could not spawn %s" % actor_class)
    if label:
        actor.set_actor_label(label)
    return actor


def add_mesh(mesh, location, scale, yaw=0.0, label="", folder=""):
    actor = spawn_actor(unreal.StaticMeshActor, location, rot(yaw), label)
    component = actor.get_editor_property("static_mesh_component")
    component.set_editor_property("static_mesh", mesh)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    actor.set_actor_scale3d(vec(scale))
    # Editor labels are useful while authoring but are not a reliable runtime
    # identity in a cooked map. Persist the semantic role as an actor tag so
    # the runtime art director can assign the correct floor/steel/cover/
    # landmark material treatment in both editor and packaged builds.
    tags = list(actor.get_editor_property("tags") or [])
    if label:
        tags.append(unreal.Name("MapRole_%s" % label))
    if folder:
        tags.append(unreal.Name("MapFolder_%s" % folder))
    actor.set_editor_property("tags", tags)
    if folder:
        actor.set_folder_path(unreal.Name(folder))
    return actor


def add_cube(mesh, location, scale, yaw=0.0, label="", folder="Environment"):
    return add_mesh(mesh, location, scale, yaw, label, folder)


def add_floor(deck, location, scale, label):
    return add_mesh(deck, location, scale, 0.0, label, "Environment")


def load_preferred_asset(project_path, fallback_path):
    asset = unreal.load_asset(project_path)
    if asset is not None:
        return asset
    log("project environment asset missing; using compatibility fallback %s" % fallback_path)
    return unreal.load_asset(fallback_path)


def add_landmark(meshes, location, accent_scale, label):
    add_cube(meshes[0], location, (3.2, 3.2, 0.08), label=label + "_Pad", folder="Landmarks")
    add_cube(meshes[0], (location[0], location[1], location[2] + 125.0), (0.18, 0.18, 1.25), label=label + "_Mast", folder="Landmarks")
    add_cube(meshes[0], (location[0], location[1], location[2] + 260.0), accent_scale, label=label + "_Signal", folder="Landmarks")
    add_mesh(meshes[1], (location[0], location[1], location[2] + 325.0), (0.34, 0.34, 0.34), label=label + "_Beacon", folder="Landmarks")


def add_lighting(label):
    directional = spawn_actor(unreal.DirectionalLight, (0.0, 0.0, 900.0), rot(-32.0), label + "_Moon")
    directional_component = directional.get_editor_property("directional_light_component")
    directional_component.set_editor_property("mobility", unreal.ComponentMobility.STATIONARY)
    directional_component.set_editor_property("intensity", 2.5)
    directional_component.set_editor_property("light_color", unreal.Color(107, 148, 184, 255))

    sky = spawn_actor(unreal.SkyLight, (0.0, 0.0, 500.0), label=label + "_Sky")
    sky_component = sky.get_editor_property("light_component")
    sky_component.set_editor_property("mobility", unreal.ComponentMobility.STATIONARY)
    sky_component.set_editor_property("intensity", 0.35)

def add_navigation(label, extent=(3000.0, 1800.0, 400.0)):
    nav = spawn_actor(unreal.NavMeshBoundsVolume, (650.0, 0.0, 150.0), label=label + "_NavBounds")
    nav.set_actor_scale3d(vec((extent[0] / 100.0, extent[1] / 100.0, extent[2] / 100.0)))


def add_player_start(location, yaw, label):
    return spawn_actor(unreal.PlayerStart, location, rot(yaw), label)


def add_cover_line(cube, start_x, y, count, spacing, scale, yaw, prefix):
    for index in range(count):
        add_cube(
            cube,
            (start_x + index * spacing, y, 55.0),
            scale,
            yaw,
            "%s_%02d" % (prefix, index),
            "Cover",
        )


def fresh_level(path):
    """Load an existing generated map and clear its authored actor set."""
    if ASSET_SUBSYSTEM.does_asset_exist(path):
        loaded = unreal.EditorLoadingAndSavingUtils.load_map(path)
        if not loaded:
            raise RuntimeError("could not load existing generated map %s" % path)
        world = EDITOR_SUBSYSTEM.get_editor_world()
        for actor in list(ACTOR_SUBSYSTEM.get_all_level_actors()):
            if isinstance(actor, unreal.WorldSettings):
                continue
            ACTOR_SUBSYSTEM.destroy_actor(actor)
        return world
    created = unreal.EditorLoadingAndSavingUtils.new_map(False)
    if not created:
        raise RuntimeError("could not create generated map %s" % path)
    world = EDITOR_SUBSYSTEM.get_editor_world()
    return world


def set_world_game_mode(world, class_path):
    game_mode_class = unreal.load_class(None, class_path)
    if game_mode_class is None:
        raise RuntimeError("could not load game mode %s" % class_path)
    settings = world.get_world_settings()
    settings.set_editor_property("default_game_mode", game_mode_class)


def build_glasswake(cube, cylinder, sphere, deck, truss, grate):
    log("creating Glasswake")
    world = fresh_level(ROOT + "/Glasswake")
    set_world_game_mode(world, "/Script/BlackMeridianRuntime.BMGameMode")

    add_floor(deck, (650.0, 0.0, -55.0), (55.0, 16.0, 1.0), "Glasswake_Floor")
    # Insertion and quiet approach.
    add_cover_line(cube, 100.0, -620.0, 5, 440.0, (2.0, 0.65, 0.9), 0.0, "ApproachCover")
    add_cube(cube, (-700.0, 540.0, 60.0), (3.5, 0.7, 1.3), label="Insertion_Wall")
    add_cube(cube, (-740.0, -260.0, 105.0), (0.55, 2.8, 2.2), label="Quiet_Checkpoint")
    # Relay house with an actual opening between two wall sections.
    add_cube(cube, (590.0, 880.0, 75.0), (0.7, 0.45, 1.6), label="Relay_WestWall")
    add_cube(cube, (970.0, 880.0, 75.0), (0.7, 0.45, 1.6), label="Relay_EastWall")
    add_cube(cube, (1750.0, 820.0, 24.0), (5.5, 3.0, 0.5), label="Relay_Roof")
    add_cube(cube, (1750.0, 820.0, 175.0), (5.5, 0.18, 1.35), label="Relay_NorthWall")
    add_cube(cube, (1250.0, 820.0, 125.0), (0.18, 2.6, 1.2), label="Relay_WestWall")
    add_cube(cube, (2250.0, 820.0, 125.0), (0.18, 2.6, 1.2), label="Relay_EastWall")
    # The breach barrier remains a replicated runtime mission prop so the
    # environmental event can remove it authoritatively; the authored shell
    # leaves this opening clear.
    add_mesh(cylinder, (1750.0, 820.0, 390.0), (0.16, 0.16, 2.0), label="Relay_Antenna", folder="Landmarks")
    add_mesh(sphere, (1750.0, 820.0, 560.0), (0.34, 0.34, 0.34), label="Relay_SignalLamp", folder="Landmarks")
    # Seawall and extraction route.
    add_cube(cube, (2100.0, 760.0, 75.0), (0.45, 3.0, 1.6), label="Seawall")
    add_cube(cube, (1300.0, 540.0, 175.0), (1.0, 0.18, 1.35), label="Pier_WestWall")
    add_cube(cube, (2050.0, 540.0, 175.0), (2.5, 0.18, 1.35), label="Pier_EastWall")
    add_cover_line(cube, 900.0, -260.0, 4, 410.0, (1.6, 0.7, 1.1), 0.0, "SeawallCover")
    # Repeated overhead gantries and grounded grating break the long deck
    # into authored industrial rooms without changing the mission route.
    add_mesh(truss, (-720.0, 980.0, 320.0), (3.2, 1.0, 2.4), 90.0, "Approach_Truss", "Dressing")
    add_mesh(truss, (1220.0, 980.0, 320.0), (3.8, 1.0, 2.4), 90.0, "Relay_Truss", "Dressing")
    add_mesh(truss, (2460.0, -980.0, 320.0), (3.0, 1.0, 2.2), 0.0, "Extraction_Truss", "Dressing")
    add_mesh(grate, (360.0, -500.0, 4.0), (2.4, 1.55, 1.0), 0.0, "Approach_Grate", "Dressing")
    add_mesh(grate, (1510.0, 420.0, 4.0), (2.2, 1.35, 1.0), 90.0, "Seawall_Grate", "Dressing")
    # Readable route landmarks are static map content, not transient debug lines.
    add_landmark((cube, sphere), (250.0, -500.0, 0.0), (0.62, 0.62, 0.12), "InsertionBeacon")
    add_landmark((cube, sphere), (780.0, 880.0, 0.0), (0.62, 0.62, 0.12), "BreachBeacon")
    add_landmark((cube, sphere), (1500.0, 0.0, 0.0), (0.62, 0.62, 0.12), "SeawallBeacon")
    add_landmark((cube, sphere), (1500.0, 650.0, 0.0), (0.62, 0.62, 0.12), "ExtractionBeacon")
    add_player_start((0.0, -950.0, 120.0), 0.0, "Glasswake_PlayerStart")
    add_lighting("Glasswake")
    add_navigation("Glasswake", (3000.0, 1800.0, 400.0))
    unreal.EditorLoadingAndSavingUtils.save_map(world, ROOT + "/Glasswake")
    log("saved Glasswake")


def build_signal_control(cube, cylinder, sphere, deck, truss, grate):
    log("creating SignalControl")
    world = fresh_level(ROOT + "/SignalControl")
    set_world_game_mode(world, "/Script/BlackMeridianRuntime.BMSignalControlGameMode")

    add_floor(deck, (650.0, 0.0, -55.0), (55.0, 16.0, 1.0), "SignalControl_Floor")
    # Outer walls, three routes, and human-scale cover.
    add_cube(cube, (-2100.0, 0.0, 245.0), (0.5, 16.0, 5.0), label="WestBoundary")
    add_cube(cube, (3400.0, 0.0, 245.0), (0.5, 16.0, 5.0), label="EastBoundary")
    add_cube(cube, (650.0, -1550.0, 245.0), (28.0, 0.5, 5.0), label="SouthBoundary")
    add_cube(cube, (650.0, 1550.0, 245.0), (28.0, 0.5, 5.0), label="NorthBoundary")
    add_cover_line(cube, -100.0, -620.0, 6, 420.0, (2.4, 0.65, 0.9), 0.0, "SouthRouteCover")
    add_cover_line(cube, 220.0, 360.0, 5, 440.0, (2.0, 0.65, 0.9), 0.0, "NorthRouteCover")
    add_cube(cube, (820.0, 230.0, 55.0), (2.2, 0.85, 1.1), 16.0, "MidRouteBarricade", "Cover")
    add_mesh(cylinder, (1180.0, 360.0, 78.0), (0.72, 0.72, 2.4), 90.0, "MidRouteTank", "Cover")
    add_mesh(cylinder, (1300.0, 360.0, 78.0), (0.72, 0.72, 2.4), 90.0, "MidRouteTank_02", "Cover")
    # Three node pads and strong vertical landmarks.
    for index, location in enumerate(((250.0, 650.0, 0.0), (1500.0, -500.0, 0.0), (2600.0, 650.0, 0.0)), 1):
        add_landmark((cube, sphere), location, (2.1, 2.1, 0.12), "Node_%d" % index)
        add_cube(cube, (location[0], location[1], 28.0), (2.1, 2.1, 0.12), label="Node_%d_Pad" % index, folder="Nodes")
    # Node lanes receive a consistent overhead service language and short
    # grating transitions so the competitive arena has readable scale cues.
    add_mesh(truss, (650.0, 1160.0, 320.0), (5.0, 1.0, 2.4), 0.0, "North_Service_Truss", "Dressing")
    add_mesh(truss, (650.0, -1160.0, 320.0), (5.0, 1.0, 2.4), 0.0, "South_Service_Truss", "Dressing")
    add_mesh(grate, (650.0, 650.0, 4.0), (2.4, 1.5, 1.0), 0.0, "Node_1_Grate", "Dressing")
    add_mesh(grate, (1500.0, -500.0, 4.0), (2.4, 1.5, 1.0), 90.0, "Node_2_Grate", "Dressing")
    add_mesh(grate, (2600.0, 650.0, 4.0), (2.4, 1.5, 1.0), 0.0, "Node_3_Grate", "Dressing")
    # Perimeter starts mirror the authority-side spawn budget and make the
    # saved map usable for a later dedicated-server start-point pass.
    starts = [
        (-1200.0, -1100.0, 120.0, 0.0), (-1200.0, -550.0, 120.0, 0.0),
        (-1200.0, 0.0, 120.0, 0.0), (-1200.0, 550.0, 120.0, 0.0),
        (-1200.0, 1100.0, 120.0, 0.0), (2800.0, -1100.0, 120.0, 180.0),
        (2800.0, -550.0, 120.0, 180.0), (2800.0, 0.0, 120.0, 180.0),
        (2800.0, 550.0, 120.0, 180.0), (2800.0, 1100.0, 120.0, 180.0),
        (650.0, -1200.0, 120.0, 90.0), (650.0, 1200.0, 120.0, -90.0),
    ]
    for index, (x, y, z, yaw) in enumerate(starts, 1):
        add_player_start((x, y, z), yaw, "SignalControl_Start_%02d" % index)
    add_lighting("SignalControl")
    add_navigation("SignalControl", (3000.0, 1800.0, 400.0))
    unreal.EditorLoadingAndSavingUtils.save_map(world, ROOT + "/SignalControl")
    log("saved SignalControl")


def main():
    ASSET_SUBSYSTEM.make_directory(ROOT)
    cube = load_preferred_asset(BLOCK_PATH, ENGINE_CUBE_PATH)
    cylinder = load_preferred_asset(PIPE_PATH, ENGINE_CYLINDER_PATH)
    sphere = load_preferred_asset(ORB_PATH, ENGINE_SPHERE_PATH)
    deck = load_preferred_asset(DECK_PATH, ENGINE_CUBE_PATH)
    truss = load_preferred_asset(TRUSS_PATH, ENGINE_CUBE_PATH)
    grate = load_preferred_asset(GRATE_PATH, ENGINE_CUBE_PATH)
    if not cube or not cylinder or not sphere or not deck or not truss or not grate:
        raise RuntimeError("environment mesh assets were not available")
    build_glasswake(cube, cylinder, sphere, deck, truss, grate)
    build_signal_control(cube, cylinder, sphere, deck, truss, grate)
    log("complete")


try:
    main()
except Exception as error:
    unreal.log_error("Nightglass authored-map tool failed: %s" % error)
    raise
