# exam_task7

# Multithreaded HTTP Server

## О проекте

Многопоточный HTTP/1.1 сервер на C++20.

## Вариант

Вариант 7 — Корутины + пул потоков.

## Команда

| Okeoke -  |
| Lalala - |

## Технологии

- C++20
- CMake
- Linux
- POSIX sockets
- std::thread
- std::mutex
- std::condition_variable
- C++20 Coroutines

## Сборка

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```


## Статус проекта

### Неделя 1

- [x] Создан репозиторий
- [x] Настроен CMake
- [x] Добавлена начальная структура проекта

### Неделя 2
Создан TCP-сокет

Настроен IP-адрес и порт сервера

Реализован приём подключений клиентов

Реализовано получение HTTP-запросов

Добавлен разбор HTTP-запросов

Реализован endpoint GET /health

Добавлен ответ 200 OK с JSON

Добавлен ответ 404 Not Found

Реализована поддержка HTTP/1.1 Keep-Alive

Добавлено корректное завершение сервера по SIGINT и SIGTERM

### Дальнейшая работа

- [ ] HTTP-сервер
- [ ] HTTP-парсер
- [ ] Все 8 API-ручек
- [ ] Thread pool
- [ ] Корутины
- [ ] Тесты
- [ ] Нагрузочное тестирование
