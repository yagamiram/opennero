ROWS = 8 # rows in maze
COLS = 8 # columns in maze
GRID_DX = 20.0 # x-dimension of the grid world
GRID_DY = 20.0 # y-dimension of the grid world
MAX_STEPS = ROWS * COLS * 2 # max number of steps - no need to visit each cell more then twice!
STEP_DELAY = 3.0 # number of seconds to wait between the sense-act-step repeats
NUDGE_X = 20.0 # shift the island in +x by ...
NUDGE_Y = 20.0 # shift the island in +y by ...
WALL_TEMPLATE = "data/shapes/wall/BrickWall.xml"
INITIAL_EPSILON = 0.1
HISTORY_LENGTH = 5 # number of state-action pairs used to determine if the agent is stuck
OBSTACLE_MASK = 1 #0b0001
AGENT_MASK = 2 #0b0010


STEPS_IN_ROUND = 3

STARTING_BARBS = 1
BARBS_PER_ROUND = 1

STARTING_CITIES = 3

STARTING_LEGIONS = 3