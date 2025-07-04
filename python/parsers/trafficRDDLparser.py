import re

def parse_rddl(rddl_str):
    # --- 1. Extract the list of intersections and cells ---

    # Regex to capture objects in the form: intersection : {name1,name2,...};
    intersection_block = re.search(r'intersection\s*:\s*\{([^}]*)\}', rddl_str, re.MULTILINE)
    cell_block = re.search(r'cell\s*:\s*\{([^}]*)\}', rddl_str, re.MULTILINE)

    if not intersection_block or not cell_block:
        raise ValueError("Could not find intersection or cell definitions in RDDL.")

    # Split the names inside the braces, remove any extra whitespace
    intersections = [x.strip() for x in intersection_block.group(1).split(',')]
    cells = [x.strip() for x in cell_block.group(1).split(',')]

    # Build ID mappings (start from 0)
    intersection_to_id = {name: i for i, name in enumerate(intersections)}
    cell_to_id = {name: i for i, name in enumerate(cells)}

    # --- 2. Prepare data structures to hold the relationships ---
    input_cells = set()
    input_rates = {}
    exit_cells = set()
    flows_cell_to_cell = []
    flows_cell_to_int_ns = []
    flows_cell_to_int_ew = []
    occupied_cells = set()

    # --- 3. Parse the lines of "non-fluents" to fill in data structures ---
    # We'll look for lines of the form:
    #   PERIMETER-INPUT-CELL(cellName);
    #   PERIMETER-INPUT-RATE(cellName) = float;
    #   PERIMETER-EXIT-CELL(cellName);
    #   FLOWS-INTO-INTERSECTION-NS(cellName, intersectionName);
    #   FLOWS-INTO-INTERSECTION-EW(cellName, intersectionName);
    #   FLOWS-INTO-CELL(cellName1, cellName2);

    lines = rddl_str.splitlines()
    for line in lines:
        line = line.strip()
        if not line or line.startswith("//"):
            continue

        # 1) PERIMETER-INPUT-CELL(...)
        m = re.match(r'PERIMETER-INPUT-CELL\s*\(\s*(\w+)\s*\)', line)
        if m:
            c_name = m.group(1)
            input_cells.add(c_name)
            continue

        # 2) PERIMETER-INPUT-RATE(cellName) = X;
        m = re.match(r'PERIMETER-INPUT-RATE\s*\(\s*(\w+)\s*\)\s*=\s*([\d\.Ee+-]+)', line)
        if m:
            c_name, rate = m.group(1), float(m.group(2))
            input_rates[c_name] = rate
            continue

        # 3) PERIMETER-EXIT-CELL(...)
        m = re.match(r'PERIMETER-EXIT-CELL\s*\(\s*(\w+)\s*\)', line)
        if m:
            c_name = m.group(1)
            exit_cells.add(c_name)
            continue

        # 4) FLOWS-INTO-INTERSECTION-NS(cell, intersection)
        m = re.match(r'FLOWS-INTO-INTERSECTION-NS\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)', line)
        if m:
            c_name, i_name = m.groups()
            flows_cell_to_int_ns.append((c_name, i_name))
            continue

        # 5) FLOWS-INTO-INTERSECTION-EW(cell, intersection)
        m = re.match(r'FLOWS-INTO-INTERSECTION-EW\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)', line)
        if m:
            c_name, i_name = m.groups()
            flows_cell_to_int_ew.append((c_name, i_name))
            continue

        # 6) FLOWS-INTO-CELL(cell1, cell2)
        m = re.match(r'FLOWS-INTO-CELL\s*\(\s*(\w+)\s*,\s*(\w+)\s*\)', line)
        if m:
            c_name1, c_name2 = m.groups()
            flows_cell_to_cell.append((c_name1, c_name2))
            continue

        # 7) OCCUPIED(cell)
        m = re.match(r'occupied\s*\(\s*(\w+)\s*\)', line)
        if m:
            c_name = m.group(1)
            occupied_cells.add(c_name)
            continue

    # --- 4. Convert names to IDs and produce output in the desired format ---
    # The desired output format is (example layout):
    #
    #   numIntersections
    #   numCells
    #   number_of_input_cells
    #   cellId1 inputRate1
    #   cellId2 inputRate2
    #   ...
    #   number_of_exit_cells
    #   exitCellId1
    #   exitCellId2
    #   ...
    #   number_of_flows_cell_to_cell
    #   fromCellId toCellId
    #   ...
    #   number_of_flows_cell_to_intersection_NS
    #   cellId intersectionId
    #   ...
    #   number_of_flows_cell_to_intersection_EW
    #   cellId intersectionId
    #   ...
    #

    # Prepare strings to print
    out_lines = []

    # 1) numIntersections
    out_lines.append(str(len(intersections)))

    # 2) numCells
    out_lines.append(str(len(cells)))

    # 3) number_of_input_cells
    out_lines.append(str(len(input_cells)))
    # For each input cell, print "cellId rate"
    # If a particular input cell doesn't have a rate, you could default to 0.0 or skip it
    for c_name in sorted(input_cells, key=lambda x: cell_to_id[x]):
        c_id = cell_to_id[c_name]
        rate = input_rates.get(c_name, 0.0)
        out_lines.append(f"{c_id} {rate}")

    # 4) number_of_exit_cells
    out_lines.append(str(len(exit_cells)))
    # Then each exit cell ID on its own line
    for c_name in sorted(exit_cells, key=lambda x: cell_to_id[x]):
        c_id = cell_to_id[c_name]
        out_lines.append(str(c_id))

    # 5) number_of_flows_cell_to_cell
    out_lines.append(str(len(flows_cell_to_cell)))
    # Then lines "fromCellId toCellId"
    for (src, dst) in flows_cell_to_cell:
        src_id = cell_to_id[src]
        dst_id = cell_to_id[dst]
        out_lines.append(f"{src_id} {dst_id}")

    # 6) number_of_flows_cell_to_intersection_NS
    out_lines.append(str(len(flows_cell_to_int_ns)))
    # Then lines "cellId intersectionId"
    for (c_name, i_name) in flows_cell_to_int_ns:
        c_id = cell_to_id[c_name]
        i_id = intersection_to_id[i_name]
        out_lines.append(f"{c_id} {i_id}")

    # 7) number_of_flows_cell_to_intersection_EW
    out_lines.append(str(len(flows_cell_to_int_ew)))
    # Then lines "cellId intersectionId"
    for (c_name, i_name) in flows_cell_to_int_ew:
        c_id = cell_to_id[c_name]
        i_id = intersection_to_id[i_name]
        out_lines.append(f"{c_id} {i_id}")

    # 8) number_of_occupied_cells
    out_lines.append(str(len(occupied_cells)))
    # Then each occupied cell ID on its own line
    for c_name in sorted(occupied_cells, key=lambda x: cell_to_id[x]):
        c_id = cell_to_id[c_name]
        out_lines.append(str(c_id))

    # Combine everything into the final output text
    final_output = "\n".join(out_lines)
    return final_output


if __name__ == "__main__":
    for i in range(1,2):
        print(i)
        with open(f"../../resources/TrafficModels/{i}.txt") as f:
            rddl_text = f.read()
        parsed_output = parse_rddl(rddl_text)
        with open(f"../../resources/TrafficModels/{i}.txt", "w") as f:
            f.write(parsed_output)
