
for map in range(1, 11):

    with open(f"../../resources/AcademicAdvisingCourses/{map}.txt") as f:
        data = f.read()
    print(data)


    data = data.split("\n")
    courses = []
    prereqs = {}
    reqs = []

    max_actions = 0

    for line in data:
        if 'max-nondef-actions' in line:
            max_actions = int(line.split("=")[1].split(";")[0])
        elif 'course' in line:
            course = line.split("{")[1].split("}")[0]
            courses = [int(x.strip()[2:]) for x in course.split(",")]
        elif 'PREREQ' in line:
            line = line.split("(")[1].split(")")[0]
            course = int(line.split(",")[1][2:])
            prereq = int(line.split(",")[0][2:])
            if course not in prereqs:
                prereqs[course] = []
            prereqs[course].append(prereq)
        elif  'PROGRAM_REQUIREMENT' in line:
            line = line.split("(")[1].split(")")[0][2:]
            reqs.append(int(line))

    courses.sort()
    courses_to_idx = {course: i for i, course in enumerate(courses)}

    out_lines = [str(len(courses))+" "+str(max_actions)]

    req_line = ""
    for req in reqs:
        req_line += str(courses_to_idx[req]) + " "
    out_lines.append(req_line.strip())

    for course in courses:
        line = ""
        if course in prereqs:
            for prereq in prereqs[course]:
                line += str(courses_to_idx[prereq]) + " "
        else:
            line += "empty"
        out_lines.append(line.strip())

    out_string = "\n".join(out_lines)

    print("Map: ",map)
    print(out_string)
    print("\n")
    with open(f"../../resources/AcademicAdvisingCourses/{map}_IPPC.txt", "w") as f:
        f.write(out_string)