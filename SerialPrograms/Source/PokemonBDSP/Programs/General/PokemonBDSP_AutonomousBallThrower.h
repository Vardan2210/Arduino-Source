/*  Autonomous Ball Thrower
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutonomousStatsReset_H
#define PokemonAutomation_PokemonBDSP_AutonomousStatsReset_H

#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "CommonFramework/OCR/OCR_LanguageOptionOCR.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/GoHomeWhenDoneOption.h"
#include "NintendoSwitch/Options/StartInGripMenuOption.h"
#include "Pokemon/Options/Pokemon_BallSelectOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace Pokemon;


class AutonomousBallThrower_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutonomousBallThrower_Descriptor();

    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};



class AutonomousBallThrower : public SingleSwitchProgramInstance{
public:
    AutonomousBallThrower();
    virtual void program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) override;

private:

    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;
    PokemonBallSelect BALL_SELECT;
    OCR::LanguageOCR LANGUAGE;

    EventNotificationOption NOTIFICATION_CATCH_SUCCESS;
    EventNotificationOption NOTIFICATION_CATCH_FAILED;
    EventNotificationsOption NOTIFICATIONS;
};


}
}
}
#endif
