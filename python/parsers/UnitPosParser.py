
read_filename = "../../resources/test.txt"
out_filename = "../../resources/test_out.txt"

p0_units = []
p1_units = []

with open(read_filename, "r") as file:
    lines = file.readlines()
    map_starts = False
    map_y_base = 0
    for y,line in enumerate(lines):
        if line.startswith("Map"):
            map_starts = True
            map_y_base = y+1
            continue
        elif line.strip() == "":
            map_starts = False
            with open(out_filename, "a") as file:
                for unit in p0_units:
                    write_str = str(unit[0]) + " " + str(unit[1]) + " " + str(unit[2]) + " " + str(unit[3])
                    file.write(write_str + "\n")
                for unit in p1_units:
                    write_str = str(unit[0]) + " " + str(unit[1]) + " " + str(unit[2]) + " " + str(unit[3])
                    file.write(write_str + "\n")
                file.write("\n")
            p0_units = []
            p1_units = []
        if map_starts:
            tiles = line.split(" ")
            #discard empty tiles
            tiles = [tile for tile in tiles if tile != ""]
            for x,tile in enumerate(tiles):
                if tile == "w0":
                    p0_units.append((x,y-map_y_base,0,0))
                elif tile == "w1":
                    p1_units.append((x,y-map_y_base,0,1))
                elif tile == "h0":
                    p0_units.append((x,y-map_y_base,1,0))
                elif tile == "h1":
                    p1_units.append((x,y-map_y_base,1,1))
                elif tile == "a0":
                    p0_units.append((x,y-map_y_base,2,0))
                elif tile == "a1":
                    p1_units.append((x,y-map_y_base,2,1))
                elif tile == "k0":
                    p0_units.append((x,y-map_y_base,3,0))
                elif tile == "k1":
                    p1_units.append((x,y-map_y_base,3,1))

