#include <cstddef>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "rigtform.h"
#include "scenegraph.h"
#include "sgutils.h"

/// Abstracts operations on a sequence of keyframes used to animate a scene
class KeyFrame {
public:
  using framelist_t = std::list<std::vector<RigTForm>>;
  using frameiter_t = framelist_t::iterator;

private:
  std::vector<std::shared_ptr<SgRbtNode>> nodes;
  framelist_t frames;
  size_t cursor = 0;

  /// Convenience wrapper over strtok calls after the first.
  /// Asserts nonnull output, separates by comma.
  /// Only useful for csv parsing
  char *strtok_csv() {
    char *sep = strtok(NULL, ",");
    assert(sep);
    return sep;
  }

public:
  const framelist_t &framelist() { return frames; }

  KeyFrame(std::shared_ptr<SgRootNode> root) { dumpSgRbtNodes(root, nodes); }

  /// Returns a pointer to the current frame or NULL if
  /// no key frames are initialized.
  ///
  /// This is not tracked in a long-lived iterator to avoid
  /// possible undefined behavior from dangling iterators.
  frameiter_t curr_frame() {
    frameiter_t frame = frames.begin();
    if (frame == frames.end()) {
      // cursor should be 0 when keyframes are totally empty
      assert(cursor == 0);
      return frames.end();
    }
    assert(cursor < frames.size());
    advance(frame, cursor);
    return frame;
  }

  /// If possible, refocuses on the frame to the left of the current frame
  bool move_cursor_left() {
    if (cursor == 0) {
      return false;
    }
    cursor--;
    assert(overwrite_sg_from_frame());
    return true;
  }

  /// If possible, refocuses on the frame to the right of the current frame
  /// Returns false if moving right is not possible.
  bool move_cursor_right(bool with_overwrite = true) {
    frameiter_t curr = curr_frame();
    if (curr == frames.end() || next(curr) == frames.end()) {
      return false;
    }
    cursor++;
    assert(cursor < frames.size());
    if (with_overwrite) {
      assert(overwrite_sg_from_frame());
    }
    return true;
  }

  /// Seeks to a given frame index and loads it into the scene graph. Panics
  /// if the given index is out of bounds.
  bool seek_frame(size_t frame_ind) {
    if (frames.size() <= frame_ind) {
      return false;
    }
    cursor = frame_ind;
    assert(overwrite_sg_from_frame());
    return true;
  }

  /// If the current keyframe is defined, writes its state to the
  /// parent scene graph.
  bool overwrite_sg_from_frame() {
    frameiter_t frame = curr_frame();
    if (frame == frames.end()) {
      return false;
    }
    // The number of nodes in the graph shouldn't change during the
    // course of keyframe manipulation.
    assert(frame->size() == nodes.size());
    for (size_t ind = 0; ind < nodes.size(); ind++) {
      nodes[ind]->setRbt((*frame)[ind]);
    }
    return true;
  }

  /// Overwrites the current frame with the contents of the
  /// scene graph. If there is no current frame, creates a
  /// new one.
  void overwrite_frame_from_sg() {
    frameiter_t curr = curr_frame();
    if (curr == frames.end()) {
      snapshot_scene();
      return;
    }
    assert(nodes.size() == curr->size());
    for (size_t ind = 0; ind < nodes.size(); ind++) {
      (*curr)[ind] = nodes[ind]->getRbt();
    }
  }

  /// Captures an RBT snapshot of the current scene graph and stores
  /// it as a new keyframe after the current frame.
  void snapshot_scene() {
    vector<RigTForm> new_frame;
    new_frame.reserve(nodes.size());
    for (auto it = nodes.begin(); it != nodes.end(); it++) {
      new_frame.push_back((*it)->getRbt());
    }
    assert(new_frame.size() == nodes.size());
    frameiter_t current = curr_frame();
    if (current == frames.end()) {
      assert(frames.size() == 0 && cursor == 0);
      frames.push_front(new_frame);
      // cursor is still 0, but now it's an initialized 0
      return;
    }
    frames.insert(next(current), new_frame);
    cursor++;
  }

