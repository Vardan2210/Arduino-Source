/*  Parent Class for all Programs
 *
 *  From: https://github.com/PokemonAutomation/Arduino-Source
 *
 */

#include <iostream>
using std::cout;
using std::endl;

#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include "Common/Cpp/Exception.h"
#include "Common/Qt/QtJsonTools.h"
#include "Tools/Tools.h"
#include "Tools/PersistentSettings.h"
#include "UI/MainWindow.h"
#include "Program.h"

namespace PokemonAutomation{


const QString Program::JSON_PROGRAM_NAME    = "0-ProgramName";
const QString Program::JSON_DESCRIPTION     = "1-Description";
const QString Program::JSON_PARAMETERS      = "2-Parameters";


const QString Program::BUILD_BUTTON_NORMAL  = "Save and generate .hex file!";
const QString Program::BUILD_BUTTON_BUSY    = "Build in progress... Please Wait.";

Program::Program(QString category, QString name, QString description)
    : RightPanel(std::move(category), std::move(name))
    , m_description(std::move(description))
{}
Program::Program(QString category, const QJsonObject& obj)
    : RightPanel(std::move(category), json_get_string_throw(obj, JSON_PROGRAM_NAME))
    , m_description(json_get_string_throw(obj, JSON_DESCRIPTION))
{}
Program::~Program(){
    if (m_builder.joinable()){
        m_builder.join();
    }
}

QWidget* Program::make_options_body(QWidget& parent){
    QWidget* box = new QWidget(&parent);
    QVBoxLayout* layout = new QVBoxLayout(box);
    QLabel* label = new QLabel("There are no program-specific options for this program.");
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    return box;
}

QJsonDocument Program::to_json() const{
    QJsonObject root;
    root.insert(JSON_PROGRAM_NAME, m_name);
    root.insert(JSON_DESCRIPTION, m_description);
    root.insert(JSON_PARAMETERS, parameters_json());
    return QJsonDocument(root);
}
std::string Program::to_cfile() const{
    std::string body;
    body += "//  This file is generated by the UI. There's no point in editing.\r\n";
    body += "#include \"";
    body += "../NativePrograms/";
    body += m_category.toUtf8().data();
    body += "/Programs/";
    body += m_name.toUtf8().data();
    body += ".h\"\r\n";
    body += parameters_cpp();
    return body;
}
QString Program::save_json() const{
//    cout << QCoreApplication::applicationDirPath().toUtf8().data() << endl;
    QString name = settings.path + CONFIG_FOLDER_NAME + "/" + m_category + "/" + m_name + ".json";
    write_json_file(name, to_json());
    return name;
}
QString Program::save_cfile() const{
    QString name = settings.path + SOURCE_FOLDER_NAME + "/" + m_category + "/" + m_name + ".c";
    std::string cpp = to_cfile();
    QFile file(name);
    if (!file.open(QFile::WriteOnly)){
        PA_THROW_FileException("Unable to create source file.", name);
    }
    if (file.write(cpp.c_str(), cpp.size()) != cpp.size()){
        PA_THROW_FileException("Unable to write source file.", name);
    }
    file.close();
    return name;
}

class ButtonRestore{
public:
    ButtonRestore(QPushButton* button)
        : m_button(button)
    {}
    ~ButtonRestore(){
        QPushButton* button = m_button;
        run_on_main_thread([=]{
            button->setText(Program::BUILD_BUTTON_NORMAL);
        });
    }
private:
    QPushButton* m_button;
};

void Program::save_and_build(const std::string& board){
//        cout << "asdf" << endl;
    if (!is_valid()){
        QMessageBox box;
        box.critical(nullptr, "Error", "The current settings are invalid.");
        return;
    }
//    if (!QDir(settings.config_path).exists()){
//        QMessageBox box;
//        box.critical(nullptr, "Error", "Source directory not found. Please unzip the package if you haven't already.");
//        return;
//    }

    if (m_builder.joinable()){
        m_builder.join();
    }

    m_build_button->setText(BUILD_BUTTON_BUSY);
    m_builder = std::thread([=]{
        ButtonRestore restore(m_build_button);
        try{
            save_json();
            save_cfile();
        }catch (const StringException& e){
            QString error = e.message_qt();
            run_on_main_thread([=]{
                QMessageBox box;
                box.critical(nullptr, "Error", error);
            });
            return;
        }

        //  Build
        QString hex_file = settings.path + m_name + ("-" + board + ".hex").c_str();
        QString log_file = settings.path + LOG_FOLDER_NAME + "/" + m_name + ("-" + board).c_str() + ".log";
        if (build_hexfile(board, m_category, m_name, hex_file, log_file) != 0){
            return;
        }

        //  Report results.
        run_on_main_thread([=]{
            if (QFileInfo(hex_file).exists()){
                QMessageBox box;
                box.information(nullptr, "Success!", ".hex file has been built!\r\n" + hex_file);
            }else{
                QMessageBox box;
                box.critical(nullptr, "Error", ".hex was not built. Please check error log.\r\n" + log_file);
            }
        });
    });
}

QWidget* Program::make_ui(MainWindow& parent){
    QWidget* box = new QWidget(&parent);
    QVBoxLayout* layout = new QVBoxLayout(box);
    layout->setMargin(0);
//    layout->setAlignment(Qt::AlignTop);
    layout->addWidget(new QLabel("<font size=4><b>Name:</b></font> " + m_name));
    {
        QLabel* text = new QLabel("<font size=4><b>Description:</b></font> " + m_description);
        layout->addWidget(text);
        text->setWordWrap(true);
    }
    {
        QString path = GITHUB_REPO + "/blob/master/Documentation/NativePrograms/" + m_name + ".md";
        QLabel* text = new QLabel("<font size=4><a href=\"" + path + "\">Online Documentation for " + m_name + "</a></font>");
        layout->addWidget(text);
        text->setTextFormat(Qt::RichText);
        text->setTextInteractionFlags(Qt::TextBrowserInteraction);
        text->setOpenExternalLinks(true);
    }
    layout->addWidget(make_options_body(*box), Qt::AlignBottom);

    QHBoxLayout* row = new QHBoxLayout();
    layout->addLayout(row);
    {
        QPushButton* button = new QPushButton("Save Settings", box);
        connect(
            button, &QPushButton::clicked,
            this, [&](bool){
                try{
                    QString name = save_json();
                    QMessageBox box;
                    box.information(nullptr, "Success!", "Settings saved to: " + name + "\n");
                }catch (const char* str){
                    QMessageBox box;
                    box.critical(nullptr, "Error", str);
                    return;
                }catch (const QString& str){
                    QMessageBox box;
                    box.critical(nullptr, "Error", str);
                    return;
                }
            }
        );
        row->addWidget(button);
    }
    {
        QPushButton* button = new QPushButton("Restore Defaults", box);
        connect(
            button, &QPushButton::clicked,
            this, [&](bool){
                restore_defaults();
                parent.change_panel(*this);
            }
        );
        row->addWidget(button);
    }
    {
        m_build_button = new QPushButton(BUILD_BUTTON_NORMAL, box);
        QFont font = m_build_button->font();
        font.setPointSize(16);
        m_build_button->setFont(font);
        connect(
            m_build_button, &QPushButton::clicked,
            this, [&](bool){
                save_and_build(parent.current_board());
            }
        );
        layout->addWidget(m_build_button);
    }
    return box;
}


}
