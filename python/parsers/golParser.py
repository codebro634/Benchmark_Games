
for map in range(1, 11):

    with open(f"../../resources/GameOfLifeMaps/{map}R.txt") as f:
        data = f.read()
    #print(data)


    data = data.split("\n")

    x_vals = []
    y_vals = []
    prob_map = {}
    init_alive = set()

    for line in data:
        if 'x_pos' in line:
            line = line.split("{")[1].split("}")[0]
            xs = line.split(",")
            xs = [int(x[1:]) for x in xs]
            x_vals = xs
        elif 'y_pos' in line:
            line = line.split("{")[1].split("}")[0]
            ys = line.split(",")
            ys = [int(y[1:]) for y in ys]
            y_vals = ys
        elif 'NOISE-PROB' in line:
            prob = float(line.split("=")[1].split(";")[0])
            x = int(line.split("(")[1].split(",")[0][1:])
            y = int(line.split(",")[1].split(")")[0][1:])
            prob_map[(x, y)] = prob
        elif 'alive' in line:
            line = line.split("(")[1].split(")")[0]
            alive = line.split(",")
            init_alive.add((int(alive[0][1:]), int(alive[1][1:])))

    out_lines = []
    x_vals.sort()
    y_vals.sort()

    for x in x_vals:
        line = ""
        for y in y_vals:
            line += "1 " if (x, y) in init_alive else "0 "
        out_lines.append(line.strip())

    out_lines.append("")
    for i, x in enumerate(x_vals):
        line = ""
        for j,y in enumerate(y_vals):
            prob = prob_map[(x, y)] if (x, y) in prob_map else 0
            line += str(prob) + " "
        out_lines.append(line[:-1])

    out_string = "\n".join(out_lines)

    print("Map: ",map)
    print(out_string)
    print("\n")
    with open(f"../../resources/GameOfLifeMaps/{map}.txt", "w") as f:
        f.write(out_string)