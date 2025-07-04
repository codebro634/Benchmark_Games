
for map in range(1, 15):

    with open(f"../../resources/ManufacturerSetups/{map}r.txt") as f:
        data = f.read()
    print(data)

    data = data.split("\n")
    num_goods = -42
    max_num_factories = -42
    processed_to = {}
    prices = {}
    price_trend_change_probs = {}
    factory_costs = {}
    factory_delay_probs = {}
    init_trends = {}
    init_factory = -42
    good_set = set()

    salary_marketing_manager = -42
    salary_lobbyist = -42
    salary_prod_manager = -42

    marketing_bonus = -42
    lobbyist_bonus = -42

    prob_apply_marketing = -42
    prob_apply_lobbying = -42
    prob_apply_prod = -42

    for line in data:

        if 'good' in line:
            num_goods = len(line.split(","))

        if 'PROCESSED_TO' in line:
            goods = line.split("(")[1].split(")")[0].strip()
            g1, g2 = goods.split(",")
            g1 = g1.strip()
            g2 = g2.strip()
            if g1 not in processed_to:
                processed_to[g1] = []
            processed_to[g1].append(g2)
            good_set.add(g1)
            good_set.add(g2)

        if 'PRICE_TREND_CHANGE_PROB' in line:
            prob = line.split("=")[1].split(";")[0].strip()
            good = line.split("(")[1].split(")")[0].strip()
            price_trend_change_probs[good] = prob
            good_set.add(good)

        elif 'PRICE(' in line:
            price = line.split("=")[1].split(";")[0].strip()
            good = line.split("(")[1].split(")")[0].strip()
            prices[good] = price
            good_set.add(good)

        if 'MAX_NUM_FACTORIES' in line:
            max_num_factories = line.split("=")[1].split(";")[0].strip()

        if 'BUILD_FACTORY_COST' in line:
            build_factory_cost = line.split("=")[1].split(";")[0].strip()
            good = line.split("(")[1].split(")")[0].strip()
            factory_costs[good] = build_factory_cost
            good_set.add(good)

        if 'PROB_CONSTRUCTION_DELAY_FACTORY' in line:
            prob_construction_delay_factory = line.split("=")[1].split(";")[0].strip()
            good = line.split("(")[1].split(")")[0].strip()
            good_set.add(good)
            factory_delay_probs[good] = prob_construction_delay_factory

        if 'SALARY_MARKETING_MANAGER' in line:
            salary_marketing_manager = line.split("=")[1].split(";")[0].strip()

        if 'PROB_MARKETING' in line:
            prob_apply_marketing = line.split("=")[1].split(";")[0].strip()

        if 'MARKETING_MANAGER_BONUS' in line:
            marketing_bonus = line.split("=")[1].split(";")[0].strip()

        if 'SALARY_LOBBYIST' in line:
            salary_lobbyist = line.split("=")[1].split(";")[0].strip()

        if 'PROB_LOBBYIST' in line:
            prob_apply_lobbying = line.split("=")[1].split(";")[0].strip()

        if 'LOBBYIST_BONUS' in line:
            lobbyist_bonus = line.split("=")[1].split(";")[0].strip()

        if 'SALARY_PRODUCTION_MANAGER' in line:
            salary_prod_manager = line.split("=")[1].split(";")[0].strip()

        if 'PROB_PRODUCTION_MANAGER' in line:
            prob_apply_prod = line.split("=")[1].split(";")[0].strip()

        if 'price-trend' in line:
            trend = line.split("=")[1].split(";")[0].strip()
            good = line.split("(")[1].split(")")[0].strip()
            good_set.add(good)
            init_trends[good] = trend

        if 'have-factory' in line:
            good = line.split("(")[1].split(")")[0].strip()
            good_set.add(good)
            init_factory = good

        print(good_set, line)

    goods_to_index = { good: i for i, good in enumerate(sorted(good_set)) }
    index_to_good = { i: good for i, good in enumerate(sorted(good_set)) }

    out_lines = [str(num_goods)]

    total_process_chains = 0
    for good_from, good_to in processed_to.items():
        total_process_chains += len(good_to)

    out_lines.append(str(total_process_chains))
    for good_from, good_to in processed_to.items():
        for good in good_to:
            out_lines.append(f"{goods_to_index[good_from]} {goods_to_index[good]}")

    for i in range(int(num_goods)):
        out_lines.append(prices.get(index_to_good[i], 0.0))

    out_lines.append(str(max_num_factories))
    for i in range(int(num_goods)):
        out_lines.append(f"{factory_costs.get(index_to_good[i], 0.0)}  {factory_delay_probs.get(index_to_good[i], 0.0)}")

    out_lines.append(f"{salary_marketing_manager} {salary_lobbyist} {salary_prod_manager}")

    out_lines.append(f"{prob_apply_marketing} {prob_apply_lobbying} {prob_apply_prod}")

    out_lines.append(f"{marketing_bonus} {lobbyist_bonus}")

    out_lines.append(str(goods_to_index[init_factory]))

    trend_map = {"@stable" :1, "@up" : 2, "@down": 0}

    for i in range(int(num_goods)):
        out_lines.append(f"{trend_map[init_trends.get(index_to_good[i], '@stable')]}")

    for i in range(int(num_goods)):
        out_lines.append(f"{price_trend_change_probs.get(index_to_good[i], 0.05)}")

    out_string = "\n".join(out_lines)

    print(out_string)
    with open(f"../../resources/ManufacturerSetups/{map}.txt", "w") as f:
        f.write(out_string)