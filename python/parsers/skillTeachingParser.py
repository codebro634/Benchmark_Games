from collections import defaultdict

for i in range(1, 11):

    with open(f"../../resources/SkillsTeachingSkills/{i}.txt") as f:
        data = f.read()

    skills_prob_all_pre = defaultdict(lambda: '0.8')
    skills_prob_per_pre = defaultdict(lambda: '0.1')
    skills_prob_all_pre_med = defaultdict(lambda: '1.0')
    skills_prob_per_pre_med = defaultdict(lambda: '0.3')
    skills_prob_high = defaultdict(lambda: '0.9')
    skills_skill_weight = defaultdict(lambda: '0.8')
    skills_lose_prob = defaultdict(lambda: '0.02')
    skills_pre_reqs = {}
    skill_set = set()

    data = data.split("\n")
    for line in data:
        non_fluent = line.split("(")[0]

        if non_fluent == "PRE_REQ":
            skill1 = line.split("(")[1].split(",")[0][1]
            skill2 = line.split(",")[1].split(")")[0].strip()[1]
            skill_set = skill_set.union({skill1, skill2})
            if skill2 in skills_pre_reqs:
                skills_pre_reqs[skill2].append(skill1)
            else:
                skills_pre_reqs[skill2] = [skill1]
            continue

        value = line.split("=")[1].split(";")[0].strip()
        skill = line.split("(")[1].split(")")[0][1]
        skill_set.add(skill)
        if non_fluent == "PROB_ALL_PRE":
            skills_prob_all_pre[skill] = value
        elif non_fluent == "PROB_PER_PRE":
            skills_prob_per_pre[skill] = value
        elif non_fluent == "PROB_ALL_PRE_MED":
            skills_prob_all_pre_med[skill] = value
        elif non_fluent == "PROB_PER_PRE_MED":
            skills_prob_per_pre_med[skill] = value
        elif non_fluent == "PROB_HIGH":
            skills_prob_high[skill] = value
        elif non_fluent == "SKILL_WEIGHT":
            skills_skill_weight[skill] = value
        elif non_fluent == "LOSE_PROB":
            skills_lose_prob[skill] = value

    out_lines = []
    out_lines.append(str(len(skill_set))+ " " + str(len(skills_pre_reqs)))
    for skill,pre_reqs in skills_pre_reqs.items():
        for pre in pre_reqs:
            out_lines.append(f"{pre} {skill}")
    for skill in sorted(skill_set):
        out_lines.append(f"{skills_skill_weight[skill]} "
                         f"{skills_prob_high[skill]} "
                         f"{skills_prob_all_pre[skill]}"
                         f" {skills_prob_all_pre_med[skill]} "
                         f"{skills_prob_per_pre[skill]} "
                         f"{skills_prob_per_pre_med[skill]}"
                          f" {skills_lose_prob[skill]}")

    out_string = "\n".join(out_lines)

    with open(f"../../resources/SkillsTeachingSkills/{i}.txt", "w") as f:
        f.write(out_string)