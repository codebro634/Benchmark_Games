
for map in range(1, 21):

    with open(f"../../resources/WildlifeSetups/{map}r.txt") as f:
        data = f.read()
    print(data)

    defense_penalties = {}
    defense_rewards = {}
    poacher_max_memory = -42
    poacher_area_num_weights = {}
    max_num = -1
    areas = set()
    num_poachers = 0
    num_rangers = 0

    data = data.split("\n")
    for line in data:

        if 'ranger' in line and ";" in line:
            assert '{' in line and '}' in line
            num_rangers = len((line.split("{")[1].split("}")[0].strip()).split(","))

        if 'poacher' in line and ";" in line:
            assert '{' in line and '}' in line

            num_poachers = len((line.split("{")[1].split("}")[0].strip()).split(","))

        if 'DEFENDER-PENALTY' in line or 'DEFENDER-REWARD' in line:
            val = float(line.split("=")[1].split(";")[0].strip())
            area = int(line.split("(")[1].split(")")[0].strip()[2:]) -1
            if 'DEFENDER-PENALTY' in line:
                defense_penalties[area] = val
            else:
                defense_rewards[area] = val

        if 'POACHER-REMEMBERS' in line:
            poacher_remembers = line.split("(")[1].split(")")[0].strip()
            memory = int(poacher_remembers.split(",")[1].strip()[1:]) -1
            poacher_max_memory = max(poacher_max_memory, memory)

        if 'ATTACK-WEIGHT' in line:
            attack_weight = float(line.split("=")[1].split(";")[0].strip())
            poacher_area = line.split("(")[1].split(")")[0].strip().split(",")
            poacher = int(poacher_area[0].strip()[1:]) -1
            area = int(poacher_area[1].strip()[2:]) -1
            num = int(line.split("_")[1].split("(")[0].strip())
            max_num = max(max_num, num)
            areas.add(area)
            poacher_area_num_weights[(poacher, area, num)] = attack_weight



    out_lines = [f"{len(areas)} {num_rangers} {num_poachers} {poacher_max_memory}"]
    out_lines.append(f"{len(poacher_area_num_weights)} {max_num}")
    for poacher_area_memory in poacher_area_num_weights:
        out_lines.append(f"{poacher_area_memory[0]} {poacher_area_memory[1]} {poacher_area_memory[2]} {poacher_area_num_weights[poacher_area_memory]}")
    for i in range(len(areas)):
        out_lines.append(f"{defense_penalties.get(i,-10.0)} {defense_rewards.get(i,0.0)}")

    out_string = "\n".join(out_lines)

    print(out_string)
    with open(f"../../resources/WildlifeSetups/{map}.txt", "w") as f:
        f.write(out_string)