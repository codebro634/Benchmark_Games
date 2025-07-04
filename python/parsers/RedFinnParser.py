
for map in range(1, 21):

    with open(f"../../resources/RedFinnedBlueEyesMaps/{map}r.txt") as f:
        data = f.read()
    print(data)

    type_to_index = {"@exceptionally-low": 0, "@very-low": 1,"@low": 2,  "@average": 3, "@high": 4, "@very-high": 5,
                     "@exceptionally-high": 6}

    fish_to_index = {"@red-finned-blue-eye": 1, "@gambusia": 2}


    water_high_prob_map = {}
    natural_mortality_prob_map = {}
    init_pops = []
    connections = []
    colonize_probs = {}
    spring_set = set()

    data = data.split("\n")
    for line in data:

        if 'POISON_SUCCESS' in line:
            poisson_succ_prob = float(line.split("=")[1].split(";")[0].strip())
        if 'TRANSLOCATION_SUCCESS' in line:
            translocation_succ_prob = float(line.split("=")[1].split(";")[0].strip())
        if 'MANUALLY_SUCCESS_PROB' in line:
            manual_succ_prob = float(line.split("=")[1].split(";")[0].strip())
        if 'ACTION_POINTS' in line:
            action_points = int(line.split("=")[1].split(";")[0].strip())
        if 'HIGH_WATER_PROB' in line:
            high_water_prob = float(line.split("=")[1].split(";")[0].strip())
            water_Type = type_to_index[line.split("(")[1].split(")")[0].strip()]
            water_high_prob_map[water_Type] = high_water_prob
        if 'NATURAL_MORTALITY_PROB' in line:
            natural_mortality_prob = float(line.split("=")[1].split(";")[0].strip())
            spring = line.split("(")[1].split(")")[0].strip()
            natural_mortality_prob_map[int(spring[1:])] = natural_mortality_prob
            spring_set.add(int(spring[1:]))
        if 'population' in line:
            population = line.split("=")[1].split(";")[0].strip()
            spring = line.split("(")[1].split(")")[0].strip()
            spring_set.add(int(spring[1:]))
            init_pops.append((int(spring[1:]), fish_to_index[population]))
        if 'COLONIZE_PROB' in line:
            colonize_prob = float(line.split("=")[1].split(";")[0].strip())
            springs = line.split("(")[1].split(")")[0].strip()
            spring1, spring2 = springs.split(",")
            spring1 = spring1.strip()
            spring2 = spring2.strip()
            spring1 = int(spring1[1:])
            spring2 = int(spring2[1:])
            spring_set.add(spring1)
            spring_set.add(spring2)
            colonize_probs[(spring1, spring2)] = colonize_prob
            colonize_probs[(spring2, spring1)] = colonize_prob

        if 'SPRINGS_CONNECTED' in line:
            springs = line.split("(")[1].split(")")[0].strip()
            spring1, spring2, water_lvl = springs.split(",")
            spring1 = spring1.strip()
            spring2 = spring2.strip()
            water_lvl = type_to_index[water_lvl.strip()]
            spring1 = int(spring1[1:])
            spring2 = int(spring2[1:])
            spring_set.add(spring1)
            spring_set.add(spring2)
            connections.append((spring1, spring2, water_lvl))
            connections.append((spring2, spring1, water_lvl))

    assert len(spring_set) == max(spring_set)+1

    out_lines = [str(len(spring_set))]
    out_lines.append(str(action_points))
    out_lines.append(f"{poisson_succ_prob} {translocation_succ_prob} {manual_succ_prob}")
    for i in range(7):
        out_lines.append(f"{water_high_prob_map[i]}")
    for i in range(len(spring_set)):
        assert i in spring_set
        out_lines.append(f"{natural_mortality_prob_map.get(i,0.0)}")
    out_lines.append(str(len(connections)))
    for s1,s2,water in connections:
        gambusian_prob = colonize_probs.get((s1,s2),0.0)
        out_lines.append(f"{s1} {s2} {water} {gambusian_prob}")
    out_lines.append(str(len(init_pops)))
    for spring, pop in init_pops:
        out_lines.append(f"{spring} {pop}")

    out_string = "\n".join(out_lines)

    #print(out_string)
    with open(f"../../resources/RedFinnedBlueEyesMaps/{map}.txt", "w") as f:
        f.write(out_string)