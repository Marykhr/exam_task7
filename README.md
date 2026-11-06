# exam_task7

# Multithreaded HTTP Server

## О проекте

Многопоточный HTTP/1.1 сервер на C++20.

## Вариант

Вариант 7 — Корутины + пул потоков.

## Команда

| Участник | Обязанности |
| Okeoke | - |
| Lalala | - |

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

### Дальнейшая работа

- [ ] HTTP-сервер
- [ ] HTTP-парсер
- [ ] Все 8 API-ручек
- [ ] Thread pool
- [ ] Корутины
- [ ] Тесты
- [ ] Нагрузочное тестирование
