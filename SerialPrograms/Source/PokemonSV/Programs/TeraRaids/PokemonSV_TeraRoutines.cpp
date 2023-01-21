/*  Tera Exit Routines
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <cmath>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/Tools/ProgramEnvironment.h"
#include "CommonFramework/InferenceInfra/InferenceRoutines.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Notification.h"
#include "PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
//#include "PokemonSV/Inference/PokemonSV_GradientArrowDetector.h"
#include "PokemonSV/Inference/Battles/PokemonSV_BattleMenuDetector.h"
#include "PokemonSV/Inference/PokemonSV_PokemonSummaryReader.h"
#include "PokemonSV/Inference/Tera/PokemonSV_TeraCardDetector.h"
#include "PokemonSV/Inference/PokemonSV_MainMenuDetector.h"
#include "PokemonSV/Inference/PokemonSV_OverworldDetector.h"
#include "PokemonSV/Inference/PokemonSV_PokePortalDetector.h"
#include "PokemonSV/Inference/Tera/PokemonSV_TeraRaidSearchDetector.h"
#include "PokemonSV/Programs/PokemonSV_ConnectToInternet.h"
#include "PokemonSV/Programs/PokemonSV_BasicCatcher.h"
#include "PokemonSV_TeraRoutines.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

using namespace Pokemon;



bool open_raid(ConsoleHandle& console, BotBaseContext& context){
    console.log("Opening raid...");

    TeraCardWatcher card_detector(COLOR_RED);
    int ret = run_until(
        console, context,
        [](BotBaseContext& context){
            pbf_press_button(context, BUTTON_A, 20, 355);
        },
        {card_detector}
    );
    if (ret < 0){
        console.log("No Tera raid found.", COLOR_ORANGE);
        return false;
    }

    console.log("Tera raid found!", COLOR_BLUE);
    return true;
}
void close_raid(const ProgramInfo& info, ConsoleHandle& console, BotBaseContext& context){
    console.log("Closing raid...");

    WallClock start = current_time();
    while (true){
        context.wait_for_all_requests();
        if (current_time() - start > std::chrono::minutes(5)){
            dump_image_and_throw_recoverable_exception(
                info, console, "CloseRaidFailed",
                "Failed to return to overworld after 5 minutes."
            );
        }

        TeraCardWatcher card_detector(COLOR_RED);
        OverworldWatcher overworld(COLOR_CYAN);
        int ret = wait_until(
            console, context,
            std::chrono::seconds(60),
            {card_detector, overworld}
        );
        context.wait_for(std::chrono::milliseconds(100));
        switch (ret){
        case 0:
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 1:
            console.log("Detected overworld.");
            return;
        default:
            dump_image_and_throw_recoverable_exception(info, console, "CloseRaidFailed",
                "close_raid(): No recognized state after 60 seconds.");
        }
    }
}



void open_hosting_lobby(
    ProgramEnvironment& env, ConsoleHandle& console, BotBaseContext& context,
    HostingMode mode
){
    bool recovery_mode = false;
    WallClock start = current_time();
    while (true){
        context.wait_for_all_requests();
        if (current_time() - start > std::chrono::minutes(5)){
            dump_image_and_throw_recoverable_exception(
                env.program_info(), console, "OpenLobbyFailed",
                "Unable to open Tera lobby after 5 minutes."
            );
        }

        OverworldWatcher overworld(COLOR_RED);
        if (recovery_mode){
            context.wait_for_all_requests();
            int ret = run_until(
                console, context,
                [](BotBaseContext& context){
                    pbf_press_button(context, BUTTON_B, 20, 355);
                },
                {overworld}
            );
            if (ret < 0){
                continue;
            }
            console.log("Detected overworld. Recovery finished.");
            recovery_mode = true;
        }

        TeraCardWatcher card_detector(COLOR_YELLOW);
        TeraLobbyWatcher lobby(console.logger(), env.realtime_dispatcher(), COLOR_BLUE);
        context.wait_for_all_requests();
        int ret = wait_until(
            console, context,
            std::chrono::seconds(30),
            {overworld, card_detector, lobby}
        );
        context.wait_for(std::chrono::milliseconds(100));
        switch (ret){
        case 0:
            console.log("Detected overworld.");
            recovery_mode = false;
            if (!open_raid(console, context)){
                throw OperationFailedException(console, "No Tera raid found.");
            }
            continue;
        case 1:
            console.log("Detected Tera card.");
            if (mode != HostingMode::LOCAL){
                pbf_press_button(context, BUTTON_A, 20, 230);
                if (mode == HostingMode::ONLINE_EVERYONE){
                    pbf_press_dpad(context, DPAD_DOWN, 20, 105);
                }
            }
            pbf_press_button(context, BUTTON_A, 20, 230);
            continue;
        case 2:
            console.log("Detected Tera lobby.");
            return;
        default:
            console.log("No state detected after 30 seconds. Backing out...", COLOR_RED);
            pbf_press_button(context, BUTTON_B, 20, 230);
            recovery_mode = true;
        }
    }
}





void enter_tera_search(
    const ProgramInfo& info, ConsoleHandle& console, BotBaseContext& context,
    bool connect_to_internet
){
    WallClock start = current_time();
    bool connected = false;
    while (true){
        if (current_time() - start > std::chrono::minutes(5)){
            dump_image_and_throw_recoverable_exception(
                info, console, "EnterTeraSearchFailed",
                "enter_tera_search(): Failed to enter Tera search."
            );
        }

        OverworldWatcher overworld(COLOR_RED);
        MainMenuWatcher main_menu(COLOR_YELLOW);
        PokePortalWatcher poke_portal(COLOR_GREEN);
        TeraRaidSearchWatcher raid_search(COLOR_CYAN);
        CodeEntryWatcher code_entry(COLOR_PURPLE);
        context.wait_for_all_requests();
        int ret = wait_until(
            console, context,
            std::chrono::seconds(60),
            {
                overworld,
                main_menu,
                poke_portal,
                raid_search,
                code_entry,
            }
        );
        context.wait_for(std::chrono::milliseconds(100));
        switch (ret){
        case 0:
            console.log("Detected overworld.");
            pbf_press_button(context, BUTTON_X, 20, 105);
            continue;
        case 1:
            console.log("Detected main menu.");
            if (connect_to_internet && !connected){
                connect_to_internet_from_menu(info, console, context);
                connected = true;
                continue;
            }
            if (main_menu.move_cursor(info, console, context, MenuSide::RIGHT, 3)){
                pbf_press_button(context, BUTTON_A, 20, 230);
            }
            continue;
        case 2:
            console.log("Detected Poke Portal.");
            if (poke_portal.move_cursor(info, console, context, 1)){
                pbf_press_button(context, BUTTON_A, 20, 230);
            }
            continue;
        case 3:
            console.log("Detected Tera Raid Search.");
            if (raid_search.move_cursor_to_search(info, console, context)){
                pbf_press_button(context, BUTTON_A, 20, 105);
            }
            continue;
        case 4:
            console.log("Detected Code Entry.");
            return;
        default:
            dump_image_and_throw_recoverable_exception(
                info, console, "EnterTeraSearchFailed",
                "enter_tera_search(): No recognized state after 60 seconds."
            );
        }
    }
}





void exit_tera_win_without_catching(
    const ProgramInfo& info,
    ConsoleHandle& console,
    BotBaseContext& context
){
    console.log("Exiting raid without catching...");

    WallClock start = current_time();
    while (true){
        context.wait_for_all_requests();
        if (current_time() - start > std::chrono::minutes(5)){
            dump_image_and_throw_recoverable_exception(
                info, console, "ExitTeraWinFailed",
                "Failed to return to overworld after 5 minutes."
            );
        }

        TeraCatchWatcher catch_menu(COLOR_BLUE);
        WhiteButtonWatcher next_button(
            COLOR_CYAN,
            WhiteButton::ButtonA,
            {0.8, 0.93, 0.2, 0.07},
            WhiteButtonWatcher::FinderType::PRESENT,
            std::chrono::seconds(1)
        );
        AdvanceDialogWatcher dialog(COLOR_YELLOW);
        OverworldWatcher overworld(COLOR_RED);
        int ret = wait_until(
            console, context,
            std::chrono::seconds(60),
            {catch_menu, next_button, dialog, overworld}
        );
        context.wait_for(std::chrono::milliseconds(100));
        switch (ret){
        case 0:
            console.log("Detected catch prompt.");
            pbf_press_dpad(context, DPAD_DOWN, 20, 30);
            pbf_press_button(context, BUTTON_A, 20, 10);
            pbf_mash_button(context, BUTTON_B, 125);
            continue;
        case 1:
            console.log("Detected possible (A) Next button.");
            pbf_press_button(context, BUTTON_A, 20, 105);
            continue;
        case 2:
            console.log("Detected dialog.");
            pbf_press_button(context, BUTTON_B, 20, 105);
            break;
        case 3:
            console.log("Detected overworld.");
            return;
        default:
            dump_image_and_throw_recoverable_exception(
                info, console, "ExitTeraWinFailed",
                "exit_tera_win_without_catching(): No recognized state after 60 seconds."
            );
        }
    }
}


void exit_tera_win_by_catching(
    ProgramEnvironment& env,
    ConsoleHandle& console,
    BotBaseContext& context,
    Language language,
    const std::string& ball_slug
){
    console.log("Exiting raid with catching...");

    VideoSnapshot screenshot;
    WallClock start = current_time();
    while (true){
        context.wait_for_all_requests();
        if (current_time() - start > std::chrono::minutes(5)){
            dump_image_and_throw_recoverable_exception(env.program_info(), console, "ExitTeraWinFailed",
                "Failed to return to overworld after 5 minutes.");
        }

        TeraCatchWatcher catch_menu(COLOR_BLUE);
        WhiteButtonWatcher next_button(
            COLOR_CYAN,
            WhiteButton::ButtonA,
            {0.8, 0.93, 0.2, 0.07},
            WhiteButtonWatcher::FinderType::PRESENT,
            std::chrono::seconds(1)
        );
        AdvanceDialogWatcher advance(COLOR_YELLOW);
        PromptDialogWatcher add_to_party(COLOR_PURPLE, {0.500, 0.395, 0.400, 0.100});
        PromptDialogWatcher nickname(COLOR_GREEN, {0.500, 0.545, 0.400, 0.100});
        MainMenuWatcher main_menu(COLOR_BLUE);
        OverworldWatcher overworld(COLOR_RED);
        context.wait_for_all_requests();
        int ret = wait_until(
            console, context,
            std::chrono::seconds(60),
            {
                catch_menu,
                next_button,
                advance,
                add_to_party,
                nickname,
                main_menu,
                overworld,
            }
        );
        context.wait_for(std::chrono::milliseconds(100));
        switch (ret){
        case 0:{
            console.log("Detected catch prompt.");
            screenshot = console.video().snapshot();

            pbf_press_button(context, BUTTON_A, 20, 150);
            context.wait_for_all_requests();

            BattleBallReader reader(console, language);
            int quantity = move_to_ball(reader, console, context, ball_slug);
            if (quantity == 0){
                throw FatalProgramException(console.logger(), "Unable to find appropriate ball. Did you run out?");
            }
            if (quantity < 0){
                console.log("Unable to read ball quantity.", COLOR_RED);
            }
            pbf_mash_button(context, BUTTON_A, 125);

            continue;
        }
        case 1:
            console.log("Detected (A) Next button.");
            pbf_press_button(context, BUTTON_A, 20, 105);
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 2:
            console.log("Detected dialog.");
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 3:
            console.log("Detected add-to-party prompt.");
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 4:
            console.log("Detected nickname prompt.");
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 5:
            console.log("Detected unexpected main menu.", COLOR_RED);
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 6:
            console.log("Detected overworld.");
            return;
        default:
            dump_image_and_throw_recoverable_exception(
                env.program_info(), console, "ExitTeraWinFailed",
                "exit_tera_win_by_catching(): No recognized state after 60 seconds."
            );
        }
    }
}


TeraResult exit_tera_win_by_catching(
    ProgramEnvironment& env,
    ConsoleHandle& console,
    BotBaseContext& context,
    Language language,
    const std::string& ball_slug,
    EventNotificationOption& notification_nonshiny,
    EventNotificationOption& notification_shiny,
    bool stop_on_shiny,
    std::atomic<uint64_t>* stat_shinies
){
    console.log("Exiting raid with catching...");

    TeraResult result = TeraResult::NO_DETECTION;
    VideoSnapshot screenshot;
    WallClock start = current_time();
    while (true){
        context.wait_for_all_requests();
        if (current_time() - start > std::chrono::minutes(5)){
            dump_image_and_throw_recoverable_exception(env.program_info(), console, "ExitTeraWinFailed",
                "Failed to return to overworld after 5 minutes.");
        }

        TeraCatchWatcher catch_menu(COLOR_BLUE);
        WhiteButtonWatcher next_button(
            COLOR_CYAN,
            WhiteButton::ButtonA,
            {0.8, 0.93, 0.2, 0.07},
            WhiteButtonWatcher::FinderType::PRESENT,
            std::chrono::seconds(1)
        );
        AdvanceDialogWatcher advance(COLOR_YELLOW);
        PromptDialogWatcher add_to_party(COLOR_PURPLE, {0.500, 0.395, 0.400, 0.100});
        PromptDialogWatcher view_summary(COLOR_PURPLE, {0.500, 0.470, 0.400, 0.100});
        PromptDialogWatcher nickname(COLOR_GREEN, {0.500, 0.545, 0.400, 0.100});
        PokemonSummaryWatcher summary(COLOR_MAGENTA);
        MainMenuWatcher main_menu(COLOR_BLUE);
        OverworldWatcher overworld(COLOR_RED);
        context.wait_for_all_requests();
        int ret = wait_until(
            console, context,
            std::chrono::seconds(60),
            {
                catch_menu,
                next_button,
                advance,
                add_to_party,
                view_summary,
                nickname,
                summary,
                main_menu,
                overworld,
            }
        );
        context.wait_for(std::chrono::milliseconds(100));
        switch (ret){
        case 0:{
            console.log("Detected catch prompt.");
            screenshot = console.video().snapshot();

            pbf_press_button(context, BUTTON_A, 20, 150);
            context.wait_for_all_requests();

            BattleBallReader reader(console, language);
            int quantity = move_to_ball(reader, console, context, ball_slug);
            if (quantity == 0){
                throw FatalProgramException(console.logger(), "Unable to find appropriate ball. Did you run out?");
            }
            if (quantity < 0){
                console.log("Unable to read ball quantity.", COLOR_RED);
            }
            pbf_mash_button(context, BUTTON_A, 125);

            continue;
        }
        case 2:
            console.log("Detected dialog.");
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 3:
            console.log("Detected add-to-party prompt.");
            if (result == TeraResult::NO_DETECTION){
                pbf_press_dpad(context, DPAD_DOWN, 20, 60);
                pbf_press_button(context, BUTTON_A, 20, 105);
            }else{
                pbf_press_button(context, BUTTON_B, 20, 105);
            }
            continue;
        case 4:
            console.log("Detected cursor over add-to-party. (unexpected), COLOR_RED");
            pbf_press_button(context, BUTTON_A, 20, 105);
            continue;
        case 5:
            console.log("Detected nickname prompt.");
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 1:
            //  Next button detector is unreliable. Check if the summary is
            //  open. If so, fall-through to that.
            if (!summary.detect(console.video().snapshot())){
                console.log("Detected possible (A) Next button.");
                pbf_press_button(context, BUTTON_A, 20, 105);
                pbf_press_button(context, BUTTON_B, 20, 105);
                break;
            }
            console.log("Detected false positive (A) Next button.", COLOR_RED);
        case 6:
            console.log("Detected summary.");
            if (result == TeraResult::NO_DETECTION){
                context.wait_for(std::chrono::milliseconds(500));
                result = run_tera_summary(
                    env, console, context,
                    notification_nonshiny,
                    notification_shiny,
                    stop_on_shiny, screenshot,
                    stat_shinies
                );
            }
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 7:
            console.log("Detected unexpected main menu.", COLOR_RED);
            pbf_press_button(context, BUTTON_B, 20, 105);
            continue;
        case 8:
            console.log("Detected overworld.");
            return result;
        default:
            dump_image_and_throw_recoverable_exception(env.program_info(), console, "ExitTeraWinFailed",
                "exit_tera_win_by_catching(): No recognized state after 60 seconds.");
        }
    }
}




TeraResult run_tera_summary(
    ProgramEnvironment& env,
    ConsoleHandle& console,
    BotBaseContext& context,
    EventNotificationOption& notification_nonshiny,
    EventNotificationOption& notification_shiny,
    bool stop_on_shiny, const ImageViewRGB32& battle_screenshot,
    std::atomic<uint64_t>* stat_shinies
){
    console.log("Reading summary...");

    VideoSnapshot screen = console.video().snapshot();
    PokemonSummaryDetector reader;
    if (reader.is_shiny(screen)){
        if (stat_shinies != nullptr){
            (*stat_shinies)++;
        }
        send_encounter_notification(
            env,
            notification_nonshiny,
            notification_shiny,
            false, true,
            {{{}, ShinyType::UNKNOWN_SHINY}},
            std::nan(""),
            battle_screenshot
        );
        if (stop_on_shiny){
            throw ProgramFinishedException();
        }
        return TeraResult::SHINY;
    }else{
        send_encounter_notification(
            env,
            notification_nonshiny,
            notification_shiny,
            false, false,
            {{{}, ShinyType::NOT_SHINY}},
            std::nan("")
        );
        return TeraResult::NOT_SHINY;
    }
}














}
}
}
