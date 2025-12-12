#include <QApplication>
#include <QMessageBox>
#include <unistd.h>
#include <QDebug>
#include <QCommandLineParser>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    // Создаем QApplication ПЕРВЫМ делом
    QApplication app(argc, argv);
    app.setApplicationName("C-mile");
    app.setApplicationVersion("0.9.5");

    // Парсер аргументов командной строки
    QCommandLineParser parser;
    parser.setApplicationDescription("C-mile v0.9.5");
    parser.addHelpOption();
    parser.addVersionOption();

    // Опция для запуска без проверки root (только для отладки)
    QCommandLineOption noRootCheckOption("no-root-check",
                                         "Запуск без проверки прав root (только для отладки)");
    parser.addOption(noRootCheckOption);

    parser.process(app);

    // Проверка root (если не указана опция no-root-check)
    if (!parser.isSet(noRootCheckOption) && geteuid() != 0) {
        QMessageBox::critical(nullptr,
                              "Ошибка прав доступа",
                              "Для работы программы требуются права администратора (root).\n\n"
                              "Запустите программу командой:\n"
                              "sudo " + QCoreApplication::applicationFilePath() + "\n\n"
                              "Или для отладки:\n"
                              + QCoreApplication::applicationFilePath() + " --no-root-check");
        return 1;
    }

    qDebug() << "Запуск C-mile v0.9.5";

    try {
        MainWindow mainWindow;
        mainWindow.show();

        qDebug() << "Главное окно создано успешно";
        return app.exec();

    } catch (const std::exception& e) {
        qCritical() << "Необработанное исключение:" << e.what();
        QMessageBox::critical(nullptr,
                              "Критическая ошибка",
                              QString("Произошла непредвиденная ошибка:\n\n%1\n\n"
                              "Программа будет закрыта.").arg(e.what()));
        return 1;
    } catch (...) {
        qCritical() << "Неизвестное исключение";
        QMessageBox::critical(nullptr,
                              "Критическая ошибка",
                              "Произошла неизвестная ошибка.\nПрограмма будет закрыта.");
        return 1;
    }
}
