from collections import defaultdict

for i in range(1, 11):

    with open(f"../../resources/TriangleTireworlds/{i}.txt") as f:
        data = f.read()
    print(data)


    roads_connections = []
    roads = []
    init_spares = []

    data = data.split("\n")
    for line in data:
        if "FLAT-PROB" in line:
            flat_prob = line.split("=")[1].split(";")[0].strip()
        if "goal" in line:
            goal_road = line.split("(")[1].split(")")[0].strip()
        if 'vehicle-at' in line:
            vehicle_at = line.split("(")[1].split(")")[0].strip()
        if 'road' in line:
            road_from = line.split("(")[1].split(",")[0].strip()
            road_to = line.split(",")[1].split(")")[0].strip()
            roads_connections.append([road_from,road_to])
        if 'spare' in line:
            init_spares.append(line.split("(")[1].split(")")[0].strip())
        if 'location :' in line:
            road_list = line.split("{")[1].split("}")[0].strip()
            roads = [road.strip() for road in road_list.split(",")]


    roads_to_indices = {road: i for i, road in enumerate(roads)}
    init_pos_index = roads_to_indices[vehicle_at]
    goal_pos_index = roads_to_indices[goal_road]
    init_spares = [roads_to_indices[road] for road in init_spares]



    out_lines = []
    out_lines.append(f"{flat_prob}")
    out_lines.append(f"{len(roads)}")
    out_lines.append(f"{len(roads_connections)}")
    for road_from, road_to in roads_connections:
        out_lines.append(f"{roads_to_indices[road_from]} {roads_to_indices[road_to]}")
    out_lines.append(f"{goal_pos_index}")
    out_lines.append(f"{init_pos_index}")
    out_lines.append(f"{len(init_spares)}")
    for init_spare in init_spares:
        out_lines.append(f"{init_spare}")

    out_string = "\n".join(out_lines)

    #print(out_string)
    with open(f"../../resources/TriangleTireworlds/{i}.txt", "w") as f:
        f.write(out_string)