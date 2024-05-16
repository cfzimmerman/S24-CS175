#ifndef PICKER_H
#define PICKER_H

#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

#include "asstcommon.h"
#include "cvec.h"
#include "drawer.h"
#include "ppm.h"
#include "scenegraph.h"

class Picker : public SgNodeVisitor {
    std::vector<std::shared_ptr<SgNode>> nodeStack_;

    typedef std::map<int, std::shared_ptr<SgRbtNode>> IdToRbtNodeMap;
    IdToRbtNodeMap idToRbtNode_;

    int idCounter_;
    bool srgbFrameBuffer_;

    Drawer drawer_;

    void addToMap(int id, std::shared_ptr<SgRbtNode> node);
    std::shared_ptr<SgRbtNode> find(int id);
    Cvec3 idToColor(int id);
    int colorToId(const PackedPixel &p);

  public:
    Picker(const RigTForm &initialRbt, const ShaderState &curSS);

    virtual bool visit(SgTransformNode &node);
    virtual bool postVisit(SgTransformNode &node);
    virtual bool visit(SgShapeNode &node);
    virtual bool postVisit(SgShapeNode &node);

    std::shared_ptr<SgRbtNode> getRbtNodeAtXY(int x, int y);
};

#endif
