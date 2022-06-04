/*  Battle Pokemon Action Table
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */


#include "Common/Compiler.h"
#include "Common/Qt/QtJsonTools.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/Options/EditableTableOption-EnumTableCell.h"
#include "CommonFramework/Options/EnumDropdownWidget.h"
#include "Pokemon/Options/Pokemon_IVCheckerWidget.h"
#include "PokemonLA_BattlePokemonActionTable.h"

#include <QLabel>
#include <iostream>
using std::cout;
using std::endl;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLA{


const QString MoveStyle_NAMES[] = {
    "No Style",
    "Agile",
    "Strong",
};
const std::map<QString, MoveStyle> MoveStyle_MAP{
    {MoveStyle_NAMES[0], MoveStyle::NoStyle},
    {MoveStyle_NAMES[1], MoveStyle::Agile},
    {MoveStyle_NAMES[2], MoveStyle::Strong},
};


BattlePokemonActionRow::BattlePokemonActionRow() {}

void BattlePokemonActionRow::load_json(const QJsonValue& json){
    QJsonObject obj = json.toObject();
    {
        QString value;
        for(int i = 0; i < 4; i++){
            if (json_get_string(value, obj, "Style" + QString::number(i))){
                const auto iter = MoveStyle_MAP.find(value);
                if (iter != MoveStyle_MAP.end()){
                    style[i] = iter->second;
                }
            }
        }
    }

    json_get_bool(switch_pokemon, obj, "Switch");
    json_get_int(num_turns_to_switch, obj, "Turns", 0);
    json_get_bool(stop_after_num_moves, obj, "StopAfterNumMoves");
    json_get_int(num_moves_to_stop, obj, "NumMovesToStop", 0);
}

QJsonValue BattlePokemonActionRow::to_json() const{
    QJsonObject obj;
    for(int i = 0; i < 4; i++){
        obj.insert("Style"+QString::number(i), MoveStyle_NAMES[(size_t)style[i]]);
    }
    
    obj.insert("Switch", switch_pokemon);
    obj.insert("Turns", num_turns_to_switch);
    obj.insert("StopAfterNumMoves", stop_after_num_moves);
    obj.insert("NumMovesToStop", num_moves_to_stop);
    return obj;
}

std::unique_ptr<EditableTableRow> BattlePokemonActionRow::clone() const{
    return std::unique_ptr<EditableTableRow>(new BattlePokemonActionRow(*this));
}

std::vector<QWidget*> BattlePokemonActionRow::make_widgets(QWidget& parent){
    std::vector<QWidget*> widgets;
    for(int i = 0; i < 4; i++){
        widgets.emplace_back(make_enum_table_cell(parent, MoveStyle_MAP.size(), MoveStyle_NAMES, style[i]));
    }
    widgets.emplace_back(make_boolean_table_cell(parent, this->switch_pokemon));
    widgets.emplace_back(make_integer_table_cell(parent, this->num_turns_to_switch));
    widgets.emplace_back(make_boolean_table_cell(parent, this->stop_after_num_moves));
    widgets.emplace_back(make_integer_table_cell(parent, this->num_moves_to_stop));
    return widgets;
}



QStringList BattlePokemonActionTableFactory::make_header() const{
    QStringList list;
    list << "Move 1 Style" << "Move 2 Style" << "Move 3 Style" << "Move 4 Style" << 
        "Switch " + STRING_POKEMON << "Num Turns to Switch" << 
        "Limit Move Attempts" << "Max Move Attempts";
    return list;
}

std::unique_ptr<EditableTableRow> BattlePokemonActionTableFactory::make_row() const{
    return std::unique_ptr<EditableTableRow>(new BattlePokemonActionRow());
}




std::vector<std::unique_ptr<EditableTableRow>> BattlePokemonActionTable::make_defaults() const{
    std::vector<std::unique_ptr<EditableTableRow>> ret;
    ret.emplace_back(std::make_unique<BattlePokemonActionRow>());
    return ret;
}

BattlePokemonActionTable::BattlePokemonActionTable()
    : m_table(
        "<b>" + STRING_POKEMON + " Action Table:</b><br>"
        "Set what move styles to use and whether to switch the " + STRING_POKEMON + " after some turns.<br><br>"
        "Each row is the action for one " + STRING_POKEMON + ". "
        "The table follows the order that " + STRING_POKEMON + " are sent to battle.<br>"
        "You can also set a target number of move attempts. After it is reached the program will finish the battle and stop.<br><br>"
        "Note: if your second last " + STRING_POKEMON + " faints, the game will send your last " + STRING_POKEMON + " automatically for you.<br>"
        "The program cannot detect this switch as there is no switch selection screen. "
        "Therefore the program will treat it as the same " + STRING_POKEMON + ".",
        m_factory, make_defaults()
    )
{}

void BattlePokemonActionTable::load_json(const QJsonValue& json){
    m_table.load_json(json);
}

QJsonValue BattlePokemonActionTable::to_json() const{
    return m_table.to_json();
}

void BattlePokemonActionTable::restore_defaults(){
    m_table.restore_defaults();
}

ConfigWidget* BattlePokemonActionTable::make_ui(QWidget& parent){
    return m_table.make_ui(parent);
}

MoveStyle BattlePokemonActionTable::get_style(size_t pokemon, size_t move) const{
    if (pokemon >= m_table.size()){
        return MoveStyle::NoStyle;
    }

    const BattlePokemonActionRow& action = static_cast<const BattlePokemonActionRow&>(m_table[pokemon]);
    return action.style[move];
}

bool BattlePokemonActionTable::switch_pokemon(size_t pokemon, size_t num_turns) const{
    if (pokemon >= m_table.size()){
        return false;
    }

    const BattlePokemonActionRow& action = static_cast<const BattlePokemonActionRow&>(m_table[pokemon]);
    return action.switch_pokemon && num_turns >= (size_t)action.num_turns_to_switch;
}

bool BattlePokemonActionTable::stop_battle(size_t pokemon, size_t num_move_attempts) const{
    if (pokemon >= m_table.size()){
        return false;
    }

    const BattlePokemonActionRow& action = static_cast<const BattlePokemonActionRow&>(m_table[pokemon]);
    return action.stop_after_num_moves && num_move_attempts >= (size_t)action.num_moves_to_stop;
}



OneMoveBattlePokemonActionRow::OneMoveBattlePokemonActionRow() {}

void OneMoveBattlePokemonActionRow::load_json(const QJsonValue& json){
    QJsonObject obj = json.toObject();
    {
        QString value;
        if (json_get_string(value, obj, "Style")){
            const auto iter = MoveStyle_MAP.find(value);
            if (iter != MoveStyle_MAP.end()){
                style = iter->second;
            }
        }
    }
}

QJsonValue OneMoveBattlePokemonActionRow::to_json() const{
    QJsonObject obj;
    obj.insert("Style", MoveStyle_NAMES[(size_t)style]);
    return obj;
}

std::unique_ptr<EditableTableRow> OneMoveBattlePokemonActionRow::clone() const{
    return std::unique_ptr<EditableTableRow>(new OneMoveBattlePokemonActionRow(*this));
}

std::vector<QWidget*> OneMoveBattlePokemonActionRow::make_widgets(QWidget& parent){
    std::vector<QWidget*> widgets;
    widgets.emplace_back(make_enum_table_cell(parent, MoveStyle_MAP.size(), MoveStyle_NAMES, style));
    return widgets;
}



QStringList OneMoveBattlePokemonActionTableFactory::make_header() const{
    QStringList list;
    list << "Move Style";
    return list;
}

std::unique_ptr<EditableTableRow> OneMoveBattlePokemonActionTableFactory::make_row() const{
    return std::unique_ptr<EditableTableRow>(new OneMoveBattlePokemonActionRow());
}




std::vector<std::unique_ptr<EditableTableRow>> OneMoveBattlePokemonActionTable::make_defaults() const{
    std::vector<std::unique_ptr<EditableTableRow>> ret;
    ret.emplace_back(std::make_unique<OneMoveBattlePokemonActionRow>());
    return ret;
}

OneMoveBattlePokemonActionTable::OneMoveBattlePokemonActionTable()
    : m_table(
        "<b>" + STRING_POKEMON + " Action Table:</b><br>"
        "Set what move style to use for each " + STRING_POKEMON + " to grind against a Magikarp. "
        "Each row is the action for one " + STRING_POKEMON + ". "
        "The table follows the order that " + STRING_POKEMON + " are sent to battle.",
        m_factory, make_defaults()
    )
{}

void OneMoveBattlePokemonActionTable::load_json(const QJsonValue& json){
    m_table.load_json(json);
}

QJsonValue OneMoveBattlePokemonActionTable::to_json() const{
    return m_table.to_json();
}

void OneMoveBattlePokemonActionTable::restore_defaults(){
    m_table.restore_defaults();
}

ConfigWidget* OneMoveBattlePokemonActionTable::make_ui(QWidget& parent){
    return m_table.make_ui(parent);
}

MoveStyle OneMoveBattlePokemonActionTable::get_style(size_t pokemon){
    const OneMoveBattlePokemonActionRow& action = static_cast<const OneMoveBattlePokemonActionRow&>(m_table[pokemon]);
    return action.style;
}


const QString MoveIndex_NAMES[] = {
    "First move",
    "Second move",
    "Third move",
    "Fourth move",
};

void MoveGrinderActionRow::load_json(const QJsonValue& json)
{
    QJsonObject obj = json.toObject();
    json_get_int(pokemon_index, obj, "PokemonIndex", 0);
    json_get_int(move_index, obj, "MoveIndex", 0);
    {
        QString value;
        if (json_get_string(value, obj, "Style")) {
            const auto iter = MoveStyle_MAP.find(value);
            if (iter != MoveStyle_MAP.end()) {
                style = iter->second;
            }
        }
    }
    json_get_int(attemps, obj, "Attempts", 0);
}

QJsonValue MoveGrinderActionRow::to_json() const
{
    QJsonObject obj;
    obj.insert("PokemonIndex", static_cast<uint16_t>(pokemon_index));
    obj.insert("MoveIndex", static_cast<uint16_t>(move_index));
    obj.insert("Style", MoveStyle_NAMES[(size_t)style]);
    obj.insert("Attempts", attemps);
    return obj;
}

std::unique_ptr<EditableTableRow> MoveGrinderActionRow::clone() const
{
    return std::unique_ptr<EditableTableRow>(new MoveGrinderActionRow(*this));
}

std::vector<QWidget*> MoveGrinderActionRow::make_widgets(QWidget& parent)
{
    const QString PokemonIndex_NAMES[] = {
        "First " + STRING_POKEMON,
        "Second " + STRING_POKEMON,
        "Third " + STRING_POKEMON,
        "Fourth " + STRING_POKEMON,
    };

    std::vector<QWidget*> widgets;
    widgets.emplace_back(make_enum_table_cell(parent, 4, PokemonIndex_NAMES, pokemon_index));
    widgets.emplace_back(make_enum_table_cell(parent, 4, MoveIndex_NAMES, move_index));
    widgets.emplace_back(make_enum_table_cell(parent, MoveStyle_MAP.size(), MoveStyle_NAMES, style));
    widgets.emplace_back(make_integer_table_cell(parent, attemps));
    return widgets;
}

QStringList MoveGrinderActionTableFactory::make_header() const
{
    QStringList list;
    list << "Pokemon index" << "Move index" << "Move Style" << "Move Attempts";
    return list;
}

std::unique_ptr<EditableTableRow> MoveGrinderActionTableFactory::make_row() const
{
    return std::unique_ptr<EditableTableRow>(new MoveGrinderActionRow());
}

MoveGrinderActionTable::MoveGrinderActionTable()
    : m_table("For every move you want to perform, input the style and the number of attemps you want to achieve.", m_factory)
{}

Move MoveGrinderActionTable::get_move(size_t pokemon, size_t move) const
{
    // Pokemon index 4 is gonna be Arceus with powerful moves, just use them with normal style and hope you'll win the battle
    if (pokemon == 4)
    {
        return {MoveStyle::NoStyle, std::numeric_limits<decltype(Move::attemps)>::max()};

    }
    for (size_t i = 0; i < m_table.size(); ++i)
    {
        const MoveGrinderActionRow& action = static_cast<const MoveGrinderActionRow&>(m_table[i]);
        if (action.pokemon_index != pokemon)
        {
            continue;
        }
        if (action.move_index != move)
        {
            continue;
        }
        return {action.style , action.attemps};
    }
    return {MoveStyle::NoStyle, 0};
}

void MoveGrinderActionTable::load_json(const QJsonValue& json) {
    m_table.load_json(json);
}

QJsonValue MoveGrinderActionTable::to_json() const {
    return m_table.to_json();
}

ConfigWidget* MoveGrinderActionTable::make_ui(QWidget& parent) {
    return m_table.make_ui(parent);
}

}
}
}
