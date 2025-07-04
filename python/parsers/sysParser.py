
for map in range(1, 11):

    with open(f"../../resources/SysAdminTopologies/{map}R.txt") as f:
        data = f.read()
    #print(data)


    data = data.split("\n")

    comps = []
    connections = {}
    reboot_prob = 0

    for line in data:
        if 'computer' in line:
            line = line.split("{")[1].split("}")[0]
            xs = line.split(",")
            comps = [int(x[1:]) for x in xs]
        elif 'CONNECTED' in line:
            line = line.split("(")[1].split(")")[0]
            cmps = line.split(",")
            from_cmp = int(cmps[1][1:])
            if from_cmp not in connections:
                connections[from_cmp] = []
            connections[from_cmp].append(int(cmps[0][1:]))
        elif 'REBOOT-PROB' in line:
            reboot_prob = float(line.split("=")[1].split(";")[0])

    out_lines = []
    comps.sort()
    comps_to_idx = {comp: i for i, comp in enumerate(comps)}

    out_lines.append(str(reboot_prob))
    out_lines.append(str(len(comps)))
    for cmp in comps:
        line = ""
        if cmp in connections:
            for cmp2 in connections[cmp]:
                line += str(comps_to_idx[cmp2]) + " "
        else:
            line = "empty"
        out_lines.append(line.strip())

    out_string = "\n".join(out_lines)

    print("Map: ",map)
    print(out_string)
    print("\n")
    with open(f"../../resources/SysAdminTopologies/{map}.txt", "w") as f:
        f.write(out_string)