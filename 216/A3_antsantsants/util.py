# states:
# 0 - exploring
# 1 - searching scent area
# 2 - returning to hill
# 3 - following pheremone trail
# 4 - stopped
states = {
    "EXPLORING": 0,
    "SEARCHING": 1,
    "RETURNING": 2,
    "FOLLOWING": 3,
    "STOP": 4
}

# matrix values:
# 0 - not explored
# 1 - explored
# 2 - food scent/pheremone
# 3 - food
# 4 - ant??? (but we'll deal with this later)
matrix_vals = {
    "UNEXPLORED": 0,
    "EXPLORED"  : 1,
    "SCENT"     : 2,
    "FOOD"      : 3
}

dirs = ((0, 1), (1, 0), (0, -1), (-1, 0))

dir_dict = {
    "N": 0,
    "E": 1,
    "S": 2,
    "W": 3
}

init_dir_probs = [
    [70, 15, 0, 15], # N
    [0, 15, 70, 15], # S
    [15, 70, 15, 0], # E
    [15, 0, 15, 70], # W
    [35, 35, 15, 15], # NE
    [15, 35, 35, 15], # SE
    [15, 15, 35, 35], # SW
    [35, 15, 15, 35], # NW
]
