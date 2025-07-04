import matplotlib.pyplot as plt
import numpy as np
import matplotlib.patches as patches

def visualize_grid(grid_string, dots=None):

    grid_lines = grid_string.strip().split("\n")
    grid = [line.split() for line in grid_lines]

    rows = len(grid)
    cols = len(grid[0]) if rows > 0 else 0

    # Convert grid to numpy array
    matrix = np.array([[0.3 if cell == 'W' else 1 for cell in row] for row in grid])  # Light gray for 'W'

    fig, ax = plt.subplots(figsize=(cols/2, rows/2))
    ax.imshow(matrix, cmap='gray', origin='upper', vmin=0, vmax=1)

    # Draw grid outlines
    for x in range(cols + 1):
        ax.axvline(x - 0.50, color='black', linewidth=0.5)
    for y in range(rows + 1):
        ax.axhline(y - 0.50, color='black', linewidth=0.5)

    if dots is not None:
        for dot_type in dots:
            dot_pos, color, shape = dot_type
            for x, y in dot_pos:
                if shape == 'circle':
                    circle = patches.Circle((y, x), 0.3, color=color)
                    ax.add_patch(circle)
                elif shape == 'triangle':
                    triangle = patches.Polygon(
                        [[y - 0.3, x + 0.3], [y + 0.3, x + 0.3], [y, x - 0.3]],
                        closed=True, color=color
                    )
                    ax.add_patch(triangle)

    ax.set_xticks([])
    ax.set_yticks([])
    ax.grid(visible=False)

    plt.show()

# Example usage
grid_string = """
W  W  W  W  W  W  W  W  W  W  W  W  W  W  W  W  W
W  .  .  .  .  .  W  W  W  W  W  W  W  W  W  W  W
W  .  .  .  .  .  .  W  .  .  .  .  .  .  .  W  W
W  .  .  .  .  .  .  .  .  .  .  .  .  .  .  W  W
W  .  .  .  .  .  .  .  .  .  .  .  W  W  .  W  W
W  .  .  .  .  .  W  W  .  .  .  .  W  W  .  W  W
W  W  .  .  .  .  W  W  .  .  .  .  .  .  .  W  W
W  W  W  W  .  .  .  W  W  W  .  .  .  .  .  W  W
W  W  W  .  .  .  .  W  W  W  W  .  .  .  W  W  W
W  W  .  .  .  .  .  .  .  W  W  W  .  .  W  W  W
W  W  .  .  .  .  .  .  .  .  W  W  .  .  .  W  W
W  W  .  .  W  W  .  .  .  .  .  .  .  .  .  .  W
W  W  .  .  W  W  .  .  .  .  .  .  .  .  .  .  W
W  W  .  .  .  .  .  .  .  W  .  .  .  .  .  .  W
W  W  .  .  .  .  .  .  W  W  .  .  .  .  .  .  W
W  W  W  W  W  W  W  W  W  W  .  .  .  .  .  .  W
W  W  W  W  W  W  W  W  W  W  W  W  W  W  W  W  W
"""

#KTK
# visualize_grid(grid_string, dots = [([(14, 7)], 'brown', 'triangle'), ([(10, 11)], 'green', 'triangle'), ([(10, 15)], 'gold', 'triangle'), ([(13, 9)], 'blue', 'triangle'),
#                                     ([(14, 19)], 'brown', 'circle'), ([(11, 24)], 'green', 'circle'), ([(12, 23)], 'gold', 'circle'), ([(10, 27)], 'blue', 'circle')])


#PUSHER
# visualize_grid(grid_string,  dots = [([(1, 2)], 'blue', 'triangle'), ([(2, 2)], 'blue', 'triangle'), ([(1, 4)], 'blue', 'triangle'), ([(2, 4)], 'blue', 'triangle'),
#                                      ([(11, 2)], 'red', 'circle'), ([(12, 2)], 'red', 'circle'), ([(11, 4)], 'red', 'circle'), ([(12, 4)], 'red', 'circle')])

#CTF
visualize_grid(grid_string,  dots = [([(3, 2)], 'blue', 'triangle'), ([(3, 3)], 'blue', 'triangle'), ([(2, 3)], 'blue', 'triangle'),([(2, 2)], 'gold', 'triangle'),
                                     ([(13,14)], 'red', 'circle'), ([(13, 13)], 'red', 'circle'), ([(14, 13)], 'red', 'circle'), ([(14, 14)], 'gold', 'circle')])