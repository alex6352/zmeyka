#pragma once

namespace snake {

// Запускает консольную игру и возвращает код завершения процесса.
// Аргументы командной строки поддерживают режим --autotest при тестовой сборке.
int runApplication(int argc, char** argv);

}  // namespace snake
