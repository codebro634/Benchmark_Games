
for map in range(1, 2):

    with open(f"../../resources/DiceProbs/{map}.txt") as f:
        data = f.read()
    print(data)

    value_map = {}
    prob_map = {}

    data = data.split("\n")
    for line in data:

        if 'VALUE' in line:
            value = line.split("=")[1].split(";")[0].strip()
            value_name = line.split("(")[1].split(")")[0].strip()
            value_map[value_name] = value

        if 'PROB' in line:
            prob = line.split("=")[1].split(";")[0].strip()
            prob_key = line.split("(")[1].split(")")[0].strip()
            dice, value = prob_key.split(",")
            dice = dice.strip()
            value = value.strip()
            if dice not in prob_map:
                prob_map[dice] = {}
            prob_map[dice][value] = prob


    sorted_dices = sorted(prob_map.keys(), key=lambda x: int(x[1:]))
    dice_to_index = { dice: i for i, dice in enumerate(sorted_dices) }
    index_to_dice = { i: dice for i, dice in enumerate(sorted_dices) }
    sorted_values = sorted(value_map.keys(), key=lambda x: int(x[1:]))

    out_lines = [str(len(prob_map.keys())) + " " + str(len(value_map.keys()))]

    for val_key in sorted_values:
        out_lines.append(f"{value_map[val_key]}")

    for dice in sorted_dices:
        dice_probs = ""
        for value in sorted_values:
            dice_probs += f"{prob_map[dice].get(value,0.0)}" + " "
        out_lines.append(dice_probs)

    out_string = "\n".join(out_lines)

    print(out_string)
    with open(f"../../resources/DiceProbs/{map}.txt", "w") as f:
        f.write(out_string)