  /// Removes the current frame from the KeyFrame sequence.
  bool del_curr_frame() {
    // using a subscope for iterator lifetimes
    frameiter_t curr = curr_frame();
    if (curr == frames.end()) {
      assert(frames.size() == 0 && cursor == 0);
      return false;
    }
    frames.erase(curr);
    // CURR IS NO LONGER VALID!!!
    if (cursor > 0) {
      cursor--;
    }
    if (curr_frame() != frames.end()) {
      assert(overwrite_sg_from_frame());
    }
    return true;
  }

  /// Prints debug info about the currently constructed frame
  void dbg_frame_info() {
    printf("cursor: %lu, len: %lu\n", cursor, frames.size());
    frameiter_t curr = curr_frame();
    if (curr == frames.end()) {
      return;
    }
    string visual = "";
    for (size_t ind = 0; ind < frames.size(); ind++) {
      if (ind == cursor) {
        visual.append("(#)");
      } else {
        visual.append("( )");
      }
      if (ind != frames.size() - 1) {
        visual.append(" -> ");
      }
    }
    printf("%s\n\n", visual.c_str());
  }

  /// Takes the current scene and serializes it into a CSV at
  /// the location of `file_path`.
  void export_scene(const string &file_path) {
    {
      frameiter_t curr = curr_frame();
      if (curr == frames.end()) {
        fprintf(stderr, "Cannot export an empty scene.\n");
        return;
      }
    }
    FILE *file = fopen(file_path.c_str(), "w+");
    if (file == nullptr) {
      fprintf(stderr, "Export failed, could not open file: %s\n",
              file_path.c_str());
      return;
    }

    fprintf(file, "frame_num,rbt_ind,tx,ty,tz,rw,rx,ry,rz\n");
    size_t frame_num = 0;
    for (frameiter_t frame = frames.begin(); frame != frames.end(); frame++) {
      for (size_t rbt_ind = 0; rbt_ind < frame->size(); rbt_ind++) {
        RigTForm &rbt = (*frame)[rbt_ind];
        Cvec3 rbt_t = rbt.getTranslation();
        Quat rbt_r = rbt.getRotation();
        fprintf(file, "%lu,%lu,%f,%f,%f,%f,%f,%f,%f\n", frame_num, rbt_ind,
                rbt_t(0), rbt_t(1), rbt_t(2), rbt_r(0), rbt_r(1), rbt_r(2),
                rbt_r(3));
      }
      frame_num++;
    }

    fclose(file);
  }

  /// Attempts to read the scene csv from the given file path and parse
  /// it into the current world. Parsing will only succeed if the data
  /// is in the right format and exactly matches the dimensions and order
  /// of the current world's objects.
  ///
  /// This is incredibly flakey and will panic unless exactly the written
  /// output of a previous scene is provided as input.
  void import_scene(const string &file_path) {
    FILE *file = fopen(file_path.c_str(), "r");
    if (file == nullptr) {
      fprintf(stderr, "Import failed, could not open file: %s\n",
              file_path.c_str());
      return;
    }

    char buf[512];
    // gets the CSV header out of the way (presumably)
    fgets(buf, sizeof(buf), file);

    frames.clear();
    cursor = 0;

    size_t curr_frame = 0;
    while (fgets(buf, sizeof(buf), file)) {
      char *sep = strtok(buf, ",");
      assert(sep);
      size_t frame_num = stoul(sep);
      size_t rbt_ind = stoul(strtok_csv());

      if (frames.begin() == frames.end() || frame_num != curr_frame) {
        assert(curr_frame == 0 || curr_frame + 1 == frame_num);
        frames.push_back(vector<RigTForm>());
        curr_frame = frame_num;
      }
      assert(frames.size() > 0);
      frameiter_t curr_frame = prev(frames.end());

      double tx = stod(strtok_csv());
      double ty = stod(strtok_csv());
      double tz = stod(strtok_csv());

      double rw = stod(strtok_csv());
      double rx = stod(strtok_csv());
      double ry = stod(strtok_csv());
      double rz = stod(strtok_csv());

      assert(curr_frame->size() == rbt_ind);
      curr_frame->push_back(RigTForm(Cvec3(tx, ty, tz), Quat(rw, rx, ry, rz)));
    }

    fclose(file);
    curr_frame = 0;
    assert(overwrite_sg_from_frame());
  }

