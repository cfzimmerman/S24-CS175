import numpy as np
import sys
from typing import List, Tuple


def usage():
    print("cat coords.txt | python ***.py")
    print("coords.txt of form `[float x] [float y] [float z]`")


# reads x y z eye coordinates from stdin
def get_eye_coords() -> List[np.matrix]:
    eye_coords: List[np.matrix] = []
    for line in sys.stdin:
        xyz = line.strip().split(" ")
        eye_coords.append(
            np.matrix([
                [float(xyz[0])],
                [float(xyz[1])],
                [float(xyz[2])],
                [1]
            ])
        )
    return eye_coords


def get_clip(proj: np.matrix, point: np.matrix) -> np.matrix:
    return np.matmul(proj, point)


def clip_to_ndc(clip: np.matrix) -> np.matrix:
    wc = clip.item(3, 0)
    return (1 / wc) * clip


proj = np.matrix([
    [1, 0, 0, 0],
    [0, 1, 0, 0],
    [0, 0, 0, 1],
    [0, 0, -1, 0]
])


def q11_2():
    q = np.matrix([
        [3, 0, 0, 0],
        [0, 3, 0, 0],
        [0, 0, 3, 0],
        [0, 0, 0, 1]
    ])
    print(np.matmul(proj, q))
    for eye_coords in get_eye_coords():
        clip_no_q = get_clip(proj, eye_coords)
        clip_w_q = get_clip(np.matmul(proj, q), eye_coords)
        print("\n")
        # print("ndc no q:\n", clip_to_ndc(clip_no_q))
        print("ndc q:\n", clip_to_ndc(clip_w_q))


if __name__ == "__main__":
    q11_2()
