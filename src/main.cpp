#include "main_window.h"
#include <QApplication>
#include <QDesktopWidget>

/**
 * @brief Entry point of the quest editor.
 *
 * The argument if any is the path of the quest to load.
 *
 * @param argc Number of arguments of the command line.
 * @param argv Command-line arguments.
 * @return 0 in case of success.
 */
int main(int argc, char* argv[]) {

  QString quest_path;
  if (argc > 1) {
    quest_path = argv[1];
  }

  QApplication application(argc, argv);
  MainWindow window;
  window.get_quest_manager().set_quest_path(quest_path);

  // Center the window initially.
  window.setGeometry(
        QStyle::alignedRect(
          Qt::LeftToRight,
          Qt::AlignCenter,
          window.size(),
          application.desktop()->availableGeometry()
          )
        );
  window.show();

  return application.exec();
}