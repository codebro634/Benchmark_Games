
for map in range(1, 11):

    with open(f"../../resources/NavigationMaps/{map}R.txt") as f:
        data = f.read()
    print(data)


    data = data.split("\n")

    x_vals = []
    y_vals = []
    prob_map = {}

    for line in data:
        if 'xpos' in line:
            line = line.split("{")[1].split("}")[0]
            xs = line.split(",")
            xs = [int(x[1:]) for x in xs]
            x_vals = xs
        elif 'ypos' in line:
            line = line.split("{")[1].split("}")[0]
            ys = line.split(",")
            ys = [int(y[1:]) for y in ys]
            y_vals = ys
        elif 'P(' in line:
            prob = float(line.split("=")[1].split(";")[0])
            x = int(line.split("(")[1].split(",")[0][1:])
            y = int(line.split(",")[1].split(")")[0][1:])
            prob_map[(x, y)] = prob

    out_lines = [str(len(x_vals)-1) + ",0"]
    out_lines.append(str(len(x_vals)-1) + "," + str(len(y_vals)-1))

    x_vals.sort()
    y_vals.sort()

    for i, y in enumerate(y_vals):
        line = ""
        for j,x in enumerate(x_vals):
            prob = prob_map[(x, y)] if (x, y) in prob_map else 0
            line += str(prob) + ","
        out_lines.append(line[:-1])

    out_string = "\n".join(out_lines)

    print(out_string)
    with open(f"../../resources/NavigationMaps/{map}.txt", "w") as f:
        f.write(out_string)