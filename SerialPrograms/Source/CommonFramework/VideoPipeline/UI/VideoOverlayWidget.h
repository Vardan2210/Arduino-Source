/*  Video Overlay Widget
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_VideoPipeline_VideoOverlayWidget_H
#define PokemonAutomation_VideoPipeline_VideoOverlayWidget_H

#include <map>
#include <QWidget>
#include "Common/Cpp/Concurrency/SpinLock.h"
#include "CommonFramework/VideoPipeline/VideoOverlaySession.h"

namespace PokemonAutomation{

struct OverlayText;

class VideoOverlayWidget : public QWidget, private VideoOverlaySession::Listener{
public:
    static constexpr bool DEFAULT_ENABLE_BOXES  = true;
    static constexpr bool DEFAULT_ENABLE_TEXT   = true;
    static constexpr bool DEFAULT_ENABLE_LOG    = false;
    static constexpr bool DEFAULT_ENABLE_STATS  = true;

public:
    ~VideoOverlayWidget();
    VideoOverlayWidget(QWidget& parent, VideoOverlaySession& session);

private:
    //  Asynchronous changes to the overlays.

    virtual void enabled_boxes(bool enabled) override;
    virtual void enabled_text (bool enabled) override;
    virtual void enabled_log  (bool enabled) override;
    virtual void enabled_stats(bool enabled) override;

    virtual void update_boxes(const std::shared_ptr<const std::vector<VideoOverlaySession::Box>>& boxes) override;
    virtual void update_text(const std::shared_ptr<const std::vector<OverlayText>>& texts) override;
    virtual void update_log_text(const std::shared_ptr<const std::vector<OverlayText>>& texts) override;
    virtual void update_log_background(const std::shared_ptr<const std::vector<VideoOverlaySession::Box>>& bg_boxes) override;
    virtual void update_stats(const std::list<OverlayStat*>* stats) override;

    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void paintEvent(QPaintEvent*) override;

private:
    VideoOverlaySession& m_session;

    SpinLock m_lock;
    std::shared_ptr<const std::vector<VideoOverlaySession::Box>> m_boxes;
    std::shared_ptr<const std::vector<OverlayText>> m_texts;
    std::shared_ptr<const std::vector<OverlayText>> m_log_texts;
    std::shared_ptr<const std::vector<VideoOverlaySession::Box>> m_log_text_bg_boxes;
    const std::list<OverlayStat*>* m_stats;
};


}
#endif

