#include "Common/PokemonSwSh/PokemonProgramIDs.h"
#include "NativePrograms/DeviceFramework/DeviceSettings.h"
#include "NativePrograms/NintendoSwitch/Libraries/FrameworkSettings.h"
#include "NativePrograms/NintendoSwitch/Libraries/NintendoSwitch_Device.h"
#include "NativePrograms/NintendoSwitch/Libraries/NintendoSwitch_PushButtons.h"
#include "NativePrograms/NintendoSwitch/Programs/CustomProgram.h"
#include "NativePrograms/PokemonSwSh/Libraries/PokemonSwSh_Settings.h"



int main(void){
  
    start_program_callback();
    initialize_framework(0);

    //  Start the program in the grip menu. Then go home.
    start_program_flash(CONNECT_CONTROLLER_DELAY);
    grip_menu_connect_go_home();

    //  Enter the game.
    pbf_press_button(BUTTON_HOME, 10, PokemonSwSh_HOME_TO_GAME_DELAY);

    //  It will alternate between attacking and backing out of the pokemon selection screen
    while (true){
        pbf_mash_button(BUTTON_A, 1875);
	    pbf_mash_button(BUTTON_B, 650);
    }
};
