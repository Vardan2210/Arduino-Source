/*  Map PokeCenter Icon Detector
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonSV_MapPokeCenterIconDetector_H
#define PokemonAutomation_PokemonSV_MapPokeCenterIconDetector_H

#include <vector>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Containers/FixedLimitVector.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/InferenceInfra/VisualInferenceCallback.h"
#include "CommonFramework/Inference/VisualDetector.h"

namespace PokemonAutomation{

class VideoOverlaySet;
class VideoOverlay;
class OverlayBoxScope;

namespace NintendoSwitch{
namespace PokemonSV{

// The area on the screen that the program can clearly read map info, when the map is opened.
extern ImageFloatBox MAP_READABLE_AREA;


class MapPokeCenterIconDetector : public StaticScreenDetector{
public:
    MapPokeCenterIconDetector(Color color, const ImageFloatBox& box);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) const override;

    std::vector<ImageFloatBox> detect_all(const ImageViewRGB32& screen) const;

protected:
    Color m_color;
    ImageFloatBox m_box;
};



class MapPokeCenterIconWatcher : public VisualInferenceCallback{
public:
    ~MapPokeCenterIconWatcher();
    MapPokeCenterIconWatcher(Color color, VideoOverlay& overlay, const ImageFloatBox& box);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override;


protected:
    VideoOverlay& m_overlay;
    MapPokeCenterIconDetector m_detector;
    FixedLimitVector<OverlayBoxScope> m_hits;
};




}
}
}
#endif
