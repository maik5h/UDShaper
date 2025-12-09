import csv
from pathlib import Path

import matplotlib.pyplot as plt


SAMPLE_RATE = 44100


def plot_shape_editor_performance(
        x_data: list[list[int]],
        y_data: list[list[float]],
        labels: list[str],
        outfile: Path,
        title: str = '',
        ylims: list[float] = [],
    ) -> None:
    """Plot the ShapeEditor::forward time against the number of points.

    Can display multiple datasets. x_data, y_data and labels are
    expected to have the same len, with pairs of x- and y-data arrays
    and a label.

    The left y-axis on the plot corresponds to the absolute time the
    ShapeEditor::forward call took. The right y-axis is the time
    relative to the time between two samples at 44100 Hz. This is
    equivalent to the 'CPU load' given in FL Studio.

    Parameters
    ----------
    x_data: `list[list[int]]`
        List of point numbers at which measurements were performed.

    y_data: `list[list[float]]`
        List of time intervals measured with the number of points
        corresponding to x_data.

    labels: `list[str]`
        Labels to distinguish the given datasets.

    outfile: `Path`
        File to save the figure.

    title: `str`
        Title of the figure.

    ylims: `list[float]`
        ylims of the plot. This corresponds to the computation time in
        microseconds.
    """

    # Plot the time per forward call depending on the number of points.
    # Use two y-axes, the first shows the absolute time in
    # microseconds, the second one shows how much that is in relation
    # to the available time per sample.
    fig, ax1 = plt.subplots()
    for i in range(len(x_data)):
        ax1.plot(x_data[i], y_data[i], '.-', label=labels[i])
    # If no ylims are given, use the default upper lim and 0 as lower lim.
    if not ylims:
        ylims = ax1.set_ylim()
        ylims = ax1.set_ylim((0, ylims[1]))
    else:
        ax1.set_ylim(ylims)
    ax1.set_xlabel('Number of points')
    ax1.set_ylabel('time [$\mu s$]')

    # Create a second axis and scale the ticks to represent fractions
    # of one sample.
    ax2 = ax1.twinx()
    ax2.set_ylabel('CPU load')
    # Go from microseconds to seconds, seconds to samples.
    scale = SAMPLE_RATE / 1e6
    ticks = ax2.get_yticks()
    ticks_label = [f'{(ticks[n] * ylims[1] * scale * 100):.2f}%' for n in range(len(ticks))]
    ax2.set_yticks(ticks)
    ax2.set_yticklabels(ticks_label)

    ax1.legend()
    plt.grid()
    plt.title(title)
    plt.tight_layout()

    if outfile:
        plt.savefig(outfile, dpi=300)
    plt.show()


def load_dataset(path: Path) -> tuple[list, list]:
    """
    Load a ShapeEditor::forward performance dataset.

    Returns a list containing the number of points and the
    measurements corresponding to these points.
    """
    out = ([], [])
    with open(path, 'r') as f:
        file = csv.reader(f)

        for line in file:
            out[0].append(int(line[0]))
            out[1].append(float(line[2]))

    return out

if __name__ == "__main__":
    parent_dir = Path(__file__).parent

    # Performance data recorded by iterating through all points and
    # comparing their x-position.
    fwd_iter = load_dataset(parent_dir / 'data/data_point_vector.csv')

    # Performance recorded by using a binary search.
    fwd_binary_search = load_dataset(parent_dir / 'data/data_binary_search.csv')

    plot_shape_editor_performance(
        x_data=[fwd_iter[0], fwd_binary_search[0]],
        y_data=[fwd_iter[1], fwd_binary_search[1]],
        labels=['iterative', 'binary search'],
        outfile=parent_dir / 'plots/performance_binary_search.png',
        title='ShapeEditor forward call\nBinary search vs iterating',
        ylims = [0, 0.3]
    )
