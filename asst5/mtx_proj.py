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


def q11_2():
    q = np.array([
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
        print("ndc no q:\n", clip_to_ndc(clip_no_q))
        print("ndc q:\n", clip_to_ndc(clip_w_q))


def q11_3():
    s = np.array([
        [3, 0, 0, 0],
        [0, 3, 0, 0],
        [0, 0, 3, 0],
        [0, 0, 0, 3]
    ])
    for eye_coords in get_eye_coords():
        clip_no_s = get_clip(proj, eye_coords)
        clip_w_s = get_clip(np.matmul(proj, s), eye_coords)
        print("\n")
        print("ndc no s:\n", clip_to_ndc(clip_no_s))
        print("ndc s:\n", clip_to_ndc(clip_w_s))


def q11_4():
    q = np.array([
        [3, 0, 0, 0],
        [0, 3, 0, 0],
        [0, 0, 3, 0],
        [0, 0, 0, 1]
    ])
    for eye_coords in get_eye_coords():
        clip_no_q = get_clip(proj, eye_coords)
        clip_w_q = get_clip(np.matmul(q, proj), eye_coords)
        print("\n")
        # print("ndc no q:\n", clip_to_ndc(clip_no_q))
        print("ndc q:\n", clip_to_ndc(clip_w_q))


def get_viewport(w: float, h: float) -> np.array:
    return np.array([
        [w / 2, 0, 0, (w - 1) / 2],
        [0, h / 2, 0, (h - 1) / 2],
        [0, 0, 0.5, 0.5],
        [0, 0, 0, 1]
    ])


def to_vector(mtx: np.array):
    print(mtx.transpose().tolist()[0])


def q12_3():
    width = 512
    height = 256
    # Note that this q is different from the previous.
    # Names follow those in the assignment
    q = np.array([
        [3, 0, 0, 0],
        [0, 3, 0, 0],
        [0, 0, 1, 0],
        [0, 0, 0, 1]
    ])
    view = get_viewport(width, height)
    q_view = np.matmul(q, view)

    for eye_coords in get_eye_coords():
        ndc = clip_to_ndc(get_clip(proj, eye_coords))
        print("original window: ",
              np.matmul(view, ndc)
                .transpose()
                .tolist())
        print("q window: ",
              np.matmul(q_view, ndc)
              .transpose()
              .tolist())


def ps6():
    width = 512
    height = 512
    view = get_viewport(width, height)
    ndc = np.array([
        [-0.1],
        [0.26],
        [0],
        [1]
    ])
    print(to_vector(np.matmul(view, ndc)))


if __name__ == "__main__":
    # q11_2()
    # q11_3()
    # q11_4()
    # q12_3()
    ps6()
