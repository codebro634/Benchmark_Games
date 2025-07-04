
for map in range(2, 21):

    with open(f"../../resources/EarthObservationMaps/{map}.txt") as f:
        data = f.read()

    failure_probs = [-42,-42,-42]
    change_probs = [[-42,-42,-42], [-42,-42,-42], [-42,-42,-42]]
    init_vis_map = {}
    connections ={}
    num_conns = 0
    init_targets = []
    patch_set = set()

    data = data.split("\n")
    for line in data:

        if "is-target" in line:
            init_targets.append(line.split("(")[1].split(")")[0].strip())

        if "CONNECTED" in line:
            inside = line.split("(")[1].split(")")[0].strip()
            p1,p2,dir = inside.split(",")
            dir = dir.strip()
            p1 = p1.strip()
            p2 = p2.strip()
            if p1 not in connections:
                connections[p1] = {}
            assert dir not in connections[p1]
            connections[p1][dir] = p2
            patch_set.add(p1)
            patch_set.add(p2)
            num_conns+=1

        if "visibility" in line:
            vis= line.split("=")[1].split(";")[0].strip()
            patch = line.split("(")[1].split(")")[0].strip()
            patch_set.add(patch)
            init_vis_map[patch] = vis

        if "focal-point" in line:
            focal_point = line.split("(")[1].split(")")[0].strip()
            patch_set.add(focal_point)

        if "FAILURE_PROB_HIGH_VIS" in line:
            p = line.split("=")[1].split(";")[0].strip()
            failure_probs[0] = float(p)
        if "FAILURE_PROB_MEDIUM_VIS" in line:
            p = line.split("=")[1].split(";")[0].strip()
            failure_probs[1] = float(p)
        if "FAILURE_PROB_LOW_VIS" in line:
            p = line.split("=")[1].split(";")[0].strip()
            failure_probs[2] = float(p)

        if "HIGH_TO_MEDIUM_VIS" in line:
            p = line.split("=")[1].split(";")[0].strip()
            change_probs[0][1] = float(p)

        if "HIGH_TO_LOW_VIS" in line:
            p = line.split("=")[1].split(";")[0].strip()
            change_probs[0][2] = float(p)

        if "MEDIUM_TO_HIGH_VIS" in line:
            p = line.split("=")[1].split(";")[0].strip()
            change_probs[1][0] = float(p)

        if "MEDIUM_TO_LOW_VIS" in line:
            p = line.split("=")[1].split(";")[0].strip()
            change_probs[1][2] = float(p)

        if "LOW_TO_HIGH_VIS" in line:
            p = line.split("=")[1].split(";")[0].strip()
            change_probs[2][0] = float(p)

        if "LOW_TO_MEDIUM_VIS" in line:
            p = line.split("=")[1].split(";")[0].strip()
            change_probs[2][1] = float(p)


    dir_map = {"@east": 2, "@north-east": 0, "@south-east": 1}
    vis_map = {"@high": 0, "@medium": 1, "@low": 2}


    change_probs[0][0] = 1.0 - change_probs[0][1] - change_probs[0][2]
    change_probs[1][1] = 1.0 - change_probs[1][0] - change_probs[1][2]
    change_probs[2][2] = 1.0 - change_probs[2][0] - change_probs[2][1]
    for i in range(3):
        for j in range(3):
            if change_probs[i][j] == -42:
                assert False


    sorted_patches = sorted(list(patch_set))
    patch_to_index = {patch: i for i, patch in enumerate(sorted_patches)}
    index_to_patch = {i: patch for i, patch in enumerate(sorted_patches)}

    out_lines = []
    out_lines.append(f"{len(patch_set)}")
    out_lines.append(f"{patch_to_index[focal_point]}")
    out_lines.append(f"{failure_probs[0]} {failure_probs[1]} {failure_probs[2]}")
    for i in range(3):
        out_lines.append(f"{change_probs[i][0]} {change_probs[i][1]} {change_probs[i][2]}")
    for i in range(len(patch_set)):
        out_lines.append(f"{vis_map[init_vis_map.get(index_to_patch[i], '@medium')]}")
    out_lines.append(f"{num_conns}")
    for p1 in connections:
        for dir in connections[p1]:
            out_lines.append(f"{patch_to_index[p1]} {dir_map[dir]} {patch_to_index[connections[p1][dir]]}")
    out_lines.append(str(len(init_targets)))
    for t in init_targets:
        out_lines.append(f"{patch_to_index[t]}")

    out_string = "\n".join(out_lines)

    #print(out_string)
    with open(f"../../resources/EarthObservationMaps/{map}.txt", "w") as f:
        f.write(out_string)