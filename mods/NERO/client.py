from OpenNero import *
from module import getMod, delMod
from common.fitness import Fitness
from common import *
import common.gui as gui
from inputConfig import *
from constants import *
from math import *

ai_state = None

def toggle_ai_callback():
    global ai_state
    toggle_ai()
    if not ai_state:
        getMod().start_rtneat()
        ai_state = 'Started'
    elif ai_state == 'Started':
        ai_state = 'Paused'
    elif ai_state == 'Paused':
        ai_state = 'Started'

def save_ai_call():
    getMod().save_rtneat()

def load_ai_call():
    getMod().load_rtneat()

def recenter(cam):
    def closure():
        cam.setPosition(Vector3f(0, 0, 100))
        cam.setTarget(Vector3f(100,100,0))
    return closure

#########################################################

def show_context_menu():
    global modify_object_id
    global object_ids

    if len(modify_object_id) > 0:
        return

    guiMan.setTransparency(1.0)

    sim_context = getSimContext()

    # find the screen position at which to open the context menu
    cursor = sim_context.getMousePosition()
    location = sim_context.getClickedPosition(cursor.x, cursor.y)
    selected_object_id = sim_context.getClickedEntityId(cursor.x, cursor.y)

    if selected_object_id not in object_ids and abs(location.z) > 1: return

    print "location:", location

    contextMenu = gui.create_context_menu(guiMan, 'context', cursor)

    def add_wall():
        object_ids.append(addObject("data/shapes/cube/Cube.xml", Vector3f(location.x,location.y,HEIGHT+OFFSET), Vector3f(0, 0, 90), scale=Vector3f(1,30,HEIGHT), type = 1 ))

    def rotate_object():
        modify_object_id['rot'] = selected_object_id

    def scale_object():
        modify_object_id['scale'] = selected_object_id

    def move_object():
        modify_object_id['move'] = selected_object_id

    def place_flag():
        from module import getMod
        getMod().change_flag([location.x,location.y,0])

    def place_basic_turret():
        from module import getMod
        getMod().place_basic_turret([location.x,location.y,0])

    def set_spawn():
        getMod().set_spawn(location.x,location.y)

    def remove_wall():
        removeObject(selected_object_id)

    if selected_object_id in object_ids:
        rotateButton = gui.create_button(guiMan, 'rotate', Pos2i(0,0), Pos2i(0,0), '')
        rotateButton.OnMouseLeftClick = lambda: rotate_object()
        contextMenu.addItem('Rotate Object', rotateButton)

        scaleButton = gui.create_button(guiMan, 'scale', Pos2i(0,0), Pos2i(0,0), '')
        scaleButton.OnMouseLeftClick = lambda: scale_object()
        contextMenu.addItem('Scale Object', scaleButton)

        moveButton = gui.create_button(guiMan, 'move', Pos2i(0,0), Pos2i(0,0), '')
        moveButton.OnMouseLeftClick = lambda: move_object()
        contextMenu.addItem('Move Object', moveButton)

        removeButton = gui.create_button(guiMan, 'remove', Pos2i(0,0), Pos2i(0,0), '')
        removeButton.OnMouseLeftClick = lambda: remove_wall()
        contextMenu.addItem('Remove Object', removeButton)

    else:
        wallButton = gui.create_button(guiMan, 'wall', Pos2i(0,0), Pos2i(0,0), '')
        wallButton.OnMouseLeftClick = lambda: add_wall()
        contextMenu.addItem('Add wall', wallButton)

        flagButton = gui.create_button(guiMan, 'flag', Pos2i(0,0), Pos2i(0,0), '')
        flagButton.OnMouseLeftClick = lambda: place_flag()
        contextMenu.addItem('Place Flag', flagButton)

        turretButton = gui.create_button(guiMan, 'b_turret', Pos2i(0,0), Pos2i(0,0), '')
        turretButton.OnMouseLeftClick = lambda: place_basic_turret()
        contextMenu.addItem('Place Basic Turret', turretButton)

        spawnButton = gui.create_button(guiMan, 'spawn', Pos2i(0,0), Pos2i(0,0), '')
        spawnButton.OnMouseLeftClick = lambda: set_spawn()
        contextMenu.addItem('Set Spawn Location', spawnButton)

def reset_mouse_action():
    global modify_object_id
    modify_object_id = {}

def mouse_action():
    import math
    global modify_object_id
    global object_ids

    if len(modify_object_id) == 0:
        return

    sim_context = getSimContext()

    cursor = sim_context.getMousePosition()
    location = sim_context.getClickedPosition(cursor.x, cursor.y)


    if 'move' in modify_object_id:
       sim_context.setObjectPosition(modify_object_id['move'], Vector3f(location.x, location.y, HEIGHT+OFFSET))

    if 'rot' in modify_object_id:
        position = sim_context.getObjectPosition(modify_object_id['rot'])
        angle = math.atan2(location.x-position.x, location.y-position.y)
        sim_context.setObjectRotation(modify_object_id['rot'], Vector3f(0,0,-math.degrees(angle)))

    if 'scale' in modify_object_id:
        position = sim_context.getObjectPosition(modify_object_id['scale'])
        rotation = sim_context.getObjectRotation(modify_object_id['scale'])
        theta = radians(rotation.z)

        # calculate mouse location in the frame of reference of the object
        localx = (location.x-position.x)*math.cos(theta) + (location.y-position.y)*math.sin(theta)
        localy = -(location.x-position.x)*math.sin(theta) + (location.y-position.y)*math.cos(theta)

        # scale < 1 if local coordinate is -ve and scale > 1 otherwise
        scalex = 1 + math.fabs(localx)
        scaley = 1 + math.fabs(localy)

        if localx < 0: scalex = 1/scalex
        if localy < 0: scaley = 1/scaley

        prev_scale = sim_context.getObjectScale(modify_object_id['scale'])
        sim_context.setObjectScale(modify_object_id['scale'],Vector3f(scalex, scaley, prev_scale.z))

#########################################################

def CreateGui(guim):
    global mode
    global modify_object_id
    global object_ids
    global guiMan
    mode = 0

    guiMan = guim
    object_ids = []
    modify_object_id = {}

def ClientMain():
    # physics off, ai off by default
    #disable_physics()
    disable_ai()

    if not getMod().setup_map():
        switchToHub()
        return

    # add a light source
    getSimContext().addLightSource(Vector3f(500,-500,1000), 1500)

    addSkyBox("data/sky/irrlicht2")

    # setup the gui
    CreateGui(getGuiManager())

    # add a camera
    camRotateSpeed = 100
    camMoveSpeed   = 15000
    camZoomSpeed   = 200
    cam = getSimContext().addCamera(camRotateSpeed, camMoveSpeed, camZoomSpeed)
    cam.setFarPlane(40000)
    cam.setEdgeScroll(False)
    recenter_cam = recenter(cam)
    recenter_cam()

    # create the io map
    ioMap = createInputMapping()
    ioMap.BindKey( "KEY_SPACE", "onPress", recenter_cam )
    getSimContext().setInputMapping(ioMap)


