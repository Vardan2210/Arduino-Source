/*  Mount Detection Test
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#ifndef PokemonAutomation_PokemonLA_MountDetectionTest_H
#define PokemonAutomation_PokemonLA_MountDetectionTest_H

#include "CommonFramework/Options/EnumDropdownOption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLA{



class MountDetectionTest_Descriptor : public SingleSwitchProgramDescriptor{
public:
    MountDetectionTest_Descriptor();
};


class MountDetectionTest : public SingleSwitchProgramInstance{
public:
    MountDetectionTest();

    virtual void program(SingleSwitchProgramEnvironment& env, BotBaseContext& context) override;

private:
    EnumDropdownOption FAILED_ACTION;
};



}
}
}
#endif