  /// Interpolates the scene graph entries by amount alpha from left to right.
  /// Panics if left and right vectors are of different lengths or alpha is
  /// not in the range (0, 1).
  /// Uses linear interpolation.
  void lin_interp_sg(const frameiter_t &left, const frameiter_t &right,
                     double alpha) {
    if (left->size() != right->size() || left->size() != nodes.size()) {
      throw runtime_error("Cannot interpolate left, right, and scene graph "
                          "entries of different sizes.");
    }
    if (alpha <= 0 or 1. <= alpha) {
      throw runtime_error(
          "Interpolation only supports alpha in the exclusive range (0, 1).");
    }
    for (size_t ind = 0; ind < left->size(); ind++) {
      nodes[ind]->setRbt(linear_interp((*left)[ind], (*right)[ind], alpha));
    }
  }

  void cubic_interp_sg(const frameiter_t &before_left, const frameiter_t &left,
                       const frameiter_t &right, const frameiter_t &after_right,
                       double alpha) {
    if (left->size() != nodes.size() || left->size() != right->size() ||
        left->size() != before_left->size() ||
        left->size() != after_right->size()) {
      throw runtime_error(
          "Cannot interpolate BL, left, right, AR, and scene graph "
          "entries of different sizes.");
    }
    if (alpha <= 0 or 1. <= alpha) {
      throw runtime_error(
          "Interpolation only supports alpha in the exclusive range (0, 1).");
    }
    for (size_t ind = 0; ind < left->size(); ind++) {
      nodes[ind]->setRbt(cubic_interp((*before_left)[ind], (*left)[ind],
                                      (*right)[ind], (*after_right)[ind],
                                      alpha));
    }
  }
};

class Animator {

private:
  std::shared_ptr<KeyFrame> script;
  double alpha = 0.;

public:
  Animator(std::shared_ptr<KeyFrame> keyframes) : script(keyframes){};

  /// Restarts the animator to the beginning of the keyframe scene.
  /// Returns false if the scene is not well-formed to animate.
  bool restart() {
    if (script->framelist().size() < 4) {
      return false;
    }
    alpha = 0.;
    return script->seek_frame(1);
  }

  /// Progresses the scene by alpha_incr if possible.
  /// Panics if alpha is outside the exclusive range (0., 1.)
  /// Uses linear interpolation.
  bool linear_poll(double alpha_incr) {
    if (alpha_incr < 0. || 1. <= alpha_incr) {
      throw std::runtime_error("Alpha must be in the exclusive range (0, 1).");
    }
    alpha += alpha_incr;
    if (1. <= alpha) {
      script->move_cursor_right();
      alpha = 0.;
      return true;
    }

    KeyFrame::frameiter_t left = script->curr_frame();
    KeyFrame::frameiter_t right = next(left);
    auto excl_end = script->framelist().end();

    if (left == excl_end || right == excl_end) {
      // cannot progress the animation unless there's a scene after the current
      // one.
      return false;
    }

    script->lin_interp_sg(left, right, alpha);
    return true;
  }

  /// Progresses the scene by alpha_incr if possible.
  /// Panics if alpha is outside the exclusive range (0., 1.)
  /// Uses cubic interpolation.
  bool cubic_poll(double alpha_incr) {
    if (alpha_incr < 0. || 1. <= alpha_incr) {
      throw std::runtime_error("Alpha must be in the exclusive range (0, 1).");
    }
    alpha += alpha_incr;
    if (1. <= alpha) {
      script->move_cursor_right(false);
      alpha -= 1.;
    }

    // The frames must be of the form
    // (X) .. () .. (X)
    // Where left and right cannot be an (X)

    KeyFrame::frameiter_t left = script->curr_frame();
    auto incl_start = script->framelist().begin();
    auto excl_end = script->framelist().end();
    if (left == incl_start || left == excl_end) {
      return false;
    }
    KeyFrame::frameiter_t right = next(left);
    if (right == excl_end) {
      return false;
    }
    auto after_right = next(right);
    if (after_right == excl_end) {
      return false;
    }
    auto before_left = prev(left);

    script->cubic_interp_sg(before_left, left, right, after_right, alpha);
    return true;
  }
};
