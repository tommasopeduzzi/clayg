use_probability_list = True

distances = [10, 12, 14, 16, 18]

# For probability list mode
probabilities = [0.01, 0.005]

# For range mode
p_start = 0.0001
p_end = 0.05
p_step = 1.2  # this is passed directly as multiplier

with open("params.txt", "w") as f:
    if use_probability_list:
        for d in distances:
            for p in probabilities:
                f.write(f"{d} prob {p}\n")
    else:
        for d in distances:
            f.write(f"{d} range {p_start} {p_end} {p_step}\n")