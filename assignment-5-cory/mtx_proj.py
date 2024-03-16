import numpy as np
import sys
from typing import List, Tuple


def usage():
    print("cat coords.txt | python ***.py")
    print("coords.txt of form `[float x] [float y] [float z]`")


# reads x y z eye coordinates from stdin
def get_eye_coords() -> List[np.array]:
    eye_coords: List[np.array] = []
    for line in sys.stdin:
        xyz = line.strip().split(" ")
        eye_coords.append(
            np.array([
                [float(xyz[0])],
                [float(xyz[1])],
                [float(xyz[2])],
                [1]
            ])
        )
    return eye_coords


def get_clip(proj: np.array, point: np.array) -> np.array:
    return np.matmul(proj, point)


def clip_to_ndc(clip: np.array) -> np.array:
    wc = clip.item(3, 0)
    return (1 / wc) * clip


proj = np.array([
    [1, 0, 0, 0],
    [0, 1, 0, 0],
    [0, 0, 0, 1],
    [0, 0, -1, 0]
])

q = np.array([
    [3, 0, 0, 0],
    [0, 3, 0, 0],
    [0, 0, 3, 0],
    [0, 0, 0, 1]
])

s = np.array([
    [3, 0, 0, 0],
    [0, 3, 0, 0],
    [0, 0, 3, 0],
    [0, 0, 0, 3]
])


def q11_2():
    print(np.matmul(proj, q))
    for eye_coords in get_eye_coords():
        clip_no_q = get_clip(proj, eye_coords)
        clip_w_q = get_clip(np.matmul(proj, q), eye_coords)
        print("\n")
        print("ndc no q:\n", clip_to_ndc(clip_no_q))
        print("ndc q:\n", clip_to_ndc(clip_w_q))


def q11_3():
    for eye_coords in get_eye_coords():
        clip_no_s = get_clip(proj, eye_coords)
        clip_w_s = get_clip(np.matmul(proj, s), eye_coords)
        print("\n")
        print("ndc no s:\n", clip_to_ndc(clip_no_s))
        print("ndc s:\n", clip_to_ndc(clip_w_s))


def q11_4():
    for eye_coords in get_eye_coords():
        clip_no_q = get_clip(proj, eye_coords)
        clip_w_q = get_clip(np.matmul(q, proj), eye_coords)
        print("\n")
        # print("ndc no q:\n", clip_to_ndc(clip_no_q))
        print("ndc q:\n", clip_to_ndc(clip_w_q))


if __name__ == "__main__":
    # q11_2()
    # q11_3()
    q11_4()